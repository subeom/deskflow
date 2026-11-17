/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelStateMachineTests.h"

#include "gui/tunnel/TunnelStateMachine.h"

using namespace deskflow::tunnel;
using Command = TunnelStateMachine::Command;

Q_DECLARE_METATYPE(deskflow::tunnel::LogEventKind)

namespace {

LogEvent logLine(LogEventKind kind, TunnelFailure problem = TunnelFailure::None)
{
  LogEvent e;
  e.kind = kind;
  e.problem = problem;
  return e;
}

TunnelStateMachine machine()
{
  TunnelStateMachine m;
  m.setTarget(QStringLiteral("mbp.example.com"), 24800);
  return m;
}

// takes the machine to Listening; \p now is the time it got there
void bringToListening(TunnelStateMachine &m, qint64 &now, qint64 startAt = 0)
{
  QCOMPARE(m.start(startAt), Command::Launch);
  QCOMPARE(m.logEvent(logLine(LogEventKind::ListenerStarted), startAt + 50), Command::ProbePort);
  m.portProbeFinished(true, startAt + 60);
  QCOMPARE(m.tick(startAt + 400), Command::None);
  QCOMPARE(m.status().state, TunnelState::Listening);
  now = startAt + 400;
}

// after a start at \p now: the line of cloudflared, the open port and the time to watch: Listening
void listen(TunnelStateMachine &m, qint64 now)
{
  m.logEvent(logLine(LogEventKind::ListenerStarted), now + 5);
  m.portProbeFinished(true, now + 10);
  m.tick(now + 400);
}

} // namespace

void TunnelStateMachineTests::aNewMachine_isIdleAndKnowsItsTarget()
{
  const auto m = machine();
  QCOMPARE(m.status().state, TunnelState::Idle);
  QCOMPARE(m.status().failure, TunnelFailure::None);
  QCOMPARE(m.status().hostname, QStringLiteral("mbp.example.com"));
  QCOMPARE(m.status().localPort, 24800);
  QVERIFY(!m.processRunning());
  QVERIFY(!m.status().isRunning());
}

void TunnelStateMachineTests::start_launchesAndWaitsForTheLocalPort()
{
  auto m = machine();
  QCOMPARE(m.start(1000), Command::Launch);
  QCOMPARE(m.status().state, TunnelState::Starting);
  QVERIFY(m.processRunning());
  QVERIFY(m.status().isRunning());
  // nothing to do before the process had time
  QCOMPARE(m.tick(1100), Command::None);
  QCOMPARE(m.status().state, TunnelState::Starting);
}

void TunnelStateMachineTests::listening_needsAnOpenPortThatStaysOpenForTheProcessToo()
{
  auto m = machine();
  QCOMPARE(m.start(0), Command::Launch);
  QCOMPARE(m.logEvent(logLine(LogEventKind::ListenerStarted), 50), Command::ProbePort);
  QCOMPARE(m.status().state, TunnelState::Starting);

  // the port is open: not Listening yet, the process has to be seen running for a moment longer
  m.portProbeFinished(true, 100);
  QCOMPARE(m.tick(200), Command::None);
  QCOMPARE(m.status().state, TunnelState::Starting);
  QCOMPARE(m.tick(399), Command::None);
  QCOMPARE(m.status().state, TunnelState::Starting);
  QCOMPARE(m.tick(400), Command::None);
  QCOMPARE(m.status().state, TunnelState::Listening);
  QCOMPARE(m.status().failure, TunnelFailure::None);
  QCOMPARE(m.status().text(), QStringLiteral("Tunnel: listening (local port open)"));
}

void TunnelStateMachineTests::probing_startsByTimeWhenTheListenerLineIsNeverSeen()
{
  auto m = machine();
  QCOMPARE(m.start(0), Command::Launch);
  QCOMPARE(m.tick(299), Command::None);
  QCOMPARE(m.tick(300), Command::ProbePort);
  // not again at once
  QCOMPARE(m.tick(301), Command::None);
  m.portProbeFinished(true, 310);
  // an open port and a process that runs are not enough without the line (see the tests of the line below)
  QCOMPARE(m.tick(610), Command::None);
  QCOMPARE(m.status().state, TunnelState::Starting);
}

