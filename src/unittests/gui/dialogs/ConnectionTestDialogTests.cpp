/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ConnectionTestDialogTests.h"

#include "../core/FakeProbeNetwork.h"
#include "common/Settings.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/ConnectionTestDialog.h"
#include "gui/tunnel/SecretStoreFactory.h"

#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QSysInfo>
#include <QTest>
#include <QTextBrowser>

using namespace deskflow::gui;
using namespace probetest;
using deskflow::diagnosis::FailureKind;
using deskflow::diagnosis::Hint;

namespace {

constexpr auto kScreenshotDirVariable = "DESKFLOW_SCREENSHOT_DIR";

// the three answers a test of three addresses gets in the screenshots and most tests
void scriptOpenTimedOutAndRefused(FakeNetwork &network)
{
  network.scripts["10.0.0.5"] = Script{};
  network.scripts["10.0.0.6"] = open(8);
  network.scripts["10.0.0.7"] = fail(FailureKind::Refused, QStringLiteral("Connection refused"), 4);
}

ConnectionTestDialog::ProbeFactory fakeProbes(FakeNetwork &network, int timeoutMs = 120)
{
  return [&network, timeoutMs](QObject *parent) {
    auto *probe = makeProbe(network, timeoutMs);
    probe->setParent(parent);
    return probe;
  };
}

void waitForTheEnd(const ConnectionTestDialog &dialog)
{
  QTRY_VERIFY_WITH_TIMEOUT(!dialog.isTesting(), 5000);
}

QString textOf(const QDialog &dialog)
{
  return dialog.findChild<QTextBrowser *>(QStringLiteral("resultsView"))->toPlainText();
}

QString statusOf(const QDialog &dialog)
{
  return dialog.findChild<QLabel *>(QStringLiteral("lblStatus"))->text();
}

bool isMac()
{
  return QSysInfo::productType() == QStringLiteral("macos");
}

} // namespace

void ConnectionTestDialogTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  // a dialog that is made without a secret store of its own gets the one in memory: no test touches a keychain
  qputenv(deskflow::tunnel::kSecretStoreVariable, "memory");
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("Deskflow.state")));
  qRegisterMetaType<ProbeResult>();
  qRegisterMetaType<QList<ProbeResult>>();
}

void ConnectionTestDialogTests::cleanup()
{
  Settings::setValue(Settings::Client::ConnectTimeout);
}

void ConnectionTestDialogTests::hintText_hasWordsForEveryHint()
{
  for (int value = static_cast<int>(Hint::ServerFirewall); value <= static_cast<int>(Hint::SlowNetwork); ++value) {
    const auto text = ConnectionTestDialog::hintText(static_cast<Hint>(value), 24800, 2.0);
    QVERIFY2(!text.isEmpty(), qPrintable(QString::number(value)));
  }
  const auto firewall = ConnectionTestDialog::hintText(Hint::ServerFirewall, 24801, 2.0);
  QVERIFY(firewall.contains(QStringLiteral("24801")));
  QVERIFY(firewall.contains(QStringLiteral("server")));
  const auto mac = ConnectionTestDialog::hintText(Hint::MacLocalNetwork, 24800, 2.0);
  QVERIFY(mac.contains(QStringLiteral("System Settings > Privacy & Security > Local Network")));
}

void ConnectionTestDialogTests::statusText_saysWhatEachResultMeans()
{
  ProbeResult result;
  result.host = result.address = QStringLiteral("10.0.0.5");
  result.port = 24800;

  result.open = true;
  result.elapsedMs = 12;
  QVERIFY(ConnectionTestDialog::statusText(result, 2.0).startsWith(QStringLiteral("Open")));

  result.open = false;
  const auto word = [&](FailureKind kind) {
    result.kind = kind;
    return ConnectionTestDialog::statusText(result, 2.0);
  };
  QVERIFY(word(FailureKind::TimedOut).startsWith(QStringLiteral("Timed out")));
  QVERIFY(word(FailureKind::Refused).startsWith(QStringLiteral("Refused")));
  QVERIFY(word(FailureKind::NoRoute).startsWith(QStringLiteral("No route")));
  QVERIFY(word(FailureKind::DnsFailure).startsWith(QStringLiteral("Name not found")));
  QVERIFY(word(FailureKind::Reset).startsWith(QStringLiteral("Reset")));
  QVERIFY(word(FailureKind::AccessDenied).startsWith(QStringLiteral("Blocked")));
  result.detail = QStringLiteral("odd error");
  QVERIFY(word(FailureKind::Unknown).contains(QStringLiteral("odd error")));
  result.problem = QStringLiteral("the port is wrong");
  QVERIFY(ConnectionTestDialog::statusText(result, 2.0).contains(QStringLiteral("the port is wrong")));
}

