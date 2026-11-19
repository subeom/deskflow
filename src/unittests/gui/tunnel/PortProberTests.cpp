/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "PortProberTests.h"

#include "gui/tunnel/QtPortProber.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

using namespace deskflow::tunnel;

namespace {

// Only ports that the system hands out: 24800 is never touched by a test
int freePort()
{
  QtPortProber prober;
  return prober.findFreePort();
}

} // namespace

void PortProberTests::canListen_isTrueForAFreePortAndFalseForABusyOrInvalidOne()
{
  QtPortProber prober;
  QTcpServer holder;
  QVERIFY(holder.listen(QHostAddress::LocalHost, 0));
  const int busy = holder.serverPort();

  QVERIFY(!prober.canListen(busy));
  QVERIFY(!prober.canListen(0));
  QVERIFY(!prober.canListen(-1));
  QVERIFY(!prober.canListen(65536));

  holder.close();
  QVERIFY(prober.canListen(busy));
}

void PortProberTests::canListen_leavesThePortFree()
{
  QtPortProber prober;
  const int port = freePort();
  QVERIFY(port > 0);

  QVERIFY(prober.canListen(port));
  QTcpServer later;
  QVERIFY(later.listen(QHostAddress::LocalHost, static_cast<quint16>(port)));
}

void PortProberTests::findFreePort_givesAPortThatCanBeListenedOn()
{
  QtPortProber prober;
  const int port = prober.findFreePort();
  QVERIFY(port > 1023);
  QVERIFY(port <= 65535);
  QVERIFY(prober.canListen(port));
}

void PortProberTests::probeOpen_tellsTrueForAPortThatListens()
{
  QtPortProber prober;
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));

  int answers = 0;
  bool open = false;
  prober.probeOpen(server.serverPort(), 2000, [&](bool result) {
    ++answers;
    open = result;
  });
  QTRY_COMPARE_WITH_TIMEOUT(answers, 1, 3000);
  QVERIFY(open);
}

void PortProberTests::probeOpen_tellsFalseForAPortThatDoesNotListen()
{
  QtPortProber prober;
  const int port = freePort();

  int answers = 0;
  bool open = true;
  prober.probeOpen(port, 2000, [&](bool result) {
    ++answers;
    open = result;
  });
  QTRY_COMPARE_WITH_TIMEOUT(answers, 1, 3000);
  QVERIFY(!open);
}

void PortProberTests::probeOpen_tellsOnceAndSendsNothing()
{
  QtPortProber prober;
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  QTcpSocket *accepted = nullptr;
  connect(&server, &QTcpServer::newConnection, this, [&] { accepted = server.nextPendingConnection(); });

  int answers = 0;
  prober.probeOpen(server.serverPort(), 2000, [&](bool) { ++answers; });
  QTRY_COMPARE_WITH_TIMEOUT(answers, 1, 3000);
  QTRY_VERIFY_WITH_TIMEOUT(accepted != nullptr, 3000);

  // the probe closes at once and has not written a byte (a Deskflow server would see garbage otherwise)
  QTRY_VERIFY_WITH_TIMEOUT(
      accepted->state() == QAbstractSocket::UnconnectedState || accepted->bytesAvailable() == 0, 2000
  );
  QCOMPARE(accepted->bytesAvailable(), 0);
  QTest::qWait(200);
  QCOMPARE(answers, 1);
  delete accepted;
}

QTEST_MAIN(PortProberTests)
