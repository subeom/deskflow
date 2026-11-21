/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IAwdlSystem.h"

#include <QObject>

#include <functional>
#include <optional>

namespace deskflow::awdl {

/*!
 * Where the option exists. Only a Mac has the wireless link, so nothing of the option is shown or done anywhere else:
 * the row of the settings is not made, the controller is not made, and no program is run.
 */
bool platformSupported();

//! Only a test sets it: the answer of platformSupported() (the tests of the pages run on every system, and say what
//! each of them shows)
inline std::optional<bool> s_supportedForTest;

//! Only a test sets it: makes the system that the window uses, in place of the real one
inline std::function<IAwdlSystem *(QObject *parent)> s_systemForTest;

/*!
 * The system that the window uses: the fake of a test when it gave one, the real one (MacAwdlSystem) when the program
 * is a Mac application with a screen (the platform plugin of Qt is cocoa), and none otherwise (the tests run offscreen
 * and never reach the programs of the system, whatever they forget to fake). Null: the window has no controller.
 */
IAwdlSystem *createSystem(QObject *parent);

} // namespace deskflow::awdl
