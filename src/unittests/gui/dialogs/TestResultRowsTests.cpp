/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TestResultRowsTests.h"

#include "../core/FakeProbeNetwork.h"
#include "common/Settings.h"
#include "gui/dialogs/ConnectionTestDialog.h"
#include "gui/dialogs/TunnelTestDialog.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/tunnel/TunnelTestRunner.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/TestResultRows.h"

#include <QApplication>
#include <QLabel>
#include <QPixmapCache>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBrowser>

using namespace deskflow::gui;
using namespace probetest;
using deskflow::diagnosis::FailureKind;
using Kind = StatusChip::Kind;
using Row = TestResultRows::Row;

namespace {

void installDeskflowStyle()
{
  auto *style = new style::DeskflowStyle;
  QApplication::setStyle(style);
  style::Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

ConnectionTestDialog::ProbeFactory fakeProbes(FakeNetwork &network)
{
  return [&network](QObject *parent) {
    auto *probe = makeProbe(network, 120);
    probe->setParent(parent);
    return probe;
  };
}

void waitForTheEnd(const ConnectionTestDialog &dialog)
{
  QTRY_VERIFY_WITH_TIMEOUT(!dialog.isTesting(), 5000);
}

QStringList titles(const TestResultRows &rows)
{
  QStringList texts;
  for (const auto &row : rows.rows())
    texts << row.title;
  return texts;
}

} // namespace

void TestResultRowsTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  qRegisterMetaType<ProbeResult>();
  qRegisterMetaType<QList<ProbeResult>>();
}

void TestResultRowsTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QPixmapCache::clear();
  Settings::setValue(Settings::Client::ConnectTimeout);
}

void TestResultRowsTests::rows_haveAChipAndASentence_andWhatToTryIsHiddenUntilItIsAskedFor()
{
  installDeskflowStyle();
  TestResultRows rows;
  rows.resize(640, 480);
  rows.setIntro(QStringLiteral("what the test does"));
  rows.setRows({
      Row{Kind::Success, QStringLiteral("Open"), QStringLiteral("10.0.0.6:24800"), QStringLiteral("Open (8 ms)"), {}},
      Row{Kind::Danger,
          QStringLiteral("Failed"),
          QStringLiteral("10.0.0.7:24800"),
          QStringLiteral("Refused: nothing listens"),
          {QStringLiteral("Check the server."), QStringLiteral("Check the port.")}},
  });
  rows.setConclusion(QStringLiteral("Put the address that works first."), false);
  rows.show();
  QVERIFY(QTest::qWaitForWindowExposed(&rows));

  QCOMPARE(rows.rowCount(), 2);
  QCOMPARE(rows.intro(), QStringLiteral("what the test does"));
  QCOMPARE(rows.conclusion(), QStringLiteral("Put the address that works first."));
  // a card for each row, with the chip that says how it went
  QCOMPARE(rows.card(0)->findChild<StatusChip *>()->text(), QStringLiteral("Open"));
  QCOMPARE(rows.card(0)->findChild<StatusChip *>()->kind(), Kind::Success);
  QCOMPARE(rows.card(1)->findChild<StatusChip *>()->text(), QStringLiteral("Failed"));
  QCOMPARE(rows.card(1)->findChild<StatusChip *>()->kind(), Kind::Danger);
  QCOMPARE(rows.card(1)->rows().first()->title(), QStringLiteral("10.0.0.7:24800"));
  // what to try is for the row that has it, and is shown when it is asked for
  QVERIFY(rows.hintsButton(0) == nullptr);
  QVERIFY(rows.hintsButton(1) != nullptr);
  QVERIFY(!rows.hintsShown(1));
  rows.hintsButton(1)->click();
  QVERIFY(rows.hintsShown(1));
  QVERIFY(rows.card(1)->findChild<QWidget *>(QStringLiteral("testHints"))->isVisibleTo(&rows));
  rows.hintsButton(1)->click();
  QVERIFY(!rows.hintsShown(1));

  // nothing is left of the rows that are taken away, and the sentences can be taken away
  rows.setRows({});
  rows.setIntro({});
  rows.setConclusion({});
  QCOMPARE(rows.rowCount(), 0);
  QVERIFY(rows.findChild<QWidget *>(QStringLiteral("testIntro"))->isHidden());
  QVERIFY(rows.findChild<QWidget *>(QStringLiteral("testConclusion"))->isHidden());
}

void TestResultRowsTests::rows_keepWhatWasOpened_whileTheTestGoesOn()
{
  installDeskflowStyle();
  TestResultRows rows;
  const Row failed{
      Kind::Danger, QStringLiteral("Failed"), QStringLiteral("a"), QStringLiteral("d"), {QStringLiteral("hint")}
  };
  rows.setRows({failed});
  rows.hintsButton(0)->click();
  QVERIFY(rows.hintsShown(0));

  // another row comes while the test goes on: the first one is still open
  rows.setRows({failed, Row{Kind::Info, QStringLiteral("Testing"), QStringLiteral("b"), {}, {}}});
  QCOMPARE(rows.rowCount(), 2);
  QVERIFY(rows.hintsShown(0));
  // the same rows again change nothing
  rows.setRows({failed, Row{Kind::Info, QStringLiteral("Testing"), QStringLiteral("b"), {}, {}}});
  QVERIFY(rows.hintsShown(0));
  // other words of what to try are another row: it is closed
  auto other = failed;
  other.hints = {QStringLiteral("other hint")};
  rows.setRows({other});
  QVERIFY(!rows.hintsShown(0));
}

