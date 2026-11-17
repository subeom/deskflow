/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientAppConnectTests.h"

#include "../deskflow/MockPlatformComputer.h"
#include "base/Event.h"
#include "base/EventQueue.h"
#include "base/ILogOutputter.h"
#include "common/Settings.h"
#include "deskflow/ClientApp.h"
#include "deskflow/Computer.h"
#include "net/SocketMultiplexer.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTcpServer>
#include <QTest>

#include <algorithm>
#include <functional>
#include <memory>

#ifdef Q_OS_WIN
#include "common/Constants.h"

#include <windows.h>
#endif

using deskflow::EventTypes;

namespace {

#ifdef Q_OS_WIN
// The application object of Windows has an event thread that opens the machine-wide event that closes Deskflow, the one
// that the GUI signals to stop a core (kCloseEventName). It is made when the application is, and the process stops if
// it cannot be: when a Deskflow runs on the computer it owns the event, and a test that opens it as well would be
// stopped with that one. So the tests run only when the event is theirs to make.
bool closeEventIsFree(QString &why)
{
  SetLastError(0);
  HANDLE event = CreateEventW(nullptr, TRUE, FALSE, kCloseEventName);
  const auto error = GetLastError();
  if (event == nullptr) {
    why = QStringLiteral("the machine-wide close event of Deskflow cannot be made (Windows error %1)").arg(error);
    return false;
  }
  CloseHandle(event);
  if (error == ERROR_ALREADY_EXISTS) {
    why = QStringLiteral("a Deskflow is running on this computer and owns its machine-wide close event");
    return false;
  }
  return true;
}
#endif

// Windows does not report a connection to a port that nothing listens on at once: it sends the SYN again after the
// reset and says "refused" after about two seconds (not checked here: the tests tell the time that it took). The time
// limit of the client has to be longer than that for the refusal to be what is reported, and what is waited for is
// longer by that.
#ifdef Q_OS_WIN
constexpr double kRefusalSeconds = 3.0;
#else
constexpr double kRefusalSeconds = 0.0;
#endif
constexpr double kLongTimeLimitSeconds = 10.0;

// the application of a client, with a computer that does nothing and so touches nothing of the system; everything
// else, the addresses, the sockets and the events, is the real one
class TestClientApp : public ClientApp
{
public:
  TestClientApp(IEventQueue *events, const QString &name) : ClientApp(events, name)
  {
    setByeFunc([](int) {});
    setSocketMultiplexer(std::make_unique<SocketMultiplexer>());
  }

  deskflow::Computer *createComputer() override
  {
    return new deskflow::Computer(new MockPlatformComputer(getEvents()), getEvents());
  }
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

  bool write(LogLevel::Level, const QString &message) override
  {
    m_lines.append(message);
    return false;
  }

  int count(const QString &text) const
  {
    return static_cast<int>(std::ranges::count_if(m_lines, [&](const QString &line) { return line.contains(text); }));
  }

  // where the first line that has \p text is, or -1; searched from \p from on
  int indexOf(const QString &text, int from = 0) const
  {
    for (int i = from; i < m_lines.size(); ++i) {
      if (m_lines.at(i).contains(text)) {
        return i;
      }
    }
    return -1;
  }

  QString dump() const
  {
    return m_lines.join(QLatin1Char('\n'));
  }

private:
  QStringList m_lines;
};

// the text of the configuration file as a person writes it: the list of servers as it was typed, without quotes
QString configurationWith(const QString &remoteHost)
{
  return QStringLiteral(
             "[client]\nremoteHost=%1\n[core]\ncomputerName=test\nport=24800\n[security]\ntlsEnabled=false\n"
  )
      .arg(remoteHost);
}

quint16 closedLocalPort()
{
  QTcpServer server;
  if (!server.listen(QHostAddress::LocalHost, 0)) {
    return 0;
  }
  return server.serverPort();
}

// a file of its own for each call: the settings skip a file they are already reading
bool useConfiguration(const QString &directory, const QString &remoteHost)
{
  static int counter = 0;
  QFile file(QStringLiteral("%1/Deskflow-%2.conf").arg(directory).arg(++counter));
  if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
    return false;
  }
  file.write(configurationWith(remoteHost).toUtf8());
  file.close();
  Settings::setSettingsFile(file.fileName());
  return true;
}

