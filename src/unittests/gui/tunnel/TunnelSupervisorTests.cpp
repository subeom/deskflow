/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelSupervisorTests.h"

#include "TunnelTestDoubles.h"

#include "gui/tunnel/InMemorySecretStore.h"
#include "gui/tunnel/OrphanRecord.h"
#include "gui/tunnel/TunnelSupervisor.h"

#include <QDateTime>
#include <QFile>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>
#include <map>

using namespace deskflow::tunnel;

namespace {

using namespace tunneltest;

// Everything the supervisor needs, made of fakes, and what it said
struct Rig
{
  QTemporaryDir dir;
  std::shared_ptr<InMemorySecretStore::State> storeState = std::make_shared<InMemorySecretStore::State>();
  std::shared_ptr<FakeProber> prober = std::make_shared<FakeProber>();
  std::shared_ptr<FakeInspector> inspector = std::make_shared<FakeInspector>();
  std::vector<QPointer<FakeProcess>> processes;
  bool cloudflaredFound = true;
  bool relativeProgram = false;
  bool nextProcessFailsToStart = false;
  bool nextProcessIgnoresTerminate = false;
  bool nextProcessIgnoresKill = false;
  qint64 clock = 10'000;
  QString orphanFile = dir.filePath(QStringLiteral("tunnel.json"));
  TunnelRequest request;
  std::unique_ptr<TunnelSupervisor> supervisor;

  QStringList logs;
  QList<TunnelStatus> statuses;
  QList<TunnelFailure> failures;
  QList<int> ports;
  int listeningCount = 0;

  explicit Rig(bool withToken = true, TunnelPolicy policy = {})
  {
    request.hostname = kHost;
    request.preferredPort = kPreferred;
    request.configuredPath = kProgram;
    request.tlsEnabled = true;

    if (withToken) {
      storeState->token = ServiceToken(kClientId, kSecret);
    }

    TunnelSupervisor::Dependencies deps;
    deps.store = std::make_shared<InMemorySecretStore>(storeState);
    deps.locate = [this](const QString &path) {
      CloudflaredLocation location;
      if (cloudflaredFound) {
        location.source = CloudflaredLocation::Source::Configured;
        location.path = relativeProgram ? QStringLiteral("cloudflared") : (path.isEmpty() ? kProgram : path);
      }
      return location;
    };
    deps.processFactory = [this](QObject *parent) -> ITunnelProcess * {
      auto *process = new FakeProcess;
      process->setParent(parent);
      process->failToStart = nextProcessFailsToStart;
      process->obeysTerminate = !nextProcessIgnoresTerminate;
      process->obeysKill = !nextProcessIgnoresKill;
      process->pid = 5000 + static_cast<qint64>(processes.size());
      // the system knows the process (a test that puts its own entry for the pid keeps it)
      inspector->processes.emplace(process->pid, ProcessInfo{QDateTime::currentMSecsSinceEpoch(), kProgram});
      processes.emplace_back(process);
      return process;
    };
    deps.prober = prober;
    deps.inspector = inspector;
    deps.orphanFile = orphanFile;
    deps.clock = [this] { return clock; };
    deps.baseEnvironment = QProcessEnvironment();
    deps.policy = std::move(policy);
    deps.tickIntervalMs = 0;
    supervisor = std::make_unique<TunnelSupervisor>(std::move(deps));

    QObject::connect(supervisor.get(), &TunnelSupervisor::logLine, supervisor.get(), [this](const QString &line) {
      logs << line;
    });
    QObject::connect(
        supervisor.get(), &TunnelSupervisor::statusChanged, supervisor.get(),
        [this](const TunnelStatus &s) { statuses << s; }
    );
    QObject::connect(supervisor.get(), &TunnelSupervisor::failed, supervisor.get(), [this](TunnelFailure f) {
      failures << f;
    });
    QObject::connect(supervisor.get(), &TunnelSupervisor::activePortChanged, supervisor.get(), [this](int port) {
      ports << port;
    });
    QObject::connect(supervisor.get(), &TunnelSupervisor::listening, supervisor.get(), [this] { ++listeningCount; });
  }

