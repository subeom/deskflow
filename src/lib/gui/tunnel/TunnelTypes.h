/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QMetaType>
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
  ProcessExited,      //!< cloudflared keeps stopping
  SettingsChanged //!< the settings were changed while the tunnel came up: it is not the tunnel that they ask for now
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

//! How the client's connection to the server through the tunnel is going; the core says it, the tunnel does not know
enum class ServerLink
{
  Unknown,
  Connecting,
  Connected,
  //! the server was reached through this tunnel, and is not now: the connection is made again
  Reconnecting
};

//! What the core says of the connection of the client (the window gives it to a ServerLinkTracker)
enum class ServerObservation
{
  Connected,
  Connecting,
  Disconnected
};

/**
 * Remembers that the server was reached through the tunnel, so that a connection that is lost is "reconnecting" and not
 * a connection that has never worked. What is wrong with the path while it is made again is most often a thing that
 * passes (the tunnel of the server makes its own connections to Cloudflare again), and is not told as a mistake of the
 * settings.
 */
class ServerLinkTracker
{
public:
  //! \p coreRunning: the core of the client runs (a core that is stopped is not reconnecting)
  ServerLink update(ServerObservation observation, bool coreRunning);

  //! Forgets that the server was reached (the tunnel stopped or failed, or the core was stopped)
  void reset();

private:
  bool m_wasReached = false;
};

//! A failure of the path through Cloudflare that passes by itself; not one of the settings, the login or the program
bool isTransientPathFailure(TunnelFailure failure);

/**
 * Whether the line and the chip are the colour of a problem: a tunnel that failed, or one that listens and has a
 * failure to tell, unless the server was reached before and the failure is one of the path that passes (see
 * ServerLinkTracker).
 */
bool tunnelStatusIsProblem(const TunnelStatus &status, ServerLink link);

//! "Via Cloudflare Tunnel: mbp.example.com (local 127.0.0.1:24800)": what is dialled instead of the saved address
QString tunnelPathLine(const QString &hostname, int localPort);

/**
 * The one line for the state of the tunnel and, while it listens, of the connection through it: the local port that is
 * open and the connection to the server that is made over it are two different things, and the line keeps them apart.
 */
QString tunnelStatusLine(const TunnelStatus &status, ServerLink link);

//! The sentence for a failure, in the language of the application; empty for None
QString failureText(TunnelFailure failure, int exitCode = 0);

//! A short name for a failure that is the same in every language (for a line that is pasted to somebody else, and for
//! tests)
QString failureKey(TunnelFailure failure);

//! Restarting cannot help: the user has to change something first
bool needsUserAction(TunnelFailure failure);

} // namespace deskflow::tunnel

Q_DECLARE_METATYPE(deskflow::tunnel::TunnelFailure)
Q_DECLARE_METATYPE(deskflow::tunnel::TunnelStatus)
