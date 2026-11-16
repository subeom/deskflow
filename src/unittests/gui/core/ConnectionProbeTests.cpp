/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ConnectionProbeTests.h"

#include "FakeProbeNetwork.h"
#include "gui/core/ConnectionProbe.h"

#include <QAbstractSocket>
#include <QElapsedTimer>
#include <QMap>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTest>
#include <QTimer>

#include <algorithm>

using namespace deskflow::gui;
using deskflow::diagnosis::FailureKind;
using namespace probetest;

namespace {

QList<ProbeResult> runToTheEnd(ConnectionProbe &probe, const QList<ServerEntry> &entries)
{
  QSignalSpy finished(&probe, &ConnectionProbe::finished);
  probe.start(entries);
  // an entry that needs no network (a bad port) is done before start() returns
  if (finished.isEmpty() && !finished.wait(5000)) {
    return {};
  }
  return qvariant_cast<QList<ProbeResult>>(finished.first().at(0));
}

QList<ServerEntry> entriesOf(const QString &text)
{
  return parseServerEntries(text, 24800);
}

} // namespace

void ConnectionProbeTests::initTestCase()
{
  qRegisterMetaType<ProbeResult>();
  qRegisterMetaType<QList<ProbeResult>>();
}

void ConnectionProbeTests::parseServerEntries_splitsTheListTheWayTheCoreReadsIt()
{
  const auto entries = parseServerEntries(
      QStringLiteral(" 10.0.0.5 ,desk.example:24801,, [fe80::1%en0]:24802 ,fe80::2, ,100.64.1.7"), 24800
  );

  QCOMPARE(entries.size(), 5);
  QCOMPARE(entries[0].text, QStringLiteral("10.0.0.5"));
  QCOMPARE(entries[0].host, QStringLiteral("10.0.0.5"));
  QCOMPARE(entries[0].port, 24800);
  QVERIFY(entries[0].isValid());
  QCOMPARE(entries[1].host, QStringLiteral("desk.example"));
  QCOMPARE(entries[1].port, 24801);
  QCOMPARE(entries[2].host, QStringLiteral("fe80::1%en0"));
  QCOMPARE(entries[2].port, 24802);
  QCOMPARE(entries[2].text, QStringLiteral("[fe80::1%en0]:24802"));
  // an IPv6 address that has more than one colon and no brackets is a host without a port
  QCOMPARE(entries[3].host, QStringLiteral("fe80::2"));
  QCOMPARE(entries[3].port, 24800);
  QCOMPARE(entries[4].host, QStringLiteral("100.64.1.7"));

  QVERIFY(parseServerEntries(QString(), 24800).isEmpty());
  QVERIFY(parseServerEntries(QStringLiteral(" , ,"), 24800).isEmpty());
}

void ConnectionProbeTests::parseServerEntries_namesWhatIsWrongWithAPort_data()
{
  QTest::addColumn<QString>("text");
  QTest::addColumn<QString>("expectedWord");
  QTest::newRow("zero") << QStringLiteral("10.0.0.5:0") << QStringLiteral("port");
  QTest::newRow("too-big") << QStringLiteral("10.0.0.5:70000") << QStringLiteral("port");
  QTest::newRow("not-a-number") << QStringLiteral("10.0.0.5:abc") << QStringLiteral("port");
  QTest::newRow("empty-port") << QStringLiteral("10.0.0.5:") << QStringLiteral("port");
  QTest::newRow("no-host") << QStringLiteral(":24800") << QStringLiteral("address");
  QTest::newRow("v6-without-port") << QStringLiteral("[::1]") << QStringLiteral("port");
  QTest::newRow("v6-open-bracket") << QStringLiteral("[::1") << QStringLiteral("bracket");
}

