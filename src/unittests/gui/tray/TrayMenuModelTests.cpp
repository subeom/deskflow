/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayMenuModelTests.h"

#include "gui/tray/TrayMenuModel.h"

#include <QSet>

using namespace deskflow::gui::tray;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;
using Kind = TrayMenuEntry::Kind;

namespace {

TrayInput makeInput(ProcessState process, ConnectionState connection, bool server = true, bool windowShown = true)
{
  TrayInput input;
  input.process = process;
  input.connection = connection;
  input.isServer = server;
  input.windowShown = windowShown;
  input.appName = QStringLiteral("Deskflow");
  input.headline = QStringLiteral("Headline");
  input.detail = QStringLiteral("The sentence behind it");
  input.labels = {
      {TrayCommand::Start, server ? QStringLiteral("Start") : QStringLiteral("Connect")},
      {TrayCommand::Restart, QStringLiteral("Rest&art")},
      {TrayCommand::Stop, server ? QStringLiteral("Stop") : QStringLiteral("Disconnect")},
      {TrayCommand::TestConnection, QStringLiteral("Test connection")},
      {TrayCommand::OpenWindow, QStringLiteral("&Open Deskflow")},
      {TrayCommand::Minimize, QStringLiteral("&Minimize to tray")},
      {TrayCommand::Preferences, QStringLiteral("&Preferences")},
      {TrayCommand::Quit, QStringLiteral("&Quit")},
  };
  input.shortcuts = {
      {TrayCommand::Start, QKeySequence(QStringLiteral("Ctrl+S"))},
      {TrayCommand::Restart, QKeySequence(QStringLiteral("Ctrl+S"))},
      {TrayCommand::Stop, QKeySequence(QStringLiteral("Ctrl+T"))},
      {TrayCommand::Quit, QKeySequence(QStringLiteral("Ctrl+Q"))},
  };
  return input;
}

QStringList ids(const TrayMenuSpec &spec)
{
  QStringList list;
  for (const auto &entry : spec.entries)
    list << (entry.kind == Kind::Separator ? QStringLiteral("---") : entry.id);
  return list;
}

const TrayMenuEntry *find(const TrayMenuSpec &spec, const QString &id)
{
  for (const auto &entry : spec.entries)
    if (entry.id == id)
      return &entry;
  return nullptr;
}

// the rows between the separators
QList<QList<TrayMenuEntry>> groupsOf(const TrayMenuSpec &spec)
{
  QList<QList<TrayMenuEntry>> groups;
  groups.emplaceBack();
  for (const auto &entry : spec.entries) {
    if (entry.kind == Kind::Separator)
      groups.emplaceBack();
    else
      groups.last().append(entry);
  }
  return groups;
}

const TrayMenuModel windows(TrayPlatform::windows());
const TrayMenuModel mac(TrayPlatform::mac());

} // namespace

void TrayMenuModelTests::stopped_offersStart_theWindowAndQuit()
{
  const auto spec = mac.build(makeInput(ProcessState::Stopped, ConnectionState::Disconnected));
  QCOMPARE(spec.state, TrayState::Stopped);
  QCOMPARE(
      ids(spec),
      (QStringList{"header", "---", "command:start", "---", "command:open", "command:minimize", "command:preferences",
                   "---", "command:quit"})
  );
  const auto *start = find(spec, "command:start");
  QCOMPARE(start->text, QStringLiteral("Start"));
  QCOMPARE(start->shortcut, QKeySequence(QStringLiteral("Ctrl+S")));
  QVERIFY(start->enabled);
  QCOMPARE(start->command, TrayCommand::Start);
  // the row that opens the window is the one in bold: nothing else is
  int bold = 0;
  for (const auto &entry : spec.entries)
    bold += entry.bold;
  QCOMPARE(bold, 1);
  QVERIFY(find(spec, "command:open")->bold);
  QCOMPARE(find(spec, "command:quit")->shortcut, QKeySequence(QStringLiteral("Ctrl+Q")));
  // a client says Connect
  QCOMPARE(
      mac.build(makeInput(ProcessState::Stopped, ConnectionState::Disconnected, false)).entries.at(2).text,
      QStringLiteral("Connect")
  );
}

