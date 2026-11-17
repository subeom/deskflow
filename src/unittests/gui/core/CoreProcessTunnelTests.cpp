/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreProcessTunnelTests.h"

#include "../tunnel/TunnelTestDoubles.h"
#include "FakeDaemonIpcClient.h"
#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/core/CoreProcess.h"
#include "gui/tunnel/ITunnelGate.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace deskflow::gui;
using namespace deskflow::tunnel;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;

namespace {

QTemporaryDir &scratchDir()
{
  static QTemporaryDir dir;
  return dir;
}

class FakeGate : public ITunnelGate
{
public:
  bool required = true;
  bool listeningNow = false;
  bool failOnAcquire = false;
  TunnelFailure problem = TunnelFailure::None;
  int acquired = 0;
  int released = 0;

  TunnelFailure validate() const override
  {
    return problem;
  }

  bool isRequired() const override
  {
    return required;
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

  void tellListening()
  {
    Q_EMIT listening();
  }

  void tellFailed(TunnelFailure failure)
  {
    Q_EMIT failed(failure);
  }
};

// A core process in service mode whose client of the service is the stand-in: nothing here talks to a daemon, a socket
// or a core. The tunnel is on and listens, so that a start goes through the guard at once.
struct ServiceRig
{
  const ServerConfig serverConfig;
  FakeGate gate;
  FakeDaemonIpcClient *client = nullptr;
  std::unique_ptr<CoreProcess> core;

  explicit ServiceRig(FakeDaemonIpcClient::Behaviour behaviour, bool connected)
  {
    Settings::setValue(Settings::Core::ProcessMode, QVariant::fromValue(Settings::ProcessMode::Service));
    gate.listeningNow = true;
    core = std::make_unique<CoreProcess>(serverConfig, [this, behaviour, connected](QObject *parent) {
      client = new FakeDaemonIpcClient(parent);
      client->behaviour = behaviour;
      client->setConnected(connected);
      return client;
    });
    core->setTunnelGate(&gate);
    core->setMode(Settings::CoreMode::Client);
  }
};

} // namespace

void CoreProcessTunnelTests::cleanup()
{
  Settings::setValue(Settings::Core::ProcessMode, QVariant::fromValue(Settings::ProcessMode::Desktop));
}

void CoreProcessTunnelTests::initTestCase()
{
  QVERIFY(scratchDir().isValid());
  m_dir = scratchDir().path();
  // the settings of this test are files of its own, and the core runs as a process of this user, never in a service
  Settings::setSettingsFile(QStringLiteral("%1/Deskflow.conf").arg(m_dir));
  Settings::setStateFile(QStringLiteral("%1/Deskflow.state").arg(m_dir));
  Settings::setValue(Settings::Core::ProcessMode, QVariant::fromValue(Settings::ProcessMode::Desktop));
  QCOMPARE(Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>(), Settings::ProcessMode::Desktop);
}

void CoreProcessTunnelTests::withoutAGate_noTunnelIsRequired()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  core.setMode(Settings::CoreMode::Client);
  QVERIFY(!core.isTunnelRequired());
}

void CoreProcessTunnelTests::theTunnelIsRequiredByAClientOnly()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate gate;
  core.setTunnelGate(&gate);

  core.setMode(Settings::CoreMode::Client);
  QVERIFY(core.isTunnelRequired());
  core.setMode(Settings::CoreMode::Server);
  QVERIFY(!core.isTunnelRequired());
  core.setMode(Settings::CoreMode::None);
  QVERIFY(!core.isTunnelRequired());

  core.setMode(Settings::CoreMode::Client);
  gate.required = false;
  QVERIFY(!core.isTunnelRequired());
}

