/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlModelTests.h"

#include "gui/awdl/AwdlModel.h"

#include <QMap>

#include <functional>

using namespace deskflow::awdl;

Q_DECLARE_METATYPE(LinkState)
Q_DECLARE_METATYPE(AdminResult)
Q_DECLARE_METATYPE(LinkKind)
Q_DECLARE_METATYPE(Pattern)
Q_DECLARE_METATYPE(Summary)
Q_DECLARE_METATYPE(Route)
Q_DECLARE_METATYPE(QList<Sample>)

namespace {

// What `ifconfig awdl0` writes on a Mac where the link is on (macOS 27.3, read once without privilege) and where it is
// off. The hardware address is made up
const auto kUp = QStringLiteral(
    "awdl0: flags=8863<UP,BROADCAST,SMART,RUNNING,SIMPLEX,MULTICAST> mtu 1500\n"
    "\toptions=6460<TSO4,TSO6,CHANNEL_IO,PARTIAL_CSUM,ZEROINVERT_CSUM>\n"
    "\tether 02:00:00:00:00:01\n"
    "\tinet6 fe80::ff:fe00:1%awdl0 prefixlen 64 scopeid 0x10 \n"
    "\tnd6 options=201<PERFORMNUD,DAD>\n"
    "\tmedia: autoselect\n"
    "\tstatus: active\n"
);
const auto kDown = QStringLiteral(
    "awdl0: flags=8822<BROADCAST,SMART,SIMPLEX,MULTICAST> mtu 1500\n"
    "\toptions=6460<TSO4,TSO6,CHANNEL_IO,PARTIAL_CSUM,ZEROINVERT_CSUM>\n"
    "\tether 02:00:00:00:00:01\n"
    "\tnd6 options=201<PERFORMNUD,DAD>\n"
    "\tmedia: autoselect\n"
    "\tstatus: inactive\n"
);

// A burst of probes: the latency of each from a function of its number, and the numbers that got no answer
QList<Sample> burst(const std::function<double(int)> &rttAt, const QList<int> &lostAt = {}, int count = kBurstCount)
{
  QList<Sample> samples;
  for (int i = 0; i < count; ++i) {
    Sample sample;
    sample.sequence = i;
    sample.lost = lostAt.contains(i);
    sample.rttMs = sample.lost ? 0 : rttAt(i);
    samples.append(sample);
  }
  return samples;
}

// A quiet network: about 3 ms, a little different from probe to probe (from 2.0 to 3.8 ms)
double quiet(int i)
{
  return 2.0 + 0.3 * ((i * 3) % 7);
}

// the same with spikes of the given latency at the given numbers
std::function<double(int)> withSpikes(const QMap<int, double> &spikes)
{
  return [spikes](int i) { return spikes.contains(i) ? spikes.value(i) : quiet(i); };
}

Route wifi(const QString &gateway = QStringLiteral("192.168.0.1"), const QString &name = QStringLiteral("en0"))
{
  Route route;
  route.interfaceName = name;
  route.gateway = gateway;
  route.kind = LinkKind::WiFi;
  return route;
}

PolicyInput
input(bool option, bool running, LinkState link, SessionMemory memory = {}, Pattern latency = Pattern::NotMeasured)
{
  PolicyInput in;
  in.supported = true;
  in.optionOn = option;
  in.coreRunning = running;
  in.link = link;
  in.latency = latency;
  in.memory = memory;
  return in;
}

} // namespace

void AwdlModelTests::parseIfconfig_readsTheStateOfTheInterface_data()
{
  QTest::addColumn<int>("exitCode");
  QTest::addColumn<QString>("out");
  QTest::addColumn<QString>("err");
  QTest::addColumn<LinkState>("state");

  QTest::newRow("up, as macOS 27 writes it") << 0 << kUp << QString() << LinkState::Up;
  QTest::newRow("down, as macOS 27 writes it") << 0 << kDown << QString() << LinkState::Down;
  QTest::newRow("up, windows line ends") << 0 << QString(kUp).replace(QLatin1Char('\n'), QStringLiteral("\r\n"))
                                         << QString() << LinkState::Up;
  QTest::newRow("up without the line of the status")
      << 0 << QStringLiteral("awdl0: flags=8863<UP,BROADCAST,SMART,RUNNING,SIMPLEX,MULTICAST> mtu 1500\n") << QString()
      << LinkState::Up;
  QTest::newRow("up, but not running yet")
      << 0 << QStringLiteral("awdl0: flags=8823<UP,BROADCAST,SMART,SIMPLEX,MULTICAST> mtu 1500\n\tstatus: inactive\n")
      << QString() << LinkState::Up;
  QTest::newRow("down without the line of the status")
      << 0 << QStringLiteral("awdl0: flags=8822<BROADCAST,SMART,SIMPLEX,MULTICAST> mtu 1500\n") << QString()
      << LinkState::Down;
  QTest::newRow("only the status says it, active")
      << 0 << QStringLiteral("awdl0: mtu 1500\n\tstatus: active\n") << QString() << LinkState::Up;
  QTest::newRow("only the status says it, inactive")
      << 0 << QStringLiteral("awdl0: mtu 1500\n\tstatus: inactive\n") << QString() << LinkState::Down;
  // a flag that only contains the letters of UP is not UP
  QTest::newRow("a flag that has UP inside a word")
      << 0 << QStringLiteral("awdl0: flags=8822<BROADCAST,SMART,SIMPLEX,MULTICAST,GROUP> mtu 1500\n") << QString()
      << LinkState::Down;
  QTest::newRow("the word up in the options is not the flag")
      << 0 << QStringLiteral("awdl0: flags=8822<BROADCAST,SMART> mtu 1500\n\toptions=6460<TSO4>\n\tmedia: up\n")
      << QString() << LinkState::Down;
  QTest::newRow("the interface is not there")
      << 1 << QString() << QStringLiteral("ifconfig: interface awdl0 does not exist\n") << LinkState::Unavailable;
  QTest::newRow("not there, in the output")
      << 1 << QStringLiteral("ifconfig: interface awdl0 does not exist\n") << QString() << LinkState::Unavailable;
  QTest::newRow("empty") << 0 << QString() << QString() << LinkState::Unknown;
  QTest::newRow("blank") << 0 << QStringLiteral("  \n\n") << QString() << LinkState::Unknown;
  QTest::newRow("garbage") << 0 << QStringLiteral("\x01\x02 not an interface \xff") << QString() << LinkState::Unknown;
  QTest::newRow("another interface") << 0 << QStringLiteral("en0: flags=8863<UP,BROADCAST,RUNNING> mtu 1500\n")
                                     << QString() << LinkState::Unknown;
  QTest::newRow("a failure that is not 'does not exist'")
      << 1 << QString() << QStringLiteral("ifconfig: socket: Operation not permitted\n") << LinkState::Unknown;
  QTest::newRow("a failure with the right words in the output of a good run is still the state")
      << 0 << kDown + QStringLiteral("\n# does not exist\n") << QString() << LinkState::Down;
  QTest::newRow("a failed run that wrote flags is not trusted") << 2 << kUp << QString() << LinkState::Unknown;
}

