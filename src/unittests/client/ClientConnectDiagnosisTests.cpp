/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientConnectDiagnosisTests.h"

#include "../deskflow/MockPlatformComputer.h"
#include "base/Event.h"
#include "base/EventQueue.h"
#include "base/IEventQueue.h"
#include "base/ILogOutputter.h"
#include "client/Client.h"
#include "common/ConnectionDiagnosis.h"
#include "common/Enums.h"
#include "common/Settings.h"
#include "deskflow/Computer.h"
#include "net/IDataSocket.h"
#include "net/ISocketFactory.h"
#include "net/NetworkAddress.h"
#include "net/SocketException.h"
#include "net/SocketMultiplexer.h"
#include "net/TCPSocketFactory.h"

#include <QTcpServer>
#include <QTest>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using deskflow::EventTypes;
using deskflow::diagnosis::FailureKind;

namespace {

// no timer ever fires by itself here: a test fires the connect timer by hand
class TestEventQueue : public IEventQueue
{
public:
  ~TestEventQueue() override
  {
    for (const auto &event : m_addedEvents) {
      Event::deleteData(event);
    }
  }

  int loop() override
  {
    return 0;
  }

  void adoptBuffer(IEventQueueBuffer *) override
  {
  }

  bool getEvent(Event &, double = -1.0) override
  {
    return false;
  }

  // like the real queue: a handler for the type, else the handler for every type of that target
  bool dispatchEvent(const Event &event) override
  {
    auto handler = m_handlers.find({event.getType(), event.getTarget()});
    if (handler == m_handlers.end()) {
      handler = m_handlers.find({EventTypes::Unknown, event.getTarget()});
    }
    if (handler == m_handlers.end()) {
      return false;
    }
    const auto callback = handler->second;
    callback(event);
    return true;
  }

  void addEvent(Event &&event) override
  {
    m_addedEvents.emplace_back(std::move(event));
  }

  EventQueueTimer *newTimer(double, void *) override
  {
    return timer();
  }

  EventQueueTimer *newOneShotTimer(double seconds, void *) override
  {
    m_requestedSeconds.push_back(seconds);
    return timer();
  }

  void deleteTimer(EventQueueTimer *) override
  {
  }

  void addHandler(EventTypes type, void *target, const EventHandler &handler) override
  {
    m_handlers[{type, target}] = handler;
  }

  void removeHandler(EventTypes type, void *target) override
  {
    m_handlers.erase({type, target});
  }

  void removeHandlers(void *target) override
  {
    std::erase_if(m_handlers, [target](const auto &entry) { return entry.first.second == target; });
  }

  void waitForReady() const override
  {
  }

  void *getSystemTarget() override
  {
    return this;
  }

  EventQueueTimer *timer()
  {
    return reinterpret_cast<EventQueueTimer *>(&m_timerStorage);
  }

  const std::vector<double> &requestedSeconds() const
  {
    return m_requestedSeconds;
  }

  const std::vector<Event> &addedEvents() const
  {
    return m_addedEvents;
  }

  // the text of the last "connection failed" event the client queued
  QString lastFailureText(void *clientTarget) const
  {
    for (auto it = m_addedEvents.rbegin(); it != m_addedEvents.rend(); ++it) {
      if (it->getType() == EventTypes::ClientConnectionFailed && it->getTarget() == clientTarget) {
        return static_cast<const Client::FailInfo *>(it->getData())->m_what;
      }
    }
    return {};
  }

private:
  using Key = std::pair<EventTypes, void *>;

  int m_timerStorage = 0;
  std::map<Key, EventHandler> m_handlers;
  std::vector<double> m_requestedSeconds;
  std::vector<Event> m_addedEvents;
};

class FakeSocket : public IDataSocket
{
public:
  FakeSocket(const IEventQueue *events, std::function<void()> onConnect, std::string *input)
      : IDataSocket(events),
        m_onConnect(std::move(onConnect)),
        m_input(input)
  {
  }

