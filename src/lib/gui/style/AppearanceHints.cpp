/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AppearanceHints.h"

#include <QGuiApplication>
#include <QStyleHints>

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#include <QAccessibilityHints>
#endif

#if defined(Q_OS_MACOS)
#include "../OSXHelpers.h"
#elif defined(Q_OS_WIN)
#include <QSettings>
#endif

namespace deskflow::gui::style {

SystemHints readSystemHints()
{
  SystemHints hints;
  const auto *styleHints = QGuiApplication::styleHints();
  if (!styleHints)
    return hints;

  hints.scheme = styleHints->colorScheme();

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  if (const auto *accessibility = styleHints->accessibility()) {
    hints.highContrast = accessibility->contrastPreference() == Qt::ContrastPreference::HighContrast;
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    hints.reduceMotion = accessibility->motionPreference() == Qt::MotionPreference::ReducedMotion;
#endif
  }
#endif

#if defined(Q_OS_MACOS)
  hints.highContrast = hints.highContrast || isOSXIncreaseContrast();
  hints.reduceTransparency = isOSXReduceTransparency();
  hints.reduceMotion = hints.reduceMotion || isOSXReduceMotion();
#elif defined(Q_OS_WIN)
  // Settings > Accessibility > Contrast themes (bit 0 of the flags is "high contrast is on") and Personalization >
  // Colors > Transparency effects. Windows has no Qt notification for either: they are read when the palette is made
  const QSettings contrast(
      QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Accessibility\\HighContrast"), QSettings::NativeFormat
  );
  hints.highContrast = hints.highContrast || (contrast.value(QStringLiteral("Flags")).toString().toInt() & 1);
  const QSettings personalize(
      QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
      QSettings::NativeFormat
  );
  hints.reduceTransparency = personalize.value(QStringLiteral("EnableTransparency"), 1).toInt() == 0;
#endif
  return hints;
}

Look lookFor(const SystemHints &hints, const QPalette &osPalette)
{
  Look look;
  if (hints.scheme == Qt::ColorScheme::Dark)
    look.scheme = Scheme::Dark;
  else if (hints.scheme == Qt::ColorScheme::Light)
    look.scheme = Scheme::Light;
  else
    look.scheme = osPalette.color(QPalette::WindowText).lightness() > osPalette.color(QPalette::Window).lightness()
                      ? Scheme::Dark
                      : Scheme::Light;
  look.highContrast = hints.highContrast;
  look.reduceTransparency = hints.reduceTransparency;
  look.reduceMotion = hints.reduceMotion;
  return look;
}

} // namespace deskflow::gui::style
