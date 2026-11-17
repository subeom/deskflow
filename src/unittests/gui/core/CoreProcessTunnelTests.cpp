/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreProcessTunnelTests.h"

#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/core/CoreProcess.h"
#include "gui/tunnel/ITunnelGate.h"

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
  int acquired = 0;
  int released = 0;

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

} // namespace

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

QTEST_MAIN(CoreProcessTunnelTests)