void TunnelStateMachineTests::listening_needsTheListenerLineOfCloudflaredToo()
{
  auto m = machine();
  QCOMPARE(m.start(0), Command::Launch);
  // a program of this computer holds the port: it answers the probe, and cloudflared has not said anything
  m.portProbeFinished(true, 310);
  QCOMPARE(m.tick(700), Command::None);
  QCOMPARE(m.tick(2000), Command::None);
  QCOMPARE(m.status().state, TunnelState::Starting);
  QVERIFY(!m.listenerLineSeen());

  // the line comes: the watch of the process starts from it, and the state is Listening when it was quiet long enough
  m.logEvent(logLine(LogEventKind::ListenerStarted), 2100);
  QCOMPARE(m.tick(2399), Command::None);
  QCOMPARE(m.status().state, TunnelState::Starting);
  QCOMPARE(m.tick(2400), Command::None);
  QCOMPARE(m.status().state, TunnelState::Listening);
  QVERIFY(m.listenerLineSeen());
}

// The wording of cloudflared can change; a tunnel that works must not stop working for that. The line is necessary, and
// when it does not come the port has to stay open for the process for longer, which a program that took the port while
// cloudflared fails to listen cannot do: cloudflared stops, or says that the address is in use.
void TunnelStateMachineTests::listening_withoutTheLine_isAcceptedOnlyAfterALongerWatch()
{
  auto m = machine();
  QCOMPARE(m.start(0), Command::Launch);
  m.portProbeFinished(true, 310);
  QCOMPARE(m.tick(3309), Command::None);
  QCOMPARE(m.status().state, TunnelState::Starting);
  QCOMPARE(m.tick(3310), Command::None);
  QCOMPARE(m.status().state, TunnelState::Listening);
  QVERIFY(!m.listenerLineSeen());

  // and a process that stops during the watch is not Listening
  auto stops = machine();
  QCOMPARE(stops.start(0), Command::Launch);
  stops.portProbeFinished(true, 310);
  QCOMPARE(stops.tick(2000), Command::None);
  stops.processExited(1, 2100);
  QCOMPARE(stops.status().state, TunnelState::Backoff);
}

void TunnelStateMachineTests::aBindFailureAfterTheTunnelListens_isAPortConflictToo()
{
  auto m = machine();
  qint64 now = 0;
  bringToListening(m, now);

  // the program that held the port answered the probe, and cloudflared says that it could not listen
  QCOMPARE(m.logEvent(logLine(LogEventKind::BindFailed, TunnelFailure::PortBusy), now + 10), Command::Kill);
  QCOMPARE(m.status().state, TunnelState::Failed);
  QCOMPARE(m.status().failure, TunnelFailure::PortBusy);
}

void TunnelStateMachineTests::probing_isRepeatedUntilThePortOpens()
{
  auto m = machine();
  QCOMPARE(m.start(0), Command::Launch);
  QCOMPARE(m.logEvent(logLine(LogEventKind::ListenerStarted), 20), Command::ProbePort);
  m.portProbeFinished(false, 25);
  QCOMPARE(m.tick(100), Command::None);
  QCOMPARE(m.tick(220), Command::ProbePort);
  m.portProbeFinished(false, 225);
  QCOMPARE(m.tick(425), Command::ProbePort);
  m.portProbeFinished(true, 430);
  QCOMPARE(m.tick(431), Command::None);
  QCOMPARE(m.tick(730), Command::None);
  QCOMPARE(m.status().state, TunnelState::Listening);
}

