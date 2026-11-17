/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelAddressTests.h"

#include "common/Settings.h"
#include "common/TunnelAddress.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace deskflow::tunnel;

namespace {

QTemporaryDir &scratchDir()
{
  static QTemporaryDir dir;
  return dir;
}

QByteArray readAll(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<unreadable>");
}

void writeAll(const QString &path, const QByteArray &content)
{
  QFile file(path);
  QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(path));
  file.write(content);
}

TunnelConfig usable()
{
  TunnelConfig config;
  config.enabled = true;
  config.hostname = QStringLiteral("mbp.example.com");
  config.tlsEnabled = true;
  return config;
}

// every shape of the saved address that the client reads, and that must come out as it went in
const QStringList kSavedAddresses = {
    QString(),
    QStringLiteral("10.0.0.5"),
    QStringLiteral("10.0.0.5:24801"),
    QStringLiteral("desk.local"),
    QStringLiteral("fe80::1%en0"),
    QStringLiteral("[fe80::1]:24800"),
    QStringLiteral("10.0.0.5, 100.64.0.7"),
    QStringLiteral(" 10.0.0.5 ,mbp.example.com:24801 , "),
    QStringLiteral("127.0.0.1"),
};

} // namespace

void TunnelAddressTests::initTestCase()
{
  QVERIFY(scratchDir().isValid());
  m_dir = scratchDir().path();
  // the settings of this test are a file of its own, never the ones of the user
  Settings::setSettingsFile(QStringLiteral("%1/start/Deskflow.conf").arg(m_dir));
  Settings::setStateFile(QStringLiteral("%1/start/Deskflow.state").arg(m_dir));
  QVERIFY(Settings::settingsFile().startsWith(QDir(m_dir).absolutePath()));
}

void TunnelAddressTests::localPort_isLimitedToTheUnprivilegedRange()
{
  QVERIFY(!isValidLocalPort(0));
  QVERIFY(!isValidLocalPort(80));
  QVERIFY(!isValidLocalPort(1023));
  QVERIFY(isValidLocalPort(1024));
  QVERIFY(isValidLocalPort(kDefaultLocalPort));
  QVERIFY(isValidLocalPort(65535));
  QVERIFY(!isValidLocalPort(65536));
  QVERIFY(!isValidLocalPort(-24800));
}

void TunnelAddressTests::localPortFromSetting_takesWholeNumbersInTheRange_data()
{
  QTest::addColumn<QVariant>("raw");
  QTest::addColumn<int>("expected");

  QTest::newRow("text") << QVariant(QStringLiteral("24800")) << 24800;
  QTest::newRow("number") << QVariant(24801) << 24801;
  QTest::newRow("padded") << QVariant(QStringLiteral("  5000 ")) << 5000;
  QTest::newRow("lowest") << QVariant(QStringLiteral("1024")) << 1024;
  QTest::newRow("highest") << QVariant(65535) << 65535;
  QTest::newRow("unset") << QVariant() << 0;
  QTest::newRow("empty") << QVariant(QString()) << 0;
  QTest::newRow("privileged") << QVariant(QStringLiteral("1023")) << 0;
  QTest::newRow("too-big") << QVariant(QStringLiteral("65536")) << 0;
  QTest::newRow("negative") << QVariant(QStringLiteral("-1")) << 0;
  QTest::newRow("word") << QVariant(QStringLiteral("soon")) << 0;
  QTest::newRow("fraction") << QVariant(QStringLiteral("24800.5")) << 0;
  QTest::newRow("with-host") << QVariant(QStringLiteral("127.0.0.1:24800")) << 0;
  QTest::newRow("list-from-a-comma") << QVariant(QStringList{QStringLiteral("24800"), QStringLiteral("24801")}) << 0;
}

void TunnelAddressTests::localPortFromSetting_takesWholeNumbersInTheRange()
{
  QFETCH(QVariant, raw);
  QFETCH(int, expected);
  QCOMPARE(localPortFromSetting(raw), expected);
}

