/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelStartGuardTests.h"

#include "gui/tunnel/TunnelStartGuard.h"

using namespace deskflow::tunnel;

namespace {

class FakeGate : public ITunnelGate
{
public:
  bool required = true;
  bool listeningNow = false;
  bool failOnAcquire = false;
  //! what validate() says: the configuration as it is now
  TunnelFailure problem = TunnelFailure::None;
  int acquired = 0;
  int released = 0;

  bool isRequired() const override
  {
    return required;
  }

  TunnelFailure validate() const override
  {
    return problem;
  }

  bool isListening() const override
  {
    return listeningNow;
  }

  void acquire() override
  {
    ++acquired;
    if (failOnAcquire) {
      Q_EMIT failed(TunnelFailure::CloudflaredMissing);
    }
  }

  void release() override
  {
    ++released;
  }

  void tunnelListens()
  {
    listeningNow = true;
    Q_EMIT listening();
  }
};

// what the process of the core does with the guard, as counts
struct Core
{
  int waiting = 0;
  int started = 0;
  int failedWaiting = 0;
  int failedRunning = 0;
  TunnelFailure lastFailure = TunnelFailure::None;
  TunnelStartGuard *guard = nullptr;
  bool clientMode = true;

  // CoreProcess::start()
  void start()
  {
    if (guard->intercept(clientMode)) {
      return;
    }
    ++started;
  }

  TunnelStartGuard::Actions actions()
  {
    TunnelStartGuard::Actions a;
    a.waiting = [this] { ++waiting; };
    a.resume = [this] { start(); };
    a.failed = [this](TunnelFailure failure, bool wasWaiting) {
      lastFailure = failure;
      wasWaiting ? ++failedWaiting : ++failedRunning;
    };
    return a;
  }
};

} // namespace

void TunnelStartGuardTests::aStartOfAServerOrWithoutATunnel_isNotTouched()
{
  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.clientMode = false;
  core.start();
  QCOMPARE(core.started, 1);

  core.clientMode = true;
  gate.required = false;
  core.start();
  QCOMPARE(core.started, 2);

  QCOMPARE(gate.acquired, 0);
  QCOMPARE(core.waiting, 0);
  QVERIFY(!guard.isWaiting());
}

void TunnelStartGuardTests::aTunnelThatListensAlready_letsTheStartThrough_andIsToldItIsNeeded()
{
  FakeGate gate;
  gate.listeningNow = true;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  QCOMPARE(core.started, 1);
  QCOMPARE(gate.acquired, 1);
  QCOMPARE(core.waiting, 0);
  QVERIFY(!guard.isWaiting());
}

void TunnelStartGuardTests::aTunnelThatDoesNotListen_postponesTheStartAndStartsTheCoreWhenItListens()
{
  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  QCOMPARE(core.started, 0);
  QCOMPARE(core.waiting, 1);
  QCOMPARE(gate.acquired, 1);
  QVERIFY(guard.isWaiting());

  gate.tunnelListens();
  QCOMPARE(core.started, 1);
  QVERIFY(!guard.isWaiting());
}

void TunnelStartGuardTests::theCoreIsStartedOnceAndTheStartThatItMakesPassesTheGuard()
{
  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  gate.tunnelListens();
  // a second signal of the tunnel, with nobody waiting, starts nothing
  Q_EMIT gate.listening();
  QCOMPARE(core.started, 1);
  QCOMPARE(core.waiting, 1);
  // the one acquire of the first start is the only one: the start that the guard let through did not ask again
  QCOMPARE(gate.acquired, 1);
}

void TunnelStartGuardTests::aTunnelThatFailsWhileTheCoreWaits_reportsItAsWaiting()
{
  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  Q_EMIT gate.failed(TunnelFailure::LoginRequired);

  QCOMPARE(core.started, 0);
  QCOMPARE(core.failedWaiting, 1);
  QCOMPARE(core.failedRunning, 0);
  QCOMPARE(core.lastFailure, TunnelFailure::LoginRequired);
  QVERIFY(!guard.isWaiting());
}

void TunnelStartGuardTests::aTunnelThatFailsAtOnce_isHandledAndTheStartStillEndsPostponed()
{
  FakeGate gate;
  gate.failOnAcquire = true;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  QCOMPARE(core.started, 0);
  QCOMPARE(core.waiting, 1);
  QCOMPARE(core.failedWaiting, 1);
  QCOMPARE(core.lastFailure, TunnelFailure::CloudflaredMissing);
  QVERIFY(!guard.isWaiting());
}

void TunnelStartGuardTests::aTunnelThatFailsAfterTheCoreStarted_reportsItAsNotWaiting()
{
  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  gate.tunnelListens();
  QCOMPARE(core.started, 1);

  Q_EMIT gate.failed(TunnelFailure::ProcessExited);
  QCOMPARE(core.failedWaiting, 0);
  QCOMPARE(core.failedRunning, 1);
  QCOMPARE(core.lastFailure, TunnelFailure::ProcessExited);
}

void TunnelStartGuardTests::aStopWhileWaiting_cancelsTheStartAndReleasesTheTunnel()
{
  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  QVERIFY(guard.isWaiting());
  guard.coreStopped();
  QVERIFY(!guard.isWaiting());
  QCOMPARE(gate.released, 1);
}

void TunnelStartGuardTests::aListeningThatCameAfterAStop_doesNotStartTheCore()
{
  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  guard.coreStopped();
  gate.tunnelListens();
  QCOMPARE(core.started, 0);
}

void TunnelStartGuardTests::aTunnelThatListensAfterTheConfigurationBecameUnusable_doesNotStartTheCore_data()
{
  QTest::addColumn<TunnelFailure>("problem");
  QTest::newRow("tls switched off") << TunnelFailure::TlsRequired;
  QTest::newRow("hostname cleared") << TunnelFailure::InvalidHostname;
  QTest::newRow("tunnel disabled or settings changed") << TunnelFailure::SettingsChanged;
}

void TunnelStartGuardTests::aTunnelThatListensAfterTheConfigurationBecameUnusable_doesNotStartTheCore()
{
  QFETCH(TunnelFailure, problem);

  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  QVERIFY(guard.isWaiting());

  // while the tunnel came up the user changed the settings: the core must not dial anything on the old ones
  gate.problem = problem;
  gate.tunnelListens();

  QCOMPARE(core.started, 0);
  QCOMPARE(core.failedWaiting, 1);
  QCOMPARE(core.failedRunning, 0);
  QCOMPARE(core.lastFailure, problem);
  QVERIFY(!guard.isWaiting());
  // and the tunnel that was started for it is let go
  QCOMPARE(gate.released, 1);
}

void TunnelStartGuardTests::theStartAfterAnUnusableResume_asksTheTunnelAgain()
{
  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  core.start();
  gate.problem = TunnelFailure::TlsRequired;
  gate.tunnelListens();
  QCOMPARE(core.started, 0);

  // the user fixes the settings and starts again
  gate.problem = TunnelFailure::None;
  gate.listeningNow = false;
  core.start();
  QCOMPARE(core.waiting, 2);
  QCOMPARE(gate.acquired, 2);
  gate.tunnelListens();
  QCOMPARE(core.started, 1);
}

void TunnelStartGuardTests::everyStopReleasesTheTunnel()
{
  FakeGate gate;
  Core core;
  TunnelStartGuard guard(&gate, core.actions());
  core.guard = &guard;

  guard.coreStopped();
  guard.coreStopped();
  QCOMPARE(gate.released, 2);
}

QTEST_MAIN(TunnelStartGuardTests)