void CoreProcessTunnelTests::aStartThatWaitsForTheTunnel_isPostponedAndShowsAsStarting()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate gate;
  core.setTunnelGate(&gate);
  core.setMode(Settings::CoreMode::Client);
  QSignalSpy processStates(&core, &CoreProcess::processStateChanged);
  QSignalSpy connectionStates(&core, &CoreProcess::connectionStateChanged);
  QSignalSpy errors(&core, &CoreProcess::error);

  core.start();

  QCOMPARE(gate.acquired, 1);
  QCOMPARE(core.processState(), ProcessState::Starting);
  QCOMPARE(core.connectionState(), ConnectionState::Connecting);
  QVERIFY(!core.isStarted());
  QCOMPARE(processStates.count(), 1);
  QCOMPARE(connectionStates.count(), 1);
  QCOMPARE(errors.count(), 0);
}

void CoreProcessTunnelTests::aTunnelThatFailsWhileTheStartWaits_endsTheStart()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate gate;
  core.setTunnelGate(&gate);
  core.setMode(Settings::CoreMode::Client);
  core.start();
  QCOMPARE(core.processState(), ProcessState::Starting);

  gate.tellFailed(TunnelFailure::LoginRequired);
  QCOMPARE(core.processState(), ProcessState::Stopped);
  QCOMPARE(core.connectionState(), ConnectionState::Disconnected);

  // the next start asks the tunnel again
  gate.failOnAcquire = true;
  core.start();
  QCOMPARE(gate.acquired, 2);
}

void CoreProcessTunnelTests::aTunnelThatFailsAtOnce_endsTheStart()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate gate;
  gate.failOnAcquire = true;
  core.setTunnelGate(&gate);
  core.setMode(Settings::CoreMode::Client);

  core.start();
  QCOMPARE(gate.acquired, 1);
  QCOMPARE(core.processState(), ProcessState::Stopped);
  QCOMPARE(core.connectionState(), ConnectionState::Disconnected);
}

void CoreProcessTunnelTests::aTunnelThatListensAfterTheSettingsBecameUnusable_endsTheStartInsteadOfStartingTheCore()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate gate;
  core.setTunnelGate(&gate);
  core.setMode(Settings::CoreMode::Client);
  core.start();
  QCOMPARE(core.processState(), ProcessState::Starting);
  QSignalSpy errors(&core, &CoreProcess::error);

  // TLS was switched off while the tunnel came up: the core is not started (it would dial the saved address)
  gate.problem = TunnelFailure::TlsRequired;
  gate.tellListening();

  QCOMPARE(core.processState(), ProcessState::Stopped);
  QCOMPARE(core.connectionState(), ConnectionState::Disconnected);
  QVERIFY(!core.isStarted());
  QCOMPARE(gate.released, 1);
  QCOMPARE(errors.count(), 0);
}

void CoreProcessTunnelTests::aStopWhileTheStartWaits_cancelsItAndLetsTheTunnelGo()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate gate;
  core.setTunnelGate(&gate);
  core.setMode(Settings::CoreMode::Client);
  core.start();
  QCOMPARE(core.processState(), ProcessState::Starting);

  core.stop();
  QCOMPARE(core.processState(), ProcessState::Stopped);
  QCOMPARE(core.connectionState(), ConnectionState::Disconnected);
  QCOMPARE(gate.released, 1);
}

void CoreProcessTunnelTests::aTunnelThatListensAfterAStop_doesNotStartTheCore()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate gate;
  core.setTunnelGate(&gate);
  core.setMode(Settings::CoreMode::Client);
  core.start();
  core.stop();

  // the tunnel that was asked for before the stop comes up late: the core stays stopped
  QSignalSpy processStates(&core, &CoreProcess::processStateChanged);
  gate.tellListening();
  QCOMPARE(core.processState(), ProcessState::Stopped);
  QCOMPARE(processStates.count(), 0);
}

void CoreProcessTunnelTests::aCleanupOfACoreThatIsNotRunning_leavesTheTunnelAlone()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate gate;
  core.setTunnelGate(&gate);
  core.setMode(Settings::CoreMode::Client);

  core.cleanup();
  QCOMPARE(gate.released, 0);
  QCOMPARE(core.processState(), ProcessState::Stopped);

  // also in service mode: nothing is sent to the service for a core that is not there
  Settings::setValue(Settings::Core::ProcessMode, QVariant::fromValue(Settings::ProcessMode::Service));
  core.cleanup();
  Settings::setValue(Settings::Core::ProcessMode, QVariant::fromValue(Settings::ProcessMode::Desktop));
  QCOMPARE(gate.released, 0);
}

