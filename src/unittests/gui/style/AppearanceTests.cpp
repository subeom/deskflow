/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AppearanceTests.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "gui/style/Appearance.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/TextSize.h"

#include <QApplication>
#include <QFile>
#include <QScopeGuard>
#include <QSettings>
#include <QStyle>
#include <QTemporaryDir>

using namespace deskflow::gui::style;

namespace {

// Puts the application back to the style that the test started with
class StyleGuard
{
public:
  StyleGuard() : m_name(QApplication::style()->name())
  {
  }
  ~StyleGuard()
  {
    QApplication::setStyle(m_name);
    QApplication::setPalette(QPalette());
  }

private:
  QString m_name;
};

} // namespace

void AppearanceTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("Deskflow.state")));
}

void AppearanceTests::init()
{
  qunsetenv(kAppearanceEnvironment);
  Settings::setValue(Settings::Gui::Appearance);
}

void AppearanceTests::cleanup()
{
  // the new look has its size of text, which the next test does not want
  TextSize::uninstall();
  qunsetenv(kAppearanceEnvironment);
  qunsetenv("XDG_CURRENT_DESKTOP");
  Settings::setValue(Settings::Gui::Appearance);
}

void AppearanceTests::mode_isReadFromTextAndWrittenBack()
{
  QCOMPARE(modeFromString(QStringLiteral("deskflow"), Mode::System), Mode::Deskflow);
  QCOMPARE(modeFromString(QStringLiteral("  Deskflow "), Mode::System), Mode::Deskflow);
  QCOMPARE(modeFromString(QStringLiteral("SYSTEM"), Mode::Deskflow), Mode::System);
  // what is not a mode is the fallback
  QCOMPARE(modeFromString(QStringLiteral("glass"), Mode::Deskflow), Mode::Deskflow);
  QCOMPARE(modeFromString(QString(), Mode::System), Mode::System);
  QCOMPARE(modeToString(Mode::Deskflow), QStringLiteral("deskflow"));
  QCOMPARE(modeToString(Mode::System), QStringLiteral("system"));
}

void AppearanceTests::configuredMode_isTheNewLookUntilAskedOtherwise()
{
  // nothing is asked: the application has the look of its own, the same on every OS
  QCOMPARE(kDefaultMode, Mode::Deskflow);
  QCOMPARE(configuredMode(), Mode::Deskflow);
}

void AppearanceTests::configuredMode_isTheDefaultOfTheSetting()
{
  // two places say what the default is (the settings, which cannot see this layer, and this layer): one word
  QCOMPARE(Settings::defaultValue(Settings::Gui::Appearance).toString(), modeToString(kDefaultMode));
  QCOMPARE(Settings::value(Settings::Gui::Appearance).toString(), modeToString(kDefaultMode));
}

void AppearanceTests::configuredMode_theSettingAsks_andTheEnvironmentOverrides()
{
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("system"));
  QCOMPARE(configuredMode(), Mode::System);

  // for one run: the environment is stronger than the file, both ways
  qputenv(kAppearanceEnvironment, "deskflow");
  QCOMPARE(configuredMode(), Mode::Deskflow);
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("deskflow"));
  qputenv(kAppearanceEnvironment, "system");
  QCOMPARE(configuredMode(), Mode::System);

  // a value that is no mode is not an answer: the setting is
  qputenv(kAppearanceEnvironment, "glass");
  QCOMPARE(configuredMode(), Mode::Deskflow);
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("system"));
  QCOMPARE(configuredMode(), Mode::System);
}

