/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelControllerTests.h"

#include "TunnelTestDoubles.h"

#include "common/Settings.h"
#include "common/TunnelAddress.h"
#include "gui/tunnel/InMemorySecretStore.h"
#include "gui/tunnel/TunnelController.h"

#include <QDir>
#include <QFile>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>

using namespace deskflow::tunnel;
using namespace tunneltest;

namespace {

QTemporaryDir &scratchDir()
{
  static QTemporaryDir dir;
  return dir;
}

QByteArray readAll(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<unreadable>");
}

} // namespace

void TunnelControllerTests::initTestCase()
{
  QVERIFY(scratchDir().isValid());
  m_dir = scratchDir().path();
  // the settings of this test are files of its own, never the ones of the user
  Settings::setSettingsFile(QStringLiteral("%1/start/Deskflow.conf").arg(m_dir));
  Settings::setStateFile(QStringLiteral("%1/start/Deskflow.state").arg(m_dir));
  QVERIFY(Settings::settingsFile().startsWith(QDir(m_dir).absolutePath()));
}

void TunnelControllerTests::withTheTunnelOff_nothingIsRequiredNothingStartsAndNothingIsWritten()
{
  ControllerRig rig(m_dir);
  const auto before = readAll(rig.file);
  QCOMPARE(before, kConfig);

  QVERIFY(!rig.controller->isRequired());
  QVERIFY(!rig.controller->isListening());
  rig.controller->setClientMode(false);
  rig.controller->setClientMode(true);
  rig.controller->release();
  rig.controller->notifyServerConnected();
  rig.controller->shutdown();

  QCOMPARE(rig.controller->status().state, TunnelState::Idle);
  QVERIFY(rig.processes.empty());
  QCOMPARE(rig.storeState->loadCount, 0);
  QCOMPARE(readAll(rig.file), before);

  // and the address that is dialled is the saved one
  Settings::setValue(Settings::Client::RemoteHost, QStringLiteral("10.0.0.5"));
  QCOMPARE(effectiveRemoteHost(), QStringLiteral("10.0.0.5"));
}

void TunnelControllerTests::withTheTunnelOn_itIsRequiredInClientModeOnly()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  QVERIFY(rig.controller->isRequired());

  rig.controller->setClientMode(false);
  QVERIFY(!rig.controller->isRequired());
  rig.controller->setClientMode(true);
  QVERIFY(rig.controller->isRequired());

  // on and not usable: still required, so that the start of the core is stopped with a reason
  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("not a host name"));
  QVERIFY(rig.controller->isRequired());
}

void TunnelControllerTests::acquire_startsTheTunnelWithWhatTheSettingsHold()
{
  ControllerRig rig(m_dir);
  rig.turnOn(QStringLiteral("  MBP.Example.com. "));
  Settings::setValue(Settings::Tunnel::LocalPort, 25000);
  Settings::setValue(Settings::Tunnel::CloudflaredPath, QStringLiteral("/opt/test/bin/cloudflared"));

  rig.controller->acquire();
  QCOMPARE(rig.processes.size(), size_t(1));
  const auto *process = rig.process();
  QCOMPARE(process->spec.program, QStringLiteral("/opt/test/bin/cloudflared"));
  QVERIFY(process->spec.arguments.contains(QStringLiteral("mbp.example.com")));
  QVERIFY(process->spec.arguments.contains(QStringLiteral("127.0.0.1:25000")));
  QCOMPARE(rig.prober->canListenAsked.front(), 25000);
  QCOMPARE(rig.controller->status().state, TunnelState::Starting);
}

void TunnelControllerTests::theGate_isListeningOnlyForTheConfigurationThatRuns()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  QSignalSpy listening(rig.controller.get(), &ITunnelGate::listening);

  QVERIFY(!rig.controller->isListening());
  QVERIFY(rig.bringUp());
  QVERIFY(rig.controller->isListening());
  QCOMPARE(listening.count(), 1);

  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("other.example.com"));
  QVERIFY(!rig.controller->isListening());
  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("mbp.example.com"));
  QVERIFY(rig.controller->isListening());
  Settings::setValue(Settings::Tunnel::LocalPort, 25001);
  QVERIFY(!rig.controller->isListening());
}

