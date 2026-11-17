/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CanvasTheme.h"

#include "Appearance.h"
#include "Contrast.h"

#include <algorithm>
#include <initializer_list>

namespace deskflow::gui::style {

namespace {

// The colour, moved as little as it takes to have the contrast against every one of the colours that it is seen on
QColor seenOn(QColor color, std::initializer_list<QColor> behind, double minimum = 3.0)
{
  for (const auto &under : behind)
    color = ensureContrast(color, under, minimum);
  // moving it for the last colour may have taken the contrast from an earlier one: a second round settles it
  for (const auto &under : behind)
    color = ensureContrast(color, under, minimum);
  return color;
}

} // namespace

CanvasTheme canvasThemeFor(const Tokens &tokens, const Look &look)
{
  const auto &c = tokens.colors;
  CanvasTheme t;

  t.background = c.surfaceAlt;
  t.fill = c.surface;
  t.fillThis = c.accentSoft;
  t.frame = seenOn(c.controlBorder, {t.background});
  t.showDots = !look.highContrast;
  t.dots = over(c.textSecondary, 0.32, t.background);

  t.border = seenOn(c.controlBorder, {t.background, t.fill});
  t.borderThis = seenOn(c.accent, {t.background, t.fillThis});
  t.text = c.text;
  t.textSecondary = c.textSecondary;
  t.mainBar = over(c.textSecondary, 0.55, t.fill);

  t.selected = seenOn(c.accent, {t.background, t.fill, t.fillThis});
  t.focus = seenOn(c.text, {t.background, t.fill, t.fillThis});

  t.link = seenOn(c.success, {t.background, t.fill, t.fillThis});
  t.linkEdge = t.background;

  t.error = seenOn(c.danger, {t.background, t.fill, t.fillThis});
  t.errorHatch = t.error;
  t.errorHatch.setAlphaF(0.30);

  t.connected = seenOn(c.success, {t.fill, t.fillThis});
  t.offline = seenOn(c.textSecondary, {t.fill, t.fillThis});

  t.guide = seenOn(c.accent, {t.background});

  t.bubble = c.text;
  t.bubbleText = c.surface;
  return t;
}

CanvasTheme currentCanvasTheme()
{
  return canvasThemeFor(currentTokens(), currentLook());
}

} // namespace deskflow::gui::style