void AppearanceTests::configuredMode_followsTheTableOfTheSettingAndTheEnvironment_data()
{
  QTest::addColumn<QString>("setting");
  QTest::addColumn<QString>("environment");
  QTest::addColumn<Mode>("mode");

  const auto none = QString();
  const auto deskflow = Mode::Deskflow;
  const auto system = Mode::System;

  // The one who has not chosen is the new look, and so is the one whose value is no mode (a typing error, or a value of
  // a later version): the rule is one sentence, "what is not an answer is the default". The one who chose the look of
  // the system keeps it, however the value is written
  QTest::newRow("nothing") << none << none << deskflow;
  QTest::newRow("the new look") << QStringLiteral("deskflow") << none << deskflow;
  QTest::newRow("the system") << QStringLiteral("system") << none << system;
  QTest::newRow("the system, in capitals and with spaces") << QStringLiteral("  SYSTEM ") << none << system;
  QTest::newRow("no mode") << QStringLiteral("nonsense") << none << deskflow;
  QTest::newRow("a mode of a later version") << QStringLiteral("glass") << none << deskflow;
  // the way back to the look of before, for one run
  QTest::newRow("the environment says the system") << none << QStringLiteral("system") << system;
  QTest::newRow("the environment says the system, in capitals") << none << QStringLiteral("SYSTEM") << system;
  QTest::newRow("the environment is stronger than the new look")
      << QStringLiteral("deskflow") << QStringLiteral("system") << system;
  QTest::newRow("the environment is stronger than the system")
      << QStringLiteral("system") << QStringLiteral("deskflow") << deskflow;
  QTest::newRow("the environment says the new look") << none << QStringLiteral("deskflow") << deskflow;
  // what is no answer of the environment is left to the setting, as it was
  QTest::newRow("the environment is no mode, the system is stored")
      << QStringLiteral("system") << QStringLiteral("glass") << system;
  QTest::newRow("the environment is no mode, nothing is stored") << none << QStringLiteral("glass") << deskflow;
  QTest::newRow("the environment is empty, the system is stored") << QStringLiteral("system") << none << system;
}

void AppearanceTests::configuredMode_followsTheTableOfTheSettingAndTheEnvironment()
{
  QFETCH(QString, setting);
  QFETCH(QString, environment);
  QFETCH(Mode, mode);

  if (!setting.isEmpty())
    Settings::setValue(Settings::Gui::Appearance, setting);
  if (!environment.isEmpty())
    qputenv(kAppearanceEnvironment, environment.toUtf8());
  QCOMPARE(configuredMode(), mode);
}

namespace {

// The program is started again: the settings are read again, and what is not kept is taken away (as Settings does it)
void startAgain(const QString &file, const QString &other)
{
  Settings::setSettingsFile(other);
  Settings::setSettingsFile(file);
}

} // namespace

