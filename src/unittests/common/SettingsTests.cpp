/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsTests.h"

#include <QDir>
#include <QFile>
#include <QRect>
#include <QScopeGuard>
#include <QSettings>
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

void SettingsTests::appearance_defaultsToSystemAndRoundTrips()
{
  const auto key = Settings::Gui::Appearance;
  QCOMPARE(key, QStringLiteral("gui/appearance"));
  QVERIFY(Settings::validKeys().contains(key));

  // a config from before the setting existed has no value for it: the look is the one that the OS gave the program
  Settings::setValue(key);
  QCOMPARE(Settings::defaultValue(key), QVariant(QStringLiteral("system")));
  QCOMPARE(Settings::value(key).toString(), QStringLiteral("system"));

  Settings::setValue(key, QStringLiteral("deskflow"));
  QCOMPARE(Settings::value(key).toString(), QStringLiteral("deskflow"));
  Settings::setValue(key);
  QCOMPARE(Settings::value(key).toString(), QStringLiteral("system"));
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

void SettingsTests::nativeWindowFrame_isTheFrameOfTheSystemOnLinuxOnlyByDefault()
{
  // the frame of the new look is what Windows and macOS have; Linux has the frame of the system until it is chosen
  const bool linux = QSysInfo::kernelType() == QLatin1String("linux");
  QCOMPARE(
      Settings::defaultValue(Settings::Gui::NativeWindowFrame).toBool(),
      linux && Settings::kLinuxNativeWindowFrameByDefault
  );
  QCOMPARE(
      Settings::value(Settings::Gui::NativeWindowFrame).toBool(), linux && Settings::kLinuxNativeWindowFrameByDefault
  );
  // a choice is kept: true is the frame of the system, false the frame of the new look
  Settings::setValue(Settings::Gui::NativeWindowFrame, true);
  QVERIFY(Settings::value(Settings::Gui::NativeWindowFrame).toBool());
  Settings::setValue(Settings::Gui::NativeWindowFrame, false);
  QVERIFY(!Settings::value(Settings::Gui::NativeWindowFrame).toBool());
  Settings::setValue(Settings::Gui::NativeWindowFrame);
  QCOMPARE(
      Settings::value(Settings::Gui::NativeWindowFrame).toBool(), linux && Settings::kLinuxNativeWindowFrameByDefault
  );
}

void SettingsTests::portableMode_keepsTheStateKeysWhenTheSettingsAreReadAgain()
{
  // portable mode is "the file settings/Deskflow.conf is next to the program": the test makes it and takes it away
  const auto portable = Settings::portableSettingsFile();
  if (QFile::exists(portable))
    QSKIP("a portable settings file is next to the test program, it is not ours to move");
  QVERIFY(QDir().mkpath(QFileInfo(portable).absolutePath()));
  QFile marker(portable);
  QVERIFY(marker.open(QIODevice::WriteOnly));
  marker.close();
  const auto cleanup = qScopeGuard([&] {
    QFile::remove(portable);
    QDir().rmdir(QFileInfo(portable).absolutePath());
  });
  QVERIFY(Settings::isPortableMode());

  // in portable mode the state keys are written to the settings file (there is no state file of their own)
  const QStringList stateKeys = {
      Settings::Gui::WindowGeometry,     Settings::Gui::ShellWindowGeometry,    Settings::Client::XdpClipboardRetried,
      Settings::Client::XdpRestoreToken, Settings::Server::XdpClipboardRetried, Settings::Server::XdpRestoreToken,
  };
  const auto reread = QStringLiteral("%1/Deskflow-portable.conf").arg(m_settingsPathTemp);
  QFile::remove(reread);
  {
    QSettings written(reread, QSettings::IniFormat);
    written.setValue(Settings::Gui::ShellWindowGeometry, QRect(208, 175, 1423, 827));
    written.setValue(Settings::Gui::WindowGeometry, QRect(10, 20, 640, 480));
    written.setValue(Settings::Client::XdpClipboardRetried, true);
    written.setValue(Settings::Client::XdpRestoreToken, QStringLiteral("token-a"));
    written.setValue(Settings::Server::XdpClipboardRetried, true);
    written.setValue(Settings::Server::XdpRestoreToken, QStringLiteral("token-b"));
    // a key that no version of the program has is still taken away
    written.setValue(QStringLiteral("gui/noSuchKey"), QStringLiteral("x"));
    written.sync();
  }

  // the settings are read again, as at the start of the program
  Settings::setSettingsFile(reread);
  QCOMPARE(Settings::value(Settings::Gui::ShellWindowGeometry).toRect(), QRect(208, 175, 1423, 827));
  QCOMPARE(Settings::value(Settings::Gui::WindowGeometry).toRect(), QRect(10, 20, 640, 480));
  QCOMPARE(Settings::value(Settings::Client::XdpRestoreToken).toString(), QStringLiteral("token-a"));
  QCOMPARE(Settings::value(Settings::Server::XdpRestoreToken).toString(), QStringLiteral("token-b"));
  QVERIFY(Settings::value(Settings::Client::XdpClipboardRetried).toBool());
  QVERIFY(Settings::value(Settings::Server::XdpClipboardRetried).toBool());
  QSettings after(reread, QSettings::IniFormat);
  for (const auto &key : stateKeys)
    QVERIFY2(after.contains(key), qPrintable(key));
  QVERIFY(!after.contains(QStringLiteral("gui/noSuchKey")));

  // a size written the way the program writes it comes back after another start
  Settings::setValue(Settings::Gui::ShellWindowGeometry, QRect(1, 2, 1100, 700));
  Settings::setSettingsFile(m_settingsFile);
  Settings::setSettingsFile(reread);
  QCOMPARE(Settings::value(Settings::Gui::ShellWindowGeometry).toRect(), QRect(1, 2, 1100, 700));

  // without portable mode the state keys live in the state file, and one that is found in the settings file is old
  // litter that is taken away, as it always was
  QFile::remove(portable);
  QVERIFY(!Settings::isPortableMode());
  {
    QSettings litter(reread, QSettings::IniFormat);
    litter.setValue(Settings::Gui::ShellWindowGeometry, QRect(3, 4, 900, 600));
    litter.sync();
  }
  Settings::setSettingsFile(m_settingsFile);
  Settings::setSettingsFile(reread);
  QSettings leftOver(reread, QSettings::IniFormat);
  QVERIFY(!leftOver.contains(Settings::Gui::ShellWindowGeometry));

  Settings::setSettingsFile(m_settingsFile);
  QFile::remove(reread);
}

QTEST_MAIN(SettingsTests)