void TunnelControllerTests::acquire_whileTheSameTunnelRuns_startsNothingNew()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  QVERIFY(rig.bringUp());

  rig.controller->acquire();
  rig.controller->acquire();
  QCOMPARE(rig.processes.size(), size_t(1));
  QCOMPARE(rig.process()->terminateCalls, 0);
  QCOMPARE(rig.controller->status().state, TunnelState::Listening);
}

void TunnelControllerTests::acquire_afterTheConfigurationChanged_startsTheTunnelAgain()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  QVERIFY(rig.bringUp());
  auto *first = rig.process();

  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("other.example.com"));
  rig.controller->acquire();
  QCOMPARE(first->terminateCalls, 1);
  QCOMPARE(rig.processes.size(), size_t(2));
  QVERIFY(rig.process()->spec.arguments.contains(QStringLiteral("other.example.com")));
  QCOMPARE(rig.controller->status().hostname, QStringLiteral("other.example.com"));
}

void TunnelControllerTests::release_keepsTheTunnelForAWhileAndAnAcquireCancelsTheStop()
{
  ControllerRig rig(m_dir, kConfig, 150);
  rig.turnOn();
  QVERIFY(rig.bringUp());

  // a restart of the core: stop, then start
  rig.controller->release();
  QTest::qWait(60);
  QCOMPARE(rig.controller->status().state, TunnelState::Listening);
  rig.controller->acquire();
  QTest::qWait(250);
  QCOMPARE(rig.controller->status().state, TunnelState::Listening);
  QCOMPARE(rig.processes.size(), size_t(1));
  QCOMPARE(rig.process()->terminateCalls, 0);
}

void TunnelControllerTests::release_stopsTheTunnelAfterTheLinger()
{
  ControllerRig rig(m_dir, kConfig, 80);
  rig.turnOn();
  QVERIFY(rig.bringUp());
  // the object of the process goes when the loop runs: what happens to it is seen through its signal
  QSignalSpy finished(rig.process(), &ITunnelProcess::finished);

  rig.controller->release();
  QTRY_COMPARE_WITH_TIMEOUT(rig.controller->status().state, TunnelState::Idle, 3000);
  QCOMPARE(finished.count(), 1);
}

void TunnelControllerTests::release_leavesAFailedTunnelAsItIsSoThatTheReasonStaysOnTheScreen()
{
  ControllerRig rig(m_dir, kConfig, 50);
  rig.turnOn();
  Settings::setValue(Settings::Security::TlsEnabled, false);
  rig.controller->acquire();
  QCOMPARE(rig.controller->status().state, TunnelState::Failed);

  // the core that waited for the tunnel is stopped: it lets the tunnel go, and the reason is still there afterwards
  rig.controller->release();
  QTest::qWait(250);
  QCOMPARE(rig.controller->status().state, TunnelState::Failed);
  QCOMPARE(rig.controller->status().failure, TunnelFailure::TlsRequired);
}

void TunnelControllerTests::aSwitchToTheServer_stopsTheTunnelAtOnce()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  QVERIFY(rig.bringUp());

  rig.controller->setClientMode(false);
  QCOMPARE(rig.controller->status().state, TunnelState::Idle);
  QCOMPARE(rig.process()->terminateCalls, 1);
  QVERIFY(!rig.controller->isRequired());
}