void TrayMenuModelTests::running_offersRestartAndStop_thenTheFacts()
{
  const auto waiting = windows.build(makeInput(ProcessState::Started, ConnectionState::Listening));
  QCOMPARE(waiting.state, TrayState::Waiting);
  QCOMPARE(
      ids(waiting),
      (QStringList{"header", "---", "command:restart", "command:stop", "---", "computers:none", "---", "command:open",
                   "command:minimize", "command:preferences", "---", "command:quit"})
  );
  QCOMPARE(find(waiting, "computers:none")->text, QStringLiteral("No computer has connected yet."));
  QVERIFY(!find(waiting, "computers:none")->enabled);

  auto connected = makeInput(ProcessState::Started, ConnectionState::Connected);
  connected.computers = {QStringLiteral("Desk-PC"), QStringLiteral("Laptop")};
  const auto spec = windows.build(connected);
  QCOMPARE(spec.state, TrayState::Connected);
  QCOMPARE(
      ids(spec),
      (QStringList{"header", "---", "command:restart", "command:stop", "---", "computer:Desk-PC", "computer:Laptop",
                   "---", "command:open", "command:minimize", "command:preferences", "---", "command:quit"})
  );
  QCOMPARE(find(spec, "command:stop")->shortcut, QKeySequence(QStringLiteral("Ctrl+T")));
  // Start is the row of a core that is not running
  QVERIFY(!find(spec, "command:start"));

  // a client that is starting is waiting, with Restart and Stop as well, and no row of computers that none has
  const auto starting = windows.build(makeInput(ProcessState::Starting, ConnectionState::Disconnected, false));
  QCOMPARE(starting.state, TrayState::Waiting);
  QVERIFY(find(starting, "command:restart") && find(starting, "command:stop"));
  QVERIFY(!find(starting, "computers:none"));
}

void TrayMenuModelTests::error_ofAClient_offersTestConnectionAndStop()
{
  auto in = makeInput(ProcessState::RetryPending, ConnectionState::Disconnected, false);
  in.canTestConnection = true;
  const auto spec = windows.build(in);
  QCOMPARE(spec.state, TrayState::Error);
  QCOMPARE(
      ids(spec),
      (QStringList{"header", "---", "command:test-connection", "command:stop", "---", "command:open",
                   "command:minimize", "command:preferences", "---", "command:quit"})
  );
  QVERIFY(find(spec, "command:test-connection")->enabled);
  QCOMPARE(find(spec, "command:stop")->text, QStringLiteral("Disconnect"));

  // without the address of a server there is nothing to test: the row is dimmed
  in.canTestConnection = false;
  QVERIFY(!find(windows.build(in), "command:test-connection")->enabled);

  // a started core that has lost its connection is an error too
  QCOMPARE(
      windows.build(makeInput(ProcessState::Started, ConnectionState::Disconnected, false)).state, TrayState::Error
  );
}

void TrayMenuModelTests::error_ofAServer_offersStopWhileItRuns_andStartWhenItCouldNotStart()
{
  const auto running = windows.build(makeInput(ProcessState::RetryPending, ConnectionState::Disconnected));
  QCOMPARE(running.state, TrayState::Error);
  QVERIFY(!find(running, "command:test-connection"));
  QVERIFY(find(running, "command:stop"));
  QVERIFY(!find(running, "command:start"));

  auto failed = makeInput(ProcessState::Stopped, ConnectionState::Disconnected);
  failed.startFailed = true;
  const auto spec = windows.build(failed);
  QCOMPARE(spec.state, TrayState::Error);
  QVERIFY(find(spec, "command:start")->enabled);
  QVERIFY(!find(spec, "command:stop"));
}

void TrayMenuModelTests::noMode_hasNothingToStart_dimmedOrLeftOutByTheOs()
{
  auto in = makeInput(ProcessState::Stopped, ConnectionState::Disconnected);
  in.hasMode = false;
  QCOMPARE(mac.build(in).state, TrayState::Idle);
  // macOS and the desktops of Linux dim what does not apply
  const auto *dimmed = find(mac.build(in), "command:start");
  QVERIFY(dimmed);
  QVERIFY(!dimmed->enabled);
  // Windows leaves it out, and the separator of its group with it
  const auto left = windows.build(in);
  QVERIFY(!find(left, "command:start"));
  QCOMPARE(
      ids(left),
      (QStringList{"header", "---", "command:open", "command:minimize", "command:preferences", "---", "command:quit"})
  );
}