// a configuration that is given as the whole text (for the keys of the tunnel and of TLS)
bool useConfigurationText(const QString &directory, const QString &text)
{
  static int counter = 0;
  QFile file(QStringLiteral("%1/Deskflow-text-%2.conf").arg(directory).arg(++counter));
  if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
    return false;
  }
  file.write(text.toUtf8());
  file.close();
  Settings::setSettingsFile(file.fileName());
  return true;
}

int g_byeCode = -1;
void recordBye(int code)
{
  g_byeCode = code;
}

// runs the events of \p events until \p done says so; false when it did not within \p limitSeconds
bool runUntil(EventQueue &events, const std::function<bool()> &done, double limitSeconds)
{
  bool reached = false;
  auto *poll = events.newTimer(0.02, nullptr);
  events.addHandler(EventTypes::Timer, poll, [&](const Event &) {
    if (!reached && done()) {
      reached = true;
      events.addEvent(Event(EventTypes::Quit));
    }
  });
  auto *guard = events.newOneShotTimer(limitSeconds, nullptr);
  events.addHandler(EventTypes::Timer, guard, [&](const Event &) { events.addEvent(Event(EventTypes::Quit)); });

  events.loop();

  events.removeHandler(EventTypes::Timer, poll);
  events.removeHandler(EventTypes::Timer, guard);
  events.deleteTimer(poll);
  events.deleteTimer(guard);
  return reached;
}

} // namespace

void ClientAppConnectTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
#ifdef Q_OS_WIN
  QString why;
  if (!closeEventIsFree(why)) {
    QSKIP(qPrintable(QStringLiteral("not run: %1. Stop Deskflow to run these tests.").arg(why)));
  }
#endif
  m_arch.init();
  m_log.setFilter(LogLevel::Level::Debug);
}

void ClientAppConnectTests::init()
{
  QVERIFY(useConfiguration(m_dir.path(), QString()));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
}

// writes the configuration and makes the settings read it, as the application does at its start
#define USE_CONFIGURATION(text) QVERIFY(useConfiguration(m_dir.path(), (text)))

void ClientAppConnectTests::parseArgs_listWithSpacesAndEmptyParts_keepsTheAddressesInOrder()
{
  USE_CONFIGURATION(QStringLiteral("\"10.0.0.5 ,, 100.64.1.7:24801 ,[fe80::1%en0]:24802\""));
  CapturingOutputter log;
  m_log.insert(&log, true);
  EventQueue events;
  TestClientApp app(&events, QStringLiteral("test"));

  app.parseArgs();
  m_log.remove(&log);

  QVERIFY2(log.count(QStringLiteral("configured 3 server address(es)")) == 1, qPrintable(log.dump()));
  const auto first = log.indexOf(QStringLiteral("added server address: 10.0.0.5"));
  const auto second = log.indexOf(QStringLiteral("added server address: 100.64.1.7:24801"));
  const auto third = log.indexOf(QStringLiteral("added server address: [fe80::1%en0]:24802"));
  QVERIFY2(first >= 0 && second > first && third > second, qPrintable(log.dump()));
}