  FakeProcess *process(int index = -1) const
  {
    const auto position = index < 0 ? static_cast<int>(processes.size()) + index : index;
    return position >= 0 && position < static_cast<int>(processes.size())
               ? processes.at(static_cast<size_t>(position)).data()
               : nullptr;
  }

  // time passes in steps of 100 ms, as the timer of the supervisor makes it pass
  void advance(qint64 ms)
  {
    for (qint64 passed = 0; passed < ms; passed += 100) {
      clock += 100;
      supervisor->tick();
    }
  }

  // from the start to Listening
  bool bringUp()
  {
    if (!supervisor->start(request)) {
      return false;
    }
    advance(400);
    prober->answer(true);
    advance(400);
    return supervisor->status().state == TunnelState::Listening;
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

} // namespace

void TunnelSupervisorTests::aStartWithoutTls_isRefusedBeforeAnythingIsRead()
{
  Rig rig;
  rig.request.tlsEnabled = false;

  QVERIFY(!rig.supervisor->start(rig.request));
  QCOMPARE(rig.state(), TunnelState::Failed);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::TlsRequired);
  QCOMPARE(rig.failures, QList<TunnelFailure>{TunnelFailure::TlsRequired});
  // the Secret is not read for a start that cannot happen
  QCOMPARE(rig.storeState->loadCount, 0);
  QVERIFY(rig.processes.empty());
}

void TunnelSupervisorTests::aHostnameThatIsNoHostname_isRefused()
{
  Rig rig;
  rig.request.hostname = QStringLiteral("not a host name; rm -rf /");

  QVERIFY(!rig.supervisor->start(rig.request));
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::InvalidHostname);
  QCOMPARE(rig.storeState->loadCount, 0);
  QVERIFY(rig.processes.empty());
}

void TunnelSupervisorTests::aMissingCloudflared_isReported()
{
  Rig rig;
  rig.cloudflaredFound = false;

  QVERIFY(!rig.supervisor->start(rig.request));
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::CloudflaredMissing);
  QCOMPARE(rig.storeState->loadCount, 0);
  QVERIFY(rig.processes.empty());
}

// a locator that gives a name without a directory (a mistake of whoever replaced it): the system would look it up in
// PATH
void TunnelSupervisorTests::aRelativePathOfCloudflared_isRefusedBeforeTheTokenIsRead()
{
  Rig rig;
  rig.relativeProgram = true;

  QVERIFY(!rig.supervisor->start(rig.request));
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::CloudflaredMissing);
  QCOMPARE(rig.storeState->loadCount, 0);
  QVERIFY(rig.processes.empty());
  QVERIFY(rig.logsContain(QStringLiteral("absolute")));
}

void TunnelSupervisorTests::missingOrIncompleteCredentials_areReported()
{
  Rig none(false);
  QVERIFY(!none.supervisor->start(none.request));
  QCOMPARE(none.supervisor->status().failure, TunnelFailure::CredentialsMissing);
  QVERIFY(none.processes.empty());

  Rig half(false);
  half.storeState->token = ServiceToken(kClientId, QString());
  QVERIFY(!half.supervisor->start(half.request));
  QCOMPARE(half.supervisor->status().failure, TunnelFailure::CredentialsMissing);

  // a store that cannot be read (a locked keychain) says why in the log, and the log never holds a secret
  Rig locked;
  locked.storeState->failWith = QStringLiteral("the keychain is locked");
  QVERIFY(!locked.supervisor->start(locked.request));
  QCOMPARE(locked.supervisor->status().failure, TunnelFailure::CredentialsMissing);
  QVERIFY(locked.logsContain(QStringLiteral("the keychain is locked")));
}

void TunnelSupervisorTests::aBusyPortWithNoFreeOne_isReported()
{
  Rig rig;
  rig.prober->preferredFree = false;
  rig.prober->freePort = 0;

  QVERIFY(!rig.supervisor->start(rig.request));
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::PortBusy);
  QVERIFY(rig.processes.empty());
  QCOMPARE(rig.supervisor->activePort(), 0);
}