void CoreProcessTunnelTests::aNewGate_replacesTheOldOne()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate first;
  FakeGate second;
  core.setMode(Settings::CoreMode::Client);
  core.setTunnelGate(&first);
  core.setTunnelGate(&second);

  core.start();
  QCOMPARE(first.acquired, 0);
  QCOMPARE(second.acquired, 1);
  core.stop();

  // no gate: the tunnel is not required any more
  core.setTunnelGate(nullptr);
  QVERIFY(!core.isTunnelRequired());
}

void CoreProcessTunnelTests::theQuitInServiceMode_stopsTheServiceCoreAndNoStartFollows()
{
  ServiceRig rig(FakeDaemonIpcClient::Behaviour::ConnectsAtOnce, true);
  rig.core->start();
  QCOMPARE(rig.core->processState(), ProcessState::Started);
  QCOMPARE(rig.client->count(QStringLiteral("start")), 1);

  rig.core->cleanup();
  QCOMPARE(rig.core->processState(), ProcessState::Stopped);
  QCOMPARE(rig.client->count(QStringLiteral("stop")), 1);
  QCOMPARE(rig.client->flushCalls, 1);

  // the program is ending: nothing starts the core any more, not even a start that was asked for late
  rig.core->start();
  QCOMPARE(rig.core->processState(), ProcessState::Stopped);
  QCOMPARE(rig.client->count(QStringLiteral("start")), 1);
}

void CoreProcessTunnelTests::theQuitWhileTheServiceIsNotConnected_connectsAndSendsTheStop()
{
  ServiceRig rig(FakeDaemonIpcClient::Behaviour::ConnectsLater, true);
  rig.core->start();
  QCOMPARE(rig.core->processState(), ProcessState::Started);

  // the connection to the service was lost since: the stop is not queued behind a connection that the program will not
  // wait for
  rig.client->setConnected(false);
  rig.client->connectDelayMs = 100;
  rig.core->cleanup();

  QCOMPARE(rig.core->processState(), ProcessState::Stopped);
  QCOMPARE(rig.client->connectCalls, 1);
  QCOMPARE(rig.client->count(QStringLiteral("stop")), 1);
  QCOMPARE(rig.client->droppedMessages, 0);
  QCOMPARE(rig.client->flushCalls, 1);
}

void CoreProcessTunnelTests::theQuitThatCannotReachTheService_isBoundedAndWarnsWhatRemains()
{
  ServiceRig rig(FakeDaemonIpcClient::Behaviour::NeverConnects, true);
  rig.core->start();
  QCOMPARE(rig.core->processState(), ProcessState::Started);
  rig.client->setConnected(false);

  QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("keeps running.*boot")));
  QElapsedTimer timer;
  timer.start();
  rig.core->cleanup();

  // it waited, but not for ever, and it did not queue a stop that nobody will send
  QVERIFY(timer.elapsed() < 4000);
  QCOMPARE(rig.core->processState(), ProcessState::Stopped);
  QCOMPARE(rig.client->count(QStringLiteral("stop")), 0);
  QCOMPARE(rig.client->droppedMessages, 0);

  // a connection that comes after the quit finds nothing to send
  rig.client->connectNow();
  QTest::qWait(50);
  QCOMPARE(rig.client->count(QStringLiteral("stop")), 0);
  QCOMPARE(rig.client->count(QStringLiteral("start")), 1);
}

