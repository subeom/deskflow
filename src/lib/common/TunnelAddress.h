/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QVariant>

/**
 * @file
 * The address the client dials when it reaches the server through a Cloudflare Tunnel: a port on this computer where
 * a cloudflared listens, not the address that the user saved. Pure functions and a plain value, so the core (client)
 * and the GUI use the same rules and the same tests. The saved address (`client/remoteHost`) is never changed by any
 * of it: with the tunnel off, the address is exactly the saved one.
 */
namespace deskflow::tunnel {

inline constexpr int kDefaultLocalPort = 24800;
inline constexpr int kMinLocalPort = 1024;
inline constexpr int kMaxLocalPort = 65535;

//! Where cloudflared listens: only this computer, never a network interface
inline constexpr auto kLoopbackHost = "127.0.0.1";

//! \p port as a local port to listen on (1024 to 65535)
bool isValidLocalPort(int port);

//! A port from the value of a setting; 0 when the value is not a whole number in the range
int localPortFromSetting(const QVariant &raw);

//! The hostname of the tunnel without the blanks around it, in lower case, and without a final dot
QString normalizedHostname(const QString &hostname);

/**
 * Whether \p hostname can be the hostname of the Access application: a DNS name of ASCII letters, digits and hyphens in
 * at least two labels, whose last label is not a number (that is an IP address, not a name). A scheme, a port, a path,
 * a blank or a leading hyphen (which cloudflared would take for an option) make it invalid.
 */
bool isValidHostname(const QString &hostname);

struct TunnelConfig
{
  bool enabled = false;
  QString hostname;
  int localPort = kDefaultLocalPort;
  QString cloudflaredPath; //!< empty: look for it
  int activePort = 0;      //!< the port in use now when it differs from localPort, otherwise 0
  bool tlsEnabled = true;  //!< Deskflow's own TLS; the tunnel carries the connection in plain text without it

  //! The tunnel is used: it is turned on, TLS is on and there is a usable hostname
  bool isActive() const;

  //! The port to listen on and to dial: the active one when there is one, otherwise the local port
  int port() const;

  //! `127.0.0.1:port`
  QString localAddress() const;

  //! What the settings hold (an unset value is the default; a value that is not usable is the default too)
  static TunnelConfig fromSettings();
};

/**
 * The address list that the client dials. With the tunnel off, or not usable (see TunnelConfig::isActive), it is
 * \p configuredRemoteHost, unchanged. With it on, it is the local address of the tunnel and nothing else: the saved
 * list is not tried, so a failing tunnel never falls back to a path that bypasses it without the user knowing.
 */
QString effectiveRemoteHost(const TunnelConfig &config, const QString &configuredRemoteHost);

//! As above with the settings: the saved address is `client/remoteHost`
QString effectiveRemoteHost();

} // namespace deskflow::tunnel