void TunnelSupervisorTests::aBusyPreferredPort_givesWayToAFreePort_andTellsIt()
{
  Rig rig;
  rig.prober->preferredFree = false;

  QVERIFY(rig.supervisor->start(rig.request));
  QCOMPARE(rig.ports, QList<int>{kFree});
  QCOMPARE(rig.supervisor->activePort(), kFree);
  QCOMPARE(rig.supervisor->status().localPort, kFree);
  QVERIFY(rig.process() != nullptr);
  QVERIFY(rig.process()->spec.arguments.contains(QStringLiteral("127.0.0.1:%1").arg(kFree)));
  QVERIFY(rig.logsContain(QStringLiteral("24800 is busy")));
}

void TunnelSupervisorTests::theTokenIsInTheEnvironmentOfTheProcessOnly()
{
  Rig rig;
  QVERIFY(rig.supervisor->start(rig.request));
  QCOMPARE(rig.storeState->loadCount, 1);

  const auto *process = rig.process();
  QVERIFY(process != nullptr);
  QCOMPARE(process->spec.program, kProgram);
  QCOMPARE(process->spec.environment.value(QStringLiteral("TUNNEL_SERVICE_TOKEN_ID")), kClientId);
  QCOMPARE(process->spec.environment.value(QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET")), kSecret);
  for (const auto &argument : process->spec.arguments) {
    QVERIFY2(!argument.contains(kSecret), "the Secret is in an argument");
    QVERIFY2(!argument.contains(kClientId), "the Client ID is in an argument");
  }
  QVERIFY(process->spec.arguments.contains(QStringLiteral("--hostname")));
  QVERIFY(process->spec.arguments.contains(kHost));
  QVERIFY(!rig.logsContain(kSecret));
  QVERIFY(!rig.logsContain(kClientId));
}

void TunnelSupervisorTests::theTunnelListens_whenThePortIsOpenAndTheProcessStays()
{
  Rig rig;
  QVERIFY(rig.supervisor->start(rig.request));
  QCOMPARE(rig.state(), TunnelState::Starting);

  rig.advance(400);
  QCOMPARE(rig.prober->probes.size(), size_t(1));
  QCOMPARE(rig.prober->probes.front().first, kPreferred);
  QCOMPARE(rig.listeningCount, 0);

  rig.prober->answer(true);
  rig.advance(400);
  QCOMPARE(rig.state(), TunnelState::Listening);
  QCOMPARE(rig.listeningCount, 1);
  QCOMPARE(rig.supervisor->status().localPort, kPreferred);
  QCOMPARE(rig.supervisor->status().hostname, kHost);
  QVERIFY(rig.supervisor->isProcessRunning());

  // a state that is the same is not told again
  rig.advance(2000);
  QCOMPARE(rig.listeningCount, 1);
}

void TunnelSupervisorTests::theLoggedListener_startsTheProbeAtOnce()
{
  Rig rig;
  QVERIFY(rig.supervisor->start(rig.request));
  QCOMPARE(rig.prober->probesAsked, 0);

  rig.process()->write(QStringLiteral("2026-01-01T00:00:00Z INF Start Websocket listener host=127.0.0.1:24800"));
  QCOMPARE(rig.prober->probesAsked, 1);
}

void TunnelSupervisorTests::aPortThatNeverOpens_timesOutAndEndsTheProcess()
{
  Rig rig;
  QVERIFY(rig.supervisor->start(rig.request));

  for (int step = 0; step < 90 && rig.state() == TunnelState::Starting; ++step) {
    rig.advance(100);
    rig.prober->answer(false);
  }
  QCOMPARE(rig.state(), TunnelState::Failed);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::StartTimedOut);
  QCOMPARE(rig.process()->terminateCalls, 1);
  QVERIFY(!rig.supervisor->isProcessRunning());
  QCOMPARE(rig.supervisor->activePort(), 0);
}

void TunnelSupervisorTests::aSecondStart_whileRunning_startsNothing()
{
  Rig rig;
  QVERIFY(rig.bringUp());
  QVERIFY(!rig.supervisor->start(rig.request));
  QCOMPARE(rig.processes.size(), size_t(1));
  QCOMPARE(rig.state(), TunnelState::Listening);
}

void TunnelSupervisorTests::everyLineThatIsLogged_isFreeOfTheToken()
{
  Rig rig;
  QVERIFY(rig.supervisor->start(rig.request));

  auto *process = rig.process();
  process->write(QStringLiteral("ERR failed to refresh token: Cf-Access-Client-Secret: %1").arg(kSecret));
  process->write(QStringLiteral("ERR the client %1 was refused with the secret %2").arg(kClientId, kSecret));
  process->write(QStringLiteral("{\"level\":\"error\",\"cf-access-client-secret\":\"%1\"}").arg(kSecret));

  for (const auto &line : rig.logs) {
    QVERIFY2(!line.contains(kSecret), qPrintable(line));
    QVERIFY2(!line.contains(kClientId), qPrintable(line));
  }
  QVERIFY(rig.logsContain(QString::fromLatin1(SecretRedactor::kMask)));
  for (const auto &status : rig.statuses) {
    QVERIFY(!status.text().contains(kSecret));
  }
}

void TunnelSupervisorTests::aLoginThatIsAskedFor_endsTheProcessAndIsNotRestarted()
{
  Rig rig;
  QVERIFY(rig.supervisor->start(rig.request));

  rig.process()->write(QStringLiteral("INF A browser window should have opened at the following URL:"));
  QCOMPARE(rig.state(), TunnelState::Failed);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::LoginRequired);
  QCOMPARE(rig.failures, QList<TunnelFailure>{TunnelFailure::LoginRequired});
  QCOMPARE(rig.process()->terminateCalls, 1);
  QVERIFY(!rig.supervisor->isProcessRunning());

  // a login is not asked for again and again: nothing starts without the user
  rig.advance(120000);
  QCOMPARE(rig.processes.size(), size_t(1));
  QCOMPARE(rig.state(), TunnelState::Failed);
  QCOMPARE(rig.supervisor->activePort(), 0);
}