void TunnelStateMachineTests::aPortThatDoesNotOpenInTime_failsAndKillsTheProcess()
{
  auto m = machine();
  QCOMPARE(m.start(0), Command::Launch);
  m.portProbeFinished(false, 100);
  QVERIFY(m.tick(7999) != Command::Kill); // probing goes on, nothing is given up before the time
  QCOMPARE(m.status().state, TunnelState::Starting);
  QCOMPARE(m.tick(8000), Command::Kill);
  QCOMPARE(m.status().state, TunnelState::Failed);
  QCOMPARE(m.status().failure, TunnelFailure::StartTimedOut);

  // the process goes: still Failed, and it is not started again by itself
  QCOMPARE(m.processExited(1, 8100), Command::None);
  QCOMPARE(m.status().state, TunnelState::Failed);
  QCOMPARE(m.tick(20000), Command::None);
  QVERIFY(!m.processRunning());
}

void TunnelStateMachineTests::aPortThatSomethingElseHolds_isNotTakenForTheTunnel()
{
  // the port answers (the other program) while cloudflared failed to listen and stops
  auto m = machine();
  QCOMPARE(m.start(0), Command::Launch);
  QCOMPARE(m.logEvent(logLine(LogEventKind::ListenerStarted), 20), Command::ProbePort);
  m.portProbeFinished(true, 30);
  QCOMPARE(m.logEvent(logLine(LogEventKind::BindFailed, TunnelFailure::PortBusy), 40), Command::Kill);
  QCOMPARE(m.status().state, TunnelState::Failed);
  QCOMPARE(m.status().failure, TunnelFailure::PortBusy);
  QCOMPARE(m.tick(1000), Command::None);
  QCOMPARE(m.status().state, TunnelState::Failed);

  // the same when the line is not seen and the process just stops within the confirmation time
  auto quiet = machine();
  QCOMPARE(quiet.start(0), Command::Launch);
  quiet.portProbeFinished(true, 310);
  QCOMPARE(quiet.processExited(1, 320), Command::None);
  QVERIFY(quiet.status().state != TunnelState::Listening);
  QCOMPARE(quiet.status().state, TunnelState::Backoff);
}

void TunnelStateMachineTests::aLoginThatIsAskedFor_failsKillsAndIsNeverRetried_data()
{
  QTest::addColumn<bool>("alreadyListening");
  QTest::newRow("while-starting") << false;
  QTest::newRow("while-listening-a-token-that-expired") << true;
}

void TunnelStateMachineTests::aLoginThatIsAskedFor_failsKillsAndIsNeverRetried()
{
  QFETCH(bool, alreadyListening);

  auto m = machine();
  qint64 now = 0;
  if (alreadyListening) {
    bringToListening(m, now);
  } else {
    QCOMPARE(m.start(now), Command::Launch);
  }

  QCOMPARE(m.logEvent(logLine(LogEventKind::LoginRequired, TunnelFailure::LoginRequired), now + 10), Command::Kill);
  QCOMPARE(m.status().state, TunnelState::Failed);
  QCOMPARE(m.status().failure, TunnelFailure::LoginRequired);
  QVERIFY(m.status().text().contains(QStringLiteral("Service Token")));

  // more lines of the dying process, the exit, and any amount of time: nothing starts it again
  QCOMPARE(m.logEvent(logLine(LogEventKind::LoginRequired, TunnelFailure::LoginRequired), now + 20), Command::None);
  QCOMPARE(m.processExited(1, now + 30), Command::None);
  QCOMPARE(m.tick(now + 120000), Command::None);
  QCOMPARE(m.status().state, TunnelState::Failed);
  QCOMPARE(m.status().failure, TunnelFailure::LoginRequired);
  QVERIFY(!m.processRunning());
}

void TunnelStateMachineTests::aProblemOfTheConnection_isShownWhileListeningAndEndsWithAConnection()
{
  auto m = machine();
  qint64 now = 0;
  bringToListening(m, now);

  QCOMPARE(m.logEvent(logLine(LogEventKind::OriginFailed, TunnelFailure::HandshakeRejected), now + 100), Command::None);
  QCOMPARE(m.status().state, TunnelState::Listening);
  QCOMPARE(m.status().failure, TunnelFailure::HandshakeRejected);
  QVERIFY(m.status().text().contains(QStringLiteral("Access policy")));

  QCOMPARE(m.logEvent(logLine(LogEventKind::OriginFailed, TunnelFailure::HostUnresolvable), now + 200), Command::None);
  QCOMPARE(m.status().failure, TunnelFailure::HostUnresolvable);

  QCOMPARE(m.serverConnected(now + 300), Command::None);
  QCOMPARE(m.status().state, TunnelState::Listening);
  QCOMPARE(m.status().failure, TunnelFailure::None);

  // nothing of it makes the process stop or start again
  QCOMPARE(m.tick(now + 500), Command::None);
  QVERIFY(m.processRunning());
}

