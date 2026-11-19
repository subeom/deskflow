/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsTests.h"

#include <QDir>
#include <QFile>
#include <QRect>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

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

void SettingsTests::appearance_defaultsToTheNewLookAndRoundTrips()
{
  const auto key = Settings::Gui::Appearance;
  QCOMPARE(key, QStringLiteral("gui/appearance"));
  QVERIFY(Settings::validKeys().contains(key));

  // a config that has no value for it (a first run, or a version from before the setting) has the look of the
  // application: the one that is the same on every OS
  Settings::setValue(key);
  QCOMPARE(Settings::defaultValue(key), QVariant(QStringLiteral("deskflow")));
  QCOMPARE(Settings::value(key).toString(), QStringLiteral("deskflow"));

  // the look of the system is a choice, and it is kept
  Settings::setValue(key, QStringLiteral("system"));
  QCOMPARE(Settings::value(key).toString(), QStringLiteral("system"));
  Settings::setValue(key, QStringLiteral("deskflow"));
  QCOMPARE(Settings::value(key).toString(), QStringLiteral("deskflow"));
  Settings::setValue(key);
  QCOMPARE(Settings::value(key).toString(), QStringLiteral("deskflow"));
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

namespace {

// The program is started again: the settings and the state are read again, and what is not kept is taken away
void startAgain(const QString &settingsFile, const QString &stateFile, const QString &otherSettingsFile)
{
  Settings::setSettingsFile(otherSettingsFile);
  Settings::setSettingsFile(settingsFile);
  Settings::setStateFile(otherSettingsFile + QStringLiteral(".state"));
  Settings::setStateFile(stateFile);
}

} // namespace

void SettingsTests::keys_aChoiceOfTheNewLookIsKeptWhenTheProgramIsStartedAgain_data()
{
  QTest::addColumn<QString>("key");
  QTest::addColumn<QVariant>("value");

  // The keys that the new look writes. A key that is not kept was written, and taken away when the program was
  // started again: the switch of the frame of the window was off at every start (Windows, the 6th run of the checks).
  // These are all of its keys that the person chooses, and the ones that the window writes by itself
  using S = Settings::Gui;
  QTest::newRow("appearance") << S::Appearance << QVariant(QStringLiteral("deskflow"));
  QTest::newRow("text size") << S::TextSize << QVariant(130);
  QTest::newRow("the frame of the window") << S::NativeWindowFrame << QVariant(true);
  QTest::newRow("the sidebar is an icon rail") << S::SidebarCollapsed << QVariant(true);
  QTest::newRow("the log is shown") << S::ShellLogExpanded << QVariant(true);
  QTest::newRow("the tray icon is symbolic") << S::SymbolicTrayIcon << QVariant(false);
  QTest::newRow("the window is closed to the tray") << S::CloseToTray << QVariant(false);
  QTest::newRow("the window of the new look") << S::ShellWindowGeometry << QVariant(QRect(208, 175, 1000, 620));
  QTest::newRow("the window of the system look") << S::WindowGeometry << QVariant(QRect(10, 20, 640, 480));
  QTest::newRow("clipboard of text only") << Settings::Server::ClipboardTextOnly << QVariant(true);
  QTest::newRow("the monitors are arranged") << Settings::Server::ExperimentalMultiMonitor << QVariant(true);
  QTest::newRow("a connect timeout") << Settings::Client::ConnectTimeout << QVariant(7.5);
}

void SettingsTests::keys_aChoiceOfTheNewLookIsKeptWhenTheProgramIsStartedAgain()
{
  QFETCH(QString, key);
  QFETCH(QVariant, value);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto file = dir.filePath(QStringLiteral("Deskflow.conf"));
  const auto state = dir.filePath(QStringLiteral("Deskflow.state"));
  const auto other = dir.filePath(QStringLiteral("Other.conf"));
  const auto restore = qScopeGuard([&] {
    Settings::setSettingsFile(m_settingsFile);
    Settings::setStateFile(m_stateFile);
  });
  Settings::setSettingsFile(file);
  Settings::setStateFile(state);

  // the choice is written the way the program writes it ...
  Settings::setValue(key, value);
  QCOMPARE(Settings::value(key), value);
  // ... and it is still the choice after the program is started again, twice (a key is taken away at the start)
  for (int start = 1; start <= 2; ++start) {
    startAgain(file, state, other);
    // (in a file: the file of the settings, or the file of the state for the keys that it keeps)
    const bool stored =
        QSettings(file, QSettings::IniFormat).contains(key) || QSettings(state, QSettings::IniFormat).contains(key);
    QVERIFY2(stored, qPrintable(QStringLiteral("%1 is gone after start %2").arg(key).arg(start)));
    QCOMPARE(Settings::value(key), value);
  }
}

namespace {

struct Declared
{
  // Struct::Name of the key and the key
  QMap<QString, QString> keys;
  QStringList valid;
  QStringList state;
  QStringList defaultFalse;
  QStringList defaultTrue;
};

// What is declared in Settings.h: the keys of its structures and the lists that say what is done with them
Declared readDeclarations()
{
  Declared declared;
  QFile header(QStringLiteral(DESKFLOW_SETTINGS_HEADER));
  if (!header.open(QIODevice::ReadOnly | QIODevice::Text))
    return declared;
  const auto text = QString::fromUtf8(header.readAll());

  const auto structs = QRegularExpression(QStringLiteral(R"(struct\s+(\w+)\s*\{)")).globalMatch(text);
  QList<QPair<int, QString>> starts;
  for (auto it = structs; it.hasNext();) {
    const auto match = it.next();
    starts.append({int(match.capturedStart()), match.captured(1)});
  }
  const auto keys =
      QRegularExpression(QStringLiteral(R"re(inline static const auto (\w+) = QStringLiteral\("([^"]*)"\);)re"))
          .globalMatch(text);
  for (auto it = keys; it.hasNext();) {
    const auto match = it.next();
    QString owner;
    for (const auto &start : starts)
      if (start.first < match.capturedStart())
        owner = start.second;
    declared.keys.insert(QStringLiteral("%1::%2").arg(owner, match.captured(1)), match.captured(2));
  }

  const auto list = [&text](const QString &name) {
    QStringList names;
    const auto block = QRegularExpression(QStringLiteral("%1\\s*=\\s*\\{([^}]*)\\}").arg(name)).match(text);
    for (auto it = QRegularExpression(QStringLiteral(R"((\w+::\w+))")).globalMatch(block.captured(1)); it.hasNext();)
      names.append(it.next().captured(1));
    return names;
  };
  declared.valid = list(QStringLiteral("m_validKeys"));
  declared.state = list(QStringLiteral("m_stateKeys"));
  declared.defaultFalse = list(QStringLiteral("m_defaultFalseValues"));
  declared.defaultTrue = list(QStringLiteral("m_defaultTrueValues"));
  return declared;
}

} // namespace

void SettingsTests::keys_everyKeyThatIsDeclaredIsEitherKeptOrAStateKey()
{
  const auto declared = readDeclarations();
  QVERIFY2(declared.keys.size() > 50, "the declarations of Settings.h were not read");
  QVERIFY(declared.valid.size() > 50);
  QVERIFY(declared.state.size() >= 6);

  // The keys of a computer are made from a pattern, and the keys of the old configuration file of the server are only
  // read to be moved: neither is a setting that is kept as it is
  const QStringList notSettings = {QStringLiteral("Computer"), QStringLiteral("InternalConfig")};
  QStringList lost;
  for (auto it = declared.keys.cbegin(); it != declared.keys.cend(); ++it) {
    const auto owner = it.key().section(QStringLiteral("::"), 0, 0);
    if (notSettings.contains(owner))
      continue;
    // the program takes away the keys that are not in the list of the valid keys (cleanSettings), but those of the
    // state, which are kept in the state file (and in the settings file of a portable program)
    if (!declared.valid.contains(it.key()) && !declared.state.contains(it.key()))
      lost.append(QStringLiteral("%1 (%2)").arg(it.key(), it.value()));
  }
  QVERIFY2(
      lost.isEmpty(),
      qPrintable(QStringLiteral("declared and taken away at every start of the program: %1").arg(lost.join(", ")))
  );
  // and the valid keys are keys that are declared (a name that is not there would not be compiled, but a typo that is
  // another key would)
  for (const auto &name : declared.valid + declared.state)
    QVERIFY2(declared.keys.contains(name), qPrintable(name));
}

void SettingsTests::keys_theListsOfDefaultsHaveNoKeyThatTheProgramDoesNotKnow()
{
  const auto declared = readDeclarations();
  QVERIFY(declared.defaultFalse.size() > 10);
  QVERIFY(declared.defaultTrue.size() > 3);

  // a default is for a key that is kept (or is a key of the state): the default of a key that is taken away at the
  // start is never seen, and it hides that it is not kept
  QStringList unknown;
  for (const auto &name : declared.defaultFalse + declared.defaultTrue)
    if (!declared.valid.contains(name) && !declared.state.contains(name))
      unknown.append(name);
  QVERIFY2(unknown.isEmpty(), qPrintable(QStringLiteral("a default but not kept: %1").arg(unknown.join(", "))));

  // the default of the frame of the window is the one of the platform (Settings::defaultValue says it), not a name in a
  // list of the ones that are false: on Linux it is true
  QVERIFY(!declared.defaultFalse.contains(QStringLiteral("Gui::NativeWindowFrame")));
  QVERIFY(!declared.defaultTrue.contains(QStringLiteral("Gui::NativeWindowFrame")));

  // no key is true and false by default
  for (const auto &name : declared.defaultTrue)
    QVERIFY2(!declared.defaultFalse.contains(name), qPrintable(name));
}

QTEST_MAIN(SettingsTests)
