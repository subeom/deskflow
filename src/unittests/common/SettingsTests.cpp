/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsTests.h"

#include <QFile>
#include <QSignalSpy>

void SettingsTests::initTestCase()
{
  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();
}

void SettingsTests::setSettingsFile()
{
  Settings::setSettingsFile(m_settingsFile);
}

void SettingsTests::setStateFile()
{
  Settings::setStateFile(m_stateFile);
}

void SettingsTests::settingsFile()
{
  QVERIFY(Settings::settingsFile().endsWith(m_settingsFile));
}

void SettingsTests::settingsPath()
{
  QVERIFY(Settings::settingsPath().endsWith(m_settingsPath));
}

void SettingsTests::tlsDir()
{
  QVERIFY(Settings::tlsDir().endsWith(m_expectedTlsDir));
}

void SettingsTests::tlsTrustedServersDb()
{
  QVERIFY(Settings::tlsTrustedServersDb().endsWith(m_expectedTlsServerDB));
}

void SettingsTests::tlsTrustedClientsDb()
{
  QVERIFY(Settings::tlsTrustedClientsDb().endsWith(m_expectedTlsClientDB));
}

void SettingsTests::checkValidSettings()
{
  QSignalSpy spy(Settings::instance(), &Settings::settingsChanged);
  QVERIFY(spy.isValid());

  const auto keysToCheck = QRegularExpression(QLatin1String("[^%1]").arg(Settings::Core::ComputerName));
  const auto validKeys = Settings::validKeys().filter(keysToCheck);
  for (const auto &setting : validKeys) {
    const auto value = Settings::value(setting).toString();
    QCOMPARE(Settings::defaultValue(setting).toString(), value);

    Settings::setValue(setting, "NEW_VALUE");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<QString>(spy.first().at(0)), setting);
    QCOMPARE(Settings::value(setting).toString(), "NEW_VALUE");

    Settings::setValue(setting);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(Settings::value(setting).toString(), value);

    // Reset the spy for the next loop
    spy.clear();
    QCOMPARE(spy.count(), 0);
  }
}

void SettingsTests::clipboardTextOnly_isOffByDefaultAndRoundTrips()
{
  const auto key = Settings::Server::ClipboardTextOnly;
  QVERIFY(Settings::validKeys().contains(key));

  // a config from before the setting existed has no value for it: it is off
  Settings::setValue(key);
  QCOMPARE(Settings::defaultValue(key), QVariant(false));
  QCOMPARE(Settings::value(key), QVariant(false));

  Settings::setValue(key, true);
  QCOMPARE(Settings::value(key).toBool(), true);
  Settings::setValue(key, false);
  QCOMPARE(Settings::value(key).toBool(), false);
  Settings::setValue(key);
  QCOMPARE(Settings::value(key).toBool(), false);
}

void SettingsTests::experimentalMultiMonitor_isOffByDefaultAndRoundTrips()
{
  const auto key = Settings::Server::ExperimentalMultiMonitor;
  QVERIFY(Settings::validKeys().contains(key));

  // a config from before the setting existed has no value for it: it is off
  Settings::setValue(key);
  QCOMPARE(Settings::defaultValue(key), QVariant(false));
  QCOMPARE(Settings::value(key), QVariant(false));

  Settings::setValue(key, true);
  QCOMPARE(Settings::value(key).toBool(), true);
  Settings::setValue(key, false);
  QCOMPARE(Settings::value(key).toBool(), false);
  Settings::setValue(key);
  QCOMPARE(Settings::value(key).toBool(), false);
}

void SettingsTests::monitorLayout_isEmptyByDefaultAndRoundTrips()
{
  const auto key = Settings::Computer::MonitorLayout.arg(QStringLiteral("some-pc"));
  QCOMPARE(key, QStringLiteral("computer_some-pc/monitorLayout"));

  // a config from before the setting existed has no layout for a computer: it is not used
  Settings::setValue(key);
  QCOMPARE(Settings::defaultValue(key).toString(), QString());
  QCOMPARE(Settings::value(key).toString(), QString());

  Settings::setValue(key, QStringLiteral("0,0,2560,1440;2560,0,1920,1080"));
  QCOMPARE(Settings::value(key).toString(), QStringLiteral("0,0,2560,1440;2560,0,1920,1080"));
  Settings::setValue(key);
  QCOMPARE(Settings::value(key).toString(), QString());
}

void SettingsTests::connectTimeout_defaultsToTwoSecondsAndRoundTrips()
{
  const auto key = Settings::Client::ConnectTimeout;
  QCOMPARE(key, QStringLiteral("client/connectTimeout"));
  QVERIFY(Settings::validKeys().contains(key));

  // a config from before the setting existed has no value for it: the limit stays the 2 s it always was
  Settings::setValue(key);
  QCOMPARE(Settings::defaultValue(key), QVariant(2.0));
  QCOMPARE(Settings::value(key).toDouble(), 2.0);

  Settings::setValue(key, 7.5);
  QCOMPARE(Settings::value(key).toDouble(), 7.5);
  Settings::setValue(key);
  QCOMPARE(Settings::value(key).toDouble(), 2.0);
}

void SettingsTests::checkCleanComputerName()
{
  const auto input = QStringLiteral("--!_ _-C@o#mpu$t%e^&*(r)= +Name\n[1]2|3?4--5>6<,7`~/8*90\\.lan--..    ..");
  const auto expected = QStringLiteral("Computer_Name_1234--567890.lan");

  Settings::setValue(Settings::Core::ComputerName, input);

  QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), expected);
}

void SettingsTests::checkCleanComputerName_LongName()
{
  QString input;
  input.fill('f', 300);
  input.prepend('.');

  QString expected;
  expected.fill('f', 255);

  Settings::setValue(Settings::Core::ComputerName, input);

  QCOMPARE(Settings::value(Settings::Core::ComputerName).toString(), expected);
}

void SettingsTests::checkClearState()
{
  Settings::setValue(Settings::Gui::WindowGeometry, QRect(1, 2, 3, 4));
  QVERIFY(Settings::value(Settings::Gui::WindowGeometry).toRect().isValid());

  Settings::clearState();

  QVERIFY(!Settings::value(Settings::Gui::WindowGeometry).toRect().isValid());
}

QTEST_MAIN(SettingsTests)