void TunnelStateMachineTests::aProcessThatStops_isStartedAgainWithGrowingPauses()
{
  auto m = machine();
  qint64 now = 0;
  bringToListening(m, now);

  const QList<int> pauses = {1, 2, 4, 8, 16, 30, 60, 60};
  for (int i = 0; i < pauses.size(); ++i) {
    // it ran long enough not to count as stopping at once, and not long enough to start the pauses over
    now += 10000;
    QCOMPARE(m.processExited(1, now), Command::None);
    QCOMPARE(m.status().state, TunnelState::Backoff);
    QCOMPARE(m.status().restartInSeconds, pauses.at(i));
    QCOMPARE(m.status().exitCode, 1);
    QVERIFY(!m.processRunning());
    QVERIFY(m.status().text().contains(QStringLiteral("restarting in %1 s").arg(pauses.at(i))));

    QCOMPARE(m.tick(now + pauses.at(i) * 1000 - 1), Command::None);
    QCOMPARE(m.status().state, TunnelState::Backoff);
    now += pauses.at(i) * 1000;
    QCOMPARE(m.tick(now), Command::Launch);
    QCOMPARE(m.status().state, TunnelState::Starting);
    QVERIFY(m.processRunning());

    listen(m, now);
    QCOMPARE(m.status().state, TunnelState::Listening);
    now += 400;
  }
}

void TunnelStateMachineTests::thePausesStartOverAfterAStableRun()
{
  auto m = machine();
  qint64 now = 0;
  bringToListening(m, now);

  // two short runs: pauses of 1 and 2 seconds
  now += 10000;
  m.processExited(1, now);
  QCOMPARE(m.status().restartInSeconds, 1);
  now += 1000;
  QCOMPARE(m.tick(now), Command::Launch);
  listen(m, now);
  now += 400;

  now += 10000;
  m.processExited(1, now);
  QCOMPARE(m.status().restartInSeconds, 2);
  now += 2000;
  QCOMPARE(m.tick(now), Command::Launch);
  listen(m, now);
  now += 400;

  // then it runs for more than a minute and stops: the pause is the first one again
  now += 61000;
  m.processExited(1, now);
  QCOMPARE(m.status().restartInSeconds, 1);
}

void TunnelStateMachineTests::aProcessThatStopsAtOnceAgainAndAgain_failsWithItsExitCode()
{
  auto m = machine();
  qint64 now = 0;
  QCOMPARE(m.start(now), Command::Launch);

  // five times in a row it stops half a second after its start
  for (int exitNumber = 1; exitNumber <= 5; ++exitNumber) {
    now += 500;
    QCOMPARE(m.processExited(3, now), Command::None);
    if (exitNumber < 5) {
      QCOMPARE(m.status().state, TunnelState::Backoff);
      now += m.status().restartInSeconds * 1000;
      QCOMPARE(m.tick(now), Command::Launch);
    }
  }

  QCOMPARE(m.status().state, TunnelState::Failed);
  QCOMPARE(m.status().failure, TunnelFailure::ProcessExited);
  QCOMPARE(m.status().exitCode, 3);
  QVERIFY(m.status().text().contains(QStringLiteral("exit code 3")));
  QCOMPARE(m.tick(now + 999999), Command::None);
}