void ConnectionProbeTests::parseServerEntries_namesWhatIsWrongWithAPort()
{
  QFETCH(QString, text);
  QFETCH(QString, expectedWord);

  const auto entries = parseServerEntries(text, 24800);
  QCOMPARE(entries.size(), 1);
  QVERIFY(!entries.first().isValid());
  QVERIFY2(entries.first().problem.contains(expectedWord), qPrintable(entries.first().problem));
  QCOMPARE(entries.first().text, text);
}

void ConnectionProbeTests::failureKindFromSocketError_saysWhatEachSocketErrorMeans_data()
{
  QTest::addColumn<int>("error");
  QTest::addColumn<int>("kind");
  const auto row = [](const char *name, QAbstractSocket::SocketError error, FailureKind kind) {
    QTest::newRow(name) << static_cast<int>(error) << static_cast<int>(kind);
  };
  row("refused", QAbstractSocket::ConnectionRefusedError, FailureKind::Refused);
  row("name-not-found", QAbstractSocket::HostNotFoundError, FailureKind::DnsFailure);
  row("timeout", QAbstractSocket::SocketTimeoutError, FailureKind::TimedOut);
  row("network", QAbstractSocket::NetworkError, FailureKind::NoRoute);
  row("access", QAbstractSocket::SocketAccessError, FailureKind::AccessDenied);
  row("remote-closed", QAbstractSocket::RemoteHostClosedError, FailureKind::Reset);
  row("unknown", QAbstractSocket::UnknownSocketError, FailureKind::Unknown);
  row("address-in-use", QAbstractSocket::AddressInUseError, FailureKind::Unknown);
}

void ConnectionProbeTests::failureKindFromSocketError_saysWhatEachSocketErrorMeans()
{
  QFETCH(int, error);
  QFETCH(int, kind);
  QCOMPARE(static_cast<int>(failureKindFromSocketError(error)), kind);
}

void ConnectionProbeTests::probe_openAddress_isReportedOpenAndClosedAtOnce()
{
  FakeNetwork network;
  network.scripts["10.0.0.5"] = open(5);
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("10.0.0.5")));

  QCOMPARE(results.size(), 1);
  QVERIFY(results[0].open);
  QCOMPARE(results[0].host, QStringLiteral("10.0.0.5"));
  QCOMPARE(results[0].address, QStringLiteral("10.0.0.5"));
  QCOMPARE(results[0].port, 24800);
  QVERIFY(results[0].elapsedMs >= 0);
  QCOMPARE(network.connected, QStringList{QStringLiteral("10.0.0.5")});
  // connect then close: nothing is kept open
  QCOMPARE(network.aborted, QStringList{QStringLiteral("10.0.0.5")});
  QVERIFY(!probe->isRunning());
}

void ConnectionProbeTests::probe_refusedAddress_isReportedRefused()
{
  FakeNetwork network;
  network.scripts["10.0.0.5"] = fail(FailureKind::Refused, QStringLiteral("Connection refused"));
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("10.0.0.5")));

  QCOMPARE(results.size(), 1);
  QVERIFY(!results[0].open);
  QCOMPARE(results[0].kind, FailureKind::Refused);
  QCOMPARE(results[0].detail, QStringLiteral("Connection refused"));
}

void ConnectionProbeTests::probe_noRoute_isReportedAsNoRoute()
{
  FakeNetwork network;
  network.scripts["10.0.0.5"] = fail(FailureKind::NoRoute, QStringLiteral("No route to host"));
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("10.0.0.5")));

  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].kind, FailureKind::NoRoute);
}

void ConnectionProbeTests::probe_silentAddress_timesOutAfterTheTimeLimit()
{
  FakeNetwork network;
  network.scripts["10.0.0.5"] = Script{};
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network, 80));
  QCOMPARE(probe->timeoutMs(), 80);

  QElapsedTimer clock;
  clock.start();
  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("10.0.0.5")));

  QCOMPARE(results.size(), 1);
  QVERIFY(!results[0].open);
  QCOMPARE(results[0].kind, FailureKind::TimedOut);
  QVERIFY2(results[0].elapsedMs >= 70, qPrintable(QString::number(results[0].elapsedMs)));
  QVERIFY(clock.elapsed() < 3000);
  // the connect that never answered is given up
  QCOMPARE(network.aborted, QStringList{QStringLiteral("10.0.0.5")});
}

