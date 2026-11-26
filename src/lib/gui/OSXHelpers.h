/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

#include <functional>

class QMenu;

void requestOSXNotificationPermission();
bool isOSXDevelopmentBuild();
bool showOSXNotification(const QString &title, const QString &body);
bool isOSXInterfaceStyleDark();
// the accessibility displays options of System Settings (they are read when asked, there is no notification for them)
bool isOSXReduceTransparency();
bool isOSXReduceMotion();
bool isOSXIncreaseContrast();
void forceAppActive();
void macOSNativeHide();
//! Brackets Qt's synchronous native termination handling so its close events retain the request's intent.
void installQuitHandler(
    std::function<bool(bool systemShuttingDown)> shouldQuit, std::function<void()> nativeQuitRequestFinished
);
//! macOS 27 draws the picture of a row of a menu only when the row says that it wants to be seen (QTBUG-151046, which Qt 6.12
//! has): the rows of this menu say it. The rows that the OS does not know the property of (macOS before 27) are left alone.
void showOSXMenuImages(QMenu *menu);