void AwdlModelTests::parseIfconfig_readsTheStateOfTheInterface()
{
  QFETCH(int, exitCode);
  QFETCH(QString, out);
  QFETCH(QString, err);
  QFETCH(LinkState, state);
  QCOMPARE(parseIfconfig(exitCode, out, err), state);
}

void AwdlModelTests::linkStateId_isAWordForEveryState()
{
  QCOMPARE(linkStateId(LinkState::Up), QStringLiteral("up"));
  QCOMPARE(linkStateId(LinkState::Down), QStringLiteral("down"));
  QCOMPARE(linkStateId(LinkState::Unavailable), QStringLiteral("unavailable"));
  QCOMPARE(linkStateId(LinkState::Unknown), QStringLiteral("unknown"));
}

void AwdlModelTests::parseAdminResult_tellsDoneFromCancelledFromFailed_data()
{
  QTest::addColumn<bool>("started");
  QTest::addColumn<int>("exitCode");
  QTest::addColumn<QString>("err");
  QTest::addColumn<AdminResult>("result");

  QTest::newRow("done") << true << 0 << QString() << AdminResult::Done;
  QTest::newRow("done with a warning on the error output")
      << true << 0 << QStringLiteral("some warning") << AdminResult::Done;
  // what osascript writes when the person closes the dialog (error -128)
  QTest::newRow("cancelled") << true << 1 << QStringLiteral("0:140: execution error: User canceled. (-128)\n")
                             << AdminResult::Cancelled;
  QTest::newRow("cancelled, only the number")
      << true << 1 << QStringLiteral("execution error: (-128)") << AdminResult::Cancelled;
  QTest::newRow("cancelled, only the words") << true << 1 << QStringLiteral("User canceled.") << AdminResult::Cancelled;
  QTest::newRow("a wrong password three times")
      << true << 1 << QStringLiteral("execution error: The administrator user name or password was incorrect. (-60007)")
      << AdminResult::Failed;
  QTest::newRow("the command failed") << true << 1
                                      << QStringLiteral("ifconfig: ioctl (SIOCSIFFLAGS): Operation not permitted")
                                      << AdminResult::Failed;
  QTest::newRow("a failure with no words") << true << 1 << QString() << AdminResult::Failed;
  QTest::newRow("not started") << false << -1 << QString() << AdminResult::Failed;
  QTest::newRow("not started, whatever it said") << false << 0 << QString() << AdminResult::Failed;
  QTest::newRow("the exit code of 0 is done even if the words say cancelled")
      << true << 0 << QStringLiteral("User canceled.") << AdminResult::Done;
}

void AwdlModelTests::parseAdminResult_tellsDoneFromCancelledFromFailed()
{
  QFETCH(bool, started);
  QFETCH(int, exitCode);
  QFETCH(QString, err);
  QFETCH(AdminResult, result);
  QCOMPARE(parseAdminResult(started, exitCode, err), result);
}