void TunnelStateMachineTests::aRunThatLastedMoreThanAMomentEndsTheCountOfQuickExits()
{
  auto m = machine();
  qint64 now = 0;
  QCOMPARE(m.start(now), Command::Launch);

  for (int exitNumber = 1; exitNumber <= 4; ++exitNumber) {
    now += 500;
    QCOMPARE(m.processExited(1, now), Command::None);
    QCOMPARE(m.status().state, TunnelState::Backoff);
    now += m.status().restartInSeconds * 1000;
    QCOMPARE(m.tick(now), Command::Launch);
  }

  // this run is three seconds long: the next quick exit is the first of a new count, not the fifth
  now += 3000;
  QCOMPARE(m.processExited(1, now), Command::None);
  QCOMPARE(m.status().state, TunnelState::Backoff);
}

void TunnelStateMachineTests::aProcessThatCannotBeStarted_meansCloudflaredIsMissing()
{
  auto m = machine();
  QCOMPARE(m.start(0), Command::Launch);
  QCOMPARE(m.launchFailed(5), Command::None);
  QCOMPARE(m.status().state, TunnelState::Failed);
  QCOMPARE(m.status().failure, TunnelFailure::CloudflaredMissing);
  QVERIFY(!m.processRunning());
  QVERIFY(m.status().text().contains(QStringLiteral("brew install cloudflared")));
  QVERIFY(m.status().text().contains(QStringLiteral("winget install --id Cloudflare.cloudflared")));

  // a later start is a new attempt
  QCOMPARE(m.start(1000), Command::Launch);
  QCOMPARE(m.status().state, TunnelState::Starting);
  QCOMPARE(m.status().failure, TunnelFailure::None);
}

void TunnelStateMachineTests::stop_killsARunningProcessAndEndsIdleWhenItIsGone()
{
  auto m = machine();
  qint64 now = 0;
  bringToListening(m, now);
  QCOMPARE(m.stop(now + 10), Command::Kill);
  QCOMPARE(m.status().state, TunnelState::Stopping);
  QCOMPARE(m.status().text(), QStringLiteral("Tunnel: stopping"));
  QVERIFY(m.processRunning());

  QCOMPARE(m.processExited(0, now + 50), Command::None);
  QCOMPARE(m.status().state, TunnelState::Idle);
  QCOMPARE(m.status().failure, TunnelFailure::None);
  QCOMPARE(m.status().hostname, QStringLiteral("mbp.example.com"));
  QVERIFY(!m.processRunning());
  QCOMPARE(m.tick(now + 100000), Command::None);

  // idle: nothing to stop
  QCOMPARE(m.stop(now + 200), Command::None);
  QCOMPARE(m.status().state, TunnelState::Idle);
}

void TunnelStateMachineTests::stop_whileWaitingToRestartIsIdleAtOnce()
{
  auto m = machine();
  qint64 now = 0;
  bringToListening(m, now);
  m.processExited(1, now + 10000);
  QCOMPARE(m.status().state, TunnelState::Backoff);

  QCOMPARE(m.stop(now + 10100), Command::None);
  QCOMPARE(m.status().state, TunnelState::Idle);
  QCOMPARE(m.tick(now + 100000), Command::None);

  // and from Failed
  auto failed = machine();
  failed.rejectStart(TunnelFailure::CredentialsMissing);
  QCOMPARE(failed.stop(10), Command::None);
  QCOMPARE(failed.status().state, TunnelState::Idle);
  QCOMPARE(failed.status().failure, TunnelFailure::None);
}

void TunnelStateMachineTests::stop_asksAgainOnceWhenTheProcessDoesNotGo()
{
  auto m = machine();
  qint64 now = 0;
  bringToListening(m, now);
  QCOMPARE(m.stop(now), Command::Kill);
  QCOMPARE(m.tick(now + 2999), Command::None);
  QCOMPARE(m.tick(now + 3000), Command::Kill);
  QCOMPARE(m.tick(now + 6000), Command::None);
  QCOMPARE(m.status().state, TunnelState::Stopping);
  QCOMPARE(m.processExited(1, now + 6100), Command::None);
  QCOMPARE(m.status().state, TunnelState::Idle);
}

