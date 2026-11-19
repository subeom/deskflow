/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelChip.h"

#include <QCoreApplication>

namespace deskflow::gui {

using deskflow::tunnel::ServerLink;
using deskflow::tunnel::TunnelFailure;
using deskflow::tunnel::TunnelState;
using Kind = StatusChip::Kind;

TunnelChipSpec tunnelChip(const deskflow::tunnel::TunnelStatus &status, ServerLink link)
{
  switch (status.state) {
  case TunnelState::Idle:
    return {QCoreApplication::translate("TunnelChip", "Tunnel: stopped"), Kind::Neutral};
  case TunnelState::Starting:
    return {QCoreApplication::translate("TunnelChip", "Tunnel: starting"), Kind::Info};
  case TunnelState::Listening:
    // the tunnel listens and what goes through it fails (the edge refuses, the login is asked for)
    if (status.failure != TunnelFailure::None)
      return {QCoreApplication::translate("TunnelChip", "Tunnel: problem"), Kind::Warning};
    if (link == ServerLink::Connected)
      return {QCoreApplication::translate("TunnelChip", "Tunnel: connected"), Kind::Success};
    if (link == ServerLink::Connecting)
      return {QCoreApplication::translate("TunnelChip", "Tunnel: connecting"), Kind::Info};
    return {QCoreApplication::translate("TunnelChip", "Tunnel: ready"), Kind::Info};
  case TunnelState::Stopping:
    return {QCoreApplication::translate("TunnelChip", "Tunnel: stopping"), Kind::Neutral};
  case TunnelState::Backoff:
    return {QCoreApplication::translate("TunnelChip", "Tunnel: restarting"), Kind::Warning};
  case TunnelState::Failed:
    return {QCoreApplication::translate("TunnelChip", "Tunnel: failed"), Kind::Danger};
  }
  return {};
}

} // namespace deskflow::gui