void TunnelAddressTests::normalizedHostname_trimsLowersAndDropsTheFinalDot()
{
  QCOMPARE(normalizedHostname(QStringLiteral("  MBP.Example.COM ")), QStringLiteral("mbp.example.com"));
  QCOMPARE(normalizedHostname(QStringLiteral("mbp.example.com.")), QStringLiteral("mbp.example.com"));
  QCOMPARE(normalizedHostname(QString()), QString());
}

void TunnelAddressTests::hostname_isADnsNameThatCloudflaredCannotTakeForAnOption_data()
{
  QTest::addColumn<QString>("hostname");
  QTest::addColumn<bool>("valid");

  QTest::newRow("plain") << QStringLiteral("mbp.example.com") << true;
  QTest::newRow("hyphen-inside") << QStringLiteral("my-mbp.example.co.uk") << true;
  QTest::newRow("punycode") << QStringLiteral("xn--abc.example.com") << true;
  QTest::newRow("digits-in-labels") << QStringLiteral("mac2.lab9.example.com") << true;
  QTest::newRow("two-labels") << QStringLiteral("example.com") << true;
  QTest::newRow("empty") << QString() << false;
  QTest::newRow("one-label") << QStringLiteral("localhost") << false;
  QTest::newRow("ip-address") << QStringLiteral("10.0.0.5") << false;
  QTest::newRow("scheme") << QStringLiteral("https://mbp.example.com") << false;
  QTest::newRow("port") << QStringLiteral("mbp.example.com:24800") << false;
  QTest::newRow("path") << QStringLiteral("mbp.example.com/ssh") << false;
  QTest::newRow("blank-inside") << QStringLiteral("mbp example.com") << false;
  QTest::newRow("blank-around") << QStringLiteral(" mbp.example.com ") << false;
  QTest::newRow("looks-like-an-option") << QStringLiteral("-mbp.example.com") << false;
  QTest::newRow("option-with-value") << QStringLiteral("--url=127.0.0.1.example.com") << false;
  QTest::newRow("hyphen-at-the-end-of-a-label") << QStringLiteral("mbp-.example.com") << false;
  QTest::newRow("empty-label") << QStringLiteral("mbp..example.com") << false;
  QTest::newRow("final-dot") << QStringLiteral("mbp.example.com.") << false;
  QTest::newRow("underscore") << QStringLiteral("mbp_1.example.com") << false;
  QTest::newRow("non-ascii") << QStringLiteral("mbp.\uC608\uC81C.com") << false;
  QTest::newRow("label-of-64") << QStringLiteral("%1.example.com").arg(QString(64, QLatin1Char('a'))) << false;
  QTest::newRow("label-of-63") << QStringLiteral("%1.example.com").arg(QString(63, QLatin1Char('a'))) << true;
  QTest::newRow("name-too-long") << QStringLiteral("%1.com").arg(QStringList(51, QString(4, QLatin1Char('a'))).join('.')
                                    )
                                 << false;
}

void TunnelAddressTests::hostname_isADnsNameThatCloudflaredCannotTakeForAnOption()
{
  QFETCH(QString, hostname);
  QFETCH(bool, valid);
  QCOMPARE(isValidHostname(hostname), valid);
}

void TunnelAddressTests::isActive_needsTheSwitchTlsAndAUsableHostname_data()
{
  QTest::addColumn<bool>("enabled");
  QTest::addColumn<bool>("tls");
  QTest::addColumn<QString>("hostname");
  QTest::addColumn<bool>("active");

  QTest::newRow("all") << true << true << QStringLiteral("mbp.example.com") << true;
  QTest::newRow("switch-off") << false << true << QStringLiteral("mbp.example.com") << false;
  QTest::newRow("tls-off") << true << false << QStringLiteral("mbp.example.com") << false;
  QTest::newRow("no-hostname") << true << true << QString() << false;
  QTest::newRow("bad-hostname") << true << true << QStringLiteral("10.0.0.5") << false;
  QTest::newRow("nothing") << false << false << QString() << false;
}

