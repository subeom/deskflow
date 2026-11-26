/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Enums.h"

#include <QString>

namespace deskflow::gui::tray {

/*!
 * The states that the tray icon tells. Each one has a shape of its own (see TrayIcons.h), because a template image of
 * macOS has no colour and a person who does not see colours must read the state as well: idle is the mark alone,
 * stopped has a slash through it, waiting has a ring, connected has a dot, an error has a triangle.
 */
enum class TrayState
{
  //! Nothing to tell: no mode is chosen, so there is nothing to start
  Idle,
  //! The core is not running (or is stopping)
  Stopped,
  //! The core runs and is waiting: for clients (server), for the server (client) or for the next try
  Waiting,
  //! The core runs and is connected: to a client or more (server) or to the server (client)
  Connected,
  //! The core could not start, or it runs without a connection and tries again
  Error
};

/*!
 * The state of the tray for the state of the core. \p hasMode is whether the person chose to share this computer or to use
 * another one, \p startFailed whether the core could not be started (until the next state of the core).
 */
TrayState trayStateFor(
    core::ProcessState process, core::ConnectionState connection, bool hasMode, bool startFailed
);

//! The name of a state in the names of the icons and of the files that are made of them: "idle", "stopped", ...
QString trayStateName(TrayState state);

} // namespace deskflow::gui::tray
