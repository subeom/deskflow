/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN

class QMainWindow;
class QObject;

namespace deskflow::gui::chrome {

class WindowHeader;

/*!
 * Takes the title bar of the system away from the window and gives its work to the header (see WindowChrome). The
 * window is created, and a filter of the native messages that is owned by \p owner answers WM_NCCALCSIZE, WM_NCHITTEST
 * and the messages of the mouse over the button of maximizing. False: the window could not be changed (nothing is).
 *
 * This code is compiled on Windows only and has been compile-checked on other systems, not run: the plan lists what is
 * to be seen on a PC (windows-runbook-ui.ko.md, the items of the frame).
 */
bool installWindowsFrame(QMainWindow *window, WindowHeader *header, QObject *owner);

} // namespace deskflow::gui::chrome

#endif