void TunnelAddressTests::isActive_needsTheSwitchTlsAndAUsableHostname()
{
  QFETCH(bool, enabled);
  QFETCH(bool, tls);
  QFETCH(QString, hostname);
  QFETCH(bool, active);

  TunnelConfig config;
  config.enabled = enabled;
  config.tlsEnabled = tls;
  config.hostname = hostname;
  QCOMPARE(config.isActive(), active);
}

void TunnelAddressTests::port_prefersTheActiveOneThenTheLocalOneThenTheDefault()
{
  TunnelConfig config = usable();
  QCOMPARE(config.port(), 24800);

  config.localPort = 25000;
  QCOMPARE(config.port(), 25000);

  config.activePort = 51823;
  QCOMPARE(config.port(), 51823);

  // an active port that is not usable (nothing was written, or it is damaged) is not used
  config.activePort = 80;
  QCOMPARE(config.port(), 25000);
  config.activePort = 0;
  QCOMPARE(config.port(), 25000);

  config.localPort = 12;
  QCOMPARE(config.port(), kDefaultLocalPort);
}

void TunnelAddressTests::localAddress_isTheLoopbackWithThePort()
{
  TunnelConfig config = usable();
  QCOMPARE(config.localAddress(), QStringLiteral("127.0.0.1:24800"));
  config.activePort = 51823;
  QCOMPARE(config.localAddress(), QStringLiteral("127.0.0.1:51823"));
}

void TunnelAddressTests::effectiveRemoteHost_isTheSavedAddressUnchangedWhenTheTunnelIsNotUsed_data()
{
  QTest::addColumn<QString>("saved");
  for (const auto &saved : kSavedAddresses) {
    QTest::newRow(saved.isEmpty() ? "empty" : qPrintable(saved)) << saved;
  }
}

void TunnelAddressTests::effectiveRemoteHost_isTheSavedAddressUnchangedWhenTheTunnelIsNotUsed()
{
  QFETCH(QString, saved);

  // off, whatever else is set
  TunnelConfig off;
  QCOMPARE(effectiveRemoteHost(off, saved), saved);

  off.hostname = QStringLiteral("mbp.example.com");
  off.localPort = 25000;
  off.activePort = 51823;
  off.cloudflaredPath = QStringLiteral("/opt/homebrew/bin/cloudflared");
  QCOMPARE(effectiveRemoteHost(off, saved), saved);

  // on but not usable: the same
  TunnelConfig noTls = usable();
  noTls.tlsEnabled = false;
  QCOMPARE(effectiveRemoteHost(noTls, saved), saved);

  TunnelConfig noHostname = usable();
  noHostname.hostname.clear();
  QCOMPARE(effectiveRemoteHost(noHostname, saved), saved);
}

void TunnelAddressTests::effectiveRemoteHost_isTheLocalAddressAndNothingElseWhenTheTunnelIsUsed()
{
  TunnelConfig config = usable();
  for (const auto &saved : kSavedAddresses) {
    QCOMPARE(effectiveRemoteHost(config, saved), QStringLiteral("127.0.0.1:24800"));
  }

  config.activePort = 51823;
  QCOMPARE(effectiveRemoteHost(config, QStringLiteral("10.0.0.5")), QStringLiteral("127.0.0.1:51823"));
}

void TunnelAddressTests::displayedTunnelHost_isTheHostnameOnlyWhileTheTunnelIsUsed()
{
  // off: nothing, so that the places that show the saved address keep showing it, unchanged
  TunnelConfig off;
  QVERIFY(displayedTunnelHost(off).isEmpty());
  off.hostname = QStringLiteral("mbp.example.com");
  QVERIFY(displayedTunnelHost(off).isEmpty());

  TunnelConfig config = usable();
  QCOMPARE(displayedTunnelHost(config), QStringLiteral("mbp.example.com"));
  config.hostname = QStringLiteral("other.example.com");
  QCOMPARE(displayedTunnelHost(config), QStringLiteral("other.example.com"));

  // on but not usable: the tunnel is not what is dialled
  TunnelConfig noTls = usable();
  noTls.tlsEnabled = false;
  QVERIFY(displayedTunnelHost(noTls).isEmpty());
}

