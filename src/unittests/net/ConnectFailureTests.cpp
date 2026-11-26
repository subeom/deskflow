/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ConnectFailureTests.h"

#include "arch/ArchException.h"
#include "base/EventQueue.h"
#include "net/ConnectFailureKind.h"
#include "net/IDataSocket.h"
#include "net/NetworkAddress.h"
#include "net/SocketException.h"
#include "net/SocketMultiplexer.h"
#include "net/TCPSocket.h"

#include <QTcpServer>
#include <QTest>

#include <optional>

using deskflow::diagnosis::FailureKind;
using deskflow::net::failureKindOf;

namespace {

// a port nothing listens on: the loopback address answers a connect to it with a reset
quint16 closedLocalPort()
{
  QTcpServer server;
  if (!server.listen(QHostAddress::LocalHost, 0)) {
    return 0;
  }
  const auto port = server.serverPort();
  server.close();
  return port;
}

} // namespace

void ConnectFailureTests::initTestCase()
{
  m_arch.init();
}

void ConnectFailureTests::failureKindOf_archErrors_data()
{
  QTest::addColumn<int>("error");
  QTest::addColumn<int>("kind");

  const auto row = [](const char *name, int error, FailureKind kind) {
    QTest::newRow(name) << error << static_cast<int>(kind);
  };
  enum
  {
    Refused,
    TimedOut,
    NoRoute,
    Disconnected,
    Access,
    Support,
    Plain
  };
  row("refused", Refused, FailureKind::Refused);
  row("timed-out", TimedOut, FailureKind::TimedOut);
  row("no-route", NoRoute, FailureKind::NoRoute);
  row("reset", Disconnected, FailureKind::Reset);
  row("access-denied", Access, FailureKind::AccessDenied);
  row("other-typed", Support, FailureKind::Unknown);
  row("untyped", Plain, FailureKind::Unknown);
}

void ConnectFailureTests::failureKindOf_archErrors()
{
  QFETCH(int, error);
  QFETCH(int, kind);

  const auto classify = [error]() {
    switch (error) {
    case 0:
      return failureKindOf(ArchNetworkConnectionRefusedException("Connection refused"));
    case 1:
      return failureKindOf(ArchNetworkTimedOutException("Operation timed out"));
    case 2:
      return failureKindOf(ArchNetworkNoRouteException("No route to host"));
    case 3:
      return failureKindOf(ArchNetworkDisconnectedException("Connection reset by peer"));
    case 4:
      return failureKindOf(ArchNetworkAccessException("Operation not permitted"));
    case 5:
      return failureKindOf(ArchNetworkSupportException("Protocol not supported"));
    default:
      return failureKindOf(ArchNetworkException("something else"));
    }
  };

  QCOMPARE(static_cast<int>(classify()), kind);
}

void ConnectFailureTests::failureKindOf_addressErrors_data()
{
  using Error = SocketAddressException::SocketError;
  QTest::addColumn<int>("error");
  QTest::addColumn<int>("kind");
  QTest::newRow("not-found") << static_cast<int>(Error::NotFound) << static_cast<int>(FailureKind::DnsFailure);
  QTest::newRow("no-address") << static_cast<int>(Error::NoAddress) << static_cast<int>(FailureKind::DnsFailure);
  QTest::newRow("unsupported") << static_cast<int>(Error::Unsupported) << static_cast<int>(FailureKind::DnsFailure);
  QTest::newRow("bad-port") << static_cast<int>(Error::BadPort) << static_cast<int>(FailureKind::Unknown);
  QTest::newRow("unknown") << static_cast<int>(Error::Unknown) << static_cast<int>(FailureKind::Unknown);
}

void ConnectFailureTests::failureKindOf_addressErrors()
{
  QFETCH(int, error);
  QFETCH(int, kind);

  const SocketAddressException exception(static_cast<SocketAddressException::SocketError>(error), "desk", 24800);
  QCOMPARE(static_cast<int>(failureKindOf(exception)), kind);
}

void ConnectFailureTests::socketConnectException_carriesKindAndKeepsTheMessage()
{
  const SocketConnectException known(QStringLiteral("No route to host"), FailureKind::NoRoute);
  QCOMPARE(known.kind(), FailureKind::NoRoute);
  QCOMPARE(known.detail(), std::string("No route to host"));
  // the text the log has always shown for this error
  QCOMPARE(QString::fromUtf8(known.what()), QStringLiteral("cannot connect socket: No route to host"));

  const SocketConnectException unknown(QStringLiteral("odd"));
  QCOMPARE(unknown.kind(), FailureKind::Unknown);
}

void ConnectFailureTests::tcpSocket_closedLocalPort_reportsRefused()
{
  const auto port = closedLocalPort();
  QVERIFY(port != 0);

  EventQueue events;
  SocketMultiplexer multiplexer;
  TCPSocket socket(&events, &multiplexer, IArchNetwork::AddressFamily::INet);

  std::optional<FailureKind> kind;
  std::string what;
  bool connected = false;
  bool timedOut = false;
  events.addHandler(EventTypes::DataSocketConnectionFailed, socket.getEventTarget(), [&](const Event &event) {
    auto *info = static_cast<IDataSocket::ConnectionFailedInfo *>(event.getData());
    kind = info->m_kind;
    what = info->m_what;
    delete info;
    events.addEvent(Event(EventTypes::Quit));
  });
  events.addHandler(EventTypes::DataSocketConnected, socket.getEventTarget(), [&](const Event &) {
    connected = true;
    events.addEvent(Event(EventTypes::Quit));
  });
  auto *guard = events.newOneShotTimer(5.0, nullptr);
  events.addHandler(EventTypes::Timer, guard, [&](const Event &) {
    timedOut = true;
    events.addEvent(Event(EventTypes::Quit));
  });

  NetworkAddress address("127.0.0.1", port);
  address.resolve();
  try {
    socket.connect(address);
    events.loop();
  } catch (const SocketConnectException &error) {
    // some systems refuse a connect to a loopback port at once instead of through the event
    kind = error.kind();
    what = error.what();
  }
  events.deleteTimer(guard);

  QVERIFY(!connected);
  QVERIFY(!timedOut);
  QVERIFY2(kind.has_value(), "no failure was reported");
  QVERIFY2(*kind == FailureKind::Refused, what.c_str());
}

QTEST_MAIN(ConnectFailureTests)