// The documented way to give several servers is to type them in the file. A list that has commas and no quotes is read
// by the settings as a list of its parts, which has no text: the client has to join the parts again
void ClientAppConnectTests::parseArgs_listWrittenByHandWithoutQuotes_isRead()
{
  USE_CONFIGURATION(QStringLiteral("10.0.0.5, 100.64.1.7"));
  CapturingOutputter log;
  m_log.insert(&log, true);
  EventQueue events;
  TestClientApp app(&events, QStringLiteral("test"));

  app.parseArgs();
  m_log.remove(&log);

  QVERIFY2(log.count(QStringLiteral("configured 2 server address(es)")) == 1, qPrintable(log.dump()));
  const auto first = log.indexOf(QStringLiteral("added server address: 10.0.0.5"));
  const auto second = log.indexOf(QStringLiteral("added server address: 100.64.1.7"));
  QVERIFY2(first >= 0 && second > first, qPrintable(log.dump()));
}

void ClientAppConnectTests::parseArgs_singleAddress_isOneAddress()
{
  USE_CONFIGURATION(QStringLiteral("10.0.0.5"));
  CapturingOutputter log;
  m_log.insert(&log, true);
  EventQueue events;
  TestClientApp app(&events, QStringLiteral("test"));

  app.parseArgs();
  m_log.remove(&log);

  QVERIFY2(log.count(QStringLiteral("configured 1 server address(es)")) == 1, qPrintable(log.dump()));
}

void ClientAppConnectTests::parseArgs_tunnelOnAndUsable_dialsTheLocalAddressOnly()
{
  QVERIFY(useConfigurationText(
      m_dir.path(), QStringLiteral(
                        "[client]\nremoteHost=10.0.0.5\n[core]\ncomputerName=test\nport=24800\n"
                        "[security]\ntlsEnabled=true\n[tunnel]\nenabled=true\nhostname=mbp.example.com\n"
                    )
  ));
  CapturingOutputter log;
  m_log.insert(&log, true);
  EventQueue events;
  TestClientApp app(&events, QStringLiteral("test"));
  g_byeCode = -1;
  app.setByeFunc(&recordBye);

  app.parseArgs();
  m_log.remove(&log);

  QVERIFY2(log.count(QStringLiteral("configured 1 server address(es)")) == 1, qPrintable(log.dump()));
  QVERIFY2(log.indexOf(QStringLiteral("added server address: 127.0.0.1:24800")) >= 0, qPrintable(log.dump()));
  QCOMPARE(log.count(QStringLiteral("10.0.0.5")), 0);
  QCOMPARE(g_byeCode, -1);
}

void ClientAppConnectTests::parseArgs_tunnelOnAndNotUsable_doesNotGoToTheSavedAddress_data()
{
  QTest::addColumn<QString>("tls");
  QTest::addColumn<QString>("hostname");
  QTest::addColumn<QString>("reason");
  QTest::newRow("tls off") << "false" << "mbp.example.com" << "TLS";
  QTest::newRow("hostname empty") << "true" << "" << "hostname";
  QTest::newRow("hostname is an address") << "true" << "10.0.0.5" << "hostname";
  QTest::newRow("hostname with a scheme") << "true" << "https://mbp.example.com" << "hostname";
}

// The tunnel is on in the settings and cannot be used. The client must not connect to the saved address instead: that
// would be a connection that the user meant to make through the tunnel, made without it (and with TLS off, in the
// clear)
void ClientAppConnectTests::parseArgs_tunnelOnAndNotUsable_doesNotGoToTheSavedAddress()
{
  QFETCH(QString, tls);
  QFETCH(QString, hostname);
  QFETCH(QString, reason);

  QVERIFY(useConfigurationText(
      m_dir.path(), QStringLiteral(
                        "[client]\nremoteHost=10.0.0.5\n[core]\ncomputerName=test\nport=24800\n"
                        "[security]\ntlsEnabled=%1\n[tunnel]\nenabled=true\nhostname=\"%2\"\n"
                    )
                        .arg(tls, hostname)
  ));
  CapturingOutputter log;
  m_log.insert(&log, true);
  EventQueue events;
  TestClientApp app(&events, QStringLiteral("test"));
  g_byeCode = -1;
  app.setByeFunc(&recordBye);

  app.parseArgs();
  m_log.remove(&log);

  // no address is made, one clear line says why, and the application ends the way that a bad configuration ends it
  QCOMPARE(log.count(QStringLiteral("added server address")), 0);
  QCOMPARE(log.count(QStringLiteral("configured")), 0);
  QCOMPARE(log.count(QStringLiteral("10.0.0.5")), 0);
  QCOMPARE(log.count(QStringLiteral("Cloudflare Tunnel")), 1);
  QVERIFY2(log.indexOf(reason) >= 0, qPrintable(log.dump()));
  QCOMPARE(g_byeCode, 4);
}

