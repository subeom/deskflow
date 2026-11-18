/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ChromePolicyTests.h"

#include "gui/chrome/ChromePolicy.h"

#include <QGuiApplication>

using namespace deskflow::gui::chrome;

Q_DECLARE_METATYPE(ChromeEnvironment)
Q_DECLARE_METATYPE(FrameKind)
Q_DECLARE_METATYPE(FrameReason)
Q_DECLARE_METATYPE(Platform)

namespace {

ChromeEnvironment environment(
    Platform platform, const QString &plugin, const QVersionNumber &qt = QVersionNumber(6, 9, 3),
    bool deskflowLook = true
)
{
  ChromeEnvironment env;
  env.platform = platform;
  env.platformPlugin = plugin;
  env.qtVersion = qt;
  env.deskflowLook = deskflowLook;
  return env;
}

} // namespace

void ChromePolicyTests::decideFrame_followsTheRulesInTheOrderOfTheirSafety_data()
{
  QTest::addColumn<ChromeEnvironment>("env");
  QTest::addColumn<FrameKind>("kind");
  QTest::addColumn<FrameReason>("reason");

  const auto native = FrameKind::Native;
  const auto client = FrameKind::ClientSide;

  // the look of the system is the window that it always was, whatever else is asked
  auto system = environment(Platform::Windows, QStringLiteral("windows"), QVersionNumber(6, 9, 3), false);
  QTest::newRow("system look, Windows") << system << native << FrameReason::SystemLook;
  system.frameVariable = QStringLiteral("client");
  QTest::newRow("system look, the trial forces the frame") << system << native << FrameReason::SystemLook;

  // what each system can do
  QTest::newRow("Windows") << environment(Platform::Windows, QStringLiteral("windows")) << client
                           << FrameReason::Default;
  QTest::newRow("Windows, a platform plugin that is not windows")
      << environment(Platform::Windows, QStringLiteral("offscreen")) << native
      << FrameReason::PlatformPluginUnsupported;
  QTest::newRow("macOS, Qt 6.9") << environment(Platform::MacOS, QStringLiteral("cocoa"), QVersionNumber(6, 9, 0))
                                 << client << FrameReason::Default;
  QTest::newRow("macOS, Qt 6.12") << environment(Platform::MacOS, QStringLiteral("cocoa"), QVersionNumber(6, 12, 0))
                                  << client << FrameReason::Default;
  QTest::newRow("macOS, Qt 6.8 has no way to expand the client area")
      << environment(Platform::MacOS, QStringLiteral("cocoa"), QVersionNumber(6, 8, 3)) << native
      << FrameReason::QtTooOld;
  QTest::newRow("macOS, offscreen") << environment(Platform::MacOS, QStringLiteral("offscreen")) << native
                                    << FrameReason::PlatformPluginUnsupported;
  QTest::newRow("Linux, X11") << environment(Platform::Linux, QStringLiteral("xcb")) << client << FrameReason::Default;
  QTest::newRow("Linux, Wayland") << environment(Platform::Linux, QStringLiteral("wayland")) << client
                                  << FrameReason::Default;
  QTest::newRow("Linux, Wayland with EGL")
      << environment(Platform::Linux, QStringLiteral("wayland-egl")) << client << FrameReason::Default;
  QTest::newRow("Linux, a console") << environment(Platform::Linux, QStringLiteral("eglfs")) << native
                                    << FrameReason::PlatformPluginUnsupported;
  QTest::newRow("Linux, offscreen") << environment(Platform::Linux, QStringLiteral("offscreen")) << native
                                    << FrameReason::PlatformPluginUnsupported;
  QTest::newRow("another system") << environment(Platform::Other, QStringLiteral("xcb")) << native
                                  << FrameReason::OtherSystem;

  // the ways out that a person has come before what the system can do
  auto byVariable = environment(Platform::Windows, QStringLiteral("windows"));
  byVariable.frameVariable = QStringLiteral("native");
  QTest::newRow("the variable says native") << byVariable << native << FrameReason::OptedOutByEnvironment;
  auto bySetting = environment(Platform::Windows, QStringLiteral("windows"));
  bySetting.nativeFrameSetting = true;
  QTest::newRow("the setting says native") << bySetting << native << FrameReason::OptedOutBySetting;
  auto both = byVariable;
  both.nativeFrameSetting = true;
  QTest::newRow("the variable and the setting") << both << native << FrameReason::OptedOutByEnvironment;
  auto forcedButOptedOut = environment(Platform::Windows, QStringLiteral("windows"));
  forcedButOptedOut.frameVariable = QStringLiteral("client");
  forcedButOptedOut.nativeFrameSetting = true;
  QTest::newRow("the setting is stronger than the trial")
      << forcedButOptedOut << native << FrameReason::OptedOutBySetting;

  // what the system says comes before the trial
  auto contrast = environment(Platform::Windows, QStringLiteral("windows"));
  contrast.highContrast = true;
  QTest::newRow("high contrast") << contrast << native << FrameReason::HighContrast;
  contrast.frameVariable = QStringLiteral("client");
  QTest::newRow("high contrast, the trial forces the frame") << contrast << native << FrameReason::HighContrast;
  auto noComposition = environment(Platform::Windows, QStringLiteral("windows"));
  noComposition.compositionEnabled = false;
  QTest::newRow("Windows without composition") << noComposition << native << FrameReason::NoComposition;
  // the composition is a matter of Windows: the others do not have it to ask for
  auto macWithout = environment(Platform::MacOS, QStringLiteral("cocoa"));
  macWithout.compositionEnabled = false;
  QTest::newRow("macOS, composition is not asked") << macWithout << client << FrameReason::Default;

  // the trial forces the frame where the platform plugin would not give it (the tests run offscreen)
  auto forced = environment(Platform::MacOS, QStringLiteral("offscreen"));
  forced.frameVariable = QStringLiteral("client");
  QTest::newRow("the trial forces the frame, offscreen") << forced << client << FrameReason::Forced;
  auto forcedOld = environment(Platform::MacOS, QStringLiteral("cocoa"), QVersionNumber(6, 8, 0));
  forcedOld.frameVariable = QStringLiteral("client");
  QTest::newRow("the trial forces the frame, with an old Qt") << forcedOld << client << FrameReason::Forced;
}

