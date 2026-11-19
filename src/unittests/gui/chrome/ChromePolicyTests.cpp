/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ChromePolicyTests.h"

#include "gui/chrome/ChromePolicy.h"

#include <QGuiApplication>
#include <QScopeGuard>
#include <QTemporaryDir>

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
  // Linux has the frame of the system in the first release: the compositors differ and none is tried here. The frame of
  // the new look is chosen there with the setting or with the variable (below)
  QTest::newRow("Linux, X11") << environment(Platform::Linux, QStringLiteral("xcb")) << native
                              << FrameReason::LinuxNativeByDefault;
  QTest::newRow("Linux, Wayland") << environment(Platform::Linux, QStringLiteral("wayland")) << native
                                  << FrameReason::LinuxNativeByDefault;
  QTest::newRow("Linux, Wayland with EGL")
      << environment(Platform::Linux, QStringLiteral("wayland-egl")) << native << FrameReason::LinuxNativeByDefault;
  auto linuxChosen = environment(Platform::Linux, QStringLiteral("xcb"));
  linuxChosen.nativeFrameSetting = false;
  QTest::newRow("Linux, X11, the setting chooses the frame of the new look")
      << linuxChosen << client << FrameReason::ChosenBySetting;
  linuxChosen.platformPlugin = QStringLiteral("wayland");
  QTest::newRow("Linux, Wayland, the setting chooses the frame of the new look")
      << linuxChosen << client << FrameReason::ChosenBySetting;
  linuxChosen.platformPlugin = QStringLiteral("eglfs");
  QTest::newRow("Linux, a console, the setting cannot give a frame that the plugin does not have")
      << linuxChosen << native << FrameReason::PlatformPluginUnsupported;
  auto linuxNative = environment(Platform::Linux, QStringLiteral("xcb"));
  linuxNative.nativeFrameSetting = true;
  QTest::newRow("Linux, the setting says native") << linuxNative << native << FrameReason::OptedOutBySetting;
  auto linuxVariable = environment(Platform::Linux, QStringLiteral("xcb"));
  linuxVariable.frameVariable = QStringLiteral("client");
  QTest::newRow("Linux, the variable chooses the frame of the new look")
      << linuxVariable << client << FrameReason::Forced;
  auto linuxContrast = linuxChosen;
  linuxContrast.platformPlugin = QStringLiteral("xcb");
  linuxContrast.highContrast = true;
  QTest::newRow("Linux, chosen, but high contrast") << linuxContrast << native << FrameReason::HighContrast;
  auto windowsChosen = environment(Platform::Windows, QStringLiteral("windows"));
  windowsChosen.nativeFrameSetting = false;
  QTest::newRow("Windows, the setting says not native: what it has by default")
      << windowsChosen << client << FrameReason::Default;
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

void ChromePolicyTests::linux_hasTheFrameOfTheSystemInTheFirstRelease()
{
  // one line of the code says it: when the frame of the new look is tried on the compositors of Linux and found good,
  // the line is changed and the rows above are changed with it
  QVERIFY(!kLinuxClientSideByDefault);
}

void ChromePolicyTests::prefersDeskflowFrame_isWhatThePersonChoseOrWhatThePlatformHasByDefault_data()
{
  QTest::addColumn<ChromeEnvironment>("env");
  QTest::addColumn<bool>("preferred");

  QTest::newRow("Windows, nothing chosen") << environment(Platform::Windows, QStringLiteral("windows")) << true;
  QTest::newRow("macOS, nothing chosen") << environment(Platform::MacOS, QStringLiteral("cocoa")) << true;
  QTest::newRow("Linux, nothing chosen") << environment(Platform::Linux, QStringLiteral("xcb")) << false;
  QTest::newRow("another system, nothing chosen") << environment(Platform::Other, QStringLiteral("xcb")) << false;

  auto windowsNative = environment(Platform::Windows, QStringLiteral("windows"));
  windowsNative.nativeFrameSetting = true;
  QTest::newRow("Windows, the setting says native") << windowsNative << false;
  auto linuxChosen = environment(Platform::Linux, QStringLiteral("xcb"));
  linuxChosen.nativeFrameSetting = false;
  QTest::newRow("Linux, the setting says not native") << linuxChosen << true;
  auto linuxNative = environment(Platform::Linux, QStringLiteral("xcb"));
  linuxNative.nativeFrameSetting = true;
  QTest::newRow("Linux, the setting says native") << linuxNative << false;

  // what the system can or cannot do is not a choice: high contrast does not change what the person chose
  auto contrast = environment(Platform::Windows, QStringLiteral("windows"));
  contrast.highContrast = true;
  QTest::newRow("Windows, high contrast") << contrast << true;

  // the variable is the way of a trial and of a person who has no window of settings
  auto variableNative = environment(Platform::Windows, QStringLiteral("windows"));
  variableNative.frameVariable = QStringLiteral("native");
  QTest::newRow("the variable says native") << variableNative << false;
  auto variableClient = environment(Platform::Linux, QStringLiteral("xcb"));
  variableClient.frameVariable = QStringLiteral("client");
  QTest::newRow("Linux, the variable says client") << variableClient << true;
}

