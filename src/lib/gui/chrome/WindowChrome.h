/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QPoint>

class QMainWindow;

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
 *    The buttons are the ones of the system, drawn over the left end of the header.
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

  //! The width of the strip along the edges that resizes the window (logical pixels)
  static constexpr int kResizeBorder = 6;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void toggleMaximized();
  void startMove();

  QMainWindow *m_window;
  WindowHeader *m_header;
  bool m_applied = false;
  bool m_edgeCursor = false;
  //! A move that the compositor could not do: the window follows the mouse (where the press was, and where the window
  //! was)
  bool m_manualMove = false;
  QPoint m_pressedAt;
  QPoint m_windowAt;
};

} // namespace deskflow::gui::chrome