void TrayMenuModelTests::minimize_isOnlyOfferedWhileTheWindowIsOnTheScreen()
{
  const auto hidden = makeInput(ProcessState::Stopped, ConnectionState::Disconnected, true, false);
  const auto shown = makeInput(ProcessState::Stopped, ConnectionState::Disconnected, true, true);

  QVERIFY(find(mac.build(shown), "command:minimize")->enabled);
  QVERIFY(!find(mac.build(hidden), "command:minimize")->enabled);
  QVERIFY(find(mac.build(hidden), "command:minimize"));

  QVERIFY(find(windows.build(shown), "command:minimize")->enabled);
  QVERIFY(!find(windows.build(hidden), "command:minimize"));
  // opening the window is offered whether it is there or not: it brings it to the front
  QVERIFY(find(windows.build(shown), "command:open")->enabled);
  QVERIFY(find(windows.build(hidden), "command:open")->enabled);
}

void TrayMenuModelTests::start_isDimmedWhenTheSettingsCannotStartTheCore()
{
  auto in = makeInput(ProcessState::Stopped, ConnectionState::Disconnected, false);
  in.canStart = false;
  for (const auto *model : {&mac, &windows}) {
    const auto *start = find(model->build(in), "command:start");
    QVERIFY(start);
    QVERIFY(!start->enabled);
  }
}

void TrayMenuModelTests::computers_areRowsOfTextThatCannotBeChosen()
{
  auto in = makeInput(ProcessState::Started, ConnectionState::Connected);
  in.computers = {QStringLiteral("Desk-PC")};
  const auto spec = mac.build(in);
  const auto *row = find(spec, "computer:Desk-PC");
  QVERIFY(row);
  QCOMPARE(row->kind, Kind::Text);
  QCOMPARE(row->text, QStringLiteral("Desk-PC: Connected"));
  QVERIFY(!row->enabled);
  // a client tells the server that it is connected to in the same way
  auto client = makeInput(ProcessState::Started, ConnectionState::Connected, false);
  client.computers = {QStringLiteral("Studio-Mac")};
  QCOMPARE(find(mac.build(client), "computer:Studio-Mac")->text, QStringLiteral("Studio-Mac: Connected"));
  // the row of "none yet" is a server that waits: a client that waits for its server has no such row
  QVERIFY(!find(mac.build(makeInput(ProcessState::Started, ConnectionState::Connecting, false)), "computers:none"));
  // and a server that is connected has the computers, not this row
  QVERIFY(!find(spec, "computers:none"));
}

void TrayMenuModelTests::update_isARowThatOpensTheDownload_aboveTheComputers()
{
  auto in = makeInput(ProcessState::Started, ConnectionState::Connected);
  in.computers = {QStringLiteral("Desk-PC")};
  in.updateVersion = QStringLiteral("1.28.0");
  const auto spec = mac.build(in);
  const auto *update = find(spec, "command:update");
  QVERIFY(update);
  QCOMPARE(update->kind, Kind::Command);
  QCOMPARE(update->command, TrayCommand::OpenUpdate);
  QCOMPARE(update->text, QStringLiteral("A new version v1.28.0 is available"));
  QVERIFY(update->enabled);
  QVERIFY(spec.entries.indexOf(*update) < spec.entries.indexOf(*find(spec, "computer:Desk-PC")));
  // with nothing to update to there is no row
  in.updateVersion.clear();
  QVERIFY(!find(mac.build(in), "command:update"));
  // an update with no computer is a group of its own
  auto stopped = makeInput(ProcessState::Stopped, ConnectionState::Disconnected);
  stopped.updateVersion = QStringLiteral("1.28.0");
  QCOMPARE(
      ids(mac.build(stopped)),
      (QStringList{"header", "---", "command:start", "---", "command:update", "---", "command:open", "command:minimize",
                   "command:preferences", "---", "command:quit"})
  );
}

