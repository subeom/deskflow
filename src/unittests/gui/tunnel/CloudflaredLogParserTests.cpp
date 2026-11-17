/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CloudflaredLogParserTests.h"

#include "gui/tunnel/CloudflaredLogParser.h"

using namespace deskflow::tunnel;

Q_DECLARE_METATYPE(deskflow::tunnel::LogEventKind)

void CloudflaredLogParserTests::line_isRecognisedInTheConsoleAndTheJsonFormat_data()
{
  QTest::addColumn<QString>("line");
  QTest::addColumn<LogEventKind>("kind");
  QTest::addColumn<TunnelFailure>("problem");

  const auto none = TunnelFailure::None;
  QTest::newRow("listener-console") << QStringLiteral(
                                           "2026-11-17T04:00:00Z INF Start Websocket listener host=127.0.0.1:24800"
                                       )
                                    << LogEventKind::ListenerStarted << none;
  QTest::newRow("listener-json") << QStringLiteral(
                                        "{\"level\":\"info\",\"host\":\"127.0.0.1:24800\",\"time\":\"2026-11-17T04:00:"
                                        "00Z\",\"message\":\"Start Websocket listener\"}"
                                    )
                                 << LogEventKind::ListenerStarted << none;
  QTest::newRow("bind-failed-console")
      << QStringLiteral(
             "2026-11-17T04:00:00Z ERR Error on Websocket listener error=\"failed to start forwarding server: listen "
             "tcp 127.0.0.1:24800: bind: address already in use\""
         )
      << LogEventKind::BindFailed << TunnelFailure::PortBusy;
  QTest::newRow("bind-failed-windows")
      << QStringLiteral(
             "{\"level\":\"error\",\"error\":\"failed to start forwarding server: listen tcp 127.0.0.1:24800: bind: "
             "Only one usage of each socket address (protocol/network address/port) is normally permitted.\"}"
         )
      << LogEventKind::BindFailed << TunnelFailure::PortBusy;
  QTest::newRow("browser-opened") << QStringLiteral("A browser window should have opened at the following URL:")
                                  << LogEventKind::LoginRequired << TunnelFailure::LoginRequired;
  QTest::newRow("open-this-url") << QStringLiteral(
                                        "Please open the following URL and log in with your Cloudflare account:"
                                    )
                                 << LogEventKind::LoginRequired << TunnelFailure::LoginRequired;
  QTest::newRow("waiting") << QStringLiteral("2026-11-17T04:00:01Z INF Waiting for login...")
                           << LogEventKind::LoginRequired << TunnelFailure::LoginRequired;
  QTest::newRow("origin-failed-uppercase-and-lowercase")
      << QStringLiteral("2026-11-17T04:00:02Z ERR FAILED TO CONNECT TO ORIGIN error=\"websocket: bad handshake\"")
      << LogEventKind::OriginFailed << TunnelFailure::HandshakeRejected;
}

void CloudflaredLogParserTests::line_isRecognisedInTheConsoleAndTheJsonFormat()
{
  QFETCH(QString, line);
  QFETCH(LogEventKind, kind);
  QFETCH(TunnelFailure, problem);

  const auto event = parseCloudflaredLine(line);
  QCOMPARE(event.kind, kind);
  QCOMPARE(event.problem, problem);
}

void CloudflaredLogParserTests::connectionFailure_saysWhyFromTheWordsOfTheError_data()
{
  QTest::addColumn<QString>("error");
  QTest::addColumn<TunnelFailure>("problem");

  QTest::newRow("bad-handshake") << QStringLiteral("websocket: bad handshake") << TunnelFailure::HandshakeRejected;
  QTest::newRow("bad-handshake-names-an-address")
      << QStringLiteral("dial tcp 104.16.0.1:443 websocket: bad handshake") << TunnelFailure::HandshakeRejected;
  QTest::newRow("no-such-host") << QStringLiteral("dial tcp: lookup mbp.example.com on 192.168.0.1:53: no such host")
                                << TunnelFailure::HostUnresolvable;
  QTest::newRow("dns-server-failed") << QStringLiteral("lookup mbp.example.com: server misbehaving")
                                     << TunnelFailure::HostUnresolvable;
  QTest::newRow("timeout") << QStringLiteral("dial tcp 104.16.0.1:443: i/o timeout") << TunnelFailure::EdgeUnreachable;
  QTest::newRow("refused") << QStringLiteral("dial tcp 104.16.0.1:443: connect: connection refused")
                           << TunnelFailure::EdgeUnreachable;
  QTest::newRow("unreachable") << QStringLiteral("dial tcp: connect: network is unreachable")
                               << TunnelFailure::EdgeUnreachable;
  QTest::newRow("windows-refused") << QStringLiteral(
                                          "dial tcp 104.16.0.1:443: connectex: No connection could be made because the "
                                          "target machine actively refused it."
                                      )
                                   << TunnelFailure::EdgeUnreachable;
  QTest::newRow("tls") << QStringLiteral("tls: failed to verify certificate") << TunnelFailure::EdgeUnreachable;
  QTest::newRow("not-known") << QStringLiteral("something new") << TunnelFailure::ConnectFailed;
}

