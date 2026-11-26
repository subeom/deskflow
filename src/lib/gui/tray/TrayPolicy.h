/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QVersionNumber>

namespace deskflow::gui::tray {

//! What closing the window does
enum class CloseAction
{
  //! The program ends
  Quit,
  //! The window goes to the tray and the program runs on
  HideToTray,
  //! There is no tray to go to: the window is minimized and the program runs on, with a window that can be found again
  Minimize
};

/*!
 * What closing the window does, for the setting "close to tray" and for the tray that the OS has. A window that is hidden
 * without a tray (GNOME without the extension of the tray) can never be shown again, so it is only hidden where there
 * is a tray; without one it is minimized instead.
 */
CloseAction closeActionFor(bool closeToTray, bool trayAvailable);

/*!
 * Whether a click on the tray icon shows and hides the window. The menu of the tray is opened by the OS on macOS, and
 * by the extension of the tray on GNOME (it tells a double click only): the program does not take the click there.
 * \p currentDesktop is the list of desktops of XDG_CURRENT_DESKTOP, separated by colons.
 */
bool clickTogglesWindow(bool isMac, const QString &currentDesktop);

/*!
 * Whether a click on the tray icon can end the program: Qt before 6.11.3 handles a click on the status item of macOS 27
 * with an event that is not one of the mouse, and the program ends (QTBUG-147449, fixed in 6.11.3 and 6.12.0). That
 * makes a build with Qt 6.9.3 (the Intel build of the program) something that is not for macOS 27. \p macOsMajor is 0 on
 * the systems that are not macOS.
 */
bool trayClickCanCrash(const QVersionNumber &qtVersion, int macOsMajor);

} // namespace deskflow::gui::tray
