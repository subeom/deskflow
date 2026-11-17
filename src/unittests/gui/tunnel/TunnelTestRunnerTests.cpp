/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelTestRunnerTests.h"

#include "TunnelTestDoubles.h"

#include "gui/tunnel/TunnelTestRunner.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

using namespace deskflow::tunnel;
using namespace tunneltest;

namespace {

QTemporaryDir &scratchDir()
{
  static QTemporaryDir dir;
  return dir;
}

// A runner on a controller made of fakes; the connection through the tunnel is whatever the test tells it
struct Rig
{
  ControllerRig tunnel;
  std::vector<std::function<void(HoldOutcome)>> holds;
  std::vector<int> heldPorts;
  bool cloudflaredFound = true;
  std::unique_ptr<TunnelTestRunner> runner;

  explicit Rig(const QString &base) : tunnel(base, kConfig, 80)
  {
    tunnel.turnOn();

    TunnelTestRunner::Dependencies deps;
    deps.controller = tunnel.controller.get();
    deps.store = std::make_shared<InMemorySecretStore>(tunnel.storeState);
    deps.locate = [this](const QString &path) {
      CloudflaredLocation location;
      if (cloudflaredFound) {
        location.source = CloudflaredLocation::Source::Configured;
        location.path = path.isEmpty() ? kProgram : path;
      }
      return location;
    };
    deps.holdConnection = [this](int port, int, std::function<void(HoldOutcome)> done) {
      heldPorts.push_back(port);
      holds.push_back(std::move(done));
    };
    deps.holdMs = 3000;
    deps.graceMs = 10;
    deps.startTimeoutMs = 60000;
    runner = std::make_unique<TunnelTestRunner>(std::move(deps));
  }

  // from the start of the tunnel to its listening
  void bringTheTunnelUp()
  {
    tunnel.advance(400);
    tunnel.prober->answer(true);
    tunnel.advance(400);
  }

  StageState state(TestStage stage) const
  {
    return runner->report().stages.at(static_cast<int>(stage)).state;
  }

  QString detail(TestStage stage) const
  {
    return runner->report().stages.at(static_cast<int>(stage)).detail;
  }
};

} // namespace

void TunnelTestRunnerTests::initTestCase()
{
  QVERIFY(scratchDir().isValid());
  m_dir = scratchDir().path();
}

void TunnelTestRunnerTests::aTunnelThatWorks_isStartedForTheTestPassesAndIsLetGo()
{
  Rig rig(m_dir);
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);

  rig.runner->start();
  QVERIFY(rig.runner->isRunning());
  for (const auto stage : {TestStage::Cloudflared, TestStage::Credentials, TestStage::Tls, TestStage::Hostname}) {
    QCOMPARE(rig.state(stage), StageState::Ok);
  }
  QCOMPARE(rig.state(TestStage::Process), StageState::Running);
  QCOMPARE(rig.state(TestStage::LocalPort), StageState::Pending);
  QCOMPARE(rig.state(TestStage::Connection), StageState::Pending);
  QCOMPARE(rig.tunnel.processes.size(), size_t(1));

  rig.bringTheTunnelUp();
  QCOMPARE(rig.state(TestStage::Process), StageState::Ok);
  QCOMPARE(rig.detail(TestStage::Process), QStringLiteral("started for this test"));
  QCOMPARE(rig.state(TestStage::LocalPort), StageState::Ok);
  QCOMPARE(rig.detail(TestStage::LocalPort), QStringLiteral("127.0.0.1:24800"));
  QCOMPARE(rig.state(TestStage::Connection), StageState::Running);
  QCOMPARE(rig.heldPorts, std::vector<int>{24800});
  QCOMPARE(finished.count(), 0);

  rig.holds.front()(HoldOutcome::Held);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
  QCOMPARE(rig.state(TestStage::Connection), StageState::Ok);
  QVERIFY(rig.runner->report().ok());
  QVERIFY(!rig.runner->isRunning());

  // the tunnel that was started for the test goes, after the short pause
  QSignalSpy gone(rig.tunnel.controller.get(), &TunnelController::statusChanged);
  QTRY_COMPARE_WITH_TIMEOUT(rig.tunnel.controller->status().state, TunnelState::Idle, 3000);
}