void TunnelAddressTests::settings_keysAreKnownAndOffByDefault()
{
  const auto keys = Settings::validKeys();
  for (const auto &key :
       {Settings::Tunnel::Enabled, Settings::Tunnel::Hostname, Settings::Tunnel::LocalPort,
        Settings::Tunnel::CloudflaredPath, Settings::Tunnel::ActivePort}) {
    QVERIFY2(keys.contains(key), qPrintable(key));
    QVERIFY2(key.startsWith(QStringLiteral("tunnel/")), qPrintable(key));
  }

  QCOMPARE(Settings::defaultValue(Settings::Tunnel::Enabled), QVariant(false));
  QCOMPARE(Settings::defaultValue(Settings::Tunnel::LocalPort), QVariant(24800));
  QVERIFY(!Settings::defaultValue(Settings::Tunnel::Hostname).isValid());
  QVERIFY(!Settings::defaultValue(Settings::Tunnel::CloudflaredPath).isValid());
  QVERIFY(!Settings::defaultValue(Settings::Tunnel::ActivePort).isValid());

  // and none of the old keys moved
  QCOMPARE(Settings::Client::RemoteHost, QStringLiteral("client/remoteHost"));
  QCOMPARE(Settings::Client::RecentRemoteHosts, QStringLiteral("client/recentRemoteHosts"));
}

void TunnelAddressTests::settings_aConfigFromBeforeTheTunnelIsReadAsOffAndIsNotChangedByReading()
{
  const auto file = QStringLiteral("%1/before/Deskflow.conf").arg(m_dir);
  QVERIFY(QDir().mkpath(QStringLiteral("%1/before").arg(m_dir)));
  // every key is one that Settings keeps as it is on loading (it adds log/level when it is missing)
  const QByteArray before = "[client]\nremoteHost=10.0.0.5\n\n[core]\ncomputerName=test-pc\n\n[log]\nlevel=Info\n\n["
                            "security]\ntlsEnabled=true\n";
  writeAll(file, before);

  Settings::setSettingsFile(file);

  const auto config = TunnelConfig::fromSettings();
  QVERIFY(!config.enabled);
  QVERIFY(!config.isActive());
  QCOMPARE(config.hostname, QString());
  QCOMPARE(config.localPort, 24800);
  QCOMPARE(config.activePort, 0);
  QCOMPARE(config.cloudflaredPath, QString());
  QCOMPARE(effectiveRemoteHost(), QStringLiteral("10.0.0.5"));

  Settings::save(false);
  QCOMPARE(readAll(file), before);
}

void TunnelAddressTests::settings_valuesOfTheTunnelAreKeptAndUnknownKeysOfItsGroupAreRemoved()
{
  const auto file = QStringLiteral("%1/known/Deskflow.conf").arg(m_dir);
  QVERIFY(QDir().mkpath(QStringLiteral("%1/known").arg(m_dir)));
  writeAll(
      file, "[core]\ncomputerName=test-pc\n\n[tunnel]\nenabled=true\nhostname=mbp.example.com\nlocalPort=24801\n"
            "cloudflaredPath=/opt/homebrew/bin/cloudflared\nactivePort=51823\nsomethingElse=1\n"
  );

  Settings::setSettingsFile(file);

  QCOMPARE(Settings::value(Settings::Tunnel::Enabled).toBool(), true);
  QCOMPARE(Settings::value(Settings::Tunnel::Hostname).toString(), QStringLiteral("mbp.example.com"));
  QCOMPARE(Settings::value(Settings::Tunnel::LocalPort).toInt(), 24801);
  QCOMPARE(
      Settings::value(Settings::Tunnel::CloudflaredPath).toString(), QStringLiteral("/opt/homebrew/bin/cloudflared")
  );
  QCOMPARE(Settings::value(Settings::Tunnel::ActivePort).toInt(), 51823);
  QVERIFY(!Settings::value(QStringLiteral("tunnel/somethingElse")).isValid());
}