  void bind(const NetworkAddress &) override
  {
  }

  void close() override
  {
  }

  void *getEventTarget() const override
  {
    return const_cast<FakeSocket *>(this);
  }

  void connect(const NetworkAddress &) override
  {
    if (m_onConnect) {
      m_onConnect();
    }
  }

  uint32_t read(void *buffer, uint32_t size) override
  {
    const auto count = std::min<size_t>(size, m_input->size());
    if (buffer != nullptr) {
      std::copy_n(m_input->data(), count, static_cast<char *>(buffer));
    }
    m_input->erase(0, count);
    return static_cast<uint32_t>(count);
  }

  void write(const void *, uint32_t) override
  {
  }

  void flush() override
  {
  }

  void shutdownInput() override
  {
  }

  void shutdownOutput() override
  {
  }

  bool isReady() const override
  {
    return !m_input->empty();
  }

  uint32_t getSize() const override
  {
    return static_cast<uint32_t>(m_input->size());
  }

  bool isFatal() const override
  {
    return false;
  }

private:
  std::function<void()> m_onConnect;
  std::string *m_input;
};

// shared with the factory the client owns, so a test can reach the socket it created
struct Wire
{
  FakeSocket *socket = nullptr;
  std::function<void()> onConnect;
  std::string input;
};

class FakeSocketFactory : public ISocketFactory
{
public:
  FakeSocketFactory(IEventQueue *events, Wire *wire) : m_events(events), m_wire(wire)
  {
  }

  IDataSocket *create(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    m_wire->socket = new FakeSocket(m_events, m_wire->onConnect, &m_wire->input);
    return m_wire->socket;
  }

  IListenSocket *createListen(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    return nullptr;
  }

private:
  IEventQueue *m_events;
  Wire *m_wire;
};

class CapturingOutputter : public ILogOutputter
{
public:
  void open(const QString &) override
  {
  }

  void close() override
  {
  }

  bool write(LogLevel::Level level, const QString &message) override
  {
    if (level == LogLevel::Level::Warning || level == LogLevel::Level::Info) {
      m_lines.append(message);
    }
    return false;
  }

  int count(const QString &text) const
  {
    return static_cast<int>(std::ranges::count_if(m_lines, [&](const QString &line) { return line.contains(text); }));
  }

  bool has(const QString &text) const
  {
    return count(text) > 0;
  }

  QString dump() const
  {
    return m_lines.join(QLatin1Char('\n'));
  }

private:
  QStringList m_lines;
};

// everything a test needs around one client that talks to 10.0.0.5:24800 through a fake socket
struct Scenario
{
  explicit Scenario(bool tls, const std::string &host = "10.0.0.5")
  {
    Settings::setValue(Settings::Security::TlsEnabled, tls);
    computer = std::make_unique<deskflow::Computer>(new MockPlatformComputer(&events), &events);
    client = std::make_unique<Client>(
        &events, "desk", NetworkAddress(host, 24800), new FakeSocketFactory(&events, &wire), computer.get()
    );
  }

  ~Scenario()
  {
    client.reset();
  }

  void connect()
  {
    client->connect();
  }

  // an event as if the socket had sent it
  void fromSocket(EventTypes type, void *data = nullptr)
  {
    QVERIFY(wire.socket != nullptr);
    events.dispatchEvent(Event(type, wire.socket->getEventTarget(), data));
  }

  void fireTimer()
  {
    events.dispatchEvent(Event(EventTypes::Timer, events.timer()));
  }

  QString failureText() const
  {
    return events.lastFailureText(client->getEventTarget());
  }

