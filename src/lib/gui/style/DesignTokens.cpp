/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DesignTokens.h"

#include "Contrast.h"

namespace deskflow::gui::style {

namespace {

QColor rgba(int r, int g, int b, qreal alpha)
{
  QColor color(r, g, b);
  color.setAlphaF(alpha);
  return color;
}

Tokens lightTokens()
{
  Tokens t;
  t.colors = {
      .bg = QColor(0xDC, 0xE6, 0xFB),
      .surface = QColor(0xFF, 0xFF, 0xFF),
      .surfaceAlt = QColor(0xF2, 0xF4, 0xFD),
      .sidebar = QColor(0xFF, 0xFF, 0xFF),
      .text = QColor(0x12, 0x16, 0x2B),
      .textSecondary = QColor(0x45, 0x4B, 0x6B),
      .textDisabled = QColor(0x8A, 0x90, 0xAE),
      .controlBorder = QColor(0x6A, 0x72, 0x98),
      .accent = QColor(0x30, 0x50, 0xE0),
      .onAccent = QColor(0xFF, 0xFF, 0xFF),
      .accentSoft = QColor(0xE3, 0xE8, 0xFF),
      .success = QColor(0x10, 0x73, 0x3A),
      .successSoft = QColor(0xDD, 0xF3, 0xE5),
      .warning = QColor(0x8A, 0x4F, 0x00),
      .warningSoft = QColor(0xFB, 0xEF, 0xD9),
      .danger = QColor(0xBD, 0x24, 0x33),
      .dangerSoft = QColor(0xFB, 0xE4, 0xE7),
      .panelEdge = rgba(255, 255, 255, 0.75),
      .scrim = rgba(15, 20, 50, 0.32),
      .shadow = rgba(24, 32, 90, 0.10),
  };
  t.backdropStops = {QColor(0xD3, 0xE0, 0xFA), QColor(0xE4, 0xDB, 0xF8), QColor(0xF8, 0xE3, 0xD8)};
  t.blobUpperLeft = rgba(255, 255, 255, 0.75);
  t.blobLowerRight = rgba(255, 190, 170, 0.55);
  t.panelOpacity = 0.78;
  t.panelDenseOpacity = 0.93;
  return t;
}

Tokens darkTokens()
{
  Tokens t;
  t.colors = {
      .bg = QColor(0x0F, 0x14, 0x30),
      .surface = QColor(0x1E, 0x23, 0x47),
      .surfaceAlt = QColor(0x2A, 0x30, 0x60),
      .sidebar = QColor(0x1A, 0x1F, 0x44),
      .text = QColor(0xF2, 0xF4, 0xFF),
      .textSecondary = QColor(0xB7, 0xBC, 0xDD),
      .textDisabled = QColor(0x74, 0x79, 0xA3),
      .controlBorder = QColor(0x88, 0x90, 0xC8),
      .accent = QColor(0x8C, 0xA2, 0xFF),
      .onAccent = QColor(0x0A, 0x0F, 0x2A),
      .accentSoft = QColor(0x2B, 0x35, 0x78),
      .success = QColor(0x5B, 0xDB, 0x8A),
      .successSoft = QColor(0x1B, 0x42, 0x30),
      .warning = QColor(0xF5, 0xBE, 0x5C),
      .warningSoft = QColor(0x43, 0x36, 0x1C),
      .danger = QColor(0xFF, 0x87, 0x94),
      .dangerSoft = QColor(0x4A, 0x22, 0x30),
      .panelEdge = rgba(255, 255, 255, 0.16),
      .scrim = rgba(0, 0, 10, 0.55),
      .shadow = rgba(24, 32, 90, 0.35),
  };
  t.backdropStops = {QColor(0x0C, 0x12, 0x30), QColor(0x1B, 0x14, 0x48), QColor(0x0B, 0x2B, 0x3C)};
  t.blobUpperLeft = rgba(80, 110, 255, 0.35);
  t.blobLowerRight = rgba(40, 200, 190, 0.22);
  t.panelOpacity = 0.70;
  t.panelDenseOpacity = 0.90;
  return t;
}

} // namespace

int motionDuration(const Look &look, int ms)
{
  return look.reduceMotion ? 0 : ms;
}

Tokens tokensFor(const Look &look, const QPalette &system)
{
  auto tokens = look.scheme == Scheme::Dark ? darkTokens() : lightTokens();

  if (look.highContrast || look.osColors) {
    // the OS has chosen the colours (in high contrast, the ones that the person needs): the program does not mix its
    // own
    auto &c = tokens.colors;
    c.bg = system.color(QPalette::Window);
    c.surface = system.color(QPalette::Base);
    c.surfaceAlt = system.color(QPalette::Base);
    c.sidebar = system.color(QPalette::Window);
    c.text = system.color(QPalette::WindowText);
    c.textSecondary = c.text;
    c.textDisabled = system.color(QPalette::Disabled, QPalette::WindowText);
    c.accent = system.color(QPalette::Highlight);
    c.onAccent = system.color(QPalette::HighlightedText);
    c.accentSoft = c.surface;
    c.successSoft = c.surface;
    c.warningSoft = c.surface;
    c.dangerSoft = c.surface;
    if (look.highContrast) {
      c.controlBorder = c.text;
      c.panelEdge = c.text;
    } else {
      // an edge that is seen, as strongly as the OS draws its own: the text colour dimmed until it is 3:1
      c.controlBorder = ensureContrast(ensureContrast(over(c.text, 0.55, c.bg), c.bg, 3.0), c.surface, 3.0);
      // the edge of a panel only parts it from what is around it
      c.panelEdge = over(c.text, 0.18, c.surface);
      // the colours of a state are the ones that the OS surface can carry as text
      for (auto *state : {&c.success, &c.warning, &c.danger})
        *state = ensureContrast(*state, c.surface, 4.5);
    }
  }

  if (look.opaque()) {
    tokens.flatBackdrop = true;
    tokens.panelOpacity = 1.0;
    tokens.panelDenseOpacity = 1.0;
  }
  return tokens;
}

QPalette paletteFor(const Tokens &tokens, const Look &look)
{
  const auto &c = tokens.colors;
  const auto dark = look.scheme == Scheme::Dark;

  QPalette p;
  p.setColor(QPalette::Window, c.bg);
  p.setColor(QPalette::WindowText, c.text);
  p.setColor(QPalette::Base, c.surface);
  p.setColor(QPalette::AlternateBase, c.surfaceAlt);
  p.setColor(QPalette::ToolTipBase, c.surface);
  p.setColor(QPalette::ToolTipText, c.text);
  p.setColor(QPalette::Text, c.text);
  p.setColor(QPalette::Button, c.surfaceAlt);
  p.setColor(QPalette::ButtonText, c.text);
  p.setColor(QPalette::BrightText, dark ? Qt::white : Qt::black);
  p.setColor(QPalette::Highlight, c.accent);
  p.setColor(QPalette::HighlightedText, c.onAccent);
  p.setColor(QPalette::Link, c.accent);
  p.setColor(QPalette::LinkVisited, c.accent);
  p.setColor(QPalette::PlaceholderText, c.textSecondary);
  p.setColor(QPalette::Accent, c.accent);

  // the shades that the styles derive their bevels and frames from
  p.setColor(QPalette::Light, c.surface);
  p.setColor(QPalette::Midlight, c.surfaceAlt);
  p.setColor(QPalette::Mid, c.controlBorder);
  p.setColor(QPalette::Dark, c.controlBorder.darker(dark ? 100 : 130));
  p.setColor(QPalette::Shadow, Qt::black);

  for (const auto role :
       {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::PlaceholderText, QPalette::ToolTipText}) {
    p.setColor(QPalette::Disabled, role, c.textDisabled);
  }
  p.setColor(QPalette::Disabled, QPalette::Highlight, c.textDisabled);
  p.setColor(QPalette::Disabled, QPalette::HighlightedText, c.surface);
  return p;
}

} // namespace deskflow::gui::style
