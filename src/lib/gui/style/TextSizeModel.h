/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>

namespace deskflow::gui::style {

enum class TextPlatform
{
  Windows,
  Mac,
  Linux,
  Other
};

//! The size of text (in pixels at 100 %) that the layout of the look was drawn for: the sizes of the roles and of the
//! controls are ratios of it
inline constexpr double kReferenceBodyPixels = 14.0;
//! The step of "Text size" that follows the size of the system
inline constexpr int kDefaultTextPercent = 100;
//! The body text is never made smaller or larger than this (logical pixels)
inline constexpr double kLeastBodyPixels = 9.0;
inline constexpr double kMostBodyPixels = 40.0;
//! The sizes of the controls follow the text up to this multiple; the text itself may be larger
inline constexpr double kMostMetricScale = 2.0;

struct TextSizeInput
{
  //! The size of the font that the system gives text, in logical pixels (before the scale of the screen)
  double systemPixels = kReferenceBodyPixels;
  //! What the system has for the size of text apart from its font (Windows: Settings > Accessibility > Text size,
  //! 1.0 to 2.25); 1.0 elsewhere
  double osTextScale = 1.0;
  TextPlatform platform = TextPlatform::Other;
  //! The step that the person chose (one of textSizePercentSteps(); anything else is the default)
  int percent = kDefaultTextPercent;
};

struct TextSizePlan
{
  //! The size of body text: what the sizes of the roles (caption, subtitle, title) are ratios of
  double bodyPixels = kReferenceBodyPixels;
  //! What sizes that carry text or are touched (a row, a switch, a chip, the sidebar) are multiplied by: never less
  //! than 1, as the controls were drawn for the reference size
  double metricScale = 1.0;
  //! The size of the system was too small for this OS and was made larger
  bool raisedByFloor = false;
};

/*!
 * The smallest body text that the application has on an OS. It is the body text of the settings of the OS, so that the
 * application is not smaller than it: Windows 11 has 14 px (Fluent: "Body"; the least that Microsoft allows is 14 px
 * Semibold or 12 px Regular), macOS 13 px (the system font is 13 pt, which the settings use), and 13 px elsewhere
 * (GNOME has 14.7 px and KDE 13.3 px by default, and Qt's own default of 12 px is too small).
 */
double minimumBodyPixels(TextPlatform platform);

/*!
 * The body text and the scale of the controls.
 *
 *   base = systemPixels x osTextScale
 *   body = max(base, minimumBodyPixels(platform)) x percent / 100, within kLeastBodyPixels and kMostBodyPixels
 *   metricScale = min(kMostMetricScale, max(1, body / kReferenceBodyPixels))
 */
TextSizePlan planTextSize(const TextSizeInput &input);

//! The steps of "Text size": 90, 100 (the default: it follows the system), 115, 130 and 150 percent
QList<int> textSizePercentSteps();

//! \p percent if it is a step, else the default
int snapTextPercent(int percent);

//! The step after (direction > 0) or before (direction < 0) the one that \p percent is, and the default for 0; the ends
//! stay where they are
int stepTextPercent(int percent, int direction);

//! What the setting of Windows (HKCU\Software\Microsoft\Accessibility\TextScaleFactor, in percent) says as a multiple;
//! 1.0 for anything that is not between 100 and 225
double osTextScaleFromPercent(int percent);

//! A size in logical pixels made larger or smaller by \p scale (not less than 1 for a positive size)
int scaledPixels(int logicalPixels, double scale);

/*!
 * How many pixels a point is: from the pixels (\p measuredPixels) that a font of \p points points was measured to have.
 * A font database that has no fonts says -1 (or nothing) for every font; the screen is then taken to have 96 dpi, as
 * the screens of Windows have, so that the size of the font of the system is still known.
 */
double pixelsPerPoint(int measuredPixels, double points);

} // namespace deskflow::gui::style