void AwdlModelTests::appleScriptString_cannotLeaveItsQuotes_data()
{
  QTest::addColumn<QString>("text");
  QTest::addColumn<QString>("literal");

  QTest::newRow("plain") << QStringLiteral("Deskflow turns it off") << QStringLiteral("\"Deskflow turns it off\"");
  QTest::newRow("empty") << QString() << QStringLiteral("\"\"");
  QTest::newRow("a quote") << QStringLiteral("say \"hi\"") << QStringLiteral("\"say \\\"hi\\\"\"");
  QTest::newRow("a backslash") << QStringLiteral("a\\b") << QStringLiteral("\"a\\\\b\"");
  QTest::newRow("a backslash before a quote") << QStringLiteral("\\\"") << QStringLiteral("\"\\\\\\\"\"");
  QTest::newRow("line breaks are spaces") << QStringLiteral("one\ntwo\r\nthree") << QStringLiteral("\"one two three\"");
  QTest::newRow("control characters go") << QStringLiteral(
                                                "a\x01\x1f\x7f"
                                                "b"
                                            )
                                         << QStringLiteral("\"ab\"");
  QTest::newRow("Korean is kept") << QStringLiteral("무선 링크를 끕니다") << QStringLiteral("\"무선 링크를 끕니다\"");
  // an attempt to end the string and run a command: it stays inside of the string
  QTest::newRow("an injection") << QStringLiteral("x\" & do shell script \"rm -rf ~\" & \"")
                                << QStringLiteral("\"x\\\" & do shell script \\\"rm -rf ~\\\" & \\\"\"");
}

void AwdlModelTests::appleScriptString_cannotLeaveItsQuotes()
{
  QFETCH(QString, text);
  QFETCH(QString, literal);
  QCOMPARE(appleScriptString(text), literal);
}

void AwdlModelTests::adminArguments_areTheConstantCommandAndTheTextOfTheDialogOnly()
{
  const auto down = adminArguments(Direction::Down, QStringLiteral("Deskflow turns the link off"));
  QCOMPARE(down.size(), 2);
  QCOMPARE(down.at(0), QStringLiteral("-e"));
  QCOMPARE(
      down.at(1), QStringLiteral(
                      "do shell script \"/sbin/ifconfig awdl0 down\" with administrator privileges with prompt "
                      "\"Deskflow turns the link off\""
                  )
  );
  const auto up = adminArguments(Direction::Up, QStringLiteral("Back on"));
  QCOMPARE(up.size(), 2);
  QCOMPARE(
      up.at(1), QStringLiteral(
                    "do shell script \"/sbin/ifconfig awdl0 up\" with administrator privileges with prompt \"Back on\""
                )
  );
  // a prompt that tries to leave its string does not change the command
  const auto hostile = adminArguments(Direction::Down, QStringLiteral("x\" & do shell script \"rm -rf ~"));
  QVERIFY2(
      hostile.at(1).startsWith(QStringLiteral("do shell script \"/sbin/ifconfig awdl0 down\" with administrator")),
      qPrintable(hostile.at(1))
  );
  QCOMPARE(hostile.at(1).count(QStringLiteral("do shell script")), 2);
  QVERIFY(hostile.at(1).contains(QStringLiteral("\\\" & do shell script \\\"rm -rf ~")));
  // the programs are the ones of the system, by their full paths
  QCOMPARE(QString::fromLatin1(kOsascriptPath), QStringLiteral("/usr/bin/osascript"));
  QCOMPARE(QString::fromLatin1(kIfconfigPath), QStringLiteral("/sbin/ifconfig"));
}

void AwdlModelTests::readArguments_askForTheInterfaceAndNothingElse()
{
  QCOMPARE(readArguments(), QStringList{QStringLiteral("awdl0")});
}

void AwdlModelTests::parseRoute_readsTheGatewayAndTheInterface_data()
{
  QTest::addColumn<QString>("text");
  QTest::addColumn<QString>("gateway");
  QTest::addColumn<QString>("interfaceName");

  QTest::newRow("a Mac on Wi-Fi") << QStringLiteral(
                                         "   route to: default\ndestination: default\n       mask: default\n    "
                                         "gateway: 192.168.0.1\n  interface: en0\n"
                                         "      flags: <UP,GATEWAY,DONE,STATIC,PRCLONING,GLOBAL>\n recvpipe  sendpipe  "
                                         "ssthresh  rtt,msec    rttvar  "
                                         "hopcount      mtu     expire\n       0         0         0         0         "
                                         "0         0      1500         0 \n"
                                     )
                                  << QStringLiteral("192.168.0.1") << QStringLiteral("en0");
  QTest::newRow("a VPN") << QStringLiteral("    gateway: 10.8.0.1\n  interface: utun4\n") << QStringLiteral("10.8.0.1")
                         << QStringLiteral("utun4");
  QTest::newRow("windows line ends") << QStringLiteral("    gateway: 10.0.0.1\r\n  interface: en7\r\n")
                                     << QStringLiteral("10.0.0.1") << QStringLiteral("en7");
  QTest::newRow("a route with no gateway (the network is the next hop)")
      << QStringLiteral("  interface: en0\n") << QString() << QStringLiteral("en0");
  QTest::newRow("not in this network") << QStringLiteral("route: writing to routing socket: not in table\n")
                                       << QString() << QString();
  QTest::newRow("empty") << QString() << QString() << QString();
  QTest::newRow("a gateway that is a link address (an interface name)")
      << QStringLiteral("    gateway: link#14\n  interface: en0\n") << QStringLiteral("link#14")
      << QStringLiteral("en0");
}

void AwdlModelTests::parseRoute_readsTheGatewayAndTheInterface()
{
  QFETCH(QString, text);
  QFETCH(QString, gateway);
  QFETCH(QString, interfaceName);
  const auto route = parseRoute(text);
  QCOMPARE(route.gateway, gateway);
  QCOMPARE(route.interfaceName, interfaceName);
  // what kind of connection it is, is not in the text
  QCOMPARE(route.kind, LinkKind::Unknown);
}