// The button is pressed as a person presses it (the mouse and the key Space), not called: what is shown follows the
// button
void TestResultRowsTests::rows_theButtonOpensAndClosesWhatToTry_forTheMouseAndTheKeySpace()
{
  installDeskflowStyle();
  TestResultRows rows;
  rows.resize(640, 480);
  rows.setRows(
      {Row{Kind::Danger, QStringLiteral("Failed"), QStringLiteral("a"), QStringLiteral("d"), {QStringLiteral("hint")}}}
  );
  rows.show();
  QVERIFY(QTest::qWaitForWindowExposed(&rows));
  auto *button = rows.hintsButton(0);
  QVERIFY(button != nullptr);
  QVERIFY(!rows.hintsShown(0));

  QTest::mouseClick(button, Qt::LeftButton);
  QVERIFY(button->isChecked());
  QVERIFY(rows.hintsShown(0));
  QTest::mouseClick(button, Qt::LeftButton);
  QVERIFY(!rows.hintsShown(0));
  QTest::keyClick(button, Qt::Key_Space);
  QVERIFY(rows.hintsShown(0));
  QTest::keyClick(button, Qt::Key_Space);
  QVERIFY(!rows.hintsShown(0));
}

void TestResultRowsTests::connectionDialog_newLook_showsRowsInThePlaceOfTheText()
{
  installDeskflowStyle();
  FakeNetwork network;
  network.scripts["10.0.0.5"] = Script{};
  network.scripts["10.0.0.6"] = open(8);
  network.scripts["10.0.0.7"] = fail(FailureKind::Refused, QStringLiteral("Connection refused"), 4);
  ConnectionTestDialog dialog(nullptr, QStringLiteral("10.0.0.5,10.0.0.6,10.0.0.7"), 24800, fakeProbes(network));
  dialog.show();
  dialog.runTest();
  waitForTheEnd(dialog);

  auto *rows = dialog.findChild<TestResultRows *>();
  QVERIFY(rows != nullptr);
  QVERIFY(dialog.findChild<QScrollArea *>(QStringLiteral("resultsRows"))->isVisibleTo(&dialog));
  // the text of the system look is not there
  QVERIFY(!dialog.findChild<QTextBrowser *>(QStringLiteral("resultsView"))->isVisibleTo(&dialog));

  QCOMPARE(
      titles(*rows),
      (QStringList{QStringLiteral("10.0.0.5:24800"), QStringLiteral("10.0.0.6:24800"), QStringLiteral("10.0.0.7:24800")}
      )
  );
  const auto all = rows->rows();
  QCOMPARE(all[0].kind, Kind::Danger);
  QVERIFY(all[0].detail.startsWith(QStringLiteral("Timed out")));
  QCOMPARE(all[1].kind, Kind::Success);
  QCOMPARE(all[1].state, QStringLiteral("Open"));
  QVERIFY(all[1].hints.isEmpty());
  QCOMPARE(all[2].kind, Kind::Danger);
  QVERIFY(all[2].detail.startsWith(QStringLiteral("Refused")));
  // what to try is what the text of the system look lists for the same results
  QVERIFY(all[0].hints.join(u' ').contains(QStringLiteral("allow inbound TCP port 24800")));
  // what the probe waited is not what the client waits: the sentence about a slow network is the client's, not a row's
  QVERIFY(!all[0].hints.contains(ConnectionTestDialog::hintText(deskflow::diagnosis::Hint::SlowNetwork, 24800, 0.12)));
  QVERIFY(all[0].hints.size() >= 3);
  QVERIFY(all[2].hints.join(u' ').contains(QStringLiteral("runs in server mode")));
  QCOMPARE(rows->intro().left(18), QStringLiteral("Tries to open a co"));
  QCOMPARE(
      rows->conclusion(),
      QStringLiteral("Put the address that works first in the list, and remove the ones that do not.")
  );
  QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("lblStatus"))->text().contains(QStringLiteral("1 of 3")));
  QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("btnCopySummary"))->isEnabled());
  // the sentence that is pasted into a message is the same in any look
  QVERIFY(dialog.summary().contains(QStringLiteral("10.0.0.6")));
}

