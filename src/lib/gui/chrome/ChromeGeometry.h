/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QMargins>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <Qt>

namespace deskflow::gui::chrome {

//! Which part of the window a point is in. The window without a title bar answers this for the system (Windows asks it
//! with WM_NCHITTEST), so that the header is dragged, the edges resize, and the button of maximizing has a flyout of
//! snap layouts. All sizes are in pixels of the window (device pixels), not in logical ones.
enum class Region
{
  //! The content: the window takes the mouse
  Client,
  //! The empty part of the header: a press drags the window, a double click maximizes it
  Caption,
  Minimize,
  Maximize,
  Close,
  Left,
  Right,
  Top,
  Bottom,
  TopLeft,
  TopRight,
  BottomLeft,
  BottomRight
};

struct ChromeMetrics
{
  QSize windowSize;
  //! The width of the strip along each edge that resizes the window; 0 for a window that is not resized
  int resizeBorder = 6;
  int headerHeight = 40;
  //! The width of one of the buttons of the window (minimize, maximize, close) that the header draws, from the right
  //! edge; 0 where the system draws them or there are none (the header of macOS has the buttons of the system)
  int buttonWidth = 46;
  bool maximized = false;
};

QRect closeRect(const ChromeMetrics &metrics);
QRect maximizeRect(const ChromeMetrics &metrics);
QRect minimizeRect(const ChromeMetrics &metrics);

/*!
 * \brief The part of the window that is at a point (in the pixels of the window, 0,0 at its top left corner).
 *
 * The edges and the corners come first, as the window is resized by them, except where a button of the window is (the
 * whole height of the close button is the button, so that it is not missed by a pixel at the top). A maximized window
 * has no edges. Then the buttons, then the header: \p interactive are the rectangles in the header that take the mouse
 * for themselves (the menu button), and the rest of the header is the caption.
 */
Region hitTest(const ChromeMetrics &metrics, const QPoint &point, const QList<QRect> &interactive = {});

//! The edges of the window that a region resizes (none for the other regions): what startSystemResize is asked for
//! where the system has no frame to do it
Qt::Edges edgesOf(Region region);

/*!
 * \brief The part of a window that is maximized and is not on the monitor: Windows makes the window bigger than the
 * monitor by the thickness of its frame, and with no frame to cover it the content is moved inside by that much.
 */
QMargins maximizedInsets(int frameThickness);

} // namespace deskflow::gui::chrome