void TunnelSupervisorTests::aProcessThatStaysAfterBeingAsked_isEndedHard()
{
  Rig rig;
  rig.nextProcessIgnoresTerminate = true;
  QVERIFY(rig.supervisor->start(rig.request));
  rig.process()->write(QStringLiteral("INF Waiting for login..."));
  QCOMPARE(rig.state(), TunnelState::Failed);
  QCOMPARE(rig.process()->terminateCalls, 1);
  QCOMPARE(rig.process()->killCalls, 0);

  rig.advance(2000);
  QCOMPARE(rig.process()->killCalls, 0);
  rig.advance(1500);
  QCOMPARE(rig.process()->killCalls, 1);
  QVERIFY(!rig.supervisor->isProcessRunning());

  // and in a stop: asked first, ended when it stays
  Rig stopping;
  stopping.nextProcessIgnoresTerminate = true;
  QVERIFY(stopping.bringUp());
  stopping.supervisor->stop();
  QCOMPARE(stopping.process()->terminateCalls, 1);
  QCOMPARE(stopping.process()->killCalls, 0);
  QCOMPARE(stopping.state(), TunnelState::Stopping);
  stopping.advance(3500);
  QCOMPARE(stopping.process()->killCalls, 1);
  QCOMPARE(stopping.state(), TunnelState::Idle);
}