void ConnectionProbeTests::probe_nameThatDoesNotResolve_isReportedAsDnsFailure()
{
  FakeNetwork network;
  network.resolveErrors["ghost.example"] = QStringLiteral("Host not found");
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("ghost.example")));

  QCOMPARE(results.size(), 1);
  QVERIFY(!results[0].open);
  QCOMPARE(results[0].kind, FailureKind::DnsFailure);
  QCOMPARE(results[0].host, QStringLiteral("ghost.example"));
  QVERIFY(results[0].address.isEmpty());
  QCOMPARE(results[0].detail, QStringLiteral("Host not found"));
  QVERIFY(network.connected.isEmpty());
}

void ConnectionProbeTests::probe_nameWithTwoAddresses_triesBothAndKeepsTheOrder()
{
  FakeNetwork network;
  network.names["desk.example"] = {QStringLiteral("::1"), QStringLiteral("10.0.0.5"), QStringLiteral("::1")};
  // the second address answers first
  network.scripts["::1"] = fail(FailureKind::Refused, QStringLiteral("Connection refused"), 30);
  network.scripts["10.0.0.5"] = open(0);
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("desk.example:24801")));

  QCOMPARE(results.size(), 2);
  QCOMPARE(results[0].address, QStringLiteral("::1"));
  QCOMPARE(results[0].kind, FailureKind::Refused);
  QCOMPARE(results[1].address, QStringLiteral("10.0.0.5"));
  QVERIFY(results[1].open);
  QCOMPARE(results[0].host, QStringLiteral("desk.example"));
  QCOMPARE(results[0].port, 24801);
  QCOMPARE(results[0].entry, QStringLiteral("desk.example:24801"));
}

void ConnectionProbeTests::probe_numericAddress_doesNotAskTheResolver()
{
  FakeNetwork network;
  network.scripts["10.0.0.5"] = open();
  network.scripts["fe80::1%en0"] = open();
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("10.0.0.5,[fe80::1%en0]:24800")));

  QCOMPARE(results.size(), 2);
  QVERIFY(network.resolved.isEmpty());
}

void ConnectionProbeTests::probe_resolverThatAnswersAtOnce_isHandled()
{
  FakeNetwork network;
  network.resolveAtOnce = true;
  network.names["desk.example"] = {QStringLiteral("10.0.0.5")};
  network.scripts["10.0.0.5"] = open();
  network.resolveErrors["ghost.example"] = QStringLiteral("Host not found");
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("ghost.example,desk.example")));

  QCOMPARE(results.size(), 2);
  QCOMPARE(results[0].kind, FailureKind::DnsFailure);
  QVERIFY(results[1].open);
}

void ConnectionProbeTests::probe_severalEntries_runAtTheSameTimeAndAreReportedInOrder()
{
  FakeNetwork network;
  network.scripts["10.0.0.5"] = Script{};
  network.scripts["100.64.1.7"] = open(10);
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network, 150));

  QSignalSpy ready(probe.get(), &ConnectionProbe::resultReady);
  QSignalSpy finished(probe.get(), &ConnectionProbe::finished);
  probe->start(entriesOf(QStringLiteral("10.0.0.5,100.64.1.7")));
  QVERIFY(probe->isRunning());
  QVERIFY(finished.wait(5000));

  // the answer that is quick is reported at once, before the silent address gives up
  QCOMPARE(ready.size(), 2);
  QCOMPARE(qvariant_cast<ProbeResult>(ready.at(0).at(0)).address, QStringLiteral("100.64.1.7"));
  QCOMPARE(qvariant_cast<ProbeResult>(ready.at(1).at(0)).address, QStringLiteral("10.0.0.5"));
  // the list at the end is in the order of the entries
  const auto results = qvariant_cast<QList<ProbeResult>>(finished.first().at(0));
  QCOMPARE(results.size(), 2);
  QCOMPARE(results[0].address, QStringLiteral("10.0.0.5"));
  QCOMPARE(results[0].kind, FailureKind::TimedOut);
  QCOMPARE(results[1].address, QStringLiteral("100.64.1.7"));
  QVERIFY(results[1].open);
  QCOMPARE(finished.size(), 1);
}

