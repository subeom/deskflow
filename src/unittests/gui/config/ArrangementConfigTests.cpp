/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ArrangementConfigTests.h"

#include "common/Settings.h"
#include "gui/config/MonitorEditing.h"
#include "gui/config/ServerConfig.h"
#include "gui/core/ReportedScreen.h"
#include "server/MonitorEditor.h"

#include <QRect>
#include <QSettings>
#include <QTextStream>

#include <optional>
#include <vector>

namespace {

// The cells of the classic grid that have a computer: its index (row * 5 + column) and name
using Cells = QList<QPair<int, QString>>;

constexpr int kCells = 15;

// Puts \p cells in the settings the way the server configuration saves them, and \p serverName as the name of this
// computer
void writeGrid(const Cells &cells, const QString &serverName)
{
  Settings::setValue(Settings::Core::ComputerName, serverName);
  auto &settings = Settings::proxy();
  settings.beginGroup(QStringLiteral("internalConfig"));
  settings.remove(QString());
  settings.beginWriteArray(QStringLiteral("screens"));
  for (int i = 0; i < kCells; ++i) {
    settings.setArrayIndex(i);
    QString name;
    for (const auto &cell : cells) {
      if (cell.first == i)
        name = cell.second;
    }
    settings.setValue(QStringLiteral("name"), name);
  }
  settings.endArray();
  settings.endGroup();
}

QString generated(const ServerConfig &config)
{
  QString text;
  QTextStream stream(&text);
  stream << config;
  stream.flush();
  return text;
}

const auto kEmpty = QStringLiteral("section: links\nend\n\nsection: options\nend\n\n");

} // namespace

void ArrangementConfigTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("Deskflow.state")));
}

namespace {
void forget(const QStringList &names);
}

void ArrangementConfigTests::cleanup()
{
  // whatever a test said about the computers it used
  forget({"A", "B", "C", "D", "E", "srv", "PC1", "PC2", "MINI", "SUBEOM-COPILOT", "9800X3D-5080"});
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  Settings::setValue(Settings::Server::UseArrangement);
  ServerConfig::overrideServerMonitors(std::nullopt);
}

void ArrangementConfigTests::classicGrid_generatesTheConfigItAlwaysDid_data()
{
  QTest::addColumn<Cells>("cells");
  QTest::addColumn<QString>("server");
  QTest::addColumn<bool>("experiment");
  QTest::addColumn<QString>("expected");

  // what the user has: a row of four, the Mac that is the server second from the left. This is the text that the
  // shipped build wrote to deskflow-server.conf
  const Cells row = {
      {6, "SUBEOM-COPILOT"}, {7, "Subeoms-MacBook-Pro.local"}, {8, "9800X3D-5080"}, {9, "Subeoms-Mac-mini.local"}
  };
  const auto rowText = QStringLiteral(
      "section: links\n"
      "\tSUBEOM-COPILOT:\n"
      "\t\tright = Subeoms-MacBook-Pro.local\n"
      "\tSubeoms-MacBook-Pro.local:\n"
      "\t\tright = 9800X3D-5080\n"
      "\t\tleft = SUBEOM-COPILOT\n"
      "\t9800X3D-5080:\n"
      "\t\tright = Subeoms-Mac-mini.local\n"
      "\t\tleft = Subeoms-MacBook-Pro.local\n"
      "\tSubeoms-Mac-mini.local:\n"
      "\t\tleft = 9800X3D-5080\n"
      "end\n"
      "\n"
      "section: options\n"
      "end\n"
      "\n"
  );
  QTest::newRow("the row of four, experiment off") << row << "Subeoms-MacBook-Pro.local" << false << rowText;
  QTest::newRow("the row of four, experiment on") << row << "Subeoms-MacBook-Pro.local" << true << rowText;

  // a cross with the server in the middle
  const Cells cross = {{2, "up"}, {6, "left"}, {7, "srv"}, {8, "right"}, {12, "down"}};
  const auto crossText = QStringLiteral(
      "section: links\n"
      "\tup:\n"
      "\t\tdown = srv\n"
      "\tleft:\n"
      "\t\tright = srv\n"
      "\tsrv:\n"
      "\t\tright = right\n"
      "\t\tleft = left\n"
      "\t\tup = up\n"
      "\t\tdown = down\n"
      "\tright:\n"
      "\t\tleft = srv\n"
      "\tdown:\n"
      "\t\tup = srv\n"
      "end\n"
      "\n"
      "section: options\n"
      "end\n"
      "\n"
  );
  QTest::newRow("a cross") << cross << "srv" << false << crossText;
  QTest::newRow("a cross, experiment on") << cross << "srv" << true << crossText;

  // a 2 by 2 block: every computer has two neighbours
  const Cells block = {{6, "a"}, {7, "b"}, {11, "c"}, {12, "d"}};
  const auto blockText = QStringLiteral(
      "section: links\n"
      "\ta:\n"
      "\t\tright = b\n"
      "\t\tdown = c\n"
      "\tb:\n"
      "\t\tleft = a\n"
      "\t\tdown = d\n"
      "\tc:\n"
      "\t\tright = d\n"
      "\t\tup = a\n"
      "\td:\n"
      "\t\tleft = c\n"
      "\t\tup = b\n"
      "end\n"
      "\n"
      "section: options\n"
      "end\n"
      "\n"
  );
  QTest::newRow("a 2 by 2 block") << block << "a" << false << blockText;

  // an empty cell between two computers is a gap: no link; the end of a row is not the start of the next one (cells 4
  // and 5), while the cell below one is its neighbour (4 and 9)
  const Cells gaps = {{4, "end-of-row-0"}, {5, "start-of-row-1"}, {7, "x"}, {9, "y"}};
  const auto gapsText = QStringLiteral(
      "section: links\n"
      "\tend-of-row-0:\n"
      "\t\tdown = y\n"
      "\tstart-of-row-1:\n"
      "\tx:\n"
      "\ty:\n"
      "\t\tup = end-of-row-0\n"
      "end\n"
      "\n"
      "section: options\n"
      "end\n"
      "\n"
  );
  QTest::newRow("gaps and the ends of the rows") << gaps << "x" << false << gapsText;

  QTest::newRow("no computer") << Cells{} << "x" << false << kEmpty;
}

