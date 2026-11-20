/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacAwdlSystemTests.h"

#include "FakeAwdlSystem.h"
#include "gui/awdl/AwdlPlatform.h"
#include "gui/awdl/MacAwdlSystem.h"

#include <QFile>
#include <QGuiApplication>
#include <QSignalSpy>

using namespace deskflow::awdl;

Q_DECLARE_METATYPE(LinkState)
Q_DECLARE_METATYPE(AdminResult)
Q_DECLARE_METATYPE(Route)
Q_DECLARE_METATYPE(QList<Sample>)

namespace {

// what a stand-in does with its arguments: one line, written next to it
const auto kWriteArguments = QStringLiteral("printf '%s\\n' \"$*\" > \"$0.args\"\n");

} // namespace

void MacAwdlSystemTests::initTestCase()
{
#ifdef Q_OS_WIN
  QSKIP("the stand-ins are shell scripts");
#endif
  QVERIFY(m_dir.isValid());
  qRegisterMetaType<LinkState>();
  qRegisterMetaType<AdminResult>();
  qRegisterMetaType<Route>();
  qRegisterMetaType<QList<Sample>>();
}

QString MacAwdlSystemTests::script(const QString &name, const QString &body) const
{
  const auto path = m_dir.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    // a stand-in that cannot be written is a failure of the test, and not a program that is not there
    QTest::qFail(qPrintable(QStringLiteral("cannot write %1").arg(path)), __FILE__, __LINE__);
    return {};
  }
  file.write("#!/bin/sh\n");
  file.write(body.toUtf8());
  file.close();
  if (!QFile::setPermissions(path, QFile::permissions(path) | QFile::ExeUser | QFile::ReadUser | QFile::WriteUser)) {
    QTest::qFail(qPrintable(QStringLiteral("cannot make %1 executable").arg(path)), __FILE__, __LINE__);
    return {};
  }
  return path;
}

QString MacAwdlSystemTests::recorded(const QString &name) const
{
  QFile file(m_dir.filePath(name) + QStringLiteral(".args"));
  if (!file.open(QIODevice::ReadOnly))
    return QStringLiteral("<not run>");
  return QString::fromUtf8(file.readAll()).trimmed();
}

void MacAwdlSystemTests::theDefaultPrograms_areThoseOfTheSystem_byTheirFullPaths()
{
  const MacAwdlSystem::Programs programs;
  QCOMPARE(programs.ifconfig, QStringLiteral("/sbin/ifconfig"));
  QCOMPARE(programs.osascript, QStringLiteral("/usr/bin/osascript"));
  QCOMPARE(programs.route, QStringLiteral("/sbin/route"));
  QCOMPARE(programs.ping, QStringLiteral("/sbin/ping"));
}

void MacAwdlSystemTests::readState_asksForTheInterfaceAndReadsWhatItSays()
{
  MacAwdlSystem::Programs programs;
  programs.ifconfig = script(
      QStringLiteral("ifconfig-up"),
      kWriteArguments + QStringLiteral(
                            "echo 'awdl0: flags=8863<UP,BROADCAST,SMART,RUNNING,SIMPLEX,MULTICAST> mtu 1500'\n"
                            "echo '\\tstatus: active'\n"
                        )
  );
  MacAwdlSystem system(programs);
  QSignalSpy read(&system, &IAwdlSystem::stateRead);
  system.readState();
  QVERIFY(read.wait(5000));
  QCOMPARE(read.first().first().value<LinkState>(), LinkState::Up);
  QCOMPARE(recorded(QStringLiteral("ifconfig-up")), QStringLiteral("awdl0"));

  programs.ifconfig = script(
      QStringLiteral("ifconfig-gone"), QStringLiteral("echo 'ifconfig: interface awdl0 does not exist' >&2\nexit 1\n")
  );
  MacAwdlSystem second(programs);
  QSignalSpy gone(&second, &IAwdlSystem::stateRead);
  second.readState();
  QVERIFY(gone.wait(5000));
  QCOMPARE(gone.first().first().value<LinkState>(), LinkState::Unavailable);
}

void MacAwdlSystemTests::readState_aProgramThatIsNotThere_isNotKnown()
{
  MacAwdlSystem::Programs programs;
  programs.ifconfig = m_dir.filePath(QStringLiteral("there-is-no-such-program"));
  MacAwdlSystem system(programs);
  QSignalSpy read(&system, &IAwdlSystem::stateRead);
  system.readState();
  QVERIFY(read.wait(5000));
  QCOMPARE(read.first().first().value<LinkState>(), LinkState::Unknown);
}