void ConnectionProbeTests::probe_entryWithABadPort_isReportedWithoutBeingTested()
{
  FakeNetwork network;
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("10.0.0.5:99999")));

  QCOMPARE(results.size(), 1);
  QVERIFY(!results[0].open);
  QVERIFY(!results[0].problem.isEmpty());
  QCOMPARE(results[0].entry, QStringLiteral("10.0.0.5:99999"));
  QVERIFY(network.connected.isEmpty());
  QVERIFY(network.resolved.isEmpty());
}

void ConnectionProbeTests::probe_emptyList_finishesAtOnce()
{
  FakeNetwork network;
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));
  QSignalSpy finished(probe.get(), &ConnectionProbe::finished);

  probe->start({});

  QCOMPARE(finished.size(), 1);
  QVERIFY(qvariant_cast<QList<ProbeResult>>(finished.first().at(0)).isEmpty());
  QVERIFY(!probe->isRunning());
}

void ConnectionProbeTests::probe_cancel_dropsTheTest()
{
  FakeNetwork network;
  network.scripts["10.0.0.5"] = open(30);
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network));
  QSignalSpy ready(probe.get(), &ConnectionProbe::resultReady);
  QSignalSpy finished(probe.get(), &ConnectionProbe::finished);

  probe->start(entriesOf(QStringLiteral("10.0.0.5")));
  probe->cancel();
  QVERIFY(!probe->isRunning());
  QTest::qWait(120);

  QCOMPARE(ready.size(), 0);
  QCOMPARE(finished.size(), 0);
}

void ConnectionProbeTests::probe_start_dropsATestThatRuns()
{
  FakeNetwork network;
  network.scripts["10.0.0.5"] = Script{};
  network.scripts["10.0.0.6"] = open();
  std::unique_ptr<ConnectionProbe> probe(makeProbe(network, 200));
  QSignalSpy finished(probe.get(), &ConnectionProbe::finished);

  probe->start(entriesOf(QStringLiteral("10.0.0.5")));
  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("10.0.0.6")));
  QTest::qWait(300);

  // the first test never reports: only the second ends
  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].address, QStringLiteral("10.0.0.6"));
  QCOMPARE(finished.size(), 1);
}

// the probe has no way to write: its socket only connects and closes
void ConnectionProbeTests::probe_neverSendsData()
{
  const auto *meta = &IProbeSocket::staticMetaObject;
  for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
    const auto name = QString::fromLatin1(meta->method(i).name());
    QVERIFY2(!name.contains(QStringLiteral("write"), Qt::CaseInsensitive), qPrintable(name));
    QVERIFY2(!name.contains(QStringLiteral("send"), Qt::CaseInsensitive), qPrintable(name));
  }
}

