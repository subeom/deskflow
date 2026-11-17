/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

namespace deskflow::tunnel {

//! Where the cloudflared that carries the connection is. The tray, the status bar and the main window all read this.
enum class TunnelState
{
  Idle,      //!< not started, or stopped
  Starting,  //!< cloudflared is started and the local port is not open yet
  Listening, //!< cloudflared runs and the local port is open
  Stopping,
  Backoff, //!< cloudflared stopped on its own and is started again after a pause
  Failed   //!< it does not run and will not be started again by itself (see TunnelStatus::failure)
};

//! What is wrong. Describes the symptom the user can act on.
enum class TunnelFailure
{
  None,
  TlsRequired,        //!< Deskflow's TLS is off: the tunnel would carry the connection in plain text
  InvalidHostname,    //!< the tunnel hostname is empty or is not a host name
  CloudflaredMissing, //!< no cloudflared at the configured path or in the usual places, or it cannot be started
  CredentialsMissing, //!< no Service Token is stored
  PortBusy,           //!< something else listens on the local port
  LoginRequired,      //!< Cloudflare Access asked for a login: the Service Token is wrong, expired or not allowed
  HostUnresolvable,   //!< the tunnel hostname does not resolve
  EdgeUnreachable,    //!< Cloudflare could not be reached
  HandshakeRejected,  //!< Cloudflare refused the connection (the Access policy or the tunnel at the server)
  ConnectFailed,      //!< cloudflared could not open the connection to the server, for a reason that is not known
  StartTimedOut,      //!< cloudflared did not open the local port in time
  ProcessExited       //!< cloudflared keeps stopping
};

/**
 * What the tunnel is doing now, as a value that is read and never changed by the readers. \p failure is why a Failed
 * tunnel does not run; on a Listening tunnel it is a problem that cloudflared reported while it runs (the local port
 * is open and a connection through it did not work).
 */
struct TunnelStatus
{
  TunnelState state = TunnelState::Idle;
  TunnelFailure failure = TunnelFailure::None;
  QString hostname;
  int localPort = 0;
  int exitCode = 0;         //!< of the last time cloudflared stopped on its own
  int restartInSeconds = 0; //!< while Backoff: the seconds left

  bool operator==(const TunnelStatus &other) const = default;

  bool isRunning() const
  {
    return state == TunnelState::Starting || state == TunnelState::Listening;
  }

  //! One line for the status bar and the main window, in the language of the application
  QString text() const;
};

//! The sentence for a failure, in the language of the application; empty for None
QString failureText(TunnelFailure failure, int exitCode = 0);

//! Restarting cannot help: the user has to change something first
bool needsUserAction(TunnelFailure failure);

} // namespace deskflow::tunnel
