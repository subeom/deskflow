/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelSupervisorProcessTests.h"

#include "gui/tunnel/InMemorySecretStore.h"
#include "gui/tunnel/OrphanRecord.h"
#include "gui/tunnel/QtPortProber.h"
#include "gui/tunnel/QtTunnelProcess.h"
#include "gui/tunnel/TunnelSupervisor.h"

#include <QHostAddress>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <algorithm>

using namespace deskflow::tunnel;

namespace {

// made up: the stand-in tells whether it got them, and writes them only in its leak mode
const QString kSecret = QStringLiteral("dummy-secret-value-0123456789");
const QString kClientId = QStringLiteral("dummy-client-id.access");

struct Rig
{
  QTemporaryDir dir;
  std::shared_ptr<InMemorySecretStore::State> storeState = std::make_shared<InMemorySecretStore::State>();
  std::shared_ptr<IProcessInspector> inspector = createSystemProcessInspector();
  QtPortProber prober;
  TunnelRequest request;
  std::unique_ptr<TunnelSupervisor> supervisor;
  QStringList logs;
  QList<TunnelStatus> statuses;
  int processesStarted = 0;
  QString orphanFile = dir.filePath(QStringLiteral("state/tunnel.json"));

  explicit Rig(const QString &mode, TunnelPolicy policy = {})
  {
    storeState->token = ServiceToken(kClientId, kSecret);
    request.hostname = QStringLiteral("%1.example.com").arg(mode);
    request.preferredPort = prober.findFreePort();
    request.configuredPath = QStringLiteral(FAKE_CLOUDFLARED_PATH);

    auto deps = TunnelSupervisor::systemDependencies();
    deps.store = std::make_shared<InMemorySecretStore>(storeState);
    deps.locate = [](const QString &path) {
      CloudflaredLocation location;
      location.source = CloudflaredLocation::Source::Configured;
      location.path = path;
      return location;
    };
    const auto real = QtTunnelProcess::factory();
    deps.processFactory = [this, real](QObject *parent) {
      ++processesStarted;
      return real(parent);
    };
    deps.inspector = inspector;
    deps.orphanFile = orphanFile;
    deps.policy = std::move(policy);
    deps.tickIntervalMs = 30;
    supervisor = std::make_unique<TunnelSupervisor>(std::move(deps));

    QObject::connect(supervisor.get(), &TunnelSupervisor::logLine, supervisor.get(), [this](const QString &line) {
      logs << line;
    });
    QObject::connect(
        supervisor.get(), &TunnelSupervisor::statusChanged, supervisor.get(),
        [this](const TunnelStatus &s) { statuses << s; }
    );
  }

  ~Rig()
  {
    supervisor->stopAndWait(3000);
  }

  bool logsContain(const QString &part) const
  {
    return std::ranges::any_of(logs, [&](const QString &line) { return line.contains(part); });
  }

  TunnelState state() const
  {
    return supervisor->status().state;
  }
};

// a short policy: the tests wait for real time
TunnelPolicy quick()
{
  TunnelPolicy policy;
  policy.readyTimeoutMs = 2500;
  policy.probeStartMs = 100;
  policy.probeIntervalMs = 100;
  policy.confirmMs = 150;
  policy.stopTimeoutMs = 2000;
  policy.backoffSeconds = {0};
  return policy;
}

// Whether a connection to the port is made. It waits for the signals of the socket: waitForConnected() of Qt tells true
// for a connection that was refused, on some systems.
bool connectTo(int port)
{
  QTcpSocket socket;
  QSignalSpy connected(&socket, &QAbstractSocket::connected);
  QSignalSpy failed(&socket, &QAbstractSocket::errorOccurred);
  socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
  QTest::qWaitFor([&] { return connected.count() > 0 || failed.count() > 0; }, 2000);
  return connected.count() > 0 && failed.count() == 0 && socket.peerPort() == port;
}

} // namespace