  TestEventQueue events;
  Wire wire;
  std::unique_ptr<deskflow::Computer> computer;
  std::unique_ptr<Client> client;
};

void *secureFailure(FailureKind kind)
{
  auto *data = static_cast<FailureKind *>(std::malloc(sizeof(FailureKind)));
  *data = kind;
  return data;
}

// a framed greeting as a server sends it
std::string framedHello(const std::string &name, int major, int minor)
{
  std::string payload = name;
  payload.push_back(static_cast<char>(major >> 8));
  payload.push_back(static_cast<char>(major & 0xff));
  payload.push_back(static_cast<char>(minor >> 8));
  payload.push_back(static_cast<char>(minor & 0xff));
  std::string frame;
  frame.push_back(0);
  frame.push_back(0);
  frame.push_back(0);
  frame.push_back(static_cast<char>(payload.size()));
  return frame + payload;
}

} // namespace

void ClientConnectDiagnosisTests::initTestCase()
{
  m_arch.init();
  m_log.setFilter(LogLevel::Level::Debug);
  Settings::setSettingsFile(QStringLiteral("tmp/test/ClientConnectDiagnosis.conf"));
  Settings::setStateFile(QStringLiteral("tmp/test/ClientConnectDiagnosis.state"));
}

void ClientConnectDiagnosisTests::init()
{
  Settings::setValue(Settings::Client::ConnectTimeout);
  Settings::setValue(Settings::Core::Interface);
}

void ClientConnectDiagnosisTests::cleanup()
{
  Settings::setValue(Settings::Client::ConnectTimeout);
}

void ClientConnectDiagnosisTests::connect_withoutTheSetting_waitsTwoSeconds()
{
  Scenario scenario(false);
  scenario.connect();

  QCOMPARE(scenario.events.requestedSeconds(), std::vector<double>{2.0});
}

void ClientConnectDiagnosisTests::connect_withTheSetting_waitsThatLong()
{
  Settings::setValue(Settings::Client::ConnectTimeout, 7.5);
  Scenario scenario(false);
  scenario.connect();

  QCOMPARE(scenario.events.requestedSeconds(), std::vector<double>{7.5});

  CapturingOutputter log;
  m_log.insert(&log, true);
  scenario.fireTimer();
  m_log.remove(&log);
  QVERIFY2(log.has(QStringLiteral("within 7.5 s")), qPrintable(log.dump()));
}

void ClientConnectDiagnosisTests::connect_withInvalidSetting_waitsTwoSecondsAndSaysSoOnce()
{
  Settings::setValue(Settings::Client::ConnectTimeout, 0.2);
  CapturingOutputter log;
  m_log.insert(&log, true);

  Scenario scenario(false);
  scenario.connect();
  scenario.fireTimer();
  scenario.connect();
  scenario.fireTimer();
  m_log.remove(&log);

  QCOMPARE(scenario.events.requestedSeconds(), (std::vector<double>{2.0, 2.0}));
  QCOMPARE(log.count(QStringLiteral("client/connectTimeout")), 1);
  QVERIFY2(log.has(QStringLiteral("using the default of 2 s")), qPrintable(log.dump()));
}

void ClientConnectDiagnosisTests::timeout_whileConnectingTcp_namesTheTcpPhaseAndKeepsTheOldMessage()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.connect();
  scenario.fireTimer();
  m_log.remove(&log);

  QVERIFY2(
      log.has(QStringLiteral(
          "cannot reach the server 10.0.0.5:24800: the connection timed out (no answer within 2 s) "
          "while connecting (TCP)."
      )),
      qPrintable(log.dump())
  );
  QVERIFY(log.has(QStringLiteral("See docs/connecting-across-networks.md")));
  // the event the application turns into "failed to connect to server: Timed out" is the same as before
  QCOMPARE(scenario.failureText(), QStringLiteral("Timed out"));
}

void ClientConnectDiagnosisTests::timeout_afterTcpConnectedWithTls_namesTheHandshake()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(true);
  scenario.connect();
  scenario.fromSocket(EventTypes::DataSocketConnected);
  scenario.fireTimer();
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("while the secure handshake (TLS) was running")), qPrintable(log.dump()));
  QVERIFY(log.has(QStringLiteral("the TCP connection to the server 10.0.0.5:24800 was made")));
  QCOMPARE(scenario.failureText(), QStringLiteral("Timed out"));
}

