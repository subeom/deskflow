/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/TunnelTypes.h"

#include "common/TunnelAddress.h"

#include <QCoreApplication>

namespace deskflow::tunnel {

namespace {

// the strings of this file, for the translation tools to find
struct TunnelTexts
{
  Q_DECLARE_TR_FUNCTIONS(TunnelTexts)
};

} // namespace

QString tunnelPathLine(const QString &hostname, int localPort)
{
  if (hostname.trimmed().isEmpty()) {
    return TunnelTexts::tr("Via Cloudflare Tunnel: no hostname is set");
  }
  return TunnelTexts::tr("Via Cloudflare Tunnel: %1 (local %2:%3)")
      .arg(hostname, QLatin1String(kLoopbackHost))
      .arg(localPort);
}

ServerLink ServerLinkTracker::update(ServerObservation observation, bool coreRunning)
{
  if (!coreRunning) {
    m_wasReached = false;
    return ServerLink::Unknown;
  }
  switch (observation) {
  case ServerObservation::Connected:
    m_wasReached = true;
    return ServerLink::Connected;
  case ServerObservation::Connecting:
    return m_wasReached ? ServerLink::Reconnecting : ServerLink::Connecting;
  case ServerObservation::Disconnected:
    break;
  }
  return m_wasReached ? ServerLink::Reconnecting : ServerLink::Unknown;
}

void ServerLinkTracker::reset()
{
  m_wasReached = false;
}

bool isTransientPathFailure(TunnelFailure failure)
{
  switch (failure) {
  case TunnelFailure::HandshakeRejected:
  case TunnelFailure::ConnectFailed:
  case TunnelFailure::EdgeUnreachable:
  case TunnelFailure::HostUnresolvable:
    return true;
  default:
    return false;
  }
}

bool tunnelStatusIsProblem(const TunnelStatus &status, ServerLink link)
{
  if (status.state == TunnelState::Failed) {
    return true;
  }
  if (status.failure == TunnelFailure::None) {
    return false;
  }
  const bool passes = status.state == TunnelState::Listening && link == ServerLink::Reconnecting &&
                      isTransientPathFailure(status.failure);
  // a failure that is told while the tunnel does not listen is a reason for that, whatever the link says
  return !passes;
}

QString tunnelStatusLine(const TunnelStatus &status, ServerLink link)
{
  const auto tunnel = status.text();
  if (status.state != TunnelState::Listening) {
    return tunnel;
  }
  if (link == ServerLink::Reconnecting) {
    if (status.failure == TunnelFailure::None) {
      return TunnelTexts::tr("%1 | Server: reconnecting").arg(tunnel);
    }
    if (isTransientPathFailure(status.failure)) {
      // what went wrong is not told as a mistake: the tunnel of the server connects to Cloudflare again, and the client
      // tries again by itself
      auto calm = status;
      calm.failure = TunnelFailure::None;
      return TunnelTexts::tr("%1 | Server: reconnecting, the path through Cloudflare is not available now")
          .arg(calm.text());
    }
    return tunnel;
  }
  switch (link) {
  case ServerLink::Connecting:
    return TunnelTexts::tr("%1 | Server: connecting").arg(tunnel);
  case ServerLink::Connected:
    return TunnelTexts::tr("%1 | Server: connected").arg(tunnel);
  case ServerLink::Unknown:
  case ServerLink::Reconnecting:
    break;
  }
  return tunnel;
}

QString failureText(TunnelFailure failure, int exitCode)
{
  switch (failure) {
  case TunnelFailure::None:
    return {};
  case TunnelFailure::TlsRequired:
    return TunnelTexts::tr("The tunnel requires TLS. Turn on encryption in Settings first.");
  case TunnelFailure::InvalidHostname:
    return TunnelTexts::tr("The tunnel hostname is not a valid host name.");
  case TunnelFailure::CloudflaredMissing:
    return TunnelTexts::tr(
        "cloudflared was not found. Install it with \"winget install --id Cloudflare.cloudflared\" (Windows) "
        "or \"brew install cloudflared\" (macOS)."
    );
  case TunnelFailure::CredentialsMissing:
    return TunnelTexts::tr("No Service Token is stored.");
  case TunnelFailure::PortBusy:
    return TunnelTexts::tr("The local port is already in use.");
  case TunnelFailure::LoginRequired:
    return TunnelTexts::tr(
        "Cloudflare Access asked for a login: the Service Token is wrong, expired or not allowed. A browser "
        "window may have opened; close it."
    );
  case TunnelFailure::HostUnresolvable:
    return TunnelTexts::tr("The tunnel hostname does not resolve.");
  case TunnelFailure::EdgeUnreachable:
    return TunnelTexts::tr("Cloudflare could not be reached (network).");
  case TunnelFailure::HandshakeRejected:
    return TunnelTexts::tr("Cloudflare refused the connection: check the server tunnel and the Access policy.");
  case TunnelFailure::ConnectFailed:
    return TunnelTexts::tr("cloudflared could not open the connection to the server.");
  case TunnelFailure::StartTimedOut:
    return TunnelTexts::tr("cloudflared did not open the local port in time.");
  case TunnelFailure::ProcessExited:
    return TunnelTexts::tr("cloudflared keeps stopping (last exit code %1).").arg(exitCode);
  case TunnelFailure::SettingsChanged:
    return TunnelTexts::tr("The tunnel settings were changed while the tunnel was starting. Start the client again.");
  }
  return {};
}

QString failureKey(TunnelFailure failure)
{
  switch (failure) {
  case TunnelFailure::None:
    return QStringLiteral("none");
  case TunnelFailure::TlsRequired:
    return QStringLiteral("tls-required");
  case TunnelFailure::InvalidHostname:
    return QStringLiteral("invalid-hostname");
  case TunnelFailure::CloudflaredMissing:
    return QStringLiteral("cloudflared-missing");
  case TunnelFailure::CredentialsMissing:
    return QStringLiteral("credentials-missing");
  case TunnelFailure::PortBusy:
    return QStringLiteral("port-busy");
  case TunnelFailure::LoginRequired:
    return QStringLiteral("login-required");
  case TunnelFailure::HostUnresolvable:
    return QStringLiteral("host-unresolvable");
  case TunnelFailure::EdgeUnreachable:
    return QStringLiteral("edge-unreachable");
  case TunnelFailure::HandshakeRejected:
    return QStringLiteral("handshake-rejected");
  case TunnelFailure::ConnectFailed:
    return QStringLiteral("connect-failed");
  case TunnelFailure::StartTimedOut:
    return QStringLiteral("start-timed-out");
  case TunnelFailure::ProcessExited:
    return QStringLiteral("process-exited");
  case TunnelFailure::SettingsChanged:
    return QStringLiteral("settings-changed");
  }
  return QStringLiteral("unknown");
}

bool needsUserAction(TunnelFailure failure)
{
  switch (failure) {
  case TunnelFailure::TlsRequired:
  case TunnelFailure::InvalidHostname:
  case TunnelFailure::CloudflaredMissing:
  case TunnelFailure::CredentialsMissing:
  case TunnelFailure::PortBusy:
  case TunnelFailure::LoginRequired:
    return true;
  default:
    return false;
  }
}

QString TunnelStatus::text() const
{
  switch (state) {
  case TunnelState::Idle:
    return TunnelTexts::tr("Tunnel: stopped");
  case TunnelState::Starting:
    return TunnelTexts::tr("Tunnel: starting");
  case TunnelState::Listening:
    if (failure != TunnelFailure::None) {
      return TunnelTexts::tr("Tunnel: listening (local port open), but: %1").arg(failureText(failure, exitCode));
    }
    return TunnelTexts::tr("Tunnel: listening (local port open)");
  case TunnelState::Stopping:
    return TunnelTexts::tr("Tunnel: stopping");
  case TunnelState::Backoff:
    return TunnelTexts::tr("Tunnel: restarting in %1 s").arg(restartInSeconds);
  case TunnelState::Failed:
    return TunnelTexts::tr("Tunnel: failed (%1)").arg(failureText(failure, exitCode));
  }
  return {};
}

} // namespace deskflow::tunnel