void ConnectionTestDialogTests::statusText_warnsWhenTheConnectIsSlowerThanTheClientWaits()
{
  ProbeResult result;
  result.open = true;
  result.elapsedMs = 2500;
  result.port = 24800;

  const auto slow = ConnectionTestDialog::statusText(result, 2.0);
  QVERIFY2(slow.contains(QStringLiteral("client/connectTimeout")), qPrintable(slow));

  result.elapsedMs = 40;
  QVERIFY(!ConnectionTestDialog::statusText(result, 2.0).contains(QStringLiteral("client/connectTimeout")));
}

void ConnectionTestDialogTests::runTest_withOpenTimedOutAndRefused_showsEachWithItsHints()
{
  FakeNetwork network;
  scriptOpenTimedOutAndRefused(network);
  ConnectionTestDialog dialog(nullptr, QStringLiteral("10.0.0.5,10.0.0.6,10.0.0.7"), 24800, fakeProbes(network));
  dialog.show();

  dialog.runTest();
  QVERIFY(dialog.isTesting());
  QVERIFY(statusOf(dialog).contains(QStringLiteral("Testing")));
  waitForTheEnd(dialog);

  const auto text = textOf(dialog);
  QVERIFY2(text.contains(QStringLiteral("10.0.0.5:24800")), qPrintable(text));
  QVERIFY(text.contains(QStringLiteral("Timed out")));
  QVERIFY(text.contains(QStringLiteral("10.0.0.6:24800")));
  QVERIFY(text.contains(QStringLiteral("Open")));
  QVERIFY(text.contains(QStringLiteral("10.0.0.7:24800")));
  QVERIFY(text.contains(QStringLiteral("Refused")));
  // the hints that go with a silent drop
  QVERIFY(text.contains(QStringLiteral("allow inbound TCP port 24800")));
  QVERIFY(text.contains(QStringLiteral("keep computers from talking to each other")));
  QVERIFY(text.contains(QStringLiteral("swapped")));
  // and with a refusal
  QVERIFY(text.contains(QStringLiteral("runs in server mode")));
  QCOMPARE(text.contains(QStringLiteral("System Settings > Privacy & Security > Local Network")), isMac());
  // the order of the field
  QVERIFY(text.indexOf(QStringLiteral("10.0.0.5")) < text.indexOf(QStringLiteral("10.0.0.6")));
  QVERIFY(text.indexOf(QStringLiteral("10.0.0.6")) < text.indexOf(QStringLiteral("10.0.0.7")));
  QVERIFY2(statusOf(dialog).contains(QStringLiteral("1 of 3")), qPrintable(statusOf(dialog)));
  QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("btnCopySummary"))->isEnabled());
  QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("btnRunAgain"))->isEnabled());
}

void ConnectionTestDialogTests::runTest_withAnOpenAndAFailedAddress_tellsToPutTheOneThatWorksFirst()
{
  FakeNetwork network;
  scriptOpenTimedOutAndRefused(network);
  ConnectionTestDialog dialog(nullptr, QStringLiteral("10.0.0.5,10.0.0.6"), 24800, fakeProbes(network));
  dialog.runTest();
  waitForTheEnd(dialog);

  QVERIFY2(textOf(dialog).contains(QStringLiteral("Put the address that works first")), qPrintable(textOf(dialog)));

  FakeNetwork allFail;
  allFail.scripts["10.0.0.5"] = Script{};
  ConnectionTestDialog failing(nullptr, QStringLiteral("10.0.0.5"), 24800, fakeProbes(allFail));
  failing.runTest();
  waitForTheEnd(failing);
  QVERIFY(!textOf(failing).contains(QStringLiteral("Put the address that works first")));
}

void ConnectionTestDialogTests::runTest_withANameThatDoesNotResolve_saysSoAndHasNoPort()
{
  FakeNetwork network;
  network.resolveErrors["ghost.example"] = QStringLiteral("Host not found");
  ConnectionTestDialog dialog(nullptr, QStringLiteral("ghost.example"), 24800, fakeProbes(network));
  dialog.runTest();
  waitForTheEnd(dialog);

  const auto text = textOf(dialog);
  QVERIFY2(text.contains(QStringLiteral("Name not found")), qPrintable(text));
  QVERIFY(text.contains(QStringLiteral("ghost.example")));
  QVERIFY(text.contains(QStringLiteral("IP address instead")));
}

void ConnectionTestDialogTests::runTest_withNothingTyped_saysThereIsNothingToTest()
{
  FakeNetwork network;
  ConnectionTestDialog dialog(nullptr, QStringLiteral("  ,  "), 24800, fakeProbes(network));
  dialog.runTest();

  QVERIFY(!dialog.isTesting());
  QVERIFY2(textOf(dialog).contains(QStringLiteral("Type the address")), qPrintable(textOf(dialog)));
  QVERIFY(network.connected.isEmpty());
  QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("btnCopySummary"))->isEnabled());
}

