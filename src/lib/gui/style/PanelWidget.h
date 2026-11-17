/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "DesignTokens.h"

#include <QMargins>
#include <QPixmap>
#include <QWidget>

namespace deskflow::gui::style {

/*!
 * The shadow of a panel as a picture that is cut in nine and stretched: its four corners, which have the curve and the
 * blur, and the sides, which do not change along an edge. Kept for the radius, the colour and the device pixel ratio.
 * A shadow that is blurred at each paint is slow and blurry on a screen of 200 %.
 */
QPixmap shadowPatch(int radius, int blur, const QColor &color, qreal dpr);

/*!
 * Paints a shadow around the rectangle with the picture above (the rectangle itself is not painted).
 */
void paintShadow(QPainter *painter, const QRect &rect, int radius, int offsetY, const QColor &color, qreal dpr);

/*!
 * \brief A floating, rounded panel of the new look: the colour of a surface with some transparency (more opaque for a
 * panel that carries much text), a 1 px light edge, a soft shadow, and the padding of its contents.
 *
 * The panel is painted over what is behind it (a BackdropWidget): the window is opaque and the transparency is that of
 * the colours that are painted, so that the contrast of its text can be worked out for every colour that is behind it.
 * With less transparency asked for (or high contrast, or no style layer) the panel is opaque and has no shadow.
 *
 * Its contents margins are the room of the shadow plus the padding: a layout that is set on the panel fills the panel
 * as it is seen, with the padding inside it.
 */
class PanelWidget : public QWidget
{
  Q_OBJECT

public:
  enum class Density
  {
    // decoration: the status card, the sidebar
    Normal,
    // it carries text (settings rows, the log, dialogs): more opaque
    Dense
  };

  explicit PanelWidget(QWidget *parent = nullptr);
  explicit PanelWidget(Density density, QWidget *parent = nullptr);

  Density density() const;
  void setDensity(Density density);

  // a panel that is not elevated has no shadow and no room for one
  bool isElevated() const;
  void setElevated(bool elevated);

  int padding() const;
  void setPadding(int padding);

  // the room that the shadow takes at each side; the panel as it is seen is the rectangle inside of it
  static QMargins shadowMargins();
  QRect panelRect() const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  void updateMargins();
  void watchLook();

  Density m_density = Density::Normal;
  bool m_elevated = true;
  int m_padding = metrics::spaceXl;
  Tokens m_tokens;
  QMetaObject::Connection m_lookConnection;
};

} // namespace deskflow::gui::style