void TunnelTestRunnerTests::aTunnelThatRunsForTheClient_isUsedAndLeftRunning()
{
  Rig rig(m_dir);
  QVERIFY(rig.tunnel.bringUp());
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);

  rig.runner->start();
  QCOMPARE(rig.tunnel.processes.size(), size_t(1));
  QCOMPARE(rig.state(TestStage::Process), StageState::Ok);
  QCOMPARE(rig.detail(TestStage::Process), QStringLiteral("running for the client"));
  QCOMPARE(rig.state(TestStage::Connection), StageState::Running);

  rig.holds.front()(HoldOutcome::Held);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
  QVERIFY(rig.runner->report().ok());

  // the tunnel is the client's: it is still there after the pause in which a tunnel of the test would have gone
  QTest::qWait(250);
  QCOMPARE(rig.tunnel.controller->status().state, TunnelState::Listening);
  QCOMPARE(rig.tunnel.processes.size(), size_t(1));
}

void TunnelTestRunnerTests::aTunnelThatIsStartingForTheClient_isWaitedForAndNotLetGoByTheTest()
{
  Rig rig(m_dir);
  // the client asked for the tunnel and it is on its way
  rig.tunnel.controller->acquire();
  QCOMPARE(rig.tunnel.controller->status().state, TunnelState::Starting);
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);

  rig.runner->start();
  QCOMPARE(rig.state(TestStage::Process), StageState::Running);
  // it is waited for, and not started a second time
  QCOMPARE(rig.tunnel.processes.size(), size_t(1));

  rig.bringTheTunnelUp();
  QCOMPARE(rig.state(TestStage::Process), StageState::Ok);
  QCOMPARE(rig.detail(TestStage::Process), QStringLiteral("running for the client"));
  rig.holds.front()(HoldOutcome::Held);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);

  // the tunnel is the client's: the test does not let it go
  QTest::qWait(250);
  QCOMPARE(rig.tunnel.controller->status().state, TunnelState::Listening);
}

void TunnelTestRunnerTests::everyPrerequisiteIsReported_andNothingIsStartedWhenOneIsMissing()
{
  Rig rig(m_dir);
  rig.cloudflaredFound = false;
  rig.tunnel.storeState->token.reset();
  Settings::setValue(Settings::Security::TlsEnabled, false);
  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("not a host name"));
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);

  rig.runner->start();
  QCOMPARE(finished.count(), 1);
  for (const auto stage : {TestStage::Cloudflared, TestStage::Credentials, TestStage::Tls, TestStage::Hostname}) {
    QCOMPARE(rig.state(stage), StageState::Failed);
    QVERIFY(!rig.detail(stage).isEmpty());
  }
  for (const auto stage : {TestStage::Process, TestStage::LocalPort, TestStage::Connection}) {
    QCOMPARE(rig.state(stage), StageState::Skipped);
  }
  // the first of them is the reason
  QCOMPARE(rig.runner->report().failure, TunnelFailure::CloudflaredMissing);
  QVERIFY(!rig.runner->report().ok());
  QVERIFY(rig.tunnel.processes.empty());
  QVERIFY(rig.holds.empty());
  QVERIFY(!rig.runner->isRunning());

  // and one at a time
  Rig only(m_dir);
  only.tunnel.storeState->token.reset();
  only.runner->start();
  QCOMPARE(only.runner->report().failure, TunnelFailure::CredentialsMissing);
  QCOMPARE(only.state(TestStage::Cloudflared), StageState::Ok);
  QCOMPARE(only.state(TestStage::Credentials), StageState::Failed);
  QVERIFY(only.tunnel.processes.empty());

  Rig noTls(m_dir);
  Settings::setValue(Settings::Security::TlsEnabled, false);
  noTls.runner->start();
  QCOMPARE(noTls.runner->report().failure, TunnelFailure::TlsRequired);

  Rig noHost(m_dir);
  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("10.0.0.5"));
  noHost.runner->start();
  QCOMPARE(noHost.runner->report().failure, TunnelFailure::InvalidHostname);
}

void TunnelTestRunnerTests::theSecretIsNotReadToTestThatATokenIsStored()
{
  Rig rig(m_dir);
  rig.runner->start();
  // the Client ID was asked for; the Secret is read by the start of the tunnel only
  QVERIFY(rig.tunnel.storeState->idReadCount >= 1);
  QCOMPARE(rig.tunnel.storeState->loadCount, 1);
  QVERIFY(rig.detail(TestStage::Credentials).contains(kClientId));
  QVERIFY(!rig.detail(TestStage::Credentials).contains(kSecret));

  Rig missing(m_dir);
  missing.cloudflaredFound = false;
  missing.runner->start();
  QCOMPARE(missing.tunnel.storeState->loadCount, 0);
}