void ConnectionTestDialogTests::runTest_again_startsANewTest()
{
  FakeNetwork network;
  network.scripts["10.0.0.6"] = open();
  ConnectionTestDialog dialog(nullptr, QStringLiteral("10.0.0.6"), 24800, fakeProbes(network));
  dialog.show();
  dialog.runTest();
  waitForTheEnd(dialog);
  QCOMPARE(network.connected.size(), 1);

  dialog.findChild<QPushButton *>(QStringLiteral("btnRunAgain"))->click();
  QVERIFY(dialog.isTesting());
  waitForTheEnd(dialog);

  QCOMPARE(network.connected.size(), 2);
  QVERIFY(textOf(dialog).contains(QStringLiteral("10.0.0.6:24800")));
}

// the system clipboard is not touched by the tests: the dialog gives its text to the writer it is given
void ConnectionTestDialogTests::copySummary_givesTheSummaryToTheWriterOnly()
{
  FakeNetwork network;
  network.scripts["10.0.0.6"] = open();
  QStringList copied;
  ConnectionTestDialog dialog(
      nullptr, QStringLiteral("10.0.0.6"), 24800, fakeProbes(network),
      [&copied](const QString &text) { copied << text; }
  );
  dialog.runTest();
  waitForTheEnd(dialog);

  dialog.findChild<QPushButton *>(QStringLiteral("btnCopySummary"))->click();

  QCOMPARE(copied.size(), 1);
  QCOMPARE(copied.first(), dialog.summary());
}

void ConnectionTestDialogTests::clientConfigDialog_showsTheConnectTimeoutTheClientUses()
{
  const auto shown = [] {
    ClientConfigDialog dialog;
    return dialog.findChild<QDoubleSpinBox *>(QStringLiteral("sbConnectTimeout"))->value();
  };

  // not set: the 2 s the client has always waited
  Settings::setValue(Settings::Client::ConnectTimeout);
  QCOMPARE(shown(), 2.0);
  Settings::setValue(Settings::Client::ConnectTimeout, 7.5);
  QCOMPARE(shown(), 7.5);
  // what the client ignores is not shown as if it were used
  Settings::setValue(Settings::Client::ConnectTimeout, QStringLiteral("soon"));
  QCOMPARE(shown(), 2.0);
  Settings::setValue(Settings::Client::ConnectTimeout, 500.0);
  QCOMPARE(shown(), 2.0);
}

void ConnectionTestDialogTests::summary_isOneLineThatNamesEveryAddress()
{
  FakeNetwork network;
  scriptOpenTimedOutAndRefused(network);
  ConnectionTestDialog dialog(nullptr, QStringLiteral("10.0.0.5,10.0.0.6,10.0.0.7"), 24800, fakeProbes(network));
  dialog.runTest();
  waitForTheEnd(dialog);

  const auto summary = dialog.summary();
  QVERIFY2(summary.startsWith(QStringLiteral("Deskflow connection test ")), qPrintable(summary));
  QVERIFY(summary.contains(QStringLiteral("10.0.0.5:24800 timed out (no answer in 120 ms)")));
  QVERIFY(summary.contains(QStringLiteral("10.0.0.6:24800 open in ")));
  QVERIFY(summary.contains(QStringLiteral("10.0.0.7:24800 refused (nothing listens there)")));
  QVERIFY(!summary.contains(QLatin1Char('\n')));
}

// Pictures of the panel for the three answers (not a check: set DESKFLOW_SCREENSHOT_DIR to get them)
void ConnectionTestDialogTests::screenshots_ofTheResultPanel()
{
  const auto dir = qEnvironmentVariable(kScreenshotDirVariable);

  struct Scene
  {
    const char *name;
    const char *servers;
    int timeoutMs;
  };
  // the silent address waits the 3 s the real probe waits, so that the picture says what a person would see
  for (const auto &scene :
       {Scene{"open", "10.0.0.6", 120}, Scene{"timed-out", "10.0.0.5", 3000}, Scene{"refused", "10.0.0.7", 120}}) {
    FakeNetwork network;
    scriptOpenTimedOutAndRefused(network);
    ConnectionTestDialog dialog(
        nullptr, QString::fromLatin1(scene.servers), 24800, fakeProbes(network, scene.timeoutMs)
    );
    // room for the whole panel, as a person who opens the dialog would give it
    dialog.resize(dialog.sizeHint().expandedTo(QSize(dialog.fontMetrics().averageCharWidth() * 80, 0)));
    dialog.show();
    dialog.runTest();
    QTRY_VERIFY_WITH_TIMEOUT(!dialog.isTesting(), 6000);
    QCoreApplication::processEvents();

    const auto picture = dialog.grab();
    QVERIFY(!picture.isNull());
    if (!dir.isEmpty() && QDir().mkpath(dir)) {
      QVERIFY(picture.save(
          QDir(dir).filePath(QStringLiteral("test-connection-%1.png").arg(QString::fromLatin1(scene.name)))
      ));
    }
  }
}

QTEST_MAIN(ConnectionTestDialogTests)