void TestResultRowsTests::connectionDialog_systemLook_keepsItsText()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  FakeNetwork network;
  network.scripts["10.0.0.6"] = open(8);
  ConnectionTestDialog dialog(nullptr, QStringLiteral("10.0.0.6"), 24800, fakeProbes(network));
  dialog.show();
  dialog.runTest();
  waitForTheEnd(dialog);
  QVERIFY(dialog.findChild<TestResultRows *>() == nullptr);
  QVERIFY(dialog.findChild<QScrollArea *>(QStringLiteral("resultsRows")) == nullptr);
  auto *view = dialog.findChild<QTextBrowser *>(QStringLiteral("resultsView"));
  QVERIFY(view->isVisibleTo(&dialog));
  QVERIFY(view->toPlainText().contains(QStringLiteral("10.0.0.6:24800")));
}

void TestResultRowsTests::connectionDialog_aSlowConnect_isAWarning()
{
  ProbeResult slow;
  slow.open = true;
  slow.host = slow.address = QStringLiteral("10.0.0.5");
  slow.port = 24800;
  slow.elapsedMs = 1500;
  ProbeResult quick = slow;
  quick.elapsedMs = 30;
  const auto rows = ConnectionTestDialog::rowsFor({slow, quick}, 2.0, 2.0);
  QCOMPARE(rows.size(), 2);
  // the connect took more than half of what the client allows for the whole connection: it is slow
  QCOMPARE(rows[0].kind, Kind::Warning);
  QCOMPARE(rows[0].state, QStringLiteral("Slow"));
  QCOMPARE(rows[1].kind, Kind::Success);
  QCOMPARE(rows[1].state, QStringLiteral("Open"));
  // an address that opens has nothing to try
  QVERIFY(rows[0].hints.isEmpty());
}

void TestResultRowsTests::connectionDialog_noAddress_saysWhatToDo()
{
  installDeskflowStyle();
  FakeNetwork network;
  ConnectionTestDialog dialog(nullptr, QString(), 24800, fakeProbes(network));
  dialog.show();
  dialog.runTest();
  auto *rows = dialog.findChild<TestResultRows *>();
  QVERIFY(rows != nullptr);
  QCOMPARE(rows->rowCount(), 0);
  QVERIFY(rows->conclusion().contains(QStringLiteral("\"Connect to\" field")));
  QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("btnCopySummary"))->isEnabled());
  QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("btnRunAgain"))->isEnabled());
}

void TestResultRowsTests::tunnelDialog_rowsFollowTheStagesAndGiveWhatToTryToTheOneThatFailed()
{
  using namespace deskflow::tunnel;
  TunnelTestReport report;
  report.hostname = QStringLiteral("mbp.example.com");
  report.localPort = 24800;
  report.stages = {
      {TestStage::Cloudflared, StageState::Ok, QStringLiteral("/usr/local/bin/cloudflared")},
      {TestStage::Credentials, StageState::Failed, QStringLiteral("No Service Token is stored.")},
      {TestStage::Tls, StageState::Skipped, {}},
      {TestStage::Hostname, StageState::Pending, {}},
      {TestStage::Process, StageState::Running, {}},
  };
  // while the test goes on: no sentence of what to try yet
  auto rows = TunnelTestDialog::rowsFor(report);
  QCOMPARE(rows.size(), 5);
  QVERIFY(rows[1].hints.isEmpty());
  QCOMPARE(rows[0].kind, Kind::Success);
  QCOMPARE(rows[0].state, QStringLiteral("OK"));
  QCOMPARE(rows[1].kind, Kind::Danger);
  QCOMPARE(rows[1].state, QStringLiteral("Failed"));
  QCOMPARE(rows[2].kind, Kind::Neutral);
  QCOMPARE(rows[2].state, QStringLiteral("Skipped"));
  QCOMPARE(rows[3].state, QStringLiteral("Waiting"));
  QCOMPARE(rows[4].kind, Kind::Info);
  QCOMPARE(rows[4].state, QStringLiteral("Running"));
  for (int i = 0; i < rows.size(); ++i)
    QCOMPARE(rows[i].title, TunnelTestReport::stageName(report.stages[i].stage));
  QCOMPARE(rows[1].detail, QStringLiteral("No Service Token is stored."));

  // the test is over: the first stage that failed has what to do about the failure, and only that one
  report.finished = true;
  report.failure = TunnelFailure::CredentialsMissing;
  rows = TunnelTestDialog::rowsFor(report);
  QCOMPARE(rows[1].hints, QStringList{TunnelTestDialog::hintText(TunnelFailure::CredentialsMissing)});
  QVERIFY(rows[1].hints.first().contains(QStringLiteral("enter the Service Token")));
  for (int i = 0; i < rows.size(); ++i) {
    if (i != 1)
      QVERIFY(rows[i].hints.isEmpty());
  }
}

void TestResultRowsTests::tunnelDialog_aFailureNoStageNames_isUnderTheRows()
{
  using namespace deskflow::tunnel;
  TunnelTestReport report;
  report.finished = true;
  report.failure = TunnelFailure::SettingsChanged;
  report.stages = {{TestStage::Cloudflared, StageState::Ok, {}}};
  const auto rows = TunnelTestDialog::rowsFor(report);
  QCOMPARE(rows.size(), 1);
  QVERIFY(rows[0].hints.isEmpty());
}

QTEST_MAIN(TestResultRowsTests)
