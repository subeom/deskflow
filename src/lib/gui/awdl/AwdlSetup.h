/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

namespace deskflow::awdl {

class AwdlController;
class IAwdlSystem;

/*!
 * The controller as the window uses it: the option is the setting (network/macAwdlOff, read when it is needed, and a
 * change of it is told to the controller at once), the dialog of the password says what AirDrop loses in the language
 * of the program, and what the controller says is a line of the log ("awdl: ..."). \p system is taken over.
 */
AwdlController *makeController(IAwdlSystem *system, QObject *parent);

} // namespace deskflow::awdl