void ArrangementConfigTests::classicGrid_generatesTheConfigItAlwaysDid()
{
  QFETCH(Cells, cells);
  QFETCH(QString, server);
  QFETCH(bool, experiment);
  QFETCH(QString, expected);

  writeGrid(cells, server);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, experiment);
  const ServerConfig config;
  QCOMPARE(generated(config), expected);
}

namespace {

void place(const QString &name, double x, double y, double zoom = 1.0, int width = 0, int height = 0)
{
  Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
  Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  if (zoom != 1.0)
    Settings::setValue(Settings::Computer::ArrangementZoom.arg(name), zoom);
  if (width > 0)
    Settings::setValue(Settings::Computer::ArrangementWidth.arg(name), width);
  if (height > 0)
    Settings::setValue(Settings::Computer::ArrangementHeight.arg(name), height);
}

// forget what a test said about the computers
void forget(const QStringList &names)
{
  for (const auto &name : names) {
    for (const auto &key :
         {Settings::Computer::ArrangementX, Settings::Computer::ArrangementY, Settings::Computer::ArrangementZoom,
          Settings::Computer::ArrangementWidth, Settings::Computer::ArrangementHeight,
          Settings::Computer::MonitorLayout})
      Settings::setValue(key.arg(name));
  }
}

// the screen of the MacBook Pro of the user below a DELL, as the operating system says
const std::vector<deskflow::server::MonitorRect> kServerDisplays = {{0, 0, 3440, 1440}, {753, 1440, 2064, 1347}};

} // namespace

void ArrangementConfigTests::arrangement_isUsedOnlyWhenTheExperimentAndTheUseAreOn_data()
{
  QTest::addColumn<bool>("experiment");
  QTest::addColumn<bool>("use");
  QTest::addColumn<bool>("arrangement");
  QTest::newRow("both off") << false << false << false;
  QTest::newRow("the experiment on, the use off") << true << false << false;
  QTest::newRow("the use on, the experiment off") << false << true << false;
  QTest::newRow("both on") << true << true << true;
}