void TunnelControllerTests::aBusyPort_isWrittenAsTheActivePortAndRemovedAtTheEnd()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  rig.prober->preferredFree = false;
  Settings::setValue(Settings::Client::RemoteHost, QStringLiteral("10.0.0.5"));

  rig.controller->acquire();
  QCOMPARE(Settings::value(Settings::Tunnel::ActivePort).toInt(), kFree);
  // the core reads this one with the address that it dials: the tunnel and nothing else
  QCOMPARE(effectiveRemoteHost(), QStringLiteral("127.0.0.1:%1").arg(kFree));
  // the saved address is untouched
  QCOMPARE(Settings::textValue(Settings::Client::RemoteHost), QStringLiteral("10.0.0.5"));

  rig.controller->setClientMode(false);
  QVERIFY(
      !Settings::value(Settings::Tunnel::ActivePort).isValid() ||
      Settings::value(Settings::Tunnel::ActivePort).toInt() == 0
  );
  QVERIFY(!readAll(rig.file).contains("activePort"));
  QCOMPARE(Settings::textValue(Settings::Client::RemoteHost), QStringLiteral("10.0.0.5"));
}

void TunnelControllerTests::thePortThatWasAsked_isNotWrittenAsAnActivePort()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  QVERIFY(rig.bringUp());

  QVERIFY(!readAll(rig.file).contains("activePort"));
  QCOMPARE(effectiveRemoteHost(), QStringLiteral("127.0.0.1:24800"));
}

void TunnelControllerTests::aStaleActivePort_isClearedWhenTheControllerIsMade()
{
  const auto config = kConfig + QByteArray("\n[tunnel]\nenabled=true\nhostname=mbp.example.com\nactivePort=51823\n");
  ControllerRig rig(m_dir, config);
  QVERIFY(!readAll(rig.file).contains("activePort"));
  QCOMPARE(effectiveRemoteHost(), QStringLiteral("127.0.0.1:24800"));
}

void TunnelControllerTests::aStartThatCannotHappen_isToldAsFailed_andTheTunnelStaysRequired()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  Settings::setValue(Settings::Security::TlsEnabled, false);
  QSignalSpy failed(rig.controller.get(), &ITunnelGate::failed);

  QVERIFY(rig.controller->isRequired());
  rig.controller->acquire();
  QCOMPARE(failed.count(), 1);
  QCOMPARE(failed.takeFirst().at(0).value<TunnelFailure>(), TunnelFailure::TlsRequired);
  QVERIFY(rig.processes.empty());
  QVERIFY(rig.controller->isRequired());
  QVERIFY(!rig.controller->isListening());
  QCOMPARE(rig.controller->status().state, TunnelState::Failed);
}

void TunnelControllerTests::shutdown_endsTheProcess()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  QVERIFY(rig.bringUp());
  QSignalSpy finished(rig.process(), &ITunnelProcess::finished);

  rig.controller->release();
  rig.controller->shutdown();
  QCOMPARE(rig.controller->status().state, TunnelState::Idle);
  QCOMPARE(finished.count(), 1);
  // the linger that was running does not start anything later
  QTest::qWait(150);
  QCOMPARE(rig.processes.size(), size_t(1));
}

void TunnelControllerTests::theLog_isForwardedWithoutTheToken()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  rig.controller->acquire();
  rig.process()->write(QStringLiteral("ERR the secret is %1").arg(kSecret));

  QVERIFY(std::ranges::any_of(rig.logs, [](const QString &line) {
    return line.contains(QStringLiteral("Starting:"));
  }));
  QVERIFY(std::ranges::any_of(rig.logs, [](const QString &line) {
    return line.contains(QString::fromLatin1(SecretRedactor::kMask));
  }));
  for (const auto &line : rig.logs) {
    QVERIFY2(!line.contains(kSecret), qPrintable(line));
    QVERIFY2(!line.contains(kClientId), qPrintable(line));
  }
}

void TunnelControllerTests::theServerConnecting_clearsAProblemOfTheConnection()
{
  ControllerRig rig(m_dir);
  rig.turnOn();
  QVERIFY(rig.bringUp());
  rig.process()->write(QStringLiteral("ERR failed to connect to origin error=\"websocket: bad handshake\""));
  QCOMPARE(rig.controller->status().failure, TunnelFailure::HandshakeRejected);

  rig.controller->notifyServerConnected();
  QCOMPARE(rig.controller->status().failure, TunnelFailure::None);
}

QTEST_MAIN(TunnelControllerTests)