void AwdlModelTests::isLocalGateway_isTheLocalNetworkOnly_data()
{
  QTest::addColumn<QString>("address");
  QTest::addColumn<bool>("local");

  for (const auto *address :
       {"192.168.0.1", "192.168.255.254", "10.0.0.1", "10.255.1.1", "172.16.0.1", "172.31.255.254", "100.64.0.1",
        "100.127.255.254", "169.254.1.1"})
    QTest::newRow(address) << QString::fromLatin1(address) << true;
  for (const auto *address : {"8.8.8.8",        "1.1.1.1",          "172.15.0.1",  "172.32.0.1",
                              "100.63.255.254", "100.128.0.1",      "192.167.0.1", "192.169.0.1",
                              "11.0.0.1",       "127.0.0.1",        "0.0.0.0",     "255.255.255.255",
                              "224.0.0.1",      "239.255.255.250",  "169.253.0.1", "169.255.0.1",
                              "192.0.2.1",      "198.51.100.1",     "203.0.113.1", "::1",
                              "fe80::1",        "fd00::1",          "2001:db8::1", "",
                              "gateway",        "link#14",          "192.168.0",   "192.168.0.1.5",
                              "192.168.0.1/24", "192.168.0.1 ; rm", "-c",          "$(id)",
                              "192.168.0.256",  "example.com"})
    QTest::newRow(qPrintable(QStringLiteral("not: ") + QString::fromLatin1(address)))
        << QString::fromLatin1(address) << false;
}

void AwdlModelTests::isLocalGateway_isTheLocalNetworkOnly()
{
  QFETCH(QString, address);
  QFETCH(bool, local);
  QCOMPARE(isLocalGateway(address), local);
}

void AwdlModelTests::canProbe_isWifiWithALocalGatewayAndAPlainDeviceOnly_data()
{
  QTest::addColumn<Route>("route");
  QTest::addColumn<bool>("can");

  QTest::newRow("Wi-Fi, a home router") << wifi() << true;
  QTest::newRow("Wi-Fi, another device name") << wifi(QStringLiteral("10.0.0.1"), QStringLiteral("en12")) << true;
  auto wired = wifi();
  wired.kind = LinkKind::Wired;
  QTest::newRow("a wire") << wired << false;
  auto other = wifi();
  other.kind = LinkKind::Other;
  QTest::newRow("a VPN") << other << false;
  auto unknown = wifi();
  unknown.kind = LinkKind::Unknown;
  QTest::newRow("not looked at") << unknown << false;
  QTest::newRow("a gateway on the internet") << wifi(QStringLiteral("8.8.8.8")) << false;
  QTest::newRow("no gateway") << wifi(QString()) << false;
  QTest::newRow("a gateway that is not an address") << wifi(QStringLiteral("link#14")) << false;
  QTest::newRow("the wireless link itself") << wifi(QStringLiteral("192.168.0.1"), QStringLiteral("awdl0")) << false;
  QTest::newRow("the low latency link") << wifi(QStringLiteral("192.168.0.1"), QStringLiteral("llw0")) << false;
  QTest::newRow("a name that is not a device")
      << wifi(QStringLiteral("192.168.0.1"), QStringLiteral("en0; rm")) << false;
  QTest::newRow("a name that is an option") << wifi(QStringLiteral("192.168.0.1"), QStringLiteral("-c")) << false;
  QTest::newRow("no name") << wifi(QStringLiteral("192.168.0.1"), QString()) << false;
}

void AwdlModelTests::canProbe_isWifiWithALocalGatewayAndAPlainDeviceOnly()
{
  QFETCH(Route, route);
  QFETCH(bool, can);
  QCOMPARE(canProbe(route), can);
}

void AwdlModelTests::pingArguments_haveTheGatewayAsTheOnlyAddress()
{
  const auto arguments = pingArguments(wifi(QStringLiteral("192.168.0.1"), QStringLiteral("en0")));
  QCOMPARE(
      arguments, (QStringList{
                     QStringLiteral("-c"), QStringLiteral("40"), QStringLiteral("-i"), QStringLiteral("0.2"),
                     QStringLiteral("-n"), QStringLiteral("-W"), QStringLiteral("1000"), QStringLiteral("-b"),
                     QStringLiteral("en0"), QStringLiteral("192.168.0.1")
                 })
  );
  // the interval is far above the 0.002 s that a person without privilege may not go under
  QVERIFY(kBurstIntervalMs >= 100);
  // a rate that stays low: not more than 50 probes in a burst, a burst a minute at the most
  QVERIFY(kBurstCount <= 50);
  QVERIFY(kBurstEveryMs >= 60000);
  QCOMPARE(QString::fromLatin1("/sbin/ping"), QStringLiteral("/sbin/ping"));
}

void AwdlModelTests::pingArguments_areNoneForARouteThatMayNotBeMeasured()
{
  auto wired = wifi();
  wired.kind = LinkKind::Wired;
  QVERIFY(pingArguments(wired).isEmpty());
  QVERIFY(pingArguments(wifi(QStringLiteral("8.8.8.8"))).isEmpty());
  QVERIFY(pingArguments(wifi(QStringLiteral("192.168.0.1"), QStringLiteral("awdl0"))).isEmpty());
  QVERIFY(pingArguments(Route()).isEmpty());
  // a count and an interval that are not sensible are not what is asked for
  QVERIFY(pingArguments(wifi(), 0, 200).isEmpty());
  QVERIFY(pingArguments(wifi(), 40, 10).isEmpty());
  QVERIFY(pingArguments(wifi(), 1000, 200).isEmpty());
}

