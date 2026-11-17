/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

#include <functional>

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
void installQuitHandler(std::function<bool()> shouldQuit);