void CloudflaredLogParserTests::connectionFailure_saysWhyFromTheWordsOfTheError()
{
  QFETCH(QString, error);
  QFETCH(TunnelFailure, problem);

  const auto console = QStringLiteral("2026-11-17T04:00:02Z ERR failed to connect to origin error=\"%1\"").arg(error);
  const auto json = QStringLiteral("{\"level\":\"error\",\"error\":\"%1\",\"message\":\"failed to connect to origin\"}")
                        .arg(QString(error).replace(QLatin1Char('"'), QLatin1String("\\\"")));
  for (const auto &line : {console, json}) {
    const auto event = parseCloudflaredLine(line);
    QCOMPARE(event.kind, LogEventKind::OriginFailed);
    QCOMPARE(event.problem, problem);
  }
}

void CloudflaredLogParserTests::level_isReadFromBothFormats_data()
{
  QTest::addColumn<QString>("line");
  QTest::addColumn<QString>("level");

  QTest::newRow("dbg") << QStringLiteral("2026-11-17T04:00:00Z DBG something") << QStringLiteral("DBG");
  QTest::newRow("inf") << QStringLiteral("2026-11-17T04:00:00Z INF something") << QStringLiteral("INF");
  QTest::newRow("wrn") << QStringLiteral("2026-11-17T04:00:00Z WRN something") << QStringLiteral("WRN");
  QTest::newRow("err") << QStringLiteral("2026-11-17T04:00:00Z ERR something") << QStringLiteral("ERR");
  QTest::newRow("json-info") << QStringLiteral("{\"level\":\"info\",\"message\":\"x\"}") << QStringLiteral("INF");
  QTest::newRow("json-warn") << QStringLiteral("{\"level\": \"warn\",\"message\":\"x\"}") << QStringLiteral("WRN");
  QTest::newRow("json-error") << QStringLiteral("{\"level\":\"error\",\"message\":\"x\"}") << QStringLiteral("ERR");
  QTest::newRow("json-fatal") << QStringLiteral("{\"level\":\"fatal\",\"message\":\"x\"}") << QStringLiteral("FTL");
  QTest::newRow("none") << QStringLiteral("Please open the following URL") << QString();
}

void CloudflaredLogParserTests::level_isReadFromBothFormats()
{
  QFETCH(QString, line);
  QFETCH(QString, level);
  QCOMPARE(parseCloudflaredLine(line).level, level);
}

void CloudflaredLogParserTests::unknownLines_areOtherAndNeverAnError_data()
{
  QTest::addColumn<QString>("line");

  QTest::newRow("empty") << QString();
  QTest::newRow("blank") << QStringLiteral("   ");
  QTest::newRow("unrelated") << QStringLiteral("2026-11-17T04:00:00Z INF Starting tunnel connection");
  QTest::newRow("the-version") << QStringLiteral("cloudflared version 2026.11.0");
  QTest::newRow("only-escapes") << QStringLiteral("\x1b[31m\x1b[0m");
  QTest::newRow("binary") << QString::fromLatin1("\x01\x02\x03\xff\xfe");
  QTest::newRow("a-url-line") << QStringLiteral("https://login.example.com/cdn-cgi/access/cli?aud=abc");
}

void CloudflaredLogParserTests::unknownLines_areOtherAndNeverAnError()
{
  QFETCH(QString, line);
  const auto event = parseCloudflaredLine(line);
  QCOMPARE(event.kind, LogEventKind::Other);
  QCOMPARE(event.problem, TunnelFailure::None);
}

void CloudflaredLogParserTests::message_hasNoTerminalEscapesOrControlCharactersAndIsCut()
{
  const auto coloured = parseCloudflaredLine(
      QStringLiteral("\x1b[90m2026-11-17T04:00:00Z\x1b[0m \x1b[32mINF\x1b[0m Start Websocket listener")
  );
  QCOMPARE(coloured.kind, LogEventKind::ListenerStarted);
  QCOMPARE(coloured.level, QStringLiteral("INF"));
  QCOMPARE(coloured.message, QStringLiteral("2026-11-17T04:00:00Z INF Start Websocket listener"));

  const auto control = parseCloudflaredLine(QStringLiteral("a\rb\nc\x07 d"));
  QVERIFY(!control.message.contains(QLatin1Char('\r')));
  QVERIFY(!control.message.contains(QLatin1Char('\n')));
  QVERIFY(!control.message.contains(QLatin1Char('\x07')));

  const auto huge = parseCloudflaredLine(QString(100000, QLatin1Char('x')));
  QVERIFY(huge.message.size() <= 4096);
}

QTEST_MAIN(CloudflaredLogParserTests)
