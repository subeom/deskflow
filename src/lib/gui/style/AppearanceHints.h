/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "DesignTokens.h"

#include <QColor>
#include <QPalette>

namespace deskflow::gui::style {

/*!
 * \brief What the OS says about the look of an application.
 */
struct SystemHints
{
  // Unknown when the platform does not say
  Qt::ColorScheme scheme = Qt::ColorScheme::Unknown;
  bool highContrast = false;
  bool reduceTransparency = false;
  bool reduceMotion = false;
};

/*!
 * \brief Asks the OS: Qt's style hints and accessibility hints (Qt 6.10 and 6.12 on), the accessibility options of
 * System Settings on macOS, the high contrast and transparency settings of the registry on Windows. What a platform
 * cannot tell is false.
 */
SystemHints readSystemHints();

/*!
 * \brief The look for what the OS says. When the OS does not say if it is dark, the lightness of the palette that it
 * gave decides.
 */
Look lookFor(const SystemHints &hints, const QPalette &osPalette);

} // namespace deskflow::gui::style