void ArrangementConfigTests::arrangement_isUsedOnlyWhenTheExperimentAndTheUseAreOn()
{
  QFETCH(bool, experiment);
  QFETCH(bool, use);
  QFETCH(bool, arrangement);

  // A, 1920x1080, with B, 2560x1440, on its right: the classic grid links them whole, the arrangement by what touches
  writeGrid({{6, "A"}, {7, "B"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  place("A", 0, 0, 1.0, 1920, 1080);
  place("B", 1920, 0, 1.0, 2560, 1440);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, experiment);
  Settings::setValue(Settings::Server::UseArrangement, use);

  const ServerConfig config;
  QCOMPARE(config.usesArrangement(), arrangement);
  const auto classic =
      QStringLiteral("section: links\n\tA:\n\t\tright = B\n\tB:\n\t\tleft = A\nend\n\nsection: options\nend\n\n");
  const auto derived = QStringLiteral(
      "section: links\n\tA:\n\t\tright = B(0,75)\n\tB:\n\t\tleft(0,75) = A\nend\n\nsection: options\nend\n\n"
  );
  QCOMPARE(generated(config), arrangement ? derived : classic);
  forget({"A", "B"});
}

void ArrangementConfigTests::arrangement_ofEqualComputersInTheClassicGrid_isTheClassicText_data()
{
  QTest::addColumn<Cells>("cells");
  QTest::addColumn<QString>("server");
  QTest::addColumn<QString>("expected");
  // the same grids as the pins of the classic grid above: with no place and no size they are the classic grid
  QTest::newRow(
      "a row of four"
  ) << Cells{{6, "SUBEOM-COPILOT"}, {7, "Subeoms-MacBook-Pro.local"}, {8, "9800X3D-5080"}, {9, "Subeoms-Mac-mini.local"}}
    << "Subeoms-MacBook-Pro.local"
    << QStringLiteral(
           "section: links\n"
           "\tSUBEOM-COPILOT:\n"
           "\t\tright = Subeoms-MacBook-Pro.local\n"
           "\tSubeoms-MacBook-Pro.local:\n"
           "\t\tright = 9800X3D-5080\n"
           "\t\tleft = SUBEOM-COPILOT\n"
           "\t9800X3D-5080:\n"
           "\t\tright = Subeoms-Mac-mini.local\n"
           "\t\tleft = Subeoms-MacBook-Pro.local\n"
           "\tSubeoms-Mac-mini.local:\n"
           "\t\tleft = 9800X3D-5080\n"
           "end\n\nsection: options\nend\n\n"
       );
  QTest::newRow("a cross") << Cells{{2, "up"}, {6, "left"}, {7, "srv"}, {8, "right"}, {12, "down"}} << "srv"
                           << QStringLiteral(
                                  "section: links\n"
                                  "\tup:\n\t\tdown = srv\n"
                                  "\tleft:\n\t\tright = srv\n"
                                  "\tsrv:\n\t\tright = right\n\t\tleft = left\n\t\tup = up\n\t\tdown = down\n"
                                  "\tright:\n\t\tleft = srv\n"
                                  "\tdown:\n\t\tup = srv\n"
                                  "end\n\nsection: options\nend\n\n"
                              );
  QTest::newRow("a 2 by 2 block") << Cells{{6, "a"}, {7, "b"}, {11, "c"}, {12, "d"}} << "a"
                                  << QStringLiteral(
                                         "section: links\n"
                                         "\ta:\n\t\tright = b\n\t\tdown = c\n"
                                         "\tb:\n\t\tleft = a\n\t\tdown = d\n"
                                         "\tc:\n\t\tright = d\n\t\tup = a\n"
                                         "\td:\n\t\tleft = c\n\t\tup = b\n"
                                         "end\n\nsection: options\nend\n\n"
                                     );
  QTest::newRow("gaps and the ends of the rows")
      << Cells{{4, "end-of-row-0"}, {5, "start-of-row-1"}, {7, "x"}, {9, "y"}} << "x"
      << QStringLiteral(
             "section: links\n"
             "\tend-of-row-0:\n\t\tdown = y\n"
             "\tstart-of-row-1:\n"
             "\tx:\n"
             "\ty:\n\t\tup = end-of-row-0\n"
             "end\n\nsection: options\nend\n\n"
         );
}

void ArrangementConfigTests::arrangement_ofEqualComputersInTheClassicGrid_isTheClassicText()
{
  QFETCH(Cells, cells);
  QFETCH(QString, server);
  QFETCH(QString, expected);
  writeGrid(cells, server);
  // the screen of this computer is not asked: it is a 1920x1080 one, like the others
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  const ServerConfig config;
  QVERIFY(!config.hasArrangement());
  QCOMPARE(generated(config), expected);
}

void ArrangementConfigTests::arrangement_theServerOfTheUser_makesPartialLinks()
{
  writeGrid({{6, "PC1"}, {7, "srv"}, {8, "MINI"}, {12, "PC2"}}, "srv");
  ServerConfig::overrideServerMonitors(kServerDisplays);
  Settings::setValue(Settings::Computer::MonitorLayout.arg("PC1"), QStringLiteral("-1920,360,1920,1080;0,0,2560,1440"));
  place("srv", 0, 0);
  place("PC1", -2560, 0);
  place("MINI", 3440, 0, 1.0, 1920, 1080);
  place("PC2", -847, 1440, 1.0, 1600, 900);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);

  const ServerConfig config;
  QVERIFY(config.hasArrangement());
  QCOMPARE(
      generated(config), QStringLiteral(
                             "section: links\n"
                             "\tPC1:\n"
                             "\t\tright = srv(0,51.6685)\n"
                             "\t\tdown(81.0938,100) = PC2(0,52.9375)\n"
                             "\tsrv:\n"
                             "\t\tright(0,38.7513) = MINI\n"
                             "\t\tleft(0,51.6685) = PC1\n"
                             "\t\tleft(51.6685,83.9612) = PC2\n"
                             "\t\tdown(0,21.8895) = PC2(52.9375,100)\n"
                             "\tMINI:\n"
                             "\t\tleft = srv(0,38.7513)\n"
                             "\tPC2:\n"
                             "\t\tright = srv(51.6685,83.9612)\n"
                             "\t\tup(0,52.9375) = PC1(81.0938,100)\n"
                             "\t\tup(52.9375,100) = srv(0,21.8895)\n"
                             "end\n\nsection: options\nend\n\n"
                         )
  );
}

void ArrangementConfigTests::arrangement_aComputerThatIsNotPlaced_isBesideTheOthers()
{
  // C was added after the others were arranged: it has no place, and links nothing until it is put somewhere
  writeGrid({{6, "A"}, {7, "B"}, {8, "C"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  place("A", 0, 0);
  place("B", 1920, 0);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  const ServerConfig config;
  QCOMPARE(
      generated(config),
      QStringLiteral("section: links\n\tA:\n\t\tright = B\n\tB:\n\t\tleft = A\n\tC:\nend\n\nsection: options\nend\n\n")
  );
  const auto arrangement = config.arrangement();
  const auto &c = arrangement.computers[static_cast<size_t>(arrangement.indexOf("C"))];
  QVERIFY(c.box().x >= 3840 + 100);
  forget({"A", "B", "C"});
}

void ArrangementConfigTests::arrangement_isSavedAndLoaded()
{
  writeGrid({{6, "A"}, {7, "B"}, {8, "C"}}, "A");
  place("A", 0, 0);
  place("B", 1706.667, -12.5, 2.0 / 3.0, 2560, 1440);
  {
    ServerConfig config;
    QVERIFY(config.hasArrangement());
    config.commit();
  }
  {
    const ServerConfig config;
    const auto &computers = config.computers();
    const auto at = [&](const QString &name) -> const Computer & {
      return *std::ranges::find_if(computers, [&](const Computer &c) { return c.name() == name; });
    };
    QVERIFY(at("A").arrangementPlaced());
    QCOMPARE(at("A").arrangementX(), 0.0);
    QVERIFY(at("B").arrangementPlaced());
    QCOMPARE(at("B").arrangementX(), 1706.667);
    QCOMPARE(at("B").arrangementY(), -12.5);
    QVERIFY(std::abs(at("B").arrangementZoom() - 2.0 / 3.0) < 1.0e-9);
    QCOMPARE(at("B").arrangementWidth(), 2560);
    QCOMPARE(at("B").arrangementHeight(), 1440);
    QVERIFY(!at("C").arrangementPlaced());
    // nothing is saved for a computer that has no place, and defaults are not saved
    QVERIFY(!Settings::value(Settings::Computer::ArrangementX.arg("C")).isValid());
    QVERIFY(
        !Settings::value(Settings::Computer::ArrangementZoom.arg("A")).isValid() ||
        Settings::value(Settings::Computer::ArrangementZoom.arg("A")).toDouble() == 1.0
    );
  }
  forget({"A", "B", "C"});
}

void ArrangementConfigTests::arrangement_oldConfigurationsLoadAsTheyWere()
{
  // a configuration of a build that did not know the arrangement: no key
  writeGrid({{6, "A"}, {7, "B"}}, "A");
  forget({"A", "B"});
  ServerConfig config;
  QVERIFY(!config.hasArrangement());
  for (const auto &computer : std::as_const(config).computers()) {
    if (computer.isNull())
      continue;
    QVERIFY(!computer.arrangementPlaced());
    QCOMPARE(computer.arrangementZoom(), 1.0);
    QCOMPARE(computer.arrangementWidth(), 0);
    QCOMPARE(computer.arrangementHeight(), 0);
  }
  // and saving it adds no key
  config.commit();
  QSettings file(Settings::settingsFile(), QSettings::IniFormat);
  QVERIFY(!file.contains(Settings::Computer::ArrangementX.arg("A")));
  QVERIFY(!file.contains(Settings::Computer::ArrangementY.arg("A")));
  QVERIFY(!file.contains(Settings::Computer::ArrangementZoom.arg("A")));
  QVERIFY(!file.contains(Settings::Computer::ArrangementWidth.arg("B")));
  QVERIFY(!file.contains(Settings::Computer::ArrangementHeight.arg("B")));
  QCOMPARE(
      generated(config),
      QStringLiteral("section: links\n\tA:\n\t\tright = B\n\tB:\n\t\tleft = A\nend\n\nsection: options\nend\n\n")
  );
}

void ArrangementConfigTests::arrangement_classicGridConversion_keepsTheLinks()
{
  // the row of the user with sizes: the server is the T, the PCs are what the diagnostics said
  writeGrid({{6, "SUBEOM-COPILOT"}, {7, "srv"}, {8, "9800X3D-5080"}, {9, "MINI"}}, "srv");
  ServerConfig::overrideServerMonitors(kServerDisplays);
  Settings::setValue(
      Settings::Computer::MonitorLayout.arg("9800X3D-5080"), QStringLiteral("-1920,360,1920,1080;0,0,2560,1440")
  );
  Settings::setValue(Settings::Computer::ArrangementZoom.arg("SUBEOM-COPILOT"), 0.8);
  const ServerConfig config;
  const auto conversion = config.classicGridAsArrangement();
  QVERIFY(conversion.notKept.empty());
  QCOMPARE(conversion.arrangement.computers.size(), size_t(4));
  // the server is where the origin is
  QCOMPARE(conversion.arrangement.computers[1].originX, 0.0);
  QCOMPARE(conversion.arrangement.computers[1].originY, 0.0);
  forget({"SUBEOM-COPILOT", "9800X3D-5080"});
}

void ArrangementConfigTests::reportedScreen_isParsedFromTheLogOfTheCore_data()
{
  QTest::addColumn<QString>("line");
  QTest::addColumn<bool>("parsed");
  QTest::addColumn<QString>("computer");
  QTest::addColumn<QRect>("shape");
  QTest::newRow(
      "a plain screen"
  ) << QStringLiteral("[2026-11-16T13:30:00] INFO: computer \"SUBEOM-COPILOT\" reports its screen: 0,0 1920x1080")
    << true << "SUBEOM-COPILOT" << QRect(0, 0, 1920, 1080);
  QTest::newRow("monitors on the left, a negative position")
      << QStringLiteral(
             "[2026-11-16T13:30:00.123] INFO: computer \"9800X3D-5080\" reports its screen: -1920,0 4480x1440\r"
         )
      << true << "9800X3D-5080" << QRect(-1920, 0, 4480, 1440);
  QTest::newRow("a name with a space and a dot")
      << QStringLiteral(
             "[2026-11-16T13:30:00.123] INFO: computer \"Subeoms Mac-mini.local\" reports its screen: 0,-300 2560x1440"
         )
      << true << "Subeoms Mac-mini.local" << QRect(0, -300, 2560, 1440);
  // only the line that the core writes: the start of it is the time and the level, and nothing else is before the text
  QTest::newRow("no time and level") << QStringLiteral("computer \"x\" reports its screen: 0,0 10x10") << false << ""
                                     << QRect();
  QTest::newRow("a level that is not INFO")
      << QStringLiteral("[2026-11-16T13:30:00.123] DEBUG: computer \"x\" reports its screen: 0,0 10x10") << false << ""
      << QRect();
  QTest::newRow("a line that has the text in the middle")
      << QStringLiteral(
             "[2026-11-16T13:30:00.123] WARNING: unrecognised client name \"a computer \"x\" reports its screen: 0,0 "
             "10x10"
         )
      << false << "" << QRect();
  QTest::newRow("a quote in the name") << QStringLiteral(
                                              "[2026-11-16T13:30:00.123] INFO: computer \"a\" reports its screen: 0,0 "
                                              "1x1\" b\" reports its screen: 0,0 10x10"
                                          )
                                       << false << "" << QRect();
  QTest::newRow("another line") << QStringLiteral("INFO: client \"x\" has connected") << false << "" << QRect();
  QTest::newRow("a size of nothing") << QStringLiteral("computer \"x\" reports its screen: 0,0 0x1080") << false << ""
                                     << QRect();
  QTest::newRow("not numbers") << QStringLiteral("computer \"x\" reports its screen: a,0 10x10") << false << ""
                               << QRect();
}

void ArrangementConfigTests::reportedScreen_isParsedFromTheLogOfTheCore()
{
  QFETCH(QString, line);
  QFETCH(bool, parsed);
  QFETCH(QString, computer);
  QFETCH(QRect, shape);
  const auto screen = deskflow::gui::parseReportedScreen(line);
  QCOMPARE(screen.has_value(), parsed);
  if (parsed) {
    QCOMPARE(screen->computer, computer);
    QCOMPARE(screen->shape, shape);
  }
}

void ArrangementConfigTests::
    reportedScreen_isTakenFromTheLogOnlyWhileTheExperimentIsOnAndOnlyForTheComputersOfTheConfig()
{
  writeGrid({{6, "A"}, {7, "B"}}, "A");
  const auto line = [](const char *name) {
    return QStringLiteral("[2026-11-16T13:30:00.123] INFO: computer \"%1\" reports its screen: 0,0 2560x1440")
        .arg(QString::fromLatin1(name));
  };
  ServerConfig config;

  // the experiment is off: the core does not write the line, and a line that is there is not taken
  QVERIFY(!config.takeReportedScreenFromLog(line("B")));
  QVERIFY(!config.reportedScreen("B"));

  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  QVERIFY(config.takeReportedScreenFromLog(line("B")));
  QCOMPARE(*config.reportedScreen("B"), QRect(0, 0, 2560, 1440));

  // a computer that the configuration does not have is not remembered
  QVERIFY(!config.takeReportedScreenFromLog(line("stranger")));
  QVERIFY(!config.reportedScreen("stranger"));

  // a name that a computer that is not known sent, with a line in it: the log of the core has it as two lines
  const auto hostile = QStringLiteral(
      "[2026-11-16T13:30:01.000] WARNING: unrecognised client name \"evil\n"
      "computer \"A\" reports its screen: 0,0 1x1\""
  );
  for (const auto &part : hostile.split(QLatin1Char('\n')))
    QVERIFY(!config.takeReportedScreenFromLog(part));
  QVERIFY(!config.reportedScreen("A"));
}

void ArrangementConfigTests::assumedSizes_areOfTheComputersThatHaveNoMonitorsAndNoSize()
{
  writeGrid({{6, "A"}, {7, "B"}, {8, "C"}, {9, "D"}, {10, "E"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  // B has a size, C has monitors, D has nothing, E has a width and no height; A is the server
  Settings::setValue(Settings::Computer::ArrangementWidth.arg("B"), 1600);
  Settings::setValue(Settings::Computer::ArrangementHeight.arg("B"), 900);
  Settings::setValue(Settings::Computer::MonitorLayout.arg("C"), QStringLiteral("0,0,2560,1440"));
  Settings::setValue(Settings::Computer::ArrangementWidth.arg("E"), 1600);
  ServerConfig config;
  QCOMPARE(config.assumedSizes(), QStringList({"D", "E"}));

  // a size that a computer reported and that is adopted is not assumed any more
  config.setReportedScreen("D", QRect(0, 0, 2560, 1440));
  config.adoptReportedScreens();
  QCOMPARE(config.assumedSizes(), QStringList({"E"}));
}

void ArrangementConfigTests::monitorEditing_drawsWhatIsStored_andStoresWhatIsMade()
{
  using namespace deskflow::server;
  // a Windows PC of two monitors that was stored as numbers, at 150 %, placed at 100,200
  Computer pc("PC");
  pc.setMonitorLayout(QStringLiteral("0,0,2560,1440;-1920,360,1920,1080"));
  pc.setArrangementZoom(100.0 / 150.0);
  pc.setMonitorScales({150.0, 100.0});
  const auto drawn = deskflow::gui::drawingOf(pc, 100, 200, std::nullopt);
  QCOMPARE(drawn.monitors.size(), size_t{2});
  QCOMPARE(drawn.monitors[0].scale, 150.0);
  QCOMPARE(drawn.monitors[0].pixelsW(), 2560);
  QVERIFY(std::abs(drawn.monitors[0].x - 100) < 1e-9);
  QVERIFY(std::abs(drawn.monitors[1].x - (100 - 1920 * 100.0 / 150.0)) < 1e-9);

  // a computer that has no layout is its typed size, or else the screen that it reported, or else a screen of the usual
  // size
  Computer typed("typed");
  typed.setArrangementSize(1600, 900);
  QCOMPARE(deskflow::gui::drawingOf(typed, 0, 0, std::nullopt).monitors[0].pixelsW(), 1600);
  Computer plain("plain");
  QCOMPARE(deskflow::gui::drawingOf(plain, 0, 0, std::nullopt).monitors[0].pixelsH(), 1080);
  const auto fromReport = deskflow::gui::drawingOf(plain, 0, 0, QRect(-1920, 0, 4480, 1440));
  QCOMPARE(fromReport.monitors[0].pixelsW(), 4480);
  QCOMPARE(fromReport.monitors[0].pixelsH(), 1440);

  // what the editor made is what the computer has: the layout, the zoom, the scales and the place
  auto layout = editor::withScale(drawn, 1, 125);
  const auto made = editor::realise(layout, MonitorRect{-1536, 0, 4096, 1440});
  QVERIFY2(made.fit == editor::Fit::Matches, made.error.c_str());
  deskflow::gui::apply(pc, made);
  QCOMPARE(pc.monitorLayout(), QString::fromStdString(made.text));
  QCOMPARE(pc.monitorScales(), (QList<double>{150.0, 125.0}));
  QCOMPARE(pc.arrangementZoom(), made.zoom);
  QVERIFY(pc.arrangementPlaced());
  QCOMPARE(pc.arrangementX(), made.originX);
  QCOMPARE(pc.arrangementY(), made.originY);
}

void ArrangementConfigTests::setComputerSize_isTheSizeOfAComputerThatHasNone_andTheMonitorOfOneThatHasOne()
{
  writeGrid({{6, "A"}, {7, "B"}, {8, "C"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  Settings::setValue(Settings::Computer::MonitorLayout.arg("C"), QStringLiteral("-100,40,1920,1080"));
  Settings::setValue(Settings::Computer::MonitorScales.arg("C"), QStringLiteral("150"));
  ServerConfig config;
  const auto find = [&](const char *name) -> const ::Computer & {
    for (const auto &c : std::as_const(config).computers()) {
      if (c.name() == name)
        return c;
    }
    qFatal("there is no %s", name);
  };

  config.setComputerSize("b", 2560, 1440);
  QCOMPARE(find("B").arrangementWidth(), 2560);
  QCOMPARE(find("B").arrangementHeight(), 1440);
  QVERIFY(find("B").monitorLayout().isEmpty());
  QVERIFY(!config.assumedSizes().contains("B"));

  // a layout of one monitor is a layout of one monitor, where it was; its scale is no longer one that was drawn
  config.setComputerSize("C", 2560, 1440);
  QCOMPARE(find("C").monitorLayout(), QStringLiteral("-100,40,2560,1440"));
  QVERIFY(find("C").monitorScales().isEmpty());
  QCOMPARE(find("C").arrangementWidth(), 0);

  // the server's screen is the operating system's, and a size that cannot be is none
  config.setComputerSize("A", 800, 600);
  QCOMPARE(find("A").arrangementWidth(), 0);
  config.setComputerSize("B", 0, 100);
  QCOMPARE(find("B").arrangementWidth(), 2560);
  forget({"C"});
}

void ArrangementConfigTests::setComputerSize_neverStoresWhatALayoutCannotHave()
{
  writeGrid({{6, "A"}, {7, "B"}, {8, "C"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  Settings::setValue(Settings::Computer::MonitorLayout.arg("C"), QStringLiteral("-100,40,1920,1080"));
  ServerConfig config;
  const auto find = [&](const char *name) -> const ::Computer & {
    for (const auto &c : std::as_const(config).computers()) {
      if (c.name() == name)
        return c;
    }
    qFatal("there is no %s", name);
  };
  // the numbers of a layout are 16 bits: a size that is more is not stored, as a size or as a monitor
  config.setComputerSize("B", 40000, 1000);
  QCOMPARE(find("B").arrangementWidth(), 0);
  config.setComputerSize("C", 40000, 1000);
  QCOMPARE(find("C").monitorLayout(), QStringLiteral("-100,40,1920,1080"));
  config.setComputerSize("B", 32767, 1000);
  QCOMPARE(find("B").arrangementWidth(), 32767);
  forget({"C"});
}

void ArrangementConfigTests::setComputerZoom_scalesTheWholeComputer_andKeepsItsMonitorsAndItsSize()
{
  writeGrid({{6, "A"}, {7, "B"}, {8, "C"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  Settings::setValue(Settings::Computer::MonitorLayout.arg("B"), QStringLiteral("0,0,2560,1440;2560,0,1920,1080"));
  Settings::setValue(Settings::Computer::MonitorScales.arg("B"), QStringLiteral("150,,100"));
  Settings::setValue(Settings::Computer::ArrangementWidth.arg("C"), 1600);
  Settings::setValue(Settings::Computer::ArrangementHeight.arg("C"), 900);
  ServerConfig config;
  const auto find = [&](const char *name) -> const ::Computer & {
    for (const auto &c : std::as_const(config).computers()) {
      if (c.name() == name)
        return c;
    }
    qFatal("there is no %s", name);
  };

  // the layout is what it was, the zoom is the new one; a drawn scale follows the zoom (half the size: twice the scale)
  config.setComputerZoom("B", 0.5);
  QCOMPARE(find("B").arrangementZoom(), 0.5);
  QCOMPARE(find("B").monitorLayout(), QStringLiteral("0,0,2560,1440;2560,0,1920,1080"));
  QCOMPARE(find("B").monitorScales().size(), 3);
  QCOMPARE(find("B").monitorScales()[0], 300.0);
  QVERIFY(std::isnan(find("B").monitorScales()[1]));
  QCOMPARE(find("B").monitorScales()[2], 200.0);
  // scales that would not be scales are dropped, and every monitor follows the zoom
  config.setComputerZoom("B", 0.05);
  QCOMPARE(find("B").arrangementZoom(), 0.05);
  QVERIFY(find("B").monitorScales().isEmpty());

  // a size that was typed is what it was
  config.setComputerZoom("c", 0.8);
  QCOMPARE(find("C").arrangementZoom(), 0.8);
  QCOMPARE(find("C").arrangementWidth(), 1600);
  QCOMPARE(find("C").arrangementHeight(), 900);

  // not the server's, and not a zoom that cannot be one
  config.setComputerZoom("A", 2.0);
  QCOMPARE(find("A").arrangementZoom(), 1.0);
  for (const double zoom : {0.0, -1.0, 1000.0, 0.001, std::nan("")}) {
    config.setComputerZoom("C", zoom);
    QCOMPARE(find("C").arrangementZoom(), 0.8);
  }
  forget({"B", "C"});
}

void ArrangementConfigTests::reportedScreen_doesNotChangeWhatIsGenerated()
{
  // what a computer reported is in memory only: the file that is written must be the same with it and without it
  writeGrid({{6, "A"}, {7, "B"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  place("A", 0, 0, 1.0, 1920, 1080);
  place("B", 1920, 0);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  ServerConfig live;
  live.setReportedScreen("B", QRect(0, 0, 2560, 1440));
  const ServerConfig fresh;
  QCOMPARE(generated(live), generated(fresh));
}

void ArrangementConfigTests::reportedScreen_isAdoptedAsTheSizeOfAComputerThatHasNone()
{
  writeGrid({{6, "A"}, {7, "B"}, {8, "C"}, {9, "D"}}, "A");
  ServerConfig::overrideServerMonitors(std::vector<deskflow::server::MonitorRect>{});
  // B has a size that was said, C and D have none; C's screen begins at -1920, which a size cannot say
  Settings::setValue(Settings::Computer::ArrangementWidth.arg("B"), 1600);
  Settings::setValue(Settings::Computer::ArrangementHeight.arg("B"), 900);
  ServerConfig config;
  config.setReportedScreen("b", QRect(0, 0, 2560, 1440));
  config.setReportedScreen("C", QRect(-1920, 0, 4480, 1440));
  config.setReportedScreen("D", QRect(0, 0, 2560, 1440));
  config.adoptReportedScreens();
  const auto arrangement = config.classicGridAsArrangement().arrangement;
  const auto shapeOf = [&](const char *name) {
    return arrangement.computers[static_cast<size_t>(arrangement.indexOf(name))].shape();
  };
  // a size that was said wins over what the computer reports, which is what the dialog warns of
  QCOMPARE(shapeOf("B"), (deskflow::server::MonitorRect{0, 0, 1600, 900}));
  QCOMPARE(shapeOf("D"), (deskflow::server::MonitorRect{0, 0, 2560, 1440}));
  // one that starts elsewhere, and one that nothing was said of, is the usual monitor
  QCOMPARE(shapeOf("C"), (deskflow::server::MonitorRect{0, 0, 1920, 1080}));
  QCOMPARE(shapeOf("A"), (deskflow::server::MonitorRect{0, 0, 1920, 1080}));
  QVERIFY(!config.reportedScreen("A"));
  QVERIFY(config.reportedScreen("c"));
}

QTEST_MAIN(ArrangementConfigTests)