void MacAwdlSystemTests::setLink_givesTheArgumentsOfTheConstantCommand_andReadsTheAnswer()
{
  MacAwdlSystem::Programs programs;
  programs.osascript = script(
      QStringLiteral("osascript-cancel"),
      kWriteArguments + QStringLiteral("echo '0:140: execution error: User canceled. (-128)' >&2\nexit 1\n")
  );
  MacAwdlSystem system(programs);
  QSignalSpy done(&system, &IAwdlSystem::linkSet);
  system.setLink(Direction::Down, QStringLiteral("Deskflow turns it off"));
  QVERIFY(done.wait(5000));
  QCOMPARE(done.first().first().value<AdminResult>(), AdminResult::Cancelled);
  // the arguments are the ones of the model: -e and the constant command, one argument each (never one string for a
  // shell)
  const auto expected = adminArguments(Direction::Down, QStringLiteral("Deskflow turns it off"));
  QCOMPARE(recorded(QStringLiteral("osascript-cancel")), expected.join(QLatin1Char(' ')));
  QVERIFY(recorded(QStringLiteral("osascript-cancel")).contains(QStringLiteral("/sbin/ifconfig awdl0 down")));

  programs.osascript = script(QStringLiteral("osascript-ok"), QStringLiteral("exit 0\n"));
  MacAwdlSystem good(programs);
  QSignalSpy ok(&good, &IAwdlSystem::linkSet);
  good.setLink(Direction::Up, QStringLiteral("Back on"));
  QVERIFY(ok.wait(5000));
  QCOMPARE(ok.first().first().value<AdminResult>(), AdminResult::Done);

  programs.osascript =
      script(QStringLiteral("osascript-fail"), QStringLiteral("echo 'ifconfig: not permitted' >&2\nexit 1\n"));
  MacAwdlSystem bad(programs);
  QSignalSpy failed(&bad, &IAwdlSystem::linkSet);
  bad.setLink(Direction::Down, QString());
  QVERIFY(failed.wait(5000));
  QCOMPARE(failed.first().first().value<AdminResult>(), AdminResult::Failed);

  programs.osascript = m_dir.filePath(QStringLiteral("there-is-no-such-program"));
  MacAwdlSystem missing(programs);
  QSignalSpy absent(&missing, &IAwdlSystem::linkSet);
  missing.setLink(Direction::Down, QString());
  QVERIFY(absent.wait(5000));
  QCOMPARE(absent.first().first().value<AdminResult>(), AdminResult::Failed);
}

void MacAwdlSystemTests::findRoute_readsTheRouteAndTheKindOfTheConnection()
{
  MacAwdlSystem::Programs programs;
  programs.route = script(
      QStringLiteral("route-wifi"),
      kWriteArguments +
          QStringLiteral("echo '   route to: default'\necho '    gateway: 192.168.0.1'\necho '  interface: en0'\n")
  );
  programs.kindOf = [](const QString &name) { return name == QLatin1String("en0") ? LinkKind::WiFi : LinkKind::Other; };
  MacAwdlSystem system(programs);
  QSignalSpy found(&system, &IAwdlSystem::routeFound);
  system.findRoute();
  QVERIFY(found.wait(5000));
  const auto route = found.first().first().value<Route>();
  QCOMPARE(route.gateway, QStringLiteral("192.168.0.1"));
  QCOMPARE(route.interfaceName, QStringLiteral("en0"));
  QCOMPARE(route.kind, LinkKind::WiFi);
  QCOMPARE(recorded(QStringLiteral("route-wifi")), QStringLiteral("-n get default"));

  // no default route: nothing, and the kind is not asked
  programs.route = script(
      QStringLiteral("route-none"),
      QStringLiteral("echo 'route: writing to routing socket: not in table' >&2\nexit 1\n")
  );
  MacAwdlSystem none(programs);
  QSignalSpy nothing(&none, &IAwdlSystem::routeFound);
  none.findRoute();
  QVERIFY(nothing.wait(5000));
  QCOMPARE(nothing.first().first().value<Route>(), Route());
}