void CoreProcessTunnelTests::aStartThatWaitsForTheService_isCancelledByTheQuitAndANewConnectionDoesNotStartTheCore()
{
  ServiceRig rig(FakeDaemonIpcClient::Behaviour::NeverConnects, false);
  rig.core->start();
  // the start is queued behind the connection to the service
  QCOMPARE(rig.core->processState(), ProcessState::Starting);
  QCOMPARE(rig.client->connectCalls, 1);
  QCOMPARE(rig.client->sent.size(), 0);

  QElapsedTimer timer;
  timer.start();
  rig.core->cleanup();
  QCOMPARE(rig.core->processState(), ProcessState::Stopped);
  // the service was never told to start: there is nothing to stop and nothing to wait for
  QVERIFY(timer.elapsed() < 500);

  // the service answers late: the start that was queued must not run
  rig.client->connectNow();
  QTest::qWait(100);
  QCOMPARE(rig.client->count(QStringLiteral("start")), 0);
  QCOMPARE(rig.client->count(QStringLiteral("configFile")), 0);
  QCOMPARE(rig.core->processState(), ProcessState::Stopped);
}

void CoreProcessTunnelTests::theQuitInServiceModeWithTheTunnelOff_isWhatItWas()
{
  ServiceRig rig(FakeDaemonIpcClient::Behaviour::ConnectsAtOnce, true);
  rig.core->start();
  QCOMPARE(rig.core->processState(), ProcessState::Started);

  // the tunnel is off: the service core keeps running when the program quits, as it always did
  rig.gate.required = false;
  rig.core->cleanup();
  QCOMPARE(rig.core->processState(), ProcessState::Started);
  QCOMPARE(rig.client->count(QStringLiteral("stop")), 0);
  QCOMPARE(rig.client->flushCalls, 0);
  QCOMPARE(rig.client->connectCalls, 0);
}

void CoreProcessTunnelTests::aModeSwitchToTheServerWhileTheStartWaits_resetsTheState()
{
  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  FakeGate gate;
  core.setTunnelGate(&gate);
  core.setMode(Settings::CoreMode::Client);
  core.start();
  QCOMPARE(core.processState(), ProcessState::Starting);
  QCOMPARE(core.connectionState(), ConnectionState::Connecting);

  // the window does this when the user picks the server: the start that waits is a start in the Starting state, and the
  // window stops the core only for a core that has started
  core.setMode(Settings::CoreMode::Server);

  QCOMPARE(core.processState(), ProcessState::Stopped);
  QCOMPARE(core.connectionState(), ConnectionState::Disconnected);
  QCOMPARE(gate.released, 1);

  // the tunnel that listens afterwards does not start anything
  QSignalSpy states(&core, &CoreProcess::processStateChanged);
  gate.tellListening();
  QCOMPARE(states.count(), 0);
  QCOMPARE(core.processState(), ProcessState::Stopped);
}

// the same with the real controller and its supervisor (made of fakes): what the window does when the user picks the
// server
void CoreProcessTunnelTests::aModeSwitchToTheServerWhileTheTunnelComesUp_leavesNothingBehind()
{
  tunneltest::ControllerRig rig(m_dir);
  rig.turnOn();
  Settings::setValue(Settings::Core::ProcessMode, QVariant::fromValue(Settings::ProcessMode::Desktop));

  const ServerConfig serverConfig;
  CoreProcess core(serverConfig);
  core.setTunnelGate(rig.controller.get());
  core.setMode(Settings::CoreMode::Client);
  core.start();
  QCOMPARE(core.processState(), ProcessState::Starting);
  QCOMPARE(rig.controller->status().state, TunnelState::Starting);
  QCOMPARE(rig.processes.size(), size_t(1));

  // MainWindow::coreModeToggled: the core, then the tunnel
  core.setMode(Settings::CoreMode::Server);
  rig.controller->setClientMode(false);

  QCOMPARE(core.processState(), ProcessState::Stopped);
  QCOMPARE(core.connectionState(), ConnectionState::Disconnected);
  QCOMPARE(rig.controller->status().state, TunnelState::Idle);
  QVERIFY(!rig.process()->running);

  // nothing comes up afterwards
  rig.advance(2000);
  QCOMPARE(core.processState(), ProcessState::Stopped);
  QCOMPARE(rig.processes.size(), size_t(1));
}

QTEST_MAIN(CoreProcessTunnelTests)