void TunnelTestRunnerTests::aTunnelThatDoesNotStart_failsAtTheProcessAndSkipsTheRest()
{
  Rig rig(m_dir);
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);
  rig.runner->start();

  rig.tunnel.process()->write(QStringLiteral("INF A browser window should have opened at the following URL:"));
  QCOMPARE(finished.count(), 1);
  QCOMPARE(rig.state(TestStage::Process), StageState::Failed);
  QCOMPARE(rig.state(TestStage::LocalPort), StageState::Skipped);
  QCOMPARE(rig.state(TestStage::Connection), StageState::Skipped);
  QCOMPARE(rig.runner->report().failure, TunnelFailure::LoginRequired);
  QVERIFY(rig.detail(TestStage::Process).contains(QStringLiteral("Service Token")));
  QVERIFY(rig.holds.empty());
}

void TunnelTestRunnerTests::aLocalPortThatDoesNotOpen_failsAtThePortAfterTheProcessStarted()
{
  Rig rig(m_dir);
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);
  rig.runner->start();

  // the port never opens: the supervisor gives up after its time out
  for (int step = 0; step < 100 && finished.count() == 0; ++step) {
    rig.tunnel.advance(100);
    rig.tunnel.prober->answer(false);
  }
  QCOMPARE(finished.count(), 1);
  QCOMPARE(rig.state(TestStage::Process), StageState::Ok);
  QCOMPARE(rig.state(TestStage::LocalPort), StageState::Failed);
  QCOMPARE(rig.state(TestStage::Connection), StageState::Skipped);
  QCOMPARE(rig.runner->report().failure, TunnelFailure::StartTimedOut);
}

void TunnelTestRunnerTests::aConnectionThatCloudflaredRefusesWithAReason_namesTheReason()
{
  Rig rig(m_dir);
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);
  rig.runner->start();
  rig.bringTheTunnelUp();
  QCOMPARE(rig.state(TestStage::Connection), StageState::Running);

  // cloudflared writes why while the connection is held, and closes it
  rig.tunnel.process()->write(QStringLiteral("ERR failed to connect to origin error=\"websocket: bad handshake\""));
  rig.holds.front()(HoldOutcome::ClosedByPeer);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);

  QCOMPARE(rig.state(TestStage::Connection), StageState::Failed);
  QCOMPARE(rig.runner->report().failure, TunnelFailure::HandshakeRejected);
  QVERIFY(rig.detail(TestStage::Connection).contains(QStringLiteral("Access policy")));
  // the earlier stages are not blamed
  QCOMPARE(rig.state(TestStage::Process), StageState::Ok);
  QCOMPARE(rig.state(TestStage::LocalPort), StageState::Ok);
}

void TunnelTestRunnerTests::aConnectionThatIsClosedWithoutAReason_isAConnectFailure()
{
  Rig rig(m_dir);
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);
  rig.runner->start();
  rig.bringTheTunnelUp();

  rig.holds.front()(HoldOutcome::ClosedByPeer);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
  QCOMPARE(rig.state(TestStage::Connection), StageState::Failed);
  QCOMPARE(rig.runner->report().failure, TunnelFailure::ConnectFailed);
}

void TunnelTestRunnerTests::aConnectionThatTheLocalPortRefuses_isAFailure()
{
  Rig rig(m_dir);
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);
  rig.runner->start();
  rig.bringTheTunnelUp();

  rig.holds.front()(HoldOutcome::Refused);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
  QCOMPARE(rig.state(TestStage::Connection), StageState::Failed);
  QVERIFY(rig.detail(TestStage::Connection).contains(QStringLiteral("refused")));
  QCOMPARE(rig.runner->report().failure, TunnelFailure::ConnectFailed);
}

void TunnelTestRunnerTests::anOldProblemOfTheConnection_isNotTakenForANewOne()
{
  Rig rig(m_dir);
  QVERIFY(rig.tunnel.bringUp());
  // an earlier connection (the client's own try) was refused, long before the test
  rig.tunnel.process()->write(QStringLiteral("ERR failed to connect to origin error=\"websocket: bad handshake\""));
  QCOMPARE(rig.tunnel.controller->status().failure, TunnelFailure::HandshakeRejected);

  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);
  rig.runner->start();
  rig.holds.front()(HoldOutcome::Held);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);
  QCOMPARE(rig.state(TestStage::Connection), StageState::Ok);
  QVERIFY(rig.runner->report().ok());
}