void ClientConnectDiagnosisTests::timeout_afterTheSecureConnectionWithoutHello_namesTheGreeting()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(true);
  scenario.connect();
  scenario.fromSocket(EventTypes::DataSocketConnected);
  scenario.fromSocket(EventTypes::DataSocketSecureConnected);
  scenario.fireTimer();
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("while waiting for the server's greeting")), qPrintable(log.dump()));
  QCOMPARE(scenario.failureText(), QStringLiteral("Timed out"));
}

void ClientConnectDiagnosisTests::timeout_afterAnEarlierTlsFailure_reportsTheTlsFailure()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(true);
  scenario.connect();
  scenario.fromSocket(EventTypes::DataSocketConnected);
  scenario.fromSocket(EventTypes::DataSocketSecureFailed, secureFailure(FailureKind::TlsHandshakeFailed));
  scenario.fireTimer();
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("but the secure handshake (TLS) failed")), qPrintable(log.dump()));
  QVERIFY(!log.has(QStringLiteral("timed out")));
  // the attempt still ends the way it always did
  QCOMPARE(scenario.failureText(), QStringLiteral("Timed out"));
}

void ClientConnectDiagnosisTests::timeout_afterAnEarlierFingerprintRejection_reportsTheFingerprint()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(true);
  scenario.connect();
  scenario.fromSocket(EventTypes::DataSocketConnected);
  scenario.fromSocket(EventTypes::DataSocketSecureFailed, secureFailure(FailureKind::FingerprintRejected));
  scenario.fireTimer();
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("certificate fingerprint this computer does not trust")), qPrintable(log.dump()));
}

void ClientConnectDiagnosisTests::timeout_withoutTls_namesTheGreetingAfterTcp()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.connect();
  scenario.fromSocket(EventTypes::DataSocketConnected);
  scenario.fireTimer();
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("while waiting for the server's greeting")), qPrintable(log.dump()));
}

void ClientConnectDiagnosisTests::connectionFailed_refused_namesTheRefusal()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.connect();
  auto *info = new IDataSocket::ConnectionFailedInfo("Connection refused", FailureKind::Refused);
  scenario.events.dispatchEvent(Event(
      EventTypes::DataSocketConnectionFailed, scenario.wire.socket->getEventTarget(), info,
      Event::EventFlags::DontFreeData
  ));
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("the server 10.0.0.5:24800 refused the connection")), qPrintable(log.dump()));
  // the text of the failure the application logs is the one of the operating system, as before
  QCOMPARE(scenario.failureText(), QStringLiteral("Connection refused"));
}

void ClientConnectDiagnosisTests::connectionFailed_noRoute_namesTheMissingRoute()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.connect();
  auto *info = new IDataSocket::ConnectionFailedInfo("No route to host", FailureKind::NoRoute);
  scenario.events.dispatchEvent(Event(
      EventTypes::DataSocketConnectionFailed, scenario.wire.socket->getEventTarget(), info,
      Event::EventFlags::DontFreeData
  ));
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("no route to the server 10.0.0.5:24800")), qPrintable(log.dump()));
  QVERIFY(log.has(QStringLiteral("(No route to host)")));
}

void ClientConnectDiagnosisTests::connectThrows_noRoute_namesTheMissingRouteAndKeepsTheOldMessage()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.wire.onConnect = [] {
    throw SocketConnectException(QStringLiteral("No route to host"), FailureKind::NoRoute);
  };
  scenario.connect();
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("no route to the server 10.0.0.5:24800")), qPrintable(log.dump()));
  QCOMPARE(scenario.failureText(), QStringLiteral("cannot connect socket: No route to host"));
}

void ClientConnectDiagnosisTests::connectThrows_unresolvedName_namesTheName()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.wire.onConnect = [] {
    throw SocketAddressException(SocketAddressException::SocketError::NotFound, "10.0.0.5", 24800);
  };
  scenario.connect();
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("cannot find the server 10.0.0.5:24800")), qPrintable(log.dump()));
  QVERIFY(log.has(QStringLiteral("could not be resolved")));
}