void AwdlModelTests::parsePing_readsAnswersAndLosses()
{
  const auto text = QStringLiteral(
      "PING 192.168.0.1 (192.168.0.1): 56 data bytes\n"
      "64 bytes from 192.168.0.1: icmp_seq=0 ttl=64 time=3.454 ms\n"
      "64 bytes from 192.168.0.1: icmp_seq=1 ttl=64 time=2.724 ms\n"
      "Request timeout for icmp_seq 2\n"
      "64 bytes from 192.168.0.1: icmp_seq=3 ttl=64 time=139.5 ms\n"
      "\n--- 192.168.0.1 ping statistics ---\n4 packets transmitted, 3 packets received, 25.0% packet loss\n"
      "round-trip min/avg/max/stddev = 2.724/48.5/139.5/60.1 ms\n"
  );
  const auto samples = parsePing(text);
  QCOMPARE(samples.size(), 4);
  QCOMPARE(samples.at(0).sequence, 0);
  QVERIFY(!samples.at(0).lost);
  QCOMPARE(samples.at(0).rttMs, 3.454);
  QCOMPARE(samples.at(1).rttMs, 2.724);
  QCOMPARE(samples.at(2).sequence, 2);
  QVERIFY(samples.at(2).lost);
  QCOMPARE(samples.at(3).rttMs, 139.5);
  // windows line ends, and the statistics line (which has "time" in it but no sequence) is not a probe
  QCOMPARE(parsePing(QString(text).replace(QLatin1Char('\n'), QStringLiteral("\r\n"))).size(), 4);
}

void AwdlModelTests::parsePing_saysNothingForWhatIsNotPing()
{
  QVERIFY(parsePing(QString()).isEmpty());
  QVERIFY(parsePing(QStringLiteral("ping: sendto: No route to host\n")).isEmpty());
  QVERIFY(parsePing(QStringLiteral("ping: cannot resolve x: Unknown host\n")).isEmpty());
  QVERIFY(parsePing(QStringLiteral("icmp_seq=abc time=xyz ms\n")).isEmpty());
  QVERIFY(parsePing(QStringLiteral("round-trip min/avg/max/stddev = 2.724/3.827/5.365/0.790 ms\n")).isEmpty());
}

void AwdlModelTests::analyzeBurst_findsTheSpikesOfAwdl_andNothingElse_data()
{
  QTest::addColumn<QList<Sample>>("samples");
  QTest::addColumn<Pattern>("pattern");
  QTest::addColumn<int>("spikes");

  QTest::newRow("a quiet network") << burst(quiet) << Pattern::Clean << 0;
  // what was measured with AWDL in use: spikes of 25 to 70 ms that come every 3.6 s, two or three probes each (0.2 s
  // apart)
  QTest::newRow("the spikes of AWDL, as measured")
      << burst(withSpikes({{4, 31.0}, {5, 64.0}, {22, 48.0}, {23, 25.5}, {40 - 1, 27.0}})) << Pattern::Stutter << 5;
  QTest::newRow("three spikes, wherever they are")
      << burst(withSpikes({{2, 25.0}, {14, 60.0}, {33, 30.0}})) << Pattern::Stutter << 3;
  QTest::newRow("two single spikes 3.6 s apart") << burst(withSpikes({{3, 70.0}, {21, 40.0}})) << Pattern::Stutter << 2;
  QTest::newRow("two single spikes 3.0 s apart") << burst(withSpikes({{3, 70.0}, {18, 40.0}})) << Pattern::Stutter << 2;
  QTest::newRow("two single spikes 4.4 s apart") << burst(withSpikes({{3, 70.0}, {25, 40.0}})) << Pattern::Stutter << 2;
  QTest::newRow("two single spikes 2.6 s apart: not the period")
      << burst(withSpikes({{3, 70.0}, {16, 40.0}})) << Pattern::Clean << 2;
  QTest::newRow("two single spikes 4.8 s apart: not the period")
      << burst(withSpikes({{3, 70.0}, {27, 40.0}})) << Pattern::Clean << 2;
  QTest::newRow("two single spikes 1 s apart") << burst(withSpikes({{3, 70.0}, {8, 40.0}})) << Pattern::Clean << 2;
  QTest::newRow("two single spikes 7.6 s apart") << burst(withSpikes({{1, 70.0}, {39, 40.0}})) << Pattern::Clean << 2;
  QTest::newRow("one big spike") << burst(withSpikes({{12, 500.0}})) << Pattern::Clean << 1;
  QTest::newRow("one long stall of two probes") << burst(withSpikes({{12, 500.0}, {13, 300.0}})) << Pattern::Clean << 2;
  QTest::newRow("one long stall of three probes")
      << burst(withSpikes({{12, 500.0}, {13, 300.0}, {14, 90.0}})) << Pattern::Stutter << 3;
  // 20 ms is not above 20 ms
  QTest::newRow("on the threshold: not a spike")
      << burst(withSpikes({{2, 20.0}, {9, 20.0}, {20, 20.0}, {31, 20.0}})) << Pattern::Clean << 0;
  QTest::newRow("just above the threshold")
      << burst(withSpikes({{2, 20.5}, {9, 20.5}, {20, 20.5}})) << Pattern::Stutter << 3;
  // a network that is slow all the time has no spikes: the threshold is five times the median
  QTest::newRow("slow all the time") << burst([](int i) { return 45.0 + 5.0 * ((i * 3) % 5); }) << Pattern::Clean << 0;
  QTest::newRow("a median of 10 ms: 30 ms is no spike")
      << burst([](int i) { return (i % 9 == 0) ? 30.0 : 10.0 + 0.1 * (i % 3); }) << Pattern::Clean << 0;
  QTest::newRow("a median of 10 ms: 60 ms is a spike")
      << burst([](int i) { return (i % 9 == 0) ? 60.0 : 10.0 + 0.1 * (i % 3); }) << Pattern::Stutter << 5;
  QTest::newRow("losses, and the answers are quiet") << burst(quiet, {3, 4, 5, 20, 21, 30}) << Pattern::Clean << 0;
  QTest::newRow("losses do not count as spikes")
      << burst(quiet, {3, 4, 5, 6, 7, 8, 9, 10, 11, 12}) << Pattern::Clean << 0;
  QTest::newRow("a spike among the losses") << burst(withSpikes({{2, 200.0}}), {3, 4, 5}) << Pattern::Clean << 1;
  QTest::newRow("too few answers: no data") << burst(quiet, [] {
    QList<int> lost;
    for (int i = 5; i < 40; ++i)
      lost.append(i);
    return lost;
  }()) << Pattern::NoData << 0;
  QTest::newRow("half of the answers are enough") << burst(withSpikes({{4, 40.0}, {5, 40.0}, {6, 40.0}}), [] {
    QList<int> lost;
    for (int i = 20; i < 40; ++i)
      lost.append(i);
    return lost;
  }()) << Pattern::Stutter << 3;
  QTest::newRow("no probes") << QList<Sample>() << Pattern::NoData << 0;
}