void AppearanceTests::firstRun_withoutAFileOpensTheNewLook_andWritesNothing()
{
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto file = dir.filePath(QStringLiteral("Deskflow.conf"));
  const auto other = dir.filePath(QStringLiteral("Other.conf"));
  const auto restore =
      qScopeGuard([&] { Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf"))); });
  QVERIFY(!QFile::exists(file));
  Settings::setSettingsFile(file);

  QCOMPARE(configuredMode(), Mode::Deskflow);
  // asking, starting and showing the pages do not choose for the person: a later version can still change the default
  QVERIFY(!Settings::proxy().contains(Settings::Gui::Appearance));
  {
    StyleGuard guard;
    applyAppearance();
    QVERIFY(isDeskflowStyleInstalled());
  }
  startAgain(file, other);
  startAgain(file, other);
  QCOMPARE(configuredMode(), Mode::Deskflow);
  QVERIFY(!Settings::proxy().contains(Settings::Gui::Appearance));
  QVERIFY(!QSettings(file, QSettings::IniFormat).contains(Settings::Gui::Appearance));
}

void AppearanceTests::anUpgrade_withTheSystemLookStoredKeepsItAtEveryStart()
{
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto file = dir.filePath(QStringLiteral("Deskflow.conf"));
  const auto other = dir.filePath(QStringLiteral("Other.conf"));
  const auto restore =
      qScopeGuard([&] { Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf"))); });
  {
    // a file that the person's program wrote when its choice was the look of the system
    QSettings written(file, QSettings::IniFormat);
    written.setValue(QStringLiteral("gui/appearance"), QStringLiteral("system"));
    written.setValue(QStringLiteral("core/port"), 24800);
    written.sync();
  }

  for (int start = 1; start <= 3; ++start) {
    startAgain(file, other);
    QVERIFY2(Settings::proxy().contains(Settings::Gui::Appearance), qPrintable(QString::number(start)));
    QCOMPARE(configuredMode(), Mode::System);
    QCOMPARE(
        QSettings(file, QSettings::IniFormat).value(Settings::Gui::Appearance).toString(), QStringLiteral("system")
    );
  }
  // the application is not asked to be anything else by starting it
  StyleGuard guard;
  applyAppearance();
  QVERIFY(!isDeskflowStyleInstalled());
}

void AppearanceTests::anUpgrade_withAFileThatHasNoLook_opensTheNewLook()
{
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto file = dir.filePath(QStringLiteral("Deskflow.conf"));
  const auto other = dir.filePath(QStringLiteral("Other.conf"));
  const auto restore =
      qScopeGuard([&] { Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf"))); });
  {
    // the file of a version from before the look: it has settings, and not this one
    QSettings written(file, QSettings::IniFormat);
    written.setValue(QStringLiteral("core/port"), 24801);
    written.setValue(QStringLiteral("gui/closeToTray"), false);
    written.sync();
  }
  startAgain(file, other);
  QCOMPARE(configuredMode(), Mode::Deskflow);
  QVERIFY(!Settings::proxy().contains(Settings::Gui::Appearance));
  // what the person chose is left as it was
  QCOMPARE(Settings::value(Settings::Core::Port).toInt(), 24801);
  QCOMPARE(Settings::value(Settings::Gui::CloseToTray).toBool(), false);
}

void AppearanceTests::apply_system_leavesTheApplicationAsToday()
{
  StyleGuard guard;
  // the look of the system is chosen: it is not what the application has when nothing is chosen
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("system"));
  qputenv("XDG_CURRENT_DESKTOP", "GNOME");
  QApplication::setStyle(QStringLiteral("Windows"));
  const auto *before = QApplication::style();

  applyAppearance();
  QVERIFY(!isDeskflowStyleInstalled());
  if (deskflow::platform::isMac()) {
    // macOS has always been left to its own style
    QCOMPARE(QApplication::style(), before);
  } else {
    // everywhere else the application has always asked for Fusion
    QCOMPARE(QApplication::style()->name().toLower(), QStringLiteral("fusion"));
  }
}

void AppearanceTests::apply_systemOnKde_leavesTheStyleToKde()
{
  StyleGuard guard;
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("system"));
  qputenv("XDG_CURRENT_DESKTOP", "KDE");
  QApplication::setStyle(QStringLiteral("Windows"));
  const auto *before = QApplication::style();

  applyAppearance();
  QVERIFY(!isDeskflowStyleInstalled());
  QCOMPARE(QApplication::style(), before);
}

void AppearanceTests::apply_deskflow_installsTheStyleAndItsPalette()
{
  StyleGuard guard;
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("deskflow"));

  applyAppearance();
  QVERIFY(isDeskflowStyleInstalled());
  const auto tokens = currentTokens();
  QCOMPARE(QApplication::palette().color(QPalette::Window), tokens.colors.bg);
  QCOMPARE(QApplication::palette().color(QPalette::Highlight), tokens.colors.accent);
  QVERIFY(!tokens.flatBackdrop);
}

void AppearanceTests::apply_withoutAChoice_installsTheNewLook()
{
  StyleGuard guard;
  QApplication::setStyle(QStringLiteral("Windows"));
  QVERIFY(!Settings::proxy().contains(Settings::Gui::Appearance));

  applyAppearance();
  QVERIFY(isDeskflowStyleInstalled());
  // and the way back for one run, with the environment, gives the look of before (the application is applied once, at
  // its start: here it is a new start)
  QApplication::setStyle(QStringLiteral("Windows"));
  qputenv(kAppearanceEnvironment, "system");
  applyAppearance();
  QVERIFY(!isDeskflowStyleInstalled());
}

void AppearanceTests::currentTokens_withoutTheStyle_areFlatAndOpaqueInTheColoursOfTheOs()
{
  StyleGuard guard;
  QApplication::setStyle(QStringLiteral("Fusion"));
  QVERIFY(!isDeskflowStyleInstalled());

  const auto tokens = currentTokens();
  const auto look = currentLook();
  QVERIFY(look.osColors);
  QVERIFY(tokens.flatBackdrop);
  QCOMPARE(tokens.panelOpacity, 1.0);
  QCOMPARE(tokens.colors.bg, QApplication::palette().color(QPalette::Window));
  QCOMPARE(tokens.colors.surface, QApplication::palette().color(QPalette::Base));
}

void AppearanceTests::currentTokens_withTheStyle_areTheOnesOfItsLook()
{
  StyleGuard guard;
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look dark;
  dark.scheme = Scheme::Dark;
  style->useLook(dark);

  QVERIFY(isDeskflowStyleInstalled());
  QCOMPARE(currentLook(), dark);
  QCOMPARE(currentTokens().colors.bg, tokensFor(dark).colors.bg);
}

QTEST_MAIN(AppearanceTests)
