/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ChromePolicy.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "gui/style/Appearance.h"
#include "gui/style/AppearanceHints.h"

#include <QGuiApplication>

namespace deskflow::gui::chrome {

namespace {

enum class Wish
{
  None,
  Native,
  Client
};

Wish wishOf(const QString &value)
{
  const auto word = value.trimmed().toLower();
  if (word == QLatin1String("native"))
    return Wish::Native;
  if (word == QLatin1String("client"))
    return Wish::Client;
  return Wish::None;
}

} // namespace

ChromeEnvironment ChromeEnvironment::current()
{
  ChromeEnvironment env;
  if (deskflow::platform::isWindows())
    env.platform = Platform::Windows;
  else if (deskflow::platform::isMac())
    env.platform = Platform::MacOS;
  else if (QSysInfo::kernelType() == QLatin1String("linux"))
    env.platform = Platform::Linux;
  env.platformPlugin = QGuiApplication::platformName();
  env.qtVersion = QVersionNumber::fromString(QString::fromLatin1(qVersion()));
  env.deskflowLook = style::isDeskflowStyleInstalled();
  env.frameVariable = qEnvironmentVariable("DESKFLOW_WINDOW_FRAME");
  env.nativeFrameSetting = Settings::value(Settings::Gui::NativeWindowFrame).toBool();
  env.highContrast = style::readSystemHints().highContrast;
  return env;
}

FrameDecision decideFrame(const ChromeEnvironment &environment)
{
  const auto native = [](FrameReason reason) { return FrameDecision{FrameKind::Native, reason}; };
  const auto clientSide = [](FrameReason reason) { return FrameDecision{FrameKind::ClientSide, reason}; };

  if (!environment.deskflowLook)
    return native(FrameReason::SystemLook);

  const auto wish = wishOf(environment.frameVariable);
  if (wish == Wish::Native)
    return native(FrameReason::OptedOutByEnvironment);
  if (environment.nativeFrameSetting)
    return native(FrameReason::OptedOutBySetting);
  if (environment.highContrast)
    return native(FrameReason::HighContrast);
  if (environment.platform == Platform::Windows && !environment.compositionEnabled)
    return native(FrameReason::NoComposition);
  // a trial: the platform plugin and the version of Qt are not asked (the tests run offscreen)
  if (wish == Wish::Client)
    return clientSide(FrameReason::Forced);

  switch (environment.platform) {
  case Platform::Windows:
    if (environment.platformPlugin != QLatin1String("windows"))
      return native(FrameReason::PlatformPluginUnsupported);
    return clientSide(FrameReason::Default);
  case Platform::MacOS:
    if (environment.platformPlugin != QLatin1String("cocoa"))
      return native(FrameReason::PlatformPluginUnsupported);
    // Qt::ExpandedClientAreaHint is the way to have the content under the title bar: it is from Qt 6.9
    if (environment.qtVersion < QVersionNumber(6, 9))
      return native(FrameReason::QtTooOld);
    return clientSide(FrameReason::Default);
  case Platform::Linux:
    if (!kLinuxClientSideByDefault)
      return native(FrameReason::OtherSystem);
    if (environment.platformPlugin != QLatin1String("xcb") &&
        !environment.platformPlugin.startsWith(QLatin1String("wayland")))
      return native(FrameReason::PlatformPluginUnsupported);
    return clientSide(FrameReason::Default);
  case Platform::Other:
    break;
  }
  return native(FrameReason::OtherSystem);
}

QString reasonName(FrameReason reason)
{
  switch (reason) {
  case FrameReason::Default:
    return QStringLiteral("the platform can do it");
  case FrameReason::Forced:
    return QStringLiteral("forced by DESKFLOW_WINDOW_FRAME=client");
  case FrameReason::SystemLook:
    return QStringLiteral("the look of the system");
  case FrameReason::OptedOutByEnvironment:
    return QStringLiteral("DESKFLOW_WINDOW_FRAME=native");
  case FrameReason::OptedOutBySetting:
    return QStringLiteral("the setting gui/nativeWindowFrame");
  case FrameReason::HighContrast:
    return QStringLiteral("high contrast");
  case FrameReason::NoComposition:
    return QStringLiteral("no composition of the desktop");
  case FrameReason::QtTooOld:
    return QStringLiteral("Qt is older than 6.9");
  case FrameReason::PlatformPluginUnsupported:
    return QStringLiteral("the platform plugin has no frame to replace");
  case FrameReason::OtherSystem:
    return QStringLiteral("another system");
  }
  return {};
}

} // namespace deskflow::gui::chrome
