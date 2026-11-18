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
#include <QStyle>

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

void AppearanceTests::configuredMode_isSystemUntilAskedOtherwise()
{
  // nothing is asked: the application looks as it always did
  QCOMPARE(configuredMode(), Mode::System);
}

void AppearanceTests::configuredMode_theSettingAsks_andTheEnvironmentOverrides()
{
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("deskflow"));
  QCOMPARE(configuredMode(), Mode::Deskflow);

  // for one run: the environment is stronger than the file
  qputenv(kAppearanceEnvironment, "system");
  QCOMPARE(configuredMode(), Mode::System);

  // a value that is no mode is not an answer: the setting is
  qputenv(kAppearanceEnvironment, "glass");
  QCOMPARE(configuredMode(), Mode::Deskflow);

  // and a setting that is no mode is System
  qunsetenv(kAppearanceEnvironment);
  Settings::setValue(Settings::Gui::Appearance, QStringLiteral("nonsense"));
  QCOMPARE(configuredMode(), Mode::System);
}

void AppearanceTests::apply_system_leavesTheApplicationAsToday()
{
  StyleGuard guard;
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
