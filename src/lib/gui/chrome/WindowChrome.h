/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QPoint>
#include <QSize>

#include <functional>

class QMainWindow;
class QWidget;

namespace deskflow::gui::chrome {

class WindowHeader;

/*!
 * \brief Gives the main window of the new look the frame that has no title bar of the system, with the header in its
 * place: it makes the window ask for it, and does what the header asks of the window (move, maximize, minimize). The
 * frame is chosen before (ChromePolicy); this makes it, and says if the platform could not.
 *
 * What it does on each system:
 *  - Windows: the window keeps its frame styles (so Aero Snap, the keys Win+Arrow, the thick edge for resizing and the
 *    shadow stay) and loses its non-client area in WM_NCCALCSIZE; WM_NCHITTEST answers with the pure model
 *    (ChromeGeometry), so that the header is the caption, the edges resize, and the button of maximizing has the flyout
 * of the snap layouts. The corners are round (DWM, Windows 11). See WindowChromeWin.cpp.
 *  - macOS: the content is expanded under the title bar, which has no background (Qt::ExpandedClientAreaHint, Qt 6.9).
 *    The buttons are the ones of the system, drawn over the left end of the header. Qt gives the window the title bar
 * as its safe area, which a layout leaves empty (it is laid out in the contents rect, and the minimum size that Qt
 * works out has none of it): the layout is given the whole window (Qt::WA_LayoutOnEntireRect), so that the backdrop is
 * under the title bar, and the header has the title bar in its height (WindowHeader::setTitleBarInset), with its row
 * below it. A press on the header moves the window the way that the program does it (see headerMoveMethod).
 *  - Linux: a window without a frame; the header moves the window and the edges resize it with the means of the
 *    compositor (QWindow::startSystemMove, startSystemResize).
 */
class WindowChrome : public QObject
{
  Q_OBJECT

public:
  WindowChrome(QMainWindow *window, WindowHeader *header, QObject *parent = nullptr);
  ~WindowChrome() override;

  /*!
   * Makes the window one without the title bar of the system. It is called before the window is shown. False: the
   * platform could not, and the window is as it was (the caller goes back to the frame of the system).
   */
  bool apply();
  bool isApplied() const;

  //! Only a test sets it: the platform cannot give the frame, to see that the window then keeps the frame of the system
  //! (a Mac that cannot expand the content under its title bar is the case that matters: nobody has seen it fail)
  inline static bool s_failsToApply = false;

  //! Only a test sets it: the height of the title bar that a Mac gives the window (what its Qt says of the safe area of
  //! the window), where there is none to ask (the tests run offscreen)
  inline static std::function<int(const QWidget &)> s_titleBarInset;

  //! The width of the strip along the edges that resizes the window (logical pixels)
  static constexpr int kResizeBorder = 6;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void toggleMaximized();
  void startMove();
  //! The title bar that the system has over the top of the window (a Mac): the header has it in its height
  void syncTitleBarInset();
  //! Says in the log what the state of the window was and is, and its size
  void logStateChange(Qt::WindowStates oldState);

  QMainWindow *m_window;
  WindowHeader *m_header;
  bool m_applied = false;
  bool m_edgeCursor = false;
  //! A move that the compositor could not do: the window follows the mouse (where the press was, and where the window
  //! was)
  bool m_manualMove = false;
  //! The size that the window had when it was last maximized (what it comes back to)
  QSize m_sizeBeforeMaximize;
  QPoint m_pressedAt;
  QPoint m_windowAt;
};

} // namespace deskflow::gui::chrome