void TunnelSupervisorTests::aProcessThatStopsOnItsOwn_isStartedAgainWithTheSameTokenAndPort()
{
  Rig rig;
  QVERIFY(rig.bringUp());

  rig.process()->end(2);
  QCOMPARE(rig.state(), TunnelState::Backoff);
  QCOMPARE(rig.supervisor->status().exitCode, 2);
  QCOMPARE(rig.processes.size(), size_t(1));
  QVERIFY(rig.logsContain(QStringLiteral("exit code 2")));

  rig.advance(1200);
  QCOMPARE(rig.processes.size(), size_t(2));
  QCOMPARE(rig.state(), TunnelState::Starting);
  const auto *second = rig.process();
  QVERIFY(second != rig.processes.front().data());
  QCOMPARE(second->spec.environment.value(QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET")), kSecret);
  QVERIFY(second->spec.arguments.contains(QStringLiteral("127.0.0.1:%1").arg(kPreferred)));
  // the Secret is read once for the tunnel, not for every restart
  QCOMPARE(rig.storeState->loadCount, 1);
}

void TunnelSupervisorTests::aProcessThatKeepsStoppingAtOnce_failsAndForgetsTheToken()
{
  Rig rig;
  QVERIFY(rig.supervisor->start(rig.request));

  // every process is ended at the step after it started: the pauses are 1, 2, 4 and 8 seconds, and the fifth stop in a
  // row is the last
  for (int step = 0; step < 400 && rig.state() != TunnelState::Failed; ++step) {
    rig.advance(100);
    if (auto *process = rig.process(); process != nullptr && process->isRunning()) {
      process->write(QStringLiteral("ERR the connection failed"));
      process->end(1);
    }
  }
  QCOMPARE(rig.state(), TunnelState::Failed);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::ProcessExited);

  // the token of that run is gone from the memory of the supervisor: a new start reads the store again
  rig.storeState->token.reset();
  QVERIFY(!rig.supervisor->start(rig.request));
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::CredentialsMissing);
}

void TunnelSupervisorTests::aProcessThatCannotBeStarted_isReported()
{
  Rig rig;
  rig.nextProcessFailsToStart = true;

  rig.supervisor->start(rig.request);
  QCOMPARE(rig.state(), TunnelState::Failed);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::CloudflaredMissing);
  QVERIFY(rig.logsContain(QStringLiteral("could not be started")));
  QCOMPARE(rig.supervisor->activePort(), 0);
  // no record is left for a process that never ran
  QVERIFY(!QFile::exists(rig.orphanFile));
}

void TunnelSupervisorTests::aProblemOfTheConnection_isShownUntilTheServerConnects()
{
  Rig rig;
  QVERIFY(rig.bringUp());

  rig.process()->write(QStringLiteral("ERR failed to connect to origin error=\"websocket: bad handshake\""));
  QCOMPARE(rig.state(), TunnelState::Listening);
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::HandshakeRejected);

  rig.supervisor->notifyServerConnected();
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::None);
  QCOMPARE(rig.state(), TunnelState::Listening);
}

void TunnelSupervisorTests::everyRefusedConnection_isCountedWithItsReason()
{
  Rig rig;
  QVERIFY(rig.bringUp());
  QCOMPARE(rig.supervisor->originFailureCount(), quint64(0));
  QCOMPARE(rig.supervisor->lastOriginFailure(), TunnelFailure::None);

  rig.process()->write(QStringLiteral("ERR failed to connect to origin error=\"websocket: bad handshake\""));
  QCOMPARE(rig.supervisor->originFailureCount(), quint64(1));
  QCOMPARE(rig.supervisor->lastOriginFailure(), TunnelFailure::HandshakeRejected);

  // the same reason again is another connection that failed, and a line that is no failure is not counted
  rig.process()->write(QStringLiteral("ERR failed to connect to origin error=\"websocket: bad handshake\""));
  rig.process()->write(QStringLiteral("INF a line that says nothing"));
  QCOMPARE(rig.supervisor->originFailureCount(), quint64(2));

  // the status forgets the problem when the server connects, the count does not go back
  rig.supervisor->notifyServerConnected();
  QCOMPARE(rig.supervisor->status().failure, TunnelFailure::None);
  QCOMPARE(rig.supervisor->originFailureCount(), quint64(2));
}

void TunnelSupervisorTests::stop_endsTheProcessAndGoesIdle()
{
  Rig rig;
  QVERIFY(rig.bringUp());
  QVERIFY(QFile::exists(rig.orphanFile));

  rig.supervisor->stop();
  QCOMPARE(rig.process()->terminateCalls, 1);
  QCOMPARE(rig.state(), TunnelState::Idle);
  QVERIFY(!rig.supervisor->isProcessRunning());
  QVERIFY(!QFile::exists(rig.orphanFile));
  QCOMPARE(rig.supervisor->activePort(), 0);
  QCOMPARE(rig.ports.last(), 0);
  QVERIFY(rig.failures.isEmpty());
  // a stop that was asked for is no event for the log of a process that stopped on its own
  QVERIFY(!rig.logsContain(QStringLiteral("cloudflared stopped")));
}

