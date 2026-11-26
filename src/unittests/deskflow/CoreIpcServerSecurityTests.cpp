/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcServerSecurityTests.h"

#include "common/VersionInfo.h"
#include "deskflow/ipc/CoreIpcServer.h"
#include "deskflow/ipc/InputIndicatorIpc.h"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTest>

#include <memory>

using namespace deskflow::input;

void CoreIpcServerSecurityTests::initTestCase()
{
  m_capability = QString(64, QLatin1Char('a'));
  m_socketName = QStringLiteral("/tmp/deskflow-core-security-%1").arg(QCoreApplication::applicationPid());
  qputenv(kInputIndicatorCapabilityEnvironment, m_capability.toLatin1());
  qputenv(
      kInputIndicatorGuiPidEnvironment,
      QByteArray::number(static_cast<qint64>(QCoreApplication::applicationPid()))
  );
  m_server =
      new deskflow::core::ipc::CoreIpcServer(this, std::chrono::milliseconds{200}, m_socketName);
  m_server->listen();
  QVERIFY(!m_server->fullServerName().isEmpty());
  QCOMPARE(m_server->fullServerName(), m_socketName);
  QVERIFY(m_socketName != QStringLiteral("deskflow-core"));
  QVERIFY(m_socketName != QStringLiteral("deskflow-gui"));
}

void CoreIpcServerSecurityTests::cleanupTestCase()
{
  delete m_server;
  m_server = nullptr;
  QLocalServer::removeServer(m_socketName);
}

void CoreIpcServerSecurityTests::cleanup()
{
  setInputIndicatorSubscribed(false);
  QTest::qWait(10);
}

std::unique_ptr<QLocalSocket> CoreIpcServerSecurityTests::connectClient(bool registered)
{
  auto client = std::make_unique<QLocalSocket>();
  client->connectToServer(m_socketName);
  if (!client->waitForConnected(1000)) {
    QTest::qFail("core ipc client did not connect", __FILE__, __LINE__);
    return client;
  }
  if (client->fullServerName() != m_server->fullServerName()) {
    QTest::qFail("client and server did not use the same isolated socket path", __FILE__, __LINE__);
    return client;
  }
  if (registered) {
    const auto hello = QStringLiteral("hello=%1+%2\n").arg(kVersion, kVersionGitSha).toLatin1();
    const auto response = send(client, hello);
    if (!response.startsWith("hello=")) {
      QTest::qFail("core ipc handshake did not succeed", __FILE__, __LINE__);
      return client;
    }
  }
  return client;
}

QByteArray
CoreIpcServerSecurityTests::send(std::unique_ptr<QLocalSocket> &client, const QByteArray &message)
{
  client->write(message);
  client->flush();
  if (!client->waitForReadyRead(250)) {
    QTest::qWait(20);
  }
  return client->readAll();
}

void CoreIpcServerSecurityTests::rejectsMissingRegistrationAndWrongCapability()
{
  auto unregistered = connectClient(false);
  const auto exact = QByteArray("inputIndicatorSubscription=") +
                     encodeInputIndicatorSubscription(true, m_capability).toLatin1() +
                     QByteArray("\n");
  send(unregistered, exact);
  QVERIFY(!isInputIndicatorSubscribed());

  auto wrong = connectClient();
  send(
      wrong,
      QByteArray("inputIndicatorSubscription=") +
          encodeInputIndicatorSubscription(true, QString(64, QLatin1Char('b'))).toLatin1() +
          QByteArray("\n")
  );
  QVERIFY(!isInputIndicatorSubscribed());
}

void CoreIpcServerSecurityTests::exactCapabilityAuthorizesAndDisconnectRemovesIt()
{
  auto client = connectClient();
  const auto response = send(
      client,
      QByteArray("inputIndicatorSubscription=") +
          encodeInputIndicatorSubscription(true, m_capability).toLatin1() + QByteArray("\n")
  );

  QVERIFY(response.contains("ok"));
  QVERIFY(isInputIndicatorSubscribed());
  client->disconnectFromServer();
  QTRY_VERIFY(!isInputIndicatorSubscribed());
}

