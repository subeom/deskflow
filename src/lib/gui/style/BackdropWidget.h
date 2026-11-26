/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "DesignTokens.h"

#include <QPixmap>
#include <QTimer>
#include <QWidget>

namespace deskflow::gui::style {

/*!
 * The picture of the background of the new look at a size: a gradient from the upper left to the lower right with two
 * soft colour blobs over it, or the flat colour of the window when the tokens ask for a flat background (the OS asks
 * for less transparency, high contrast, or the program has no style layer). It is not a blur of what is behind the
 * window: the window is opaque and the colours are painted, the same on every OS.
 */
QPixmap backdropPixmap(const QSize &size, qreal dpr, const Tokens &tokens);

/*!
 * \brief The background of a window or a page: paints the picture above, kept for the size and the device pixel ratio.
 * During a continuous resize it scales the previous picture instead of rendering every frame; once the resize settles
 * it makes the exact picture for the final size. Screen and look changes also invalidate the picture.
 *
 * The panels (PanelWidget) are its children, and the window is opaque: nothing is transparent to the desktop.
 */
class BackdropWidget : public QWidget
{
  Q_OBJECT

public:
  explicit BackdropWidget(QWidget *parent = nullptr);

  // how many times the picture was made: it is kept between paints (for tests and to find a repaint that is too dear)
  int renderCount() const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void changeEvent(QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void invalidate();
  void watchLook();

  Tokens m_tokens;
  QPixmap m_picture;
  QMetaObject::Connection m_lookConnection;
  QTimer m_resizeSettled;
  int m_renderCount = 0;
};

} // namespace deskflow::gui::style