void TunnelSupervisorTests::stop_inBackoff_needsNoKill()
{
  Rig rig;
  QVERIFY(rig.bringUp());
  rig.process()->end(2);
  QCOMPARE(rig.state(), TunnelState::Backoff);

  rig.supervisor->stop();
  QCOMPARE(rig.state(), TunnelState::Idle);
  rig.advance(5000);
  QCOMPARE(rig.processes.size(), size_t(1));
  QCOMPARE(rig.process()->terminateCalls, 0);
}

void TunnelSupervisorTests::aStartWhileStopping_isDoneWhenTheProcessIsGone()
{
  Rig rig;
  rig.nextProcessIgnoresTerminate = true;
  QVERIFY(rig.bringUp());
  rig.supervisor->stop();
  QCOMPARE(rig.state(), TunnelState::Stopping);

  auto changed = rig.request;
  changed.hostname = QStringLiteral("other.example.com");
  QVERIFY(!rig.supervisor->start(changed));
  QCOMPARE(rig.processes.size(), size_t(1));

  rig.nextProcessIgnoresTerminate = false;
  rig.process()->end(0);
  QCOMPARE(rig.processes.size(), size_t(2));
  QCOMPARE(rig.state(), TunnelState::Starting);
  QVERIFY(rig.process()->spec.arguments.contains(QStringLiteral("other.example.com")));
}

void TunnelSupervisorTests::stop_forgetsAStartThatWaits()
{
  Rig rig;
  rig.nextProcessIgnoresTerminate = true;
  QVERIFY(rig.bringUp());
  rig.supervisor->stop();
  rig.supervisor->start(rig.request);
  rig.supervisor->stop();

  rig.process()->end(0);
  QCOMPARE(rig.processes.size(), size_t(1));
  QCOMPARE(rig.state(), TunnelState::Idle);
}

void TunnelSupervisorTests::stopAndWait_waitsForTheProcess()
{
  Rig rig;
  QVERIFY(rig.bringUp());

  QVERIFY(rig.supervisor->stopAndWait(500));
  QCOMPARE(rig.state(), TunnelState::Idle);
  QVERIFY(!rig.supervisor->isProcessRunning());
  QVERIFY(!QFile::exists(rig.orphanFile));
}

void TunnelSupervisorTests::stopAndWait_dropsAProcessThatNeverReports()
{
  Rig rig;
  rig.nextProcessIgnoresTerminate = true;
  rig.nextProcessIgnoresKill = true;
  QVERIFY(rig.bringUp());

  QVERIFY(rig.supervisor->stopAndWait(150));
  QCOMPARE(rig.state(), TunnelState::Idle);
  QVERIFY(!rig.supervisor->isProcessRunning());
  QVERIFY(!QFile::exists(rig.orphanFile));
}

void TunnelSupervisorTests::theSupervisorGoing_endsTheProcessAndRemovesTheRecord()
{
  QString file;
  QPointer<FakeProcess> process;
  {
    Rig rig;
    file = rig.orphanFile;
    QVERIFY(rig.bringUp());
    process = rig.process();
    QVERIFY(QFile::exists(file));
    rig.supervisor.reset();
    QVERIFY(!QFile::exists(file));
    QVERIFY(process.isNull());
  }
}

void TunnelSupervisorTests::aSleep_doesNotTimeOutAStart()
{
  Rig rig;
  QVERIFY(rig.supervisor->start(rig.request));
  rig.advance(300);

  // the computer sleeps for an hour: the clock jumps, the timeout of the start (8 s) is not used up
  rig.clock += 3'600'000;
  rig.supervisor->tick();
  QCOMPARE(rig.state(), TunnelState::Starting);
  rig.prober->answer(false);

  // the start goes on as before: the port opens and the tunnel listens
  rig.advance(300);
  rig.prober->answer(true);
  rig.advance(400);
  QCOMPARE(rig.state(), TunnelState::Listening);
}