void ChromePolicyTests::prefersDeskflowFrame_isWhatThePersonChoseOrWhatThePlatformHasByDefault()
{
  QFETCH(ChromeEnvironment, env);
  QFETCH(bool, preferred);
  QCOMPARE(prefersDeskflowFrame(env), preferred);
}

void ChromePolicyTests::frameSwitch_whatThePersonChoseIsKeptAtTheNextStartAndChoosesTheFrame_data()
{
  QTest::addColumn<int>("platform");
  QTest::addColumn<QString>("plugin");
  QTest::addColumn<bool>("native");
  QTest::addColumn<int>("kind");
  QTest::addColumn<int>("reason");

  const auto windows = int(Platform::Windows);
  const auto mac = int(Platform::MacOS);
  const auto linux = int(Platform::Linux);
  const auto native = int(FrameKind::Native);
  const auto client = int(FrameKind::ClientSide);
  // The switch off writes true: the frame of the system. It was taken away when the program was started (Windows, the
  // 6th run of the checks), and the window had the new frame at the next start. What is stored is what decides.
  QTest::newRow("Windows, the frame of the system")
      << windows << QStringLiteral("windows") << true << native << int(FrameReason::OptedOutBySetting);
  QTest::newRow("macOS, the frame of the system")
      << mac << QStringLiteral("cocoa") << true << native << int(FrameReason::OptedOutBySetting);
  QTest::newRow("Linux (xcb), the frame of the system")
      << linux << QStringLiteral("xcb") << true << native << int(FrameReason::OptedOutBySetting);
  // the switch on: the frame of the new look is what the person chose (it is the default except on Linux)
  QTest::newRow("Windows, the frame of the new look")
      << windows << QStringLiteral("windows") << false << client << int(FrameReason::Default);
  QTest::newRow("macOS, the frame of the new look")
      << mac << QStringLiteral("cocoa") << false << client << int(FrameReason::Default);
  QTest::newRow("Linux (xcb), the frame of the new look")
      << linux << QStringLiteral("xcb") << false << client << int(FrameReason::ChosenBySetting);
  QTest::newRow("Linux (Wayland), the frame of the new look")
      << linux << QStringLiteral("wayland") << false << client << int(FrameReason::ChosenBySetting);
}

void ChromePolicyTests::frameSwitch_whatThePersonChoseIsKeptAtTheNextStartAndChoosesTheFrame()
{
  QFETCH(int, platform);
  QFETCH(QString, plugin);
  QFETCH(bool, native);
  QFETCH(int, kind);
  QFETCH(int, reason);

  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto file = dir.filePath(QStringLiteral("Deskflow.conf"));
  const auto other = dir.filePath(QStringLiteral("Other.conf"));
  const auto before = Settings::settingsFile();
  const auto restore = qScopeGuard([&] { Settings::setSettingsFile(before); });
  Settings::setSettingsFile(file);

  // the program as it is at the start: the settings are read, and the frame is chosen from them
  const auto start = [&] {
    Settings::setSettingsFile(other);
    Settings::setSettingsFile(file);
    auto env = ChromeEnvironment::current();
    env.platform = Platform(platform);
    env.platformPlugin = plugin;
    env.qtVersion = QVersionNumber(6, 9, 3);
    env.deskflowLook = true;
    env.frameVariable.clear();
    env.highContrast = false;
    env.compositionEnabled = true;
    return env;
  };

  // a person who never chose has the frame that the platform has
  QVERIFY(!start().nativeFrameSetting.has_value());

  // the choice is made in the settings, the way that the switch writes it, and the program is started again
  Settings::setValue(Settings::Gui::NativeWindowFrame, native);
  for (int again = 1; again <= 2; ++again) {
    const auto env = start();
    QVERIFY2(
        env.nativeFrameSetting.has_value(), qPrintable(QStringLiteral("the choice is gone at start %1").arg(again))
    );
    QCOMPARE(*env.nativeFrameSetting, native);
    const auto decision = decideFrame(env);
    QCOMPARE(int(decision.kind), kind);
    QCOMPARE(int(decision.reason), reason);
    QCOMPARE(prefersDeskflowFrame(env), !native);
  }
}

void ChromePolicyTests::reasonName_isAWordForEveryReason()
{
  QStringList names;
  for (const auto reason :
       {FrameReason::Default, FrameReason::Forced, FrameReason::SystemLook, FrameReason::OptedOutByEnvironment,
        FrameReason::OptedOutBySetting, FrameReason::HighContrast, FrameReason::NoComposition, FrameReason::QtTooOld,
        FrameReason::PlatformPluginUnsupported, FrameReason::OtherSystem, FrameReason::LinuxNativeByDefault,
        FrameReason::ChosenBySetting}) {
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