void ClientConnectDiagnosisTests::hello_fromAnotherProgram_saysItIsNotDeskflow()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.connect();
  scenario.wire.input = framedHello("Foobar!", 1, 9);
  scenario.fromSocket(EventTypes::DataSocketConnected);
  scenario.fromSocket(EventTypes::StreamInputReady);
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("answered, but it is not a Deskflow server")), qPrintable(log.dump()));
  QVERIFY(log.has(QStringLiteral("Foobar!")));
  // the old warning is still there
  QVERIFY(log.has(QStringLiteral("hello back received with protocol: 'Foobar!'")));
  QCOMPARE(scenario.failureText(), QStringLiteral("got invalid hello message from server"));
}

void ClientConnectDiagnosisTests::hello_withIncompatibleMajorVersion_namesTheVersion()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.connect();
  scenario.wire.input = framedHello("Barrier", 2, 0);
  scenario.fromSocket(EventTypes::DataSocketConnected);
  scenario.fromSocket(EventTypes::StreamInputReady);
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("speaks a protocol version this computer cannot use")), qPrintable(log.dump()));
  QVERIFY(log.has(QStringLiteral("server speaks 2.0")));
  QVERIFY(log.has(QStringLiteral("server protocol version not compatible: 2.0")));
}

void ClientConnectDiagnosisTests::refuseConnection_unknownName_namesTheScreenName()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.connect();
  scenario.client->refuseConnection(
      deskflow::core::ConnectionRefusal::UnknownClient, "server refused client with our name"
  );
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("does not know this computer's name")), qPrintable(log.dump()));
}

void ClientConnectDiagnosisTests::refuseConnection_alreadyConnected_namesTheDuplicate()
{
  CapturingOutputter log;
  m_log.insert(&log, true);
  Scenario scenario(false);
  scenario.connect();
  scenario.client->refuseConnection(
      deskflow::core::ConnectionRefusal::AlreadyConnected, "server already has a connected client with our name"
  );
  m_log.remove(&log);

  QVERIFY2(log.has(QStringLiteral("a computer with this name is already connected")), qPrintable(log.dump()));
}

void ClientConnectDiagnosisTests::connect_toAClosedLocalPort_logsTheRefusalOfTheRealSocket()
{
  // a port nothing listens on: the loopback address answers a connect to it with a reset
  quint16 port = 0;
  {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    port = server.serverPort();
  }
  Settings::setValue(Settings::Security::TlsEnabled, false);

  CapturingOutputter log;
  m_log.insert(&log, true);

  EventQueue events;
  SocketMultiplexer multiplexer;
  deskflow::Computer computer(new MockPlatformComputer(&events), &events);
  Client client(
      &events, "desk", NetworkAddress("127.0.0.1", port), new TCPSocketFactory(&events, &multiplexer), &computer
  );

  QString failure;
  bool timedOut = false;
  events.addHandler(EventTypes::ClientConnectionFailed, client.getEventTarget(), [&](const Event &event) {
    auto *info = static_cast<Client::FailInfo *>(event.getData());
    failure = info->m_what;
    delete info;
    events.addEvent(Event(EventTypes::Quit));
  });
  auto *guard = events.newOneShotTimer(5.0, nullptr);
  events.addHandler(EventTypes::Timer, guard, [&](const Event &) {
    timedOut = true;
    events.addEvent(Event(EventTypes::Quit));
  });

  client.connect();
  events.loop();
  events.deleteTimer(guard);
  m_log.remove(&log);

  QVERIFY(!timedOut);
  QVERIFY2(log.has(QStringLiteral("the server 127.0.0.1:%1 refused the connection").arg(port)), qPrintable(log.dump()));
  QVERIFY2(failure.contains(QStringLiteral("refused"), Qt::CaseInsensitive), qPrintable(failure));
}

QTEST_MAIN(ClientConnectDiagnosisTests)