void TunnelStateMachineTests::start_isIgnoredWhileRunningStoppingOrWhileTheLastProcessIsStillGoing()
{
  auto starting = machine();
  QCOMPARE(starting.start(0), Command::Launch);
  QCOMPARE(starting.start(10), Command::None);

  auto m = machine();
  qint64 now = 0;
  bringToListening(m, now, 100);
  QCOMPARE(m.start(now), Command::None);

  QCOMPARE(m.stop(now), Command::Kill);
  QCOMPARE(m.start(now + 1), Command::None);
  QCOMPARE(m.status().state, TunnelState::Stopping);
  QCOMPARE(m.processExited(0, now + 5), Command::None);

  // a login was asked for: the process was told to go and has not gone yet, so a start waits for it
  auto login = machine();
  QCOMPARE(login.start(0), Command::Launch);
  QCOMPARE(login.logEvent(logLine(LogEventKind::LoginRequired, TunnelFailure::LoginRequired), 10), Command::Kill);
  QCOMPARE(login.start(20), Command::None);
  QCOMPARE(login.status().state, TunnelState::Failed);
  QCOMPARE(login.processExited(1, 30), Command::None);
  QCOMPARE(login.start(40), Command::Launch);
}

void TunnelStateMachineTests::rejectStart_failsWithoutAProcessAndAStartLeavesIt()
{
  auto m = machine();
  QCOMPARE(m.rejectStart(TunnelFailure::TlsRequired), Command::None);
  QCOMPARE(m.status().state, TunnelState::Failed);
  QCOMPARE(m.status().failure, TunnelFailure::TlsRequired);
  QVERIFY(!m.processRunning());
  QVERIFY(m.status().text().contains(QStringLiteral("requires TLS")));

  QCOMPARE(m.start(10), Command::Launch);
  QCOMPARE(m.status().state, TunnelState::Starting);

  // not while a process runs
  QCOMPARE(m.rejectStart(TunnelFailure::InvalidHostname), Command::None);
  QCOMPARE(m.status().state, TunnelState::Starting);
}

void TunnelStateMachineTests::status_hasTheTextForEveryStateAndFailure()
{
  TunnelStatus status;
  status.state = TunnelState::Idle;
  QCOMPARE(status.text(), QStringLiteral("Tunnel: stopped"));
  status.state = TunnelState::Starting;
  QCOMPARE(status.text(), QStringLiteral("Tunnel: starting"));

  status.state = TunnelState::Failed;
  for (const auto failure :
       {TunnelFailure::TlsRequired, TunnelFailure::InvalidHostname, TunnelFailure::CloudflaredMissing,
        TunnelFailure::CredentialsMissing, TunnelFailure::PortBusy, TunnelFailure::LoginRequired,
        TunnelFailure::HostUnresolvable, TunnelFailure::EdgeUnreachable, TunnelFailure::HandshakeRejected,
        TunnelFailure::ConnectFailed, TunnelFailure::StartTimedOut, TunnelFailure::ProcessExited,
        TunnelFailure::SettingsChanged}) {
    status.failure = failure;
    QVERIFY(!failureText(failure).isEmpty());
    QVERIFY(status.text().startsWith(QStringLiteral("Tunnel: failed (")));
    QVERIFY(status.text().contains(failureText(failure)));
  }
  QVERIFY(failureText(TunnelFailure::None).isEmpty());
}

void TunnelStateMachineTests::needsUserAction_isTrueWhereRestartingCannotHelp()
{
  for (const auto failure :
       {TunnelFailure::TlsRequired, TunnelFailure::InvalidHostname, TunnelFailure::CloudflaredMissing,
        TunnelFailure::CredentialsMissing, TunnelFailure::PortBusy, TunnelFailure::LoginRequired}) {
    QVERIFY(needsUserAction(failure));
  }
  for (const auto failure :
       {TunnelFailure::None, TunnelFailure::HostUnresolvable, TunnelFailure::EdgeUnreachable,
        TunnelFailure::HandshakeRejected, TunnelFailure::ConnectFailed, TunnelFailure::StartTimedOut,
        TunnelFailure::ProcessExited, TunnelFailure::SettingsChanged}) {
    QVERIFY(!needsUserAction(failure));
  }
}

QTEST_MAIN(TunnelStateMachineTests)
