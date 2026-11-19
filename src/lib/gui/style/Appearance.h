/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "DesignTokens.h"

#include <QObject>
#include <QString>

#include <functional>

namespace deskflow::gui::style {

enum class Mode
{
  // the look of this application (DeskflowStyle), the same on every OS
  Deskflow,
  // the look that each OS gave the application: what it was before the look above was made. A person chooses it with
  // the setting gui/appearance or, for one run, with DESKFLOW_APPEARANCE=system
  System
};

// The environment variable that sets the mode for one run (deskflow or system); it wins over the setting
inline constexpr auto kAppearanceEnvironment = "DESKFLOW_APPEARANCE";

// The mode of a person who has not chosen one (no gui/appearance in the settings), and of a value that is no mode. It
// is the default of the setting too: Settings::defaultValue(Gui::Appearance) says the same word
inline constexpr Mode kDefaultMode = Mode::Deskflow;

Mode modeFromString(const QString &text, Mode fallback);
QString modeToString(Mode mode);

/*!
 * The mode that is asked for: the environment variable, else the setting gui/appearance, else kDefaultMode (also for a
 * value that is no mode).
 */
Mode configuredMode();

/*!
 * Installs the look for the mode that is asked for. With System the application is left as it was: Fusion on the OS
 * that have no native look that is wanted (all but macOS and KDE), the native style elsewhere. Call once, after the
 * application object and before the first window.
 */
void applyAppearance();

// true when the application is painted by DeskflowStyle
bool isDeskflowStyleInstalled();

/*!
 * The look that the application is painted with now, and its tokens. Without DeskflowStyle the tokens are made of the
 * palette that the OS gave (flat and opaque, no translucency), so that widgets that paint with tokens are readable in
 * any mode.
 */
Look currentLook();
Tokens currentTokens();

/*!
 * Calls the function when the look of the style layer changes (the OS went dark, the person asked for less
 * transparency): for widgets that paint with the tokens. It is not connected when there is no style layer. A widget
 * calls it again when its style is another (QEvent::StyleChange). The connection ends with the context object.
 */
QMetaObject::Connection onLookChanged(const QObject *context, const std::function<void()> &callback);

} // namespace deskflow::gui::style