void TunnelSupervisorTests::aSleep_restartsAProcessWhoseListenerIsGone()
{
  Rig rig;
  QVERIFY(rig.bringUp());
  rig.prober->probes.clear();

  rig.clock += 600'000;
  rig.supervisor->tick();
  QCOMPARE(rig.prober->probes.size(), size_t(1));
  QCOMPARE(rig.state(), TunnelState::Listening);

  rig.prober->answer(false);
  QVERIFY(rig.logsContain(QStringLiteral("woke up")));
  QCOMPARE(rig.process()->killCalls, 1);
  QCOMPARE(rig.state(), TunnelState::Backoff);

  rig.advance(1200);
  QCOMPARE(rig.processes.size(), size_t(2));
}

void TunnelSupervisorTests::aSleep_leavesAProcessWhoseListenerIsThere()
{
  Rig rig;
  QVERIFY(rig.bringUp());
  rig.prober->probes.clear();

  rig.clock += 600'000;
  rig.supervisor->tick();
  rig.prober->answer(true);
  QCOMPARE(rig.process()->killCalls, 0);
  QCOMPARE(rig.state(), TunnelState::Listening);
  QCOMPARE(rig.processes.size(), size_t(1));
}

void TunnelSupervisorTests::theRunningProcess_isRecordedAndTheRecordRemovedAtItsEnd()
{
  Rig rig;
  const auto startTime = QDateTime::currentMSecsSinceEpoch();
  rig.inspector->processes[5000] = {startTime, kProgram};

  QVERIFY(rig.supervisor->start(rig.request));
  const auto record = readOrphanRecord(rig.orphanFile);
  QVERIFY(record.has_value());
  QCOMPARE(record->pid, qint64(5000));
  QCOMPARE(record->startTimeMs, startTime);
  QCOMPARE(record->path, kProgram);
  QCOMPARE(record->hostname, kHost);
  QCOMPARE(record->port, kPreferred);

  // the record holds no token
  QFile file(rig.orphanFile);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const auto content = QString::fromUtf8(file.readAll());
  QVERIFY(!content.contains(kSecret));
  QVERIFY(!content.contains(kClientId));
  file.close();

  rig.process()->end(1);
  QVERIFY(!QFile::exists(rig.orphanFile));
}

void TunnelSupervisorTests::aLeftoverProcessOfAnEarlierRun_isEndedBeforeTheStart()
{
  Rig rig;
  const auto startTime = QDateTime::currentMSecsSinceEpoch() - 60000;
  const QString leftover = QStringLiteral("/opt/test/cloudflared");
  rig.inspector->processes[777] = {startTime, leftover};
  OrphanRecord record;
  record.pid = 777;
  record.startTimeMs = startTime;
  record.path = leftover;
  QVERIFY(writeOrphanRecord(rig.orphanFile, record));

  QVERIFY(rig.supervisor->start(rig.request));
  QCOMPARE(rig.inspector->asked.size(), size_t(1));
  QCOMPARE(rig.inspector->asked.front().first, qint64(777));
  QVERIFY(rig.logsContain(QStringLiteral("left behind")));
  QCOMPARE(rig.processes.size(), size_t(1));
}

void TunnelSupervisorTests::aLeftoverRecordOfAnotherProcess_isLeftAlone()
{
  Rig rig;
  const auto startTime = QDateTime::currentMSecsSinceEpoch() - 60000;
  rig.inspector->processes[777] = {startTime + 3'600'000, QStringLiteral("/opt/test/cloudflared")};
  OrphanRecord record;
  record.pid = 777;
  record.startTimeMs = startTime;
  record.path = QStringLiteral("/opt/test/cloudflared");
  QVERIFY(writeOrphanRecord(rig.orphanFile, record));

  QVERIFY(rig.supervisor->start(rig.request));
  QVERIFY(rig.inspector->asked.empty());
  QVERIFY(rig.inspector->processes.contains(777));
  QVERIFY(rig.logsContain(QStringLiteral("left alone")));
}

QTEST_MAIN(TunnelSupervisorTests)