void CoreIpcServerSecurityTests::malformedRawSubscriptionNeverAuthorizes()
{
  const QList<QByteArray> malformed{
      QByteArray("inputIndicatorSubscription=") +
          encodeInputIndicatorSubscription(true, m_capability).toLatin1() + QByteArray("=garbage\n"),
      QByteArray("inputIndicatorSubscription =") +
          encodeInputIndicatorSubscription(true, m_capability).toLatin1() + QByteArray("\n"),
      QByteArray("inputIndicatorSubscription=\n"),
      QByteArray("inputIndicatorSubscription=") +
          encodeInputIndicatorSubscription(true, m_capability).toLatin1() + QByteArray("\r\n"),
      QByteArray("inputIndicatorSubscription=1:") +
          m_capability.toLatin1() + QByteArray("\0ignored\n", 10),
      QByteArray("inputIndicatorSubscription=") + QByteArray(300, 'x') + QByteArray("\n"),
      QByteArray("inputIndicatorSubscription=") + QByteArray(5000, 'x'),
  };

  for (const auto &message : malformed) {
    auto client = connectClient();
    send(client, message);
    QTRY_VERIFY(!isInputIndicatorSubscribed());
  }
}

void CoreIpcServerSecurityTests::malformedRawMessageRemovesExistingAuthorization()
{
  auto client = connectClient();
  send(
      client,
      QByteArray("inputIndicatorSubscription=") +
          encodeInputIndicatorSubscription(true, m_capability).toLatin1() + QByteArray("\n")
  );
  QVERIFY(isInputIndicatorSubscribed());

  send(
      client,
      QByteArray("inputIndicatorSubscription=") +
          encodeInputIndicatorSubscription(true, m_capability).toLatin1() + QByteArray("=extra\n")
  );
  QTRY_VERIFY(!isInputIndicatorSubscribed());
}

void CoreIpcServerSecurityTests::ordinaryPayloadsContainingIndicatorNamesStayConnected()
{
  auto client = connectClient();
  const QList<QByteArray> ordinary{
      QByteArray("connectedClients=inputIndicatorLocal\n"),
      QByteArray("unrecognisedClient=foo-inputIndicatorActivity-bar\n"),
      QByteArray("configFile=/tmp/inputIndicatorSubscription/config\n"),
      QByteArray("connectedClients=inputIndicatorLocal=extra\n"),
      QByteArray("unrecognisedClient=\xed\x95\x9c-inputIndicatorActivity\n"),
      QByteArray("unrecognisedClient=\0inputIndicatorLocal\n", 43),
  };

  for (const auto &message : ordinary) {
    send(client, message);
    QCOMPARE(client->state(), QLocalSocket::ConnectedState);
  }
  QVERIFY(!isInputIndicatorSubscribed());
}

void CoreIpcServerSecurityTests::subscriptionExpiresAndCanRenew()
{
  auto client = connectClient();
  const auto subscribe = QByteArray("inputIndicatorSubscription=") +
                         encodeInputIndicatorSubscription(true, m_capability).toLatin1() +
                         QByteArray("\n");
  send(client, subscribe);
  QVERIFY(isInputIndicatorSubscribed());

  QTRY_VERIFY_WITH_TIMEOUT(!isInputIndicatorSubscribed(), 500);
  send(client, subscribe);
  QTRY_VERIFY_WITH_TIMEOUT(isInputIndicatorSubscribed(), 500);
  client->disconnectFromServer();
  QTRY_VERIFY(!isInputIndicatorSubscribed());
}

void CoreIpcServerSecurityTests::disconnectFromOnePeerKeepsOtherSubscription()
{
  auto first = connectClient();
  auto second = connectClient();
  const auto subscribe = QByteArray("inputIndicatorSubscription=") +
                         encodeInputIndicatorSubscription(true, m_capability).toLatin1() +
                         QByteArray("\n");
  send(first, subscribe);
  send(second, subscribe);
  QVERIFY(isInputIndicatorSubscribed());

  first->disconnectFromServer();
  QTest::qWait(30);
  QVERIFY(isInputIndicatorSubscribed());
  second->disconnectFromServer();
  QTRY_VERIFY(!isInputIndicatorSubscribed());
}

QTEST_MAIN(CoreIpcServerSecurityTests)
