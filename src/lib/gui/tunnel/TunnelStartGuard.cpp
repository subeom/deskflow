/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/TunnelStartGuard.h"

namespace deskflow::tunnel {

TunnelStartGuard::TunnelStartGuard(ITunnelGate *gate, Actions actions, QObject *parent)
    : QObject(parent),
      m_gate(gate),
      m_actions(std::move(actions))
{
  connect(m_gate, &ITunnelGate::listening, this, &TunnelStartGuard::onListening);
  connect(m_gate, &ITunnelGate::failed, this, &TunnelStartGuard::onFailed);
}

bool TunnelStartGuard::intercept(bool clientMode)
{
  // the start that this guard let through itself
  if (m_resuming || !clientMode || !m_gate->isRequired()) {
    return false;
  }

  if (m_gate->isListening()) {
    // nothing to wait for; the tunnel is told that it is needed again (it may be about to stop after an earlier stop)
    m_gate->acquire();
    return false;
  }

  // the state is set before the tunnel is asked: a tunnel that fails at once reports through the same signal
  m_waiting = true;
  if (m_actions.waiting) {
    m_actions.waiting();
  }
  m_gate->acquire();
  return true;
}

void TunnelStartGuard::coreStopped()
{
  m_waiting = false;
  m_gate->release();
}

void TunnelStartGuard::onListening()
{
  if (!m_waiting) {
    return;
  }

  // The settings may have changed while the tunnel came up (TLS switched off, the hostname cleared, the tunnel turned
  // off): the core is started only on the settings that the tunnel was started for, otherwise it would dial the saved
  // address
  if (const auto problem = m_gate->validate(); problem != TunnelFailure::None) {
    m_waiting = false;
    m_gate->release();
    if (m_actions.failed) {
      m_actions.failed(problem, true);
    }
    return;
  }

  m_waiting = false;
  m_resuming = true;
  if (m_actions.resume) {
    m_actions.resume();
  }
  m_resuming = false;
}

void TunnelStartGuard::onFailed(TunnelFailure failure)
{
  const bool wasWaiting = m_waiting;
  m_waiting = false;
  if (m_actions.failed) {
    m_actions.failed(failure, wasWaiting);
  }
}

} // namespace deskflow::tunnel