void TunnelSupervisorProcessTests::aTunnelThatWorks_listensCarriesAConnectionAndStops()
{
  Rig rig(QStringLiteral("ok"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Listening, 8000);

  QCOMPARE(rig.supervisor->status().localPort, rig.request.preferredPort);
  QVERIFY(rig.supervisor->isProcessRunning());
  QVERIFY(connectTo(rig.request.preferredPort));

  const auto record = readOrphanRecord(rig.orphanFile);
  QVERIFY(record.has_value());
  QVERIFY(rig.inspector->inspect(record->pid).has_value());
  const auto pid = record->pid;

  rig.supervisor->stop();
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Idle, 8000);
  QTRY_VERIFY_WITH_TIMEOUT(!rig.inspector->inspect(pid).has_value(), 5000);
  QVERIFY(!QFile::exists(rig.orphanFile));
  // the system takes a moment to close the sockets of a process that ended
  QTRY_VERIFY_WITH_TIMEOUT(!connectTo(rig.request.preferredPort), 5000);
  QCOMPARE(rig.supervisor->activePort(), 0);
}

void TunnelSupervisorProcessTests::theTokenReachesTheProcessThroughTheEnvironmentOnly()
{
  Rig rig(QStringLiteral("ok"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Listening, 8000);

  QVERIFY2(
      rig.logsContain(QStringLiteral("tokenId=1 tokenSecret=1 tokenInArguments=0")),
      qPrintable(rig.logs.join(QLatin1Char('\n')))
  );
  // the command line in the log names the program and the arguments, and no token
  QVERIFY(rig.logsContain(QStringLiteral("access tcp --hostname ok.example.com")));
  QVERIFY(!rig.logsContain(kSecret));
  QVERIFY(!rig.logsContain(kClientId));
}

void TunnelSupervisorProcessTests::aProcessThatWritesTheToken_isNeverLoggedWithIt()
{
  Rig rig(QStringLiteral("leak"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Listening, 8000);
  QTRY_VERIFY_WITH_TIMEOUT(rig.logsContain(QStringLiteral("the id is")), 5000);

  QVERIFY(rig.logsContain(QString::fromLatin1(SecretRedactor::kMask)));
  for (const auto &line : rig.logs) {
    QVERIFY2(!line.contains(kSecret), qPrintable(line));
    QVERIFY2(!line.contains(kClientId), qPrintable(line));
  }
  for (const auto &status : rig.statuses) {
    QVERIFY(!status.text().contains(kSecret));
  }
}

void TunnelSupervisorProcessTests::aLoginThatIsAskedFor_failsAndLeavesNoProcess()
{
  Rig rig(QStringLiteral("login"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Failed, 8000);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::LoginRequired);

  QTRY_VERIFY_WITH_TIMEOUT(!rig.supervisor->isProcessRunning(), 8000);
  QTRY_VERIFY_WITH_TIMEOUT(!QFile::exists(rig.orphanFile), 5000);
  // it is not tried again by itself
  QTest::qWait(500);
  QCOMPARE(rig.processesStarted, 1);
  QCOMPARE(rig.state(), TunnelState::Failed);
}

void TunnelSupervisorProcessTests::aProcessThatStopsAtOnce_failsAfterItsRestarts()
{
  Rig rig(QStringLiteral("exit"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Failed, 15000);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::ProcessExited);
  QCOMPARE(rig.supervisor->status().exitCode, 3);
  QCOMPARE(rig.processesStarted, 5);
  QVERIFY(rig.logsContain(QStringLiteral("the connection to the edge failed")));
}

void TunnelSupervisorProcessTests::aProcessThatNeverListens_failsWithATimeOut()
{
  Rig rig(QStringLiteral("silent"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Failed, 10000);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::StartTimedOut);
  QTRY_VERIFY_WITH_TIMEOUT(!rig.supervisor->isProcessRunning(), 8000);
}

void TunnelSupervisorProcessTests::aListenerThatIsNotOpened_failsWithATimeOut()
{
  // it says that it listens, and the port stays closed: the log is not taken for the port
  Rig rig(QStringLiteral("noport"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Failed, 10000);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::StartTimedOut);
  QVERIFY(rig.logsContain(QStringLiteral("Start Websocket listener")));
}

void TunnelSupervisorProcessTests::aPortThatIsBusy_givesWayToAFreeOne()
{
  Rig rig(QStringLiteral("ok"), quick());

  // something else listens on the port that the tunnel would like
  QTcpServer other;
  QVERIFY(other.listen(QHostAddress::LocalHost, 0));
  rig.request.preferredPort = other.serverPort();

  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Listening, 8000);
  QVERIFY(rig.supervisor->activePort() != other.serverPort());
  QVERIFY(rig.supervisor->activePort() > 0);
  QCOMPARE(rig.supervisor->status().localPort, rig.supervisor->activePort());
  QVERIFY(connectTo(rig.supervisor->activePort()));
}

void TunnelSupervisorProcessTests::anEdgeThatRefuses_isShownWhileTheTunnelListens()
{
  Rig rig(QStringLiteral("deny"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Listening, 8000);

  // the probe of the supervisor is a connection, and the stand-in refuses it like an edge that refuses: the tunnel
  // listens and says what is wrong with the connections through it
  QTRY_COMPARE_WITH_TIMEOUT(rig.supervisor->status().failure, TunnelFailure::HandshakeRejected, 5000);
  QCOMPARE(rig.state(), TunnelState::Listening);

  rig.supervisor->notifyServerConnected();
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::None);

  // and the next connection that is refused is shown again
  QTest::qWait(300);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::None);
  QVERIFY(connectTo(rig.request.preferredPort));
  QTRY_COMPARE_WITH_TIMEOUT(rig.supervisor->status().failure, TunnelFailure::HandshakeRejected, 5000);
}