// With the tunnel off or unset nothing is different: the keys of the tunnel and what TLS is do not matter
void ClientAppConnectTests::parseArgs_tunnelOffOrAbsent_isTheSavedAddressWhateverElseTheTunnelKeysHold()
{
  for (const auto &text :
       {QStringLiteral(
            "[client]\nremoteHost=10.0.0.5\n[core]\ncomputerName=test\nport=24800\n"
            "[security]\ntlsEnabled=false\n"
        ),
        QStringLiteral(
            "[client]\nremoteHost=10.0.0.5\n[core]\ncomputerName=test\nport=24800\n"
            "[security]\ntlsEnabled=false\n[tunnel]\nenabled=false\n"
            "hostname=\"not a host name\"\n"
        )}) {
    QVERIFY(useConfigurationText(m_dir.path(), text));
    CapturingOutputter log;
    m_log.insert(&log, true);
    EventQueue events;
    TestClientApp app(&events, QStringLiteral("test"));
    g_byeCode = -1;
    app.setByeFunc(&recordBye);

    app.parseArgs();
    m_log.remove(&log);

    QVERIFY2(log.count(QStringLiteral("configured 1 server address(es)")) == 1, qPrintable(log.dump()));
    QVERIFY2(log.indexOf(QStringLiteral("added server address: 10.0.0.5")) >= 0, qPrintable(log.dump()));
    QCOMPARE(log.count(QStringLiteral("Cloudflare Tunnel")), 0);
    QCOMPARE(g_byeCode, -1);
  }
}

// What a client does with "A,B" when A does not take it: it goes on to B, after the retry delay of 1 s
void ClientAppConnectTests::connect_firstAddressRefuses_triesTheSecondAtOnceAfterTheRetryDelay()
{
  const auto closedPort = closedLocalPort();
  QTcpServer second;
  QVERIFY(second.listen(QHostAddress::LocalHost, 0));
  QVERIFY(closedPort != 0);
  USE_CONFIGURATION(QStringLiteral("\"127.0.0.1:%1,127.0.0.1:%2\"").arg(closedPort).arg(second.serverPort()));
  Settings::setValue(Settings::Client::ConnectTimeout, kLongTimeLimitSeconds);

  CapturingOutputter log;
  m_log.insert(&log, true);
  EventQueue events;
  TestClientApp app(&events, QStringLiteral("test"));
  app.parseArgs();
  QElapsedTimer clock;
  clock.start();
  QVERIFY(app.startClient());

  const bool reached = runUntil(events, [&] { return second.waitForNewConnection(0); }, 6.0 + kRefusalSeconds);
  const auto elapsed = clock.elapsed();
  app.stopClient();
  m_log.remove(&log);

  QVERIFY2(reached, qPrintable(log.dump()));
  QVERIFY2(
      log.count(QStringLiteral("failed to connect to server=Connection refused, trying next server in list")) == 1,
      qPrintable(log.dump())
  );
  // the refusal is at once, the next address is tried after the retry delay of a second
  QVERIFY2(
      elapsed >= 900 && elapsed < 3000 + static_cast<qint64>(kRefusalSeconds * 1000),
      qPrintable(QString::number(elapsed))
  );
}