void TunnelAddressTests::settings_fromSettingsFallsBackToTheDefaultsForUnusableValues()
{
  const auto file = QStringLiteral("%1/unusable/Deskflow.conf").arg(m_dir);
  QVERIFY(QDir().mkpath(QStringLiteral("%1/unusable").arg(m_dir)));
  writeAll(
      file, "[core]\ncomputerName=test-pc\n\n[tunnel]\nenabled=true\nhostname=  MBP.Example.COM.\nlocalPort=70000\n"
            "activePort=twelve\n"
  );
  Settings::setSettingsFile(file);

  const auto config = TunnelConfig::fromSettings();
  QVERIFY(config.enabled);
  QCOMPARE(config.hostname, QStringLiteral("mbp.example.com"));
  QCOMPARE(config.localPort, kDefaultLocalPort);
  QCOMPARE(config.activePort, 0);
  QVERIFY(config.isActive());
  QCOMPARE(config.localAddress(), QStringLiteral("127.0.0.1:24800"));
}

void TunnelAddressTests::settings_effectiveRemoteHostFollowsTheSavedAddressWhenOffAndNeverChangesIt()
{
  const auto file = QStringLiteral("%1/saved/Deskflow.conf").arg(m_dir);
  QVERIFY(QDir().mkpath(QStringLiteral("%1/saved").arg(m_dir)));
  writeAll(file, "[core]\ncomputerName=test-pc\n");
  Settings::setSettingsFile(file);

  // the saved address as a string, as a list (what a comma in the file turns into) and unset: what the client reads
  // is what comes out, byte for byte
  for (const auto &saved : kSavedAddresses) {
    Settings::setValue(Settings::Client::RemoteHost, saved.isEmpty() ? QVariant() : QVariant(saved));
    QCOMPARE(effectiveRemoteHost(), Settings::textValue(Settings::Client::RemoteHost));
  }
  Settings::setValue(
      Settings::Client::RemoteHost, QStringList{QStringLiteral("10.0.0.5"), QStringLiteral("100.64.0.7")}
  );
  QCOMPARE(effectiveRemoteHost(), QStringLiteral("10.0.0.5,100.64.0.7"));
  QCOMPARE(effectiveRemoteHost(), Settings::textValue(Settings::Client::RemoteHost));

  // on: the local address; the saved address is still what it was, and so are the recent ones
  Settings::setValue(Settings::Client::RemoteHost, QStringLiteral("10.0.0.5"));
  Settings::setValue(Settings::Client::RecentRemoteHosts, QStringList{QStringLiteral("10.0.0.5")});
  Settings::setValue(Settings::Tunnel::Enabled, true);
  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("mbp.example.com"));
  QCOMPARE(effectiveRemoteHost(), QStringLiteral("127.0.0.1:24800"));
  QCOMPARE(Settings::textValue(Settings::Client::RemoteHost), QStringLiteral("10.0.0.5"));
  QCOMPARE(
      Settings::value(Settings::Client::RecentRemoteHosts).toStringList(), QStringList{QStringLiteral("10.0.0.5")}
  );

  // off again: the saved address is dialled again
  Settings::setValue(Settings::Tunnel::Enabled, false);
  QCOMPARE(effectiveRemoteHost(), QStringLiteral("10.0.0.5"));

  // TLS off blocks the tunnel even when it is on
  Settings::setValue(Settings::Tunnel::Enabled, true);
  Settings::setValue(Settings::Security::TlsEnabled, false);
  QCOMPARE(effectiveRemoteHost(), QStringLiteral("10.0.0.5"));
}

QTEST_MAIN(TunnelAddressTests)
