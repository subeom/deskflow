/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "DesignTokens.h"

#include <QProxyStyle>
#include <QStyleOption>

#include <optional>

namespace deskflow::gui::style {

/*!
 * \brief The style of the application with the look of "Glass refined", the same on every OS: Fusion with the palette
 * of the tokens, capsule primary buttons, rounded secondary buttons and inputs (radius 12), toggles for check boxes and
 * the focus ring of the design.
 *
 * Menus (the menu bar of a window, the menu of a tray icon on the OS where Qt draws it, context menus) are painted in
 * the tokens too: rows 36 px high with a rounded highlight, icons in a column of their own, shortcuts in the secondary
 * colour, hairline separators. What the style does not paint (tabs, group boxes, scroll bars, radio buttons, tool
 * tips) is Fusion in the colours of the palette. The fonts are the ones of the OS and the sizes of the contents are the ones of Fusion plus
 * the room of the rounder shapes, so that no layout has to change.
 *
 * The look (light or dark, high contrast, less transparency, less motion) follows the OS: the style is asked to polish
 * the palette each time that Qt takes the palette of the OS, and keeps the tokens of that look for the widgets that
 * paint themselves (BackdropWidget, PanelWidget).
 */
class DeskflowStyle : public QProxyStyle
{
  Q_OBJECT

public:
  // the property of a widget that is true when what is in it is not valid: the style draws it with the colour of danger
  static constexpr auto kInvalidProperty = "deskflowInvalid";
  // the property of a menu that is true when it has a frame of its own: no window frame and a clear ground (it is made
  // translucent by whoever sets it), so that the style draws it with round corners and a shadow
  static constexpr auto kRoundedMenuProperty = "deskflowRoundedMenu";

  DeskflowStyle();

  const Look &look() const;
  const Tokens &tokens() const;

  /*!
   * Paints with this look instead of the one that the OS says, until it is given another or none. For tests, previews
   * and the screens of the documentation: the palette of the application is made again at once.
   */
  void useLook(const std::optional<Look> &look);

  void polish(QPalette &palette) override;
  using QProxyStyle::polish;
  QPalette standardPalette() const override;

  int pixelMetric(
      PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr
  ) const override;
  QSize sizeFromContents(
      ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget = nullptr
  ) const override;
  QRect subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget = nullptr) const override;
  QRect subControlRect(
      ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget = nullptr
  ) const override;
  int styleHint(
      StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
      QStyleHintReturn *returnData = nullptr
  ) const override;

  void drawPrimitive(
      PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr
  ) const override;
  void drawControl(
      ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr
  ) const override;
  void drawComplexControl(
      ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget = nullptr
  ) const override;

Q_SIGNALS:
  // the look or the tokens are other: what the widgets drew with has to be drawn again
  void lookChanged();

private:
  void remake(const QPalette &osPalette);
  void paintButton(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget) const;
  void
  paintInput(const QRect &rect, State state, const QPalette &palette, QPainter *painter, const QWidget *widget) const;
  void paintToggle(const QStyleOptionButton *option, QPainter *painter) const;
  void paintMenuItem(const QStyleOptionMenuItem *option, QPainter *painter, const QWidget *widget) const;
  void paintMenuBarItem(const QStyleOptionMenuItem *option, QPainter *painter, const QWidget *widget) const;
  QSize menuItemSize(const QStyleOptionMenuItem *option, const QSize &contentsSize) const;

  Look m_look;
  Tokens m_tokens;
  QPalette m_palette;
  QPalette m_osPalette;
  std::optional<Look> m_forced;
};

} // namespace deskflow::gui::style
