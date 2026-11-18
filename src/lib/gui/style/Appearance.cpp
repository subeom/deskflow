/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Appearance.h"

#include "AppearanceHints.h"
#include "DeskflowStyle.h"
#include "TextSize.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"

#include <QApplication>
#include <QStyle>

namespace deskflow::gui::style {

namespace {

const DeskflowStyle *installedStyle()
{
  return qobject_cast<const DeskflowStyle *>(QApplication::style());
}

} // namespace

Mode modeFromString(const QString &text, Mode fallback)
{
  const auto name = text.trimmed().toLower();
  if (name == QLatin1String("deskflow"))
    return Mode::Deskflow;
  if (name == QLatin1String("system"))
    return Mode::System;
  return fallback;
}

QString modeToString(Mode mode)
{
  return mode == Mode::Deskflow ? QStringLiteral("deskflow") : QStringLiteral("system");
}

Mode configuredMode()
{
  const auto fromSettings = modeFromString(Settings::value(Settings::Gui::Appearance).toString(), Mode::System);
  return modeFromString(qEnvironmentVariable(kAppearanceEnvironment), fromSettings);
}

void applyAppearance()
{
  if (configuredMode() == Mode::Deskflow) {
    QApplication::setStyle(new DeskflowStyle);
    // the size of text of the new look: the system's, not smaller than the OS has in its own settings, as the person
    // chose
    TextSize::install();
    return;
  }
  // the look of the system keeps the font that the OS gave
  TextSize::uninstall();

  // System: the look that the application has always had. Fusion where the native style is not what is wanted, and
  // the native style (and the palette of the OS) on macOS and in KDE
  if (!deskflow::platform::isMac() && qEnvironmentVariable("XDG_CURRENT_DESKTOP") != QLatin1String("KDE")) {
    QApplication::setStyle("fusion");
  }
}

bool isDeskflowStyleInstalled()
{
  return installedStyle() != nullptr;
}

Look currentLook()
{
  if (const auto *style = installedStyle())
    return style->look();

  auto look = lookFor(readSystemHints(), QApplication::palette());
  look.osColors = true;
  return look;
}

Tokens currentTokens()
{
  if (const auto *style = installedStyle())
    return style->tokens();
  return tokensFor(currentLook(), QApplication::palette());
}

QMetaObject::Connection onLookChanged(const QObject *context, const std::function<void()> &callback)
{
  if (const auto *style = installedStyle())
    return QObject::connect(style, &DeskflowStyle::lookChanged, context, callback);
  return {};
}

} // namespace deskflow::gui::style
