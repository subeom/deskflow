/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServiceQuitTests.h"

#include "FakeDaemonIpcClient.h"
#include "gui/core/ServiceQuit.h"

#include <QElapsedTimer>

using namespace deskflow::gui;

void ServiceQuitTests::aConnectedClient_sendsTheStopAndFlushesIt()
{
  FakeDaemonIpcClient client;
  client.setConnected(true);

  QCOMPARE(deliverStopToService(client, 1000, 1000), StopDelivery::Delivered);
  QCOMPARE(client.sent, QStringList{QStringLiteral("stop")});
  QCOMPARE(client.flushCalls, 1);
  // it was connected: nothing is connected again
  QCOMPARE(client.connectCalls, 0);
}

void ServiceQuitTests::aClientThatConnectsWithinTheWait_sendsTheStopAfterTheConnection()
{
  FakeDaemonIpcClient client;
  client.behaviour = FakeDaemonIpcClient::Behaviour::ConnectsLater;
  client.connectDelayMs = 150;

  QElapsedTimer timer;
  timer.start();
  QCOMPARE(deliverStopToService(client, 2000, 1000), StopDelivery::Delivered);

  QCOMPARE(client.connectCalls, 1);
  QCOMPARE(client.sent, QStringList{QStringLiteral("stop")});
  QCOMPARE(client.droppedMessages, 0);
  QCOMPARE(client.flushCalls, 1);
  // it waited for the connection, and not for the whole time
  QVERIFY(timer.elapsed() >= 100);
  QVERIFY(timer.elapsed() < 1500);

  // a client that connects at once (the connected signal comes inside connectToServer)
  FakeDaemonIpcClient quick;
  QCOMPARE(deliverStopToService(quick, 1000, 1000), StopDelivery::Delivered);
  QCOMPARE(quick.sent, QStringList{QStringLiteral("stop")});
}

void ServiceQuitTests::aClientThatNeverConnects_givesUpAfterTheWait_andSendsNothing()
{
  FakeDaemonIpcClient client;
  client.behaviour = FakeDaemonIpcClient::Behaviour::NeverConnects;

  QElapsedTimer timer;
  timer.start();
  QCOMPARE(deliverStopToService(client, 300, 1000), StopDelivery::NotConnected);

  // bounded: about the wait, never more than a little over it
  QVERIFY(timer.elapsed() >= 250);
  QVERIFY(timer.elapsed() < 2000);
  QCOMPARE(client.connectCalls, 1);
  QVERIFY(client.sent.isEmpty());
  QCOMPARE(client.droppedMessages, 0);
  QCOMPARE(client.flushCalls, 0);
}

void ServiceQuitTests::aConnectionThatFailsAtOnce_doesNotWaitForTheWholeTime()
{
  FakeDaemonIpcClient client;
  client.behaviour = FakeDaemonIpcClient::Behaviour::FailsAtOnce;

  QElapsedTimer timer;
  timer.start();
  QCOMPARE(deliverStopToService(client, 5000, 1000), StopDelivery::NotConnected);
  QVERIFY(timer.elapsed() < 2000);
  QVERIFY(client.sent.isEmpty());
}

void ServiceQuitTests::aStopThatCannotBeFlushed_isReported()
{
  FakeDaemonIpcClient client;
  client.setConnected(true);
  client.flushResult = false;

  QCOMPARE(deliverStopToService(client, 1000, 1000), StopDelivery::NotFlushed);
  QCOMPARE(client.sent, QStringList{QStringLiteral("stop")});
}

// the warning says what remains, so that whoever reads the log knows what to do
void ServiceQuitTests::theWarning_namesWhatRemains()
{
  QVERIFY(stopDeliveryWarning(StopDelivery::Delivered).isEmpty());
  for (const auto result : {StopDelivery::NotConnected, StopDelivery::NotFlushed}) {
    const auto text = stopDeliveryWarning(result);
    QVERIFY(text.contains(QStringLiteral("keeps running")));
    QVERIFY(text.contains(QStringLiteral("boot")));
  }
}

QTEST_MAIN(ServiceQuitTests)