void MacAwdlSystemTests::measure_sendsToTheGatewayOfTheRouteOnly()
{
  MacAwdlSystem::Programs programs;
  programs.ping = script(
      QStringLiteral("ping-fake"),
      kWriteArguments + QStringLiteral(
                            "echo 'PING 192.168.0.1 (192.168.0.1): 56 data bytes'\n"
                            "echo '64 bytes from 192.168.0.1: icmp_seq=0 ttl=64 time=3.454 ms'\n"
                            "echo 'Request timeout for icmp_seq 1'\n"
                            "echo '64 bytes from 192.168.0.1: icmp_seq=2 ttl=64 time=139.5 ms'\n"
                        )
  );
  MacAwdlSystem system(programs);
  Route route;
  route.interfaceName = QStringLiteral("en0");
  route.gateway = QStringLiteral("192.168.0.1");
  route.kind = LinkKind::WiFi;
  QSignalSpy measured(&system, &IAwdlSystem::measured);
  system.measure(route);
  QVERIFY(measured.wait(10000));
  const auto samples = measured.first().first().value<QList<Sample>>();
  QCOMPARE(samples.size(), 3);
  QVERIFY(samples.at(1).lost);
  QCOMPARE(samples.at(2).rttMs, 139.5);
  // the arguments are those of the model: the gateway is the only address in them
  QCOMPARE(recorded(QStringLiteral("ping-fake")), pingArguments(route).join(QLatin1Char(' ')));
  QVERIFY(recorded(QStringLiteral("ping-fake")).endsWith(QStringLiteral("192.168.0.1")));
}

void MacAwdlSystemTests::measure_aRouteThatMayNotBeMeasured_runsNothing()
{
  MacAwdlSystem::Programs programs;
  programs.ping = script(
      QStringLiteral("ping-never"), kWriteArguments + QStringLiteral("echo '64 bytes from x: icmp_seq=0 time=1 ms'\n")
  );
  MacAwdlSystem system(programs);
  Route route;
  route.interfaceName = QStringLiteral("en0");
  route.kind = LinkKind::WiFi;
  for (const auto &gateway :
       {QStringLiteral("8.8.8.8"), QStringLiteral("link#14"), QString(), QStringLiteral("192.168.0.1; id")}) {
    route.gateway = gateway;
    QSignalSpy measured(&system, &IAwdlSystem::measured);
    system.measure(route);
    QVERIFY(measured.wait(5000));
    QVERIFY(measured.first().first().value<QList<Sample>>().isEmpty());
  }
  route.gateway = QStringLiteral("192.168.0.1");
  route.kind = LinkKind::Wired;
  QSignalSpy wired(&system, &IAwdlSystem::measured);
  system.measure(route);
  QVERIFY(wired.wait(5000));
  QVERIFY(wired.first().first().value<QList<Sample>>().isEmpty());
  // not one of them started the program
  QCOMPARE(recorded(QStringLiteral("ping-never")), QStringLiteral("<not run>"));
}

void MacAwdlSystemTests::createSystem_isNoneOffscreen_theFakeOfATest_andTheSeamsSayWhereTheOptionIs()
{
  struct Reset
  {
    ~Reset()
    {
      s_systemForTest = nullptr;
      s_supportedForTest.reset();
    }
  } reset;

  // the tests run offscreen: whatever they forget to fake, they never get the programs of the system
  QVERIFY(QGuiApplication::platformName() != QLatin1String("cocoa"));
  s_supportedForTest = true;
  QObject parent;
  QVERIFY(createSystem(&parent) == nullptr);

  // a test gives its own
  s_systemForTest = [](QObject *owner) -> IAwdlSystem * { return new FakeAwdlSystem(owner); };
  auto *fake = createSystem(&parent);
  QVERIFY(fake != nullptr);
  QVERIFY(dynamic_cast<FakeAwdlSystem *>(fake) != nullptr);

  s_systemForTest = nullptr;
  s_supportedForTest = false;
  QVERIFY(!platformSupported());
  QVERIFY(createSystem(&parent) == nullptr);
  s_supportedForTest = true;
  QVERIFY(platformSupported());
  s_supportedForTest.reset();
#ifdef Q_OS_MACOS
  QVERIFY(platformSupported());
#else
  QVERIFY(!platformSupported());
#endif
}

QTEST_MAIN(MacAwdlSystemTests)
