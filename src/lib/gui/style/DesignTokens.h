/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QColor>
#include <QPalette>

#include <array>

namespace deskflow::gui::style {

enum class Scheme
{
  Light,
  Dark
};

/*!
 * \brief What the OS asks of the look of an application: its colour scheme and the accessibility settings that change
 * how the program is painted.
 */
struct Look
{
  Scheme scheme = Scheme::Light;
  // the OS draws with its own high contrast colours: the colours of the program are the ones of the OS then
  bool highContrast = false;
  // the colours of the program are the ones of the OS palette, for a program that has no style layer installed
  bool osColors = false;
  // "Reduce transparency" (macOS), "Transparency effects" off (Windows): no alpha panels, no gradient
  bool reduceTransparency = false;
  // "Reduce motion": changes are instant
  bool reduceMotion = false;

  // Panels are painted opaque over a flat background (the fallback that needs no blending to keep the contrast)
  bool opaque() const
  {
    return highContrast || osColors || reduceTransparency;
  }

  bool operator==(const Look &other) const = default;
};

/*!
 * \brief The colour roles of the look. The values are the ones of the "Glass refined" design direction; every text
 * pair is checked against its background in DesignTokensTests, panels over the colours of the backdrop included.
 */
struct Colors
{
  QColor bg;
  QColor surface;
  QColor surfaceAlt;
  QColor sidebar;
  QColor text;
  QColor textSecondary;
  QColor textDisabled;
  QColor controlBorder;
  QColor accent;
  QColor onAccent;
  QColor accentSoft;
  QColor success;
  QColor successSoft;
  QColor warning;
  QColor warningSoft;
  QColor danger;
  QColor dangerSoft;
  // the 1 px edge of a panel (translucent white); the highlight on the upper edge of it
  QColor panelEdge;
  // dims what is behind an overlay (the sidebar over a narrow window, a modal sheet)
  QColor scrim;
  // the colour of the shadow below a panel, with its opacity
  QColor shadow;
};

struct Tokens
{
  Colors colors;
  // the gradient of the background: from the upper left, the middle, to the lower right
  std::array<QColor, 3> backdropStops;
  // two soft colour blobs painted over the gradient: the upper left and the lower right
  QColor blobUpperLeft;
  QColor blobLowerRight;
  // the opacity of a panel over the backdrop; of a panel that carries text (settings rows, log, dialogs); 1 when opaque
  qreal panelOpacity = 1.0;
  qreal panelDenseOpacity = 1.0;
  // no gradient, no blobs and no shadow: the background is the flat colour of the window
  bool flatBackdrop = false;
};

// Geometry, type and motion: the same in every colour scheme. Lengths are in device independent pixels
namespace metrics {

inline constexpr int radiusControl = 12; // secondary buttons, inputs, selects
inline constexpr int radiusCard = 18;    // panels
inline constexpr int radiusSidebar = 20;
inline constexpr int borderWidth = 1;
inline constexpr int focusRingWidth = 2;
inline constexpr int focusRingGap = 2;

inline constexpr int spaceXs = 4;
inline constexpr int spaceSm = 8;
inline constexpr int spaceMd = 12; // between panels
inline constexpr int spaceLg = 16;
inline constexpr int spaceXl = 20; // inside a panel
inline constexpr int space2xl = 24;

inline constexpr int rowHeight = 52;
inline constexpr int hitTarget = 36; // minimum height of something to click (24 is the least that WCAG 2.2 allows)
inline constexpr int toggleWidth = 46;
inline constexpr int toggleHeight = 28;
// the toggle of a check box in a form that was laid out for check boxes
inline constexpr int toggleCompactWidth = 36;
inline constexpr int toggleCompactHeight = 20;

inline constexpr int shadowBlur = 20;
inline constexpr int shadowOffsetY = 6;

inline constexpr int sidebarExpanded = 236;
inline constexpr int sidebarCollapsed = 76;

// type scale in pixels at 100 %: the size of a role; the line height is in Metrics' next constants
inline constexpr int fontCaption = 12;
inline constexpr int fontBody = 14;
inline constexpr int fontSubtitle = 16;
inline constexpr int fontTitle = 26;

// motion in ms; 0 when the OS asks for less motion
inline constexpr int motionSidebar = 200;
inline constexpr int motionHover = 120;
inline constexpr int motionFade = 120;

} // namespace metrics

/*!
 * \brief The motion of the look in ms: the duration, or 0 when the look asks for less motion.
 */
int motionDuration(const Look &look, int ms);

/*!
 * \brief The tokens for a look.
 * With a high contrast look the colours of surfaces, text and accent are the ones of the OS palette that is given.
 */
Tokens tokensFor(const Look &look, const QPalette &system = QPalette());

/*!
 * \brief The palette of the application for a look: the one that the style layer gives to QApplication.
 */
QPalette paletteFor(const Tokens &tokens, const Look &look);

} // namespace deskflow::gui::style