void ConnectionProbeTests::summaryLine_namesEveryAddressAndWhatCameOfIt()
{
  QList<ProbeResult> results;
  ProbeResult timedOut;
  timedOut.entry = timedOut.host = timedOut.address = QStringLiteral("10.0.0.5");
  timedOut.port = 24800;
  timedOut.kind = FailureKind::TimedOut;
  timedOut.elapsedMs = 3000;
  ProbeResult open;
  open.entry = open.host = QStringLiteral("desk.example");
  open.address = QStringLiteral("100.64.1.7");
  open.port = 24800;
  open.open = true;
  open.elapsedMs = 12;
  ProbeResult ghost;
  ghost.entry = ghost.host = QStringLiteral("ghost.example");
  ghost.port = 24801;
  ghost.kind = FailureKind::DnsFailure;
  ProbeResult refused;
  refused.entry = refused.host = refused.address = QStringLiteral("10.0.0.6");
  refused.port = 24800;
  refused.kind = FailureKind::Refused;
  ProbeResult route;
  route.entry = route.host = route.address = QStringLiteral("fe80::1");
  route.port = 24800;
  route.kind = FailureKind::NoRoute;
  route.detail = QStringLiteral("No route to host");
  ProbeResult skipped;
  skipped.entry = QStringLiteral("10.0.0.7:0");
  skipped.host = QStringLiteral("10.0.0.7");
  skipped.problem = QStringLiteral("the port is not a number from 1 to 65535");
  results << timedOut << open << ghost << refused << route << skipped;

  const auto line =
      summaryLine(results, 3000, QDateTime(QDate(2026, 11, 16), QTime(22, 40)), QStringLiteral("macOS 15"));

  QCOMPARE(
      line, QStringLiteral(
                "Deskflow connection test 2026-11-16 22:40 (macOS 15): "
                "10.0.0.5:24800 timed out (no answer in 3000 ms); "
                "desk.example (100.64.1.7):24800 open in 12 ms; "
                "ghost.example:24801 name not found; "
                "10.0.0.6:24800 refused (nothing listens there); "
                "[fe80::1]:24800 no route (No route to host); "
                "10.0.0.7:0 not tested (the port is not a number from 1 to 65535)"
            )
  );
  QVERIFY(!line.contains(QLatin1Char('\n')));
}

void ConnectionProbeTests::summaryLine_withoutResults_saysNothingWasTested()
{
  QCOMPARE(
      summaryLine({}, 3000, QDateTime(QDate(2026, 11, 16), QTime(22, 40)), QStringLiteral("Windows 11")),
      QStringLiteral("Deskflow connection test 2026-11-16 22:40 (Windows 11): no address was tested")
  );
}

void ConnectionProbeTests::system_openLocalPort_isOpen()
{
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  std::unique_ptr<ConnectionProbe> probe(ConnectionProbe::createSystem(nullptr, 2000));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("127.0.0.1:%1").arg(server.serverPort())));

  QCOMPARE(results.size(), 1);
  QVERIFY2(results[0].open, qPrintable(results[0].detail));
  QCOMPARE(results[0].address, QStringLiteral("127.0.0.1"));
  QCOMPARE(results[0].port, static_cast<int>(server.serverPort()));
}

void ConnectionProbeTests::system_closedLocalPort_isRefused()
{
  quint16 port = 0;
  {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    port = server.serverPort();
  }
  std::unique_ptr<ConnectionProbe> probe(ConnectionProbe::createSystem(nullptr, 2000));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("127.0.0.1:%1").arg(port)));

  QCOMPARE(results.size(), 1);
  QVERIFY(!results[0].open);
  QCOMPARE(results[0].kind, FailureKind::Refused);
}

void ConnectionProbeTests::system_localhostName_isResolvedAndTried()
{
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost, 0));
  std::unique_ptr<ConnectionProbe> probe(ConnectionProbe::createSystem(nullptr, 2000));

  const auto results = runToTheEnd(*probe, entriesOf(QStringLiteral("localhost:%1").arg(server.serverPort())));

  QVERIFY(!results.isEmpty());
  const auto openIpv4 = std::ranges::any_of(results, [](const ProbeResult &result) {
    return result.open && result.address == QStringLiteral("127.0.0.1");
  });
  QVERIFY(openIpv4);
  for (const auto &result : results) {
    QCOMPARE(result.host, QStringLiteral("localhost"));
    QVERIFY(!result.address.isEmpty());
  }
}

QTEST_MAIN(ConnectionProbeTests)