void TunnelTestRunnerTests::aTestThatIsCancelled_tellsNothingMoreAndLetsTheTunnelGo()
{
  Rig rig(m_dir);
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);
  rig.runner->start();
  rig.bringTheTunnelUp();
  QCOMPARE(rig.state(TestStage::Connection), StageState::Running);

  rig.runner->cancel();
  QVERIFY(!rig.runner->isRunning());
  // the connection that was held answers late: nothing is told and nothing changes
  rig.holds.front()(HoldOutcome::Held);
  QTest::qWait(100);
  QCOMPARE(finished.count(), 0);
  QCOMPARE(rig.state(TestStage::Connection), StageState::Running);
  QTRY_COMPARE_WITH_TIMEOUT(rig.tunnel.controller->status().state, TunnelState::Idle, 3000);
}

void TunnelTestRunnerTests::theSummaryLine_isEnglishAndNamesNoToken()
{
  Rig rig(m_dir);
  QSignalSpy finished(rig.runner.get(), &TunnelTestRunner::finished);
  rig.runner->start();
  rig.bringTheTunnelUp();
  rig.tunnel.process()->write(QStringLiteral("ERR failed to connect to origin error=\"websocket: bad handshake\""));
  rig.holds.front()(HoldOutcome::ClosedByPeer);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 3000);

  const auto line =
      rig.runner->report().summaryLine(QDateTime(QDate(2026, 11, 17), QTime(12, 30)), QStringLiteral("macOS 15"));
  QCOMPARE(
      line, QStringLiteral(
                "Deskflow tunnel test 2026-11-17 12:30 (macOS 15): does not work via mbp.example.com; cloudflared "
                "ok, service token ok, TLS ok, hostname ok, process ok, local port ok, connection FAILED; reason: "
                "handshake-rejected"
            )
  );
  QVERIFY(!line.contains(kClientId));
  QVERIFY(!line.contains(kSecret));
  QVERIFY(!line.contains(QLatin1Char('\n')));
}

void TunnelTestRunnerTests::failureKeys_areTheSameInEveryLanguage()
{
  QStringList keys;
  for (int value = static_cast<int>(TunnelFailure::None); value <= static_cast<int>(TunnelFailure::SettingsChanged);
       ++value) {
    const auto key = failureKey(static_cast<TunnelFailure>(value));
    QVERIFY(!key.isEmpty());
    QVERIFY(key != QStringLiteral("unknown"));
    QVERIFY2(!keys.contains(key), qPrintable(key));
    keys << key;
    for (const auto ch : key) {
      QVERIFY(ch.isLower() || ch == QLatin1Char('-'));
    }
  }
}

void TunnelTestRunnerTests::theSystemHold_tellsHeldClosedAndRefused()
{
  const auto hold = TunnelTestRunner::systemHoldConnection();

  // held: the server accepts and keeps the connection, and gets no byte
  {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QTcpSocket *accepted = nullptr;
    connect(&server, &QTcpServer::newConnection, this, [&] { accepted = server.nextPendingConnection(); });
    std::optional<HoldOutcome> outcome;
    hold(server.serverPort(), 300, [&](HoldOutcome result) { outcome = result; });
    QTRY_VERIFY_WITH_TIMEOUT(outcome.has_value(), 5000);
    QCOMPARE(*outcome, HoldOutcome::Held);
    QVERIFY(accepted != nullptr);
    QCOMPARE(accepted->bytesAvailable(), 0);
  }

  // closed by the other side
  {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    connect(&server, &QTcpServer::newConnection, this, [&] {
      auto *socket = server.nextPendingConnection();
      socket->close();
      socket->deleteLater();
    });
    std::optional<HoldOutcome> outcome;
    hold(server.serverPort(), 3000, [&](HoldOutcome result) { outcome = result; });
    QTRY_VERIFY_WITH_TIMEOUT(outcome.has_value(), 5000);
    QCOMPARE(*outcome, HoldOutcome::ClosedByPeer);
  }

  // refused: nothing listens
  {
    int port = 0;
    {
      QTcpServer probe;
      QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
      port = probe.serverPort();
    }
    std::optional<HoldOutcome> outcome;
    hold(port, 3000, [&](HoldOutcome result) { outcome = result; });
    QTRY_VERIFY_WITH_TIMEOUT(outcome.has_value(), 5000);
    QCOMPARE(*outcome, HoldOutcome::Refused);
  }
}

QTEST_MAIN(TunnelTestRunnerTests)