// A server that takes the connection and never says hello is given the time limit of the client (here 1 s), and then
// the retry delay (1 s) passes before the next address is tried: 2 s per address that does not answer
void ClientAppConnectTests::connect_firstAddressNeverGreets_triesTheSecondAfterTheTimeLimitAndTheRetryDelay()
{
  QTcpServer first;
  QTcpServer second;
  QVERIFY(first.listen(QHostAddress::LocalHost, 0));
  QVERIFY(second.listen(QHostAddress::LocalHost, 0));
  USE_CONFIGURATION(QStringLiteral("\"127.0.0.1:%1,127.0.0.1:%2\"").arg(first.serverPort()).arg(second.serverPort()));
  Settings::setValue(Settings::Client::ConnectTimeout, 1.0);

  CapturingOutputter log;
  m_log.insert(&log, true);
  EventQueue events;
  TestClientApp app(&events, QStringLiteral("test"));
  app.parseArgs();
  QElapsedTimer clock;
  clock.start();
  qint64 firstAt = -1;
  qint64 secondAt = -1;
  QVERIFY(app.startClient());

  runUntil(
      events,
      [&] {
        if (firstAt < 0 && first.waitForNewConnection(0)) {
          firstAt = clock.elapsed();
        }
        if (secondAt < 0 && second.waitForNewConnection(0)) {
          secondAt = clock.elapsed();
        }
        return secondAt >= 0;
      },
      8.0
  );
  app.stopClient();
  m_log.remove(&log);

  QVERIFY2(firstAt >= 0 && secondAt > firstAt, qPrintable(log.dump()));
  // fallback happens after the time limit, not before, and after the retry delay on top of it
  QVERIFY2(secondAt - firstAt >= 1800 && secondAt - firstAt < 4000, qPrintable(QString::number(secondAt - firstAt)));
  QVERIFY2(
      log.count(QStringLiteral("failed to connect to server=Timed out, trying next server in list")) == 1,
      qPrintable(log.dump())
  );
  QVERIFY(
      log.count(QStringLiteral("cannot reach the server")) + log.count(QStringLiteral("was made, but it timed out")) >=
      1
  );
}

// After the last address failed the round is over: that is the "failed to connect to server" line, and the client
// starts again with the first address
void ClientAppConnectTests::connect_afterAFullRoundOfFailures_startsAgainWithTheFirstAddress()
{
  const auto closedPortA = closedLocalPort();
  const auto closedPortB = closedLocalPort();
  QVERIFY(closedPortA != 0 && closedPortB != 0 && closedPortA != closedPortB);
  USE_CONFIGURATION(QStringLiteral("\"127.0.0.1:%1,127.0.0.1:%2\"").arg(closedPortA).arg(closedPortB));
  Settings::setValue(Settings::Client::ConnectTimeout, kLongTimeLimitSeconds);

  CapturingOutputter log;
  m_log.insert(&log, true);
  EventQueue events;
  TestClientApp app(&events, QStringLiteral("test"));
  app.parseArgs();
  QVERIFY(app.startClient());

  const bool reached = runUntil(
      events, [&] { return log.count(QStringLiteral("trying next server in list")) >= 2; }, 8.0 + 2 * kRefusalSeconds
  );
  app.stopClient();
  m_log.remove(&log);

  QVERIFY2(reached, qPrintable(log.dump()));
  // A fails (next), B fails (round over), A fails again (next): the round starts with the first address
  const auto next = log.indexOf(QStringLiteral("trying next server in list"));
  const auto roundOver = log.indexOf(QStringLiteral("failed to connect to server: Connection refused"), next);
  const auto nextAgain = log.indexOf(QStringLiteral("trying next server in list"), roundOver + 1);
  QVERIFY2(next >= 0 && roundOver > next && nextAgain > roundOver, qPrintable(log.dump()));
}

QTEST_MAIN(ClientAppConnectTests)