void AwdlModelTests::analyzeBurst_findsTheSpikesOfAwdl_andNothingElse()
{
  QFETCH(QList<Sample>, samples);
  QFETCH(Pattern, pattern);
  QFETCH(int, spikes);
  const auto stats = analyzeBurst(samples);
  QCOMPARE(stats.pattern, pattern);
  if (pattern != Pattern::NoData)
    QCOMPARE(stats.spikes, spikes);
  QVERIFY(stats.thresholdMs >= 20.0 || pattern == Pattern::NoData);
}

void AwdlModelTests::analyzeBurst_hasTheMedianAndTheWorst()
{
  // 1, 2, 3 ... 40 ms: the median is between the 20th and the 21st
  const auto ramp = burst([](int i) { return double(i + 1); });
  const auto stats = analyzeBurst(ramp);
  QCOMPARE(stats.medianMs, 20.5);
  QCOMPARE(stats.worstMs, 40.0);
  QCOMPARE(stats.received, 40);
  QCOMPARE(stats.lost, 0);
  // the lost ones are counted (of the 40 that were sent), the median and the worst are of the answers
  const auto lossy = analyzeBurst(burst([](int i) { return double(i + 1); }, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
  QCOMPARE(lossy.received, 30);
  QCOMPARE(lossy.lost, 10);
  QCOMPARE(lossy.medianMs, 25.5);
  QCOMPARE(lossy.worstMs, 40.0);
  // an odd number of answers has the one in the middle
  const auto odd = analyzeBurst(burst([](int i) { return double(i + 1); }, {39}), 40);
  QCOMPARE(odd.received, 39);
  QCOMPARE(odd.medianMs, 20.0);
  // without answers there is nothing to say
  const auto none = analyzeBurst({});
  QCOMPARE(none.pattern, Pattern::NoData);
  QCOMPARE(none.received, 0);
  QCOMPARE(none.lost, 40);
  QCOMPARE(none.medianMs, 0.0);
  QCOMPARE(none.worstMs, 0.0);
}

void AwdlModelTests::shouldProbe_isOnlyWithTheOptionTheCoreAndTheLink_data()
{
  QTest::addColumn<bool>("supported");
  QTest::addColumn<bool>("option");
  QTest::addColumn<bool>("running");
  QTest::addColumn<LinkState>("link");
  QTest::addColumn<bool>("askRunning");
  QTest::addColumn<bool>("busy");
  QTest::addColumn<qint64>("since");
  QTest::addColumn<bool>("probe");

  const qint64 never = -1;
  QTest::newRow("everything, never measured")
      << true << true << true << LinkState::Up << false << false << never << true;
  QTest::newRow("everything, long ago") << true << true << true << LinkState::Up << false << false << qint64(60000)
                                        << true;
  QTest::newRow("everything, a minute less a bit")
      << true << true << true << LinkState::Up << false << false << qint64(59999) << false;
  QTest::newRow("not a Mac") << false << true << true << LinkState::Up << false << false << never << false;
  QTest::newRow("the option is off") << true << false << true << LinkState::Up << false << false << never << false;
  QTest::newRow("the core does not run") << true << true << false << LinkState::Up << false << false << never << false;
  QTest::newRow("the link is down") << true << true << true << LinkState::Down << false << false << never << false;
  QTest::newRow("there is no awdl0") << true << true << true << LinkState::Unavailable << false << false << never
                                     << false;
  QTest::newRow("the state is not known")
      << true << true << true << LinkState::Unknown << false << false << never << false;
  QTest::newRow("the dialog is open") << true << true << true << LinkState::Up << true << false << never << false;
  QTest::newRow("one is under way") << true << true << true << LinkState::Up << false << true << never << false;
}

void AwdlModelTests::shouldProbe_isOnlyWithTheOptionTheCoreAndTheLink()
{
  QFETCH(bool, supported);
  QFETCH(bool, option);
  QFETCH(bool, running);
  QFETCH(LinkState, link);
  QFETCH(bool, askRunning);
  QFETCH(bool, busy);
  QFETCH(qint64, since);
  QFETCH(bool, probe);

  ProbeInput in;
  in.supported = supported;
  in.optionOn = option;
  in.coreRunning = running;
  in.link = link;
  in.askRunning = askRunning;
  in.busy = busy;
  in.sinceLastMs = since;
  QCOMPARE(shouldProbe(in), probe);
}

void AwdlModelTests::summarize_saysWhatTheStatusLineSays_data()
{
  QTest::addColumn<LinkState>("link");
  QTest::addColumn<LinkKind>("kind");
  QTest::addColumn<Pattern>("latency");
  QTest::addColumn<Summary>("summary");

  using L = LinkState;
  using K = LinkKind;
  using P = Pattern;
  QTest::newRow("no awdl0") << L::Unavailable << K::WiFi << P::Clean << Summary::NotAvailable;
  QTest::newRow("not read") << L::Unknown << K::Unknown << P::NotMeasured << Summary::Unknown;
  QTest::newRow("off") << L::Down << K::WiFi << P::NotMeasured << Summary::Off;
  QTest::newRow("off, measured before") << L::Down << K::WiFi << P::Stutter << Summary::Off;
  QTest::newRow("up, on a wire") << L::Up << K::Wired << P::NotMeasured << Summary::OnWired;
  QTest::newRow("up, on a wire, measured") << L::Up << K::Wired << P::Stutter << Summary::OnWired;
  QTest::newRow("up, a VPN") << L::Up << K::Other << P::NotMeasured << Summary::OnNotWiFi;
  QTest::newRow("up, not looked at, not measured") << L::Up << K::Unknown << P::NotMeasured << Summary::OnNotMeasured;
  QTest::newRow("up, Wi-Fi, not measured") << L::Up << K::WiFi << P::NotMeasured << Summary::OnNotMeasured;
  QTest::newRow("up, Wi-Fi, no data") << L::Up << K::WiFi << P::NoData << Summary::OnNoData;
  QTest::newRow("up, Wi-Fi, clean: no problem seen") << L::Up << K::WiFi << P::Clean << Summary::OnClean;
  QTest::newRow("up, Wi-Fi, the spikes") << L::Up << K::WiFi << P::Stutter << Summary::OnStutter;
}

void AwdlModelTests::summarize_saysWhatTheStatusLineSays()
{
  QFETCH(LinkState, link);
  QFETCH(LinkKind, kind);
  QFETCH(Pattern, latency);
  QFETCH(Summary, summary);
  QCOMPARE(summarize(link, kind, latency), summary);
}

void AwdlModelTests::decide_theHintNeedsTheSpikes_theLinkBeingUpIsNotEnough()
{
  SessionMemory asked;
  asked.asked = true;
  // the link is up, the person was asked: the hint is for the spikes only
  for (const auto latency : {Pattern::NotMeasured, Pattern::NoData, Pattern::Clean}) {
    const auto decision = decide(input(true, true, LinkState::Up, asked, latency));
    QVERIFY(!decision.ask);
    QVERIFY(!decision.hint);
  }
  const auto spikes = decide(input(true, true, LinkState::Up, asked, Pattern::Stutter));
  QVERIFY(!spikes.ask);
  QVERIFY(spikes.hint);
  // the spikes of a burst that was taken while the link was down are not the link's: no hint with the link down
  const auto down = decide(input(true, true, LinkState::Down, asked, Pattern::Stutter));
  QVERIFY(!down.hint);
  // and the spikes do not make a second dialog: before the first one it is the dialog, never the hint
  const auto first = decide(input(true, true, LinkState::Up, SessionMemory(), Pattern::Stutter));
  QVERIFY(first.ask);
  QVERIFY(!first.hint);
  // the option off: the spikes say nothing
  QVERIFY(!decide(input(false, true, LinkState::Up, asked, Pattern::Stutter)).hint);
}

void AwdlModelTests::decide_followsTheOptionTheCoreAndTheLink_data()
{
  QTest::addColumn<bool>("option");
  QTest::addColumn<bool>("running");
  QTest::addColumn<LinkState>("link");
  QTest::addColumn<bool>("ask");
  QTest::addColumn<bool>("hint");

  QTest::newRow("option off, core running, link up: nothing") << false << true << LinkState::Up << false << false;
  QTest::newRow("option off, core stopped, link up: nothing") << false << false << LinkState::Up << false << false;
  QTest::newRow("option on, core stopped, link up: nothing") << true << false << LinkState::Up << false << false;
  QTest::newRow("option on, core running, link up: ask") << true << true << LinkState::Up << true << false;
  QTest::newRow("option on, core running, link down: nothing") << true << true << LinkState::Down << false << false;
  QTest::newRow("option on, core running, no such interface: nothing")
      << true << true << LinkState::Unavailable << false << false;
  QTest::newRow("option on, core running, unknown: nothing") << true << true << LinkState::Unknown << false << false;
}

void AwdlModelTests::decide_followsTheOptionTheCoreAndTheLink()
{
  QFETCH(bool, option);
  QFETCH(bool, running);
  QFETCH(LinkState, link);
  QFETCH(bool, ask);
  QFETCH(bool, hint);

  const auto decision = decide(input(option, running, link));
  QCOMPARE(decision.ask, ask);
  QCOMPARE(decision.hint, hint);
  // the dialog is open from the moment that it is asked for
  QCOMPARE(decision.memory.asked, ask);
  QCOMPARE(decision.memory.askRunning, ask);
}

void AwdlModelTests::decide_asksOnceForARun_andNeverTwiceByItself()
{
  // the first reading with the link up: ask, and remember it
  auto first = decide(input(true, true, LinkState::Up));
  QVERIFY(first.ask);
  QVERIFY(first.memory.asked);
  // the person closed the dialog: it is not open, the link is still up
  auto memory = first.memory;
  memory.askRunning = false;
  for (int reading = 0; reading < 5; ++reading) {
    const auto next = decide(input(true, true, LinkState::Up, memory, Pattern::Stutter));
    QVERIFY2(!next.ask, "a second dialog by itself");
    // the person is told (the latency has the spikes), and is the one who asks again
    QVERIFY(next.hint);
    memory = next.memory;
  }
}

void AwdlModelTests::decide_whenTheLinkComesBackItIsAHintOnly()
{
  // the password was given and the link went down; AirDrop or Handoff turned it on again while the core runs
  SessionMemory memory;
  memory.asked = true;
  const auto down = decide(input(true, true, LinkState::Down, memory));
  QVERIFY(!down.ask);
  QVERIFY(!down.hint);
  QVERIFY(down.memory.asked);
  const auto back = decide(input(true, true, LinkState::Up, down.memory, Pattern::Stutter));
  QVERIFY(!back.ask);
  QVERIFY(back.hint);
  // and it goes down again (the person pressed the button): the hint goes
  const auto again = decide(input(true, true, LinkState::Down, back.memory));
  QVERIFY(!again.hint);
}

void AwdlModelTests::decide_aStoppedCoreForgetsTheRun_andNeverRestores()
{
  SessionMemory memory;
  memory.asked = true;
  memory.restoredByUser = true;
  for (const auto link : {LinkState::Up, LinkState::Down, LinkState::Unavailable, LinkState::Unknown}) {
    const auto stopped = decide(input(true, false, link, memory));
    QVERIFY(!stopped.ask);
    QVERIFY(!stopped.hint);
    QCOMPARE(stopped.memory, SessionMemory());
  }
  // the next run asks again, once
  const auto next = decide(input(true, true, LinkState::Up, SessionMemory()));
  QVERIFY(next.ask);
  // there is nothing in the decision that turns the link on: the program never does it by itself
  const QStringList members{QStringLiteral("ask"), QStringLiteral("hint"), QStringLiteral("memory")};
  Q_UNUSED(members)
}

void AwdlModelTests::decide_anAnswerThatIsStillOpenSaysNothing()
{
  // the link is still up, and the latency has the spikes, while the dialog is open (it was asked for by the start of
  // the core, or by a button before that): no second dialog, and no hint over it
  for (const bool asked : {true, false}) {
    SessionMemory memory;
    memory.asked = asked;
    memory.askRunning = true;
    const auto decision = decide(input(true, true, LinkState::Up, memory, Pattern::Stutter));
    QVERIFY(!decision.ask);
    QVERIFY(!decision.hint);
    QVERIFY(decision.memory.askRunning);
    QCOMPARE(decision.memory.asked, asked);
  }
}

void AwdlModelTests::decide_whatThePersonRestoredIsNotTakenBack()
{
  // "Turn back on" in the settings while the core runs: no dialog, no hint, for this run
  SessionMemory memory;
  memory.asked = true;
  memory.restoredByUser = true;
  const auto decision = decide(input(true, true, LinkState::Up, memory));
  QVERIFY(!decision.ask);
  QVERIFY(!decision.hint);
  QVERIFY(decision.memory.restoredByUser);
  // even when nothing was asked for before the person pressed it
  SessionMemory fresh;
  fresh.restoredByUser = true;
  const auto again = decide(input(true, true, LinkState::Up, fresh));
  QVERIFY(!again.ask);
  QVERIFY(!again.hint);
}

void AwdlModelTests::decide_isNothingWithTheOptionOffOrOffAMac_forEveryInput()
{
  // every combination of the other inputs: with the option off, or not on a Mac, the program does nothing and remembers
  // nothing
  for (const bool supported : {false, true}) {
    for (const bool option : {false, true}) {
      if (supported && option)
        continue;
      for (const bool running : {false, true}) {
        for (const auto link : {LinkState::Up, LinkState::Down, LinkState::Unavailable, LinkState::Unknown}) {
          for (const bool asked : {false, true}) {
            for (const bool askRunning : {false, true}) {
              for (const bool restored : {false, true}) {
                PolicyInput in;
                in.supported = supported;
                in.optionOn = option;
                in.coreRunning = running;
                in.link = link;
                in.memory.asked = asked;
                in.memory.askRunning = askRunning;
                in.memory.restoredByUser = restored;
                const auto decision = decide(in);
                QVERIFY(!decision.ask);
                QVERIFY(!decision.hint);
                QCOMPARE(decision.memory, SessionMemory());
              }
            }
          }
        }
      }
    }
  }
}

QTEST_APPLESS_MAIN(AwdlModelTests)