void TunnelSupervisorProcessTests::aProcessThatDiesWhileListening_isStartedAgain()
{
  Rig rig(QStringLiteral("flaky"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Listening, 8000);

  // it stops with the code 2 shortly after it listens, and is started again, on the same port
  QTRY_VERIFY_WITH_TIMEOUT(rig.processesStarted >= 2, 8000);
  QTRY_COMPARE_WITH_TIMEOUT(rig.supervisor->status().localPort, rig.request.preferredPort, 2000);
  QVERIFY(rig.logsContain(QStringLiteral("exit code 2")));
}

void TunnelSupervisorProcessTests::aLeftoverProcessOfACrashedRun_isEndedAtTheNextStart()
{
  Rig rig(QStringLiteral("ok"), quick());

  // the cloudflared of a run of Deskflow that crashed: it runs, and the record of it is in the state directory
  QProcess leftover;
  leftover.setProgram(QStringLiteral(FAKE_CLOUDFLARED_PATH));
  leftover.setArguments(
      {QStringLiteral("access"), QStringLiteral("tcp"), QStringLiteral("--hostname"),
       QStringLiteral("silent.example.com"), QStringLiteral("--url"),
       QStringLiteral("127.0.0.1:%1").arg(rig.prober.findFreePort())}
  );
  leftover.start();
  QVERIFY(leftover.waitForStarted(5000));
  const auto pid = static_cast<qint64>(leftover.processId());
  QTRY_VERIFY_WITH_TIMEOUT(rig.inspector->inspect(pid).has_value(), 5000);
  const auto info = rig.inspector->inspect(pid);
  QVERIFY(info.has_value());

  OrphanRecord record;
  record.pid = pid;
  record.startTimeMs = info->startTimeMs;
  record.path = info->path;
  QVERIFY(writeOrphanRecord(rig.orphanFile, record));

  QVERIFY(rig.supervisor->start(rig.request));
  QVERIFY(rig.logsContain(QStringLiteral("left behind")));
  QVERIFY(leftover.waitForFinished(5000));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Listening, 8000);
}

void TunnelSupervisorProcessTests::stopAndWait_leavesNoProcessBehind()
{
  Rig rig(QStringLiteral("ok"), quick());
  QVERIFY(rig.supervisor->start(rig.request));
  QTRY_COMPARE_WITH_TIMEOUT(rig.state(), TunnelState::Listening, 8000);
  const auto record = readOrphanRecord(rig.orphanFile);
  QVERIFY(record.has_value());

  QVERIFY(rig.supervisor->stopAndWait(5000));
  QCOMPARE(rig.state(), TunnelState::Idle);
  QVERIFY(!rig.inspector->inspect(record->pid).has_value());
  QVERIFY(!QFile::exists(rig.orphanFile));
}

QTEST_MAIN(TunnelSupervisorProcessTests)