void TrayMenuModelTests::header_saysTheStateInWordsAndInAChip()
{
  struct Row
  {
    ProcessState process;
    ConnectionState connection;
    QString chip;
    StatusChip::Kind kind;
  };
  const QList<Row> rows{
      {ProcessState::Stopped, ConnectionState::Disconnected, "Stopped", StatusChip::Kind::Neutral},
      {ProcessState::Started, ConnectionState::Listening, "Waiting", StatusChip::Kind::Info},
      {ProcessState::Started, ConnectionState::Connected, "Running", StatusChip::Kind::Success},
      {ProcessState::RetryPending, ConnectionState::Disconnected, "Needs attention", StatusChip::Kind::Danger},
  };
  for (const auto &row : rows) {
    const auto spec = mac.build(makeInput(row.process, row.connection));
    const auto &header = spec.entries.first();
    QCOMPARE(header.kind, Kind::Header);
    QCOMPARE(header.text, QStringLiteral("Headline"));
    QCOMPARE(header.detail, QStringLiteral("The sentence behind it"));
    QCOMPARE(header.chipText, row.chip);
    QCOMPARE(header.chipKind, row.kind);
    QVERIFY(!header.enabled);
  }
  // nothing to tell, no chip
  auto in = makeInput(ProcessState::Stopped, ConnectionState::Disconnected);
  in.hasMode = false;
  QVERIFY(mac.build(in).entries.first().chipText.isEmpty());
}

void TrayMenuModelTests::toolTip_saysTheStateInWords()
{
  auto in = makeInput(ProcessState::Started, ConnectionState::Connected);
  in.headline = QStringLiteral("Sharing with 2 computers");
  QCOMPARE(mac.build(in).toolTip, QStringLiteral("Deskflow: Sharing with 2 computers"));
  in.updateVersion = QStringLiteral("1.28.0");
  QCOMPARE(mac.build(in).toolTip, QStringLiteral("Deskflow: Sharing with 2 computers (update available)"));
}

void TrayMenuModelTests::externalRows_areUnderTheHeader_andGoWhenTheirTextIsEmpty()
{
  TrayMenuModel model(TrayPlatform::mac());
  const auto in = makeInput(ProcessState::Stopped, ConnectionState::Disconnected);
  const auto without = model.build(in);

  model.setExternalRow(QStringLiteral("tunnel"), QStringLiteral("Tunnel: up, trycloudflare.example"));
  auto spec = model.build(in);
  QCOMPARE(
      ids(spec),
      (QStringList{"header", "external:tunnel", "---", "command:start", "---", "command:open", "command:minimize",
                   "command:preferences", "---", "command:quit"})
  );
  const auto *row = find(spec, "external:tunnel");
  QCOMPARE(row->kind, Kind::Text);
  QCOMPARE(row->text, QStringLiteral("Tunnel: up, trycloudflare.example"));
  QVERIFY(!row->enabled);

  // a second feature has its own row, after the first; changing a text keeps the place of the row
  model.setExternalRow(QStringLiteral("other"), QStringLiteral("Other"));
  model.setExternalRow(QStringLiteral("tunnel"), QStringLiteral("Tunnel: down"));
  spec = model.build(in);
  QCOMPARE(ids(spec).mid(0, 3), (QStringList{"header", "external:tunnel", "external:other"}));
  QCOMPARE(find(spec, "external:tunnel")->text, QStringLiteral("Tunnel: down"));
  QCOMPARE(model.externalRows().size(), 2);

  // an empty text takes the row away, and the menu is what it was
  model.setExternalRow(QStringLiteral("tunnel"), QString());
  model.setExternalRow(QStringLiteral("other"), QString());
  QCOMPARE(model.build(in), without);
  QVERIFY(model.externalRows().isEmpty());
  // taking away what was never there is nothing
  model.setExternalRow(QStringLiteral("never"), QString());
  QCOMPARE(model.build(in), without);
}