void ChromePolicyTests::decideFrame_followsTheRulesInTheOrderOfTheirSafety()
{
  QFETCH(ChromeEnvironment, env);
  QFETCH(FrameKind, kind);
  QFETCH(FrameReason, reason);

  const auto decision = decideFrame(env);
  QCOMPARE(decision.kind, kind);
  QCOMPARE(decision.reason, reason);
  QCOMPARE(decision.clientSide(), kind == FrameKind::ClientSide);
}

void ChromePolicyTests::decideFrame_readsTheVariableWithoutCareForCaseOrSpaces()
{
  auto env = environment(Platform::Windows, QStringLiteral("windows"));
  for (const auto &value :
       {QStringLiteral("native"), QStringLiteral("NATIVE"), QStringLiteral("  Native "), QStringLiteral("native\n")}) {
    env.frameVariable = value;
    QCOMPARE(decideFrame(env).kind, FrameKind::Native);
    QCOMPARE(decideFrame(env).reason, FrameReason::OptedOutByEnvironment);
  }
  env = environment(Platform::MacOS, QStringLiteral("offscreen"));
  env.frameVariable = QStringLiteral(" CLIENT ");
  QCOMPARE(decideFrame(env).reason, FrameReason::Forced);
  // a word that is neither is not a wish: the rules go on as if there were none
  env = environment(Platform::Windows, QStringLiteral("windows"));
  env.frameVariable = QStringLiteral("banana");
  QCOMPARE(decideFrame(env).reason, FrameReason::Default);
}

void ChromePolicyTests::reasonName_isAWordForEveryReason()
{
  QStringList names;
  for (const auto reason :
       {FrameReason::Default, FrameReason::Forced, FrameReason::SystemLook, FrameReason::OptedOutByEnvironment,
        FrameReason::OptedOutBySetting, FrameReason::HighContrast, FrameReason::NoComposition, FrameReason::QtTooOld,
        FrameReason::PlatformPluginUnsupported, FrameReason::OtherSystem}) {
    const auto name = reasonName(reason);
    QVERIFY(!name.isEmpty());
    QVERIFY2(!names.contains(name), qPrintable(name));
    names << name;
  }
}

void ChromePolicyTests::current_hasTheLookAndThePlatformThatItIsAskedAbout()
{
  const auto env = ChromeEnvironment::current();
  QCOMPARE(env.platformPlugin, QGuiApplication::platformName());
  QCOMPARE(env.qtVersion, QVersionNumber::fromString(QString::fromLatin1(qVersion())));
#ifdef Q_OS_MACOS
  QCOMPARE(env.platform, Platform::MacOS);
#elif defined(Q_OS_WIN)
  QCOMPARE(env.platform, Platform::Windows);
#elif defined(Q_OS_LINUX)
  QCOMPARE(env.platform, Platform::Linux);
#endif
  // the tests run offscreen, where the frame of the system is what there is
  if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
    qputenv("DESKFLOW_WINDOW_FRAME", "");
    QCOMPARE(decideFrame(ChromeEnvironment::current()).kind, FrameKind::Native);
  }
  // the variable is read when the environment is
  qputenv("DESKFLOW_WINDOW_FRAME", "native");
  QCOMPARE(ChromeEnvironment::current().frameVariable, QStringLiteral("native"));
  qunsetenv("DESKFLOW_WINDOW_FRAME");
  QVERIFY(ChromeEnvironment::current().frameVariable.isEmpty());
}

QTEST_MAIN(ChromePolicyTests)