void TrayMenuModelTests::rows_areOrderedInGroups_withNoSeparatorBesideAnother_data()
{
  QTest::addColumn<bool>("windowsOs");
  QTest::addColumn<bool>("server");
  QTest::addColumn<int>("process");
  QTest::addColumn<int>("connection");
  QTest::addColumn<bool>("shown");
  QTest::addColumn<bool>("hasMode");
  QTest::addColumn<bool>("update");
  QTest::addColumn<int>("computers");
  QTest::addColumn<bool>("external");

  for (const bool windowsOs : {true, false})
    for (const bool server : {true, false})
      for (const int process : {0, 1, 2, 3, 4})
        for (const int connection : {0, 1, 2, 3})
          for (const bool shown : {true, false})
            for (const bool hasMode : {true, false})
              for (const bool update : {true, false})
                for (const int computers : {0, 2})
                  for (const bool external : {true, false})
                    QTest::addRow(
                        "%s %s p%d c%d %s %s %s %dpc %s", windowsOs ? "win" : "mac", server ? "server" : "client",
                        process, connection, shown ? "shown" : "hidden", hasMode ? "mode" : "nomode",
                        update ? "update" : "noupdate", computers, external ? "ext" : "noext"
                    ) << windowsOs
                      << server << process << connection << shown << hasMode << update << computers << external;
}

void TrayMenuModelTests::rows_areOrderedInGroups_withNoSeparatorBesideAnother()
{
  QFETCH(bool, windowsOs);
  QFETCH(bool, server);
  QFETCH(int, process);
  QFETCH(int, connection);
  QFETCH(bool, shown);
  QFETCH(bool, hasMode);
  QFETCH(bool, update);
  QFETCH(int, computers);
  QFETCH(bool, external);

  TrayMenuModel model(windowsOs ? TrayPlatform::windows() : TrayPlatform::mac());
  if (external)
    model.setExternalRow(QStringLiteral("tunnel"), QStringLiteral("Tunnel"));
  auto in = makeInput(static_cast<ProcessState>(process), static_cast<ConnectionState>(connection), server, shown);
  in.hasMode = hasMode;
  if (update)
    in.updateVersion = QStringLiteral("2.0");
  for (int i = 0; i < computers; ++i)
    in.computers << QStringLiteral("PC%1").arg(i);
  const auto spec = model.build(in);

  QVERIFY(!spec.entries.isEmpty());
  QCOMPARE(spec.entries.first().kind, Kind::Header);
  QCOMPARE(spec.entries.last().id, QStringLiteral("command:quit"));
  // no separator first, last or beside another
  for (int i = 0; i < spec.entries.size(); ++i) {
    if (spec.entries.at(i).kind != Kind::Separator)
      continue;
    QVERIFY(i > 0 && i < spec.entries.size() - 1);
    QVERIFY(spec.entries.at(i - 1).kind != Kind::Separator);
  }
  // every row has a name of its own
  QSet<QString> names;
  for (const auto &entry : spec.entries) {
    QVERIFY2(!names.contains(entry.id), qPrintable(entry.id));
    names.insert(entry.id);
  }
  // what is always there: the window, the settings, Quit
  for (const auto *id : {"command:open", "command:preferences", "command:quit"})
    QVERIFY(find(spec, QLatin1String(id)));
  // the header cannot be chosen; every command has the text that the window gave it
  for (const auto &entry : spec.entries) {
    if (entry.kind == Kind::Command)
      QVERIFY2(!entry.text.isEmpty(), qPrintable(entry.id));
    if (entry.kind == Kind::Header || entry.kind == Kind::Text)
      QVERIFY(!entry.enabled);
  }
}

void TrayMenuModelTests::icons_areAllOrNoneInAGroup()
{
  for (const bool windowsOs : {true, false}) {
    TrayMenuModel model(windowsOs ? TrayPlatform::windows() : TrayPlatform::mac());
    for (const int process : {0, 1, 2, 3, 4}) {
      for (const int connection : {0, 2, 3}) {
        for (const bool update : {true, false}) {
          for (const int computers : {0, 2}) {
            auto in = makeInput(
                static_cast<ProcessState>(process), static_cast<ConnectionState>(connection), true, windowsOs
            );
            if (update)
              in.updateVersion = QStringLiteral("2.0");
            for (int i = 0; i < computers; ++i)
              in.computers << QStringLiteral("PC%1").arg(i);
            for (const auto &group : groupsOf(model.build(in))) {
              int with = 0;
              for (const auto &entry : group)
                with += !entry.icon.isEmpty();
              QVERIFY2(with == 0 || with == group.size(), qPrintable(QString::number(process)));
            }
          }
        }
      }
    }
  }

  // a desktop that shows no icons gets none
  TrayPlatform plain;
  plain.icons = false;
  const auto spec = TrayMenuModel(plain).build(makeInput(ProcessState::Started, ConnectionState::Connected));
  for (const auto &entry : spec.entries)
    QVERIFY(entry.icon.isEmpty());
  // and the rows that do are with the names of the set of the program
  const auto with = mac.build(makeInput(ProcessState::Started, ConnectionState::Connected));
  QCOMPARE(find(with, "command:restart")->icon, QStringLiteral("arrow-clockwise"));
  QCOMPARE(find(with, "command:stop")->icon, QStringLiteral("stop"));
  QCOMPARE(find(with, "command:quit")->icon, QStringLiteral("power"));
  QCOMPARE(
      find(mac.build(makeInput(ProcessState::Stopped, ConnectionState::Disconnected, false)), "command:start")->icon,
      QStringLiteral("plugs-connected")
  );
}

void TrayMenuModelTests::longSentences_areCutShortAndKeepTheWholeForTheToolTip()
{
  const auto sentence = QStringLiteral("Deskflow is connected, with 2 client(s): Studio-Mac-Mini-Under-The-Desk, Laptop-Of-A-Long-Name");
  auto in = makeInput(ProcessState::Started, ConnectionState::Connected);
  in.computers = {QString(70, QLatin1Char('x'))};
  const auto spec = mac.build(in);
  const auto *row = find(spec, "computer:" + in.computers.first());
  QVERIFY(row->text.size() <= 60);
  QVERIFY(row->text.endsWith(QChar(0x2026)));
  QCOMPARE(row->toolTip, QStringLiteral("%1: Connected").arg(in.computers.first()));

  QCOMPARE(TrayMenuModel::elided(QStringLiteral("short")), QStringLiteral("short"));
  const auto cut = TrayMenuModel::elided(sentence, 40);
  QCOMPARE(cut.size(), 40);
  QVERIFY(cut.endsWith(QChar(0x2026)));
  QVERIFY(sentence.startsWith(cut.left(30)));
  // the text of a row that fits has no tool tip of its own
  QVERIFY(find(spec, "header")->toolTip.isEmpty());
}

void TrayMenuModelTests::themeIcons_haveANameInTheThemeOfTheDesktop()
{
  // the names that the program has always asked the theme for stay the same names
  const QMap<QString, QString> expected{
      {"play", "system-run"},      {"plugs-connected", "network-connect"}, {"stop", "process-stop"},
      {"plug", "network-disconnect"}, {"arrow-clockwise", "view-refresh"}, {"desktop", "window-restore-pip"},
      {"sidebar-simple", "window-minimize-pip"}, {"gear", "configure"},     {"power", "application-exit"},
  };
  for (auto it = expected.cbegin(); it != expected.cend(); ++it)
    QCOMPARE(TrayMenuModel::themeIconName(it.key()), it.value());
  // every icon of every row has one, so that no row of a desktop is without
  for (const auto *model : {&mac, &windows}) {
    auto in = makeInput(ProcessState::Started, ConnectionState::Connected);
    in.computers = {QStringLiteral("PC")};
    in.updateVersion = QStringLiteral("2.0");
    auto client = in;
    client.isServer = false;
    client.process = ProcessState::RetryPending;
    client.canTestConnection = true;
    for (const auto &spec : {model->build(in), model->build(client)})
      for (const auto &entry : spec.entries)
        if (!entry.icon.isEmpty())
          QVERIFY2(!TrayMenuModel::themeIconName(entry.icon).isEmpty(), qPrintable(entry.icon));
  }
}

QTEST_APPLESS_MAIN(TrayMenuModelTests)
