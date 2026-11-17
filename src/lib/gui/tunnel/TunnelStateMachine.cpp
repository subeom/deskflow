/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/TunnelStateMachine.h"

#include <algorithm>

namespace deskflow::tunnel {

using Command = TunnelStateMachine::Command;

TunnelStateMachine::TunnelStateMachine(Policy policy) : m_policy(std::move(policy))
{
  if (m_policy.backoffSeconds.empty()) {
    m_policy.backoffSeconds = {1};
  }
}

void TunnelStateMachine::setTarget(const QString &hostname, int localPort)
{
  if (m_running) {
    return;
  }
  m_status.hostname = hostname;
  m_status.localPort = localPort;
}

TunnelStatus TunnelStateMachine::status() const
{
  auto status = m_status;
  if (status.state == TunnelState::Backoff) {
    const auto leftMs = std::max<qint64>(0, m_restartAt - m_now);
    status.restartInSeconds = static_cast<int>((leftMs + 999) / 1000);
  }
  return status;
}

int TunnelStateMachine::nextPauseSeconds()
{
  const auto index = std::min<size_t>(static_cast<size_t>(m_pauseCount), m_policy.backoffSeconds.size() - 1);
  ++m_pauseCount;
  return m_policy.backoffSeconds.at(index);
}

Command TunnelStateMachine::launch(qint64 now)
{
  m_status.state = TunnelState::Starting;
  m_status.failure = TunnelFailure::None;
  m_status.restartInSeconds = 0;
  m_running = true;
  m_startedAt = now;
  m_lastProbeAt = -1;
  m_firstOpenProbeAt = -1;
  m_listenerLogged = false;
  m_stopAskedAgain = false;
  return Command::Launch;
}

Command TunnelStateMachine::fail(TunnelFailure failure, bool killProcess)
{
  m_status.state = TunnelState::Failed;
  m_status.failure = failure;
  m_status.restartInSeconds = 0;
  return killProcess && m_running ? Command::Kill : Command::None;
}

Command TunnelStateMachine::start(qint64 now)
{
  m_now = now;
  switch (m_status.state) {
  case TunnelState::Idle:
  case TunnelState::Failed:
    if (m_running) {
      // the process of the last run was asked to go and has not been seen going
      return Command::None;
    }
    // a start by the user is a new attempt: the history of the old one does not count
    m_pauseCount = 0;
    m_quickExits = 0;
    return launch(now);
  case TunnelState::Backoff:
    return launch(now);
  default:
    return Command::None;
  }
}

Command TunnelStateMachine::rejectStart(TunnelFailure failure)
{
  if (m_running || m_status.state == TunnelState::Stopping) {
    return Command::None;
  }
  return fail(failure, false);
}

Command TunnelStateMachine::launchFailed(qint64 now)
{
  m_now = now;
  if (m_status.state != TunnelState::Starting) {
    return Command::None;
  }
  m_running = false;
  return fail(TunnelFailure::CloudflaredMissing, false);
}

Command TunnelStateMachine::logEvent(const LogEvent &event, qint64 now)
{
  m_now = now;
  const bool alive = m_status.state == TunnelState::Starting || m_status.state == TunnelState::Listening;
  if (!alive) {
    return Command::None;
  }

  switch (event.kind) {
  case LogEventKind::ListenerStarted:
    if (m_status.state == TunnelState::Starting) {
      m_listenerLogged = true;
      if (m_firstOpenProbeAt < 0) {
        m_lastProbeAt = now;
        return Command::ProbePort;
      }
    }
    return Command::None;

  case LogEventKind::LoginRequired:
    return fail(TunnelFailure::LoginRequired, true);

  case LogEventKind::BindFailed:
    if (m_status.state == TunnelState::Starting) {
      return fail(TunnelFailure::PortBusy, true);
    }
    return Command::None;

  case LogEventKind::OriginFailed:
    // the process runs and the port is open: it is the connection through it that failed
    m_status.failure = event.problem;
    return Command::None;

  case LogEventKind::Other:
    break;
  }
  return Command::None;
}

Command TunnelStateMachine::portProbeFinished(bool open, qint64 now)
{
  m_now = now;
  if (m_status.state != TunnelState::Starting) {
    return Command::None;
  }
  if (open && m_firstOpenProbeAt < 0) {
    m_firstOpenProbeAt = now;
  }
  return Command::None;
}

Command TunnelStateMachine::processExited(int exitCode, qint64 now)
{
  m_now = now;
  const auto wasRunning = m_running;
  m_running = false;

  switch (m_status.state) {
  case TunnelState::Stopping:
    m_status = {TunnelState::Idle, TunnelFailure::None, m_status.hostname, m_status.localPort, 0, 0};
    return Command::None;

  case TunnelState::Failed:
  case TunnelState::Idle:
  case TunnelState::Backoff:
    return Command::None;

  case TunnelState::Starting:
  case TunnelState::Listening:
    break;
  }

  if (!wasRunning) {
    return Command::None;
  }

  if (m_status.state == TunnelState::Listening && now - m_listeningSince >= m_policy.stableAfterMs) {
    m_pauseCount = 0;
    m_quickExits = 0;
  }

  m_status.exitCode = exitCode;
  if (now - m_startedAt < m_policy.quickExitMs) {
    ++m_quickExits;
  } else {
    m_quickExits = 0;
  }

  if (m_quickExits >= m_policy.maxQuickExits) {
    return fail(TunnelFailure::ProcessExited, false);
  }

  m_status.state = TunnelState::Backoff;
  m_status.failure = TunnelFailure::None;
  m_restartAt = now + static_cast<qint64>(nextPauseSeconds()) * 1000;
  return Command::None;
}

Command TunnelStateMachine::serverConnected(qint64 now)
{
  m_now = now;
  if (m_status.state == TunnelState::Listening) {
    m_status.failure = TunnelFailure::None;
    m_pauseCount = 0;
    m_quickExits = 0;
  }
  return Command::None;
}

Command TunnelStateMachine::stop(qint64 now)
{
  m_now = now;
  switch (m_status.state) {
  case TunnelState::Idle:
  case TunnelState::Stopping:
    return Command::None;

  case TunnelState::Backoff:
  case TunnelState::Failed:
    // nothing runs (a process that was asked to go after a login was asked for is on its way out; it is not waited for)
    m_status.state = TunnelState::Idle;
    m_status.failure = TunnelFailure::None;
    m_status.restartInSeconds = 0;
    return m_running ? Command::Kill : Command::None;

  case TunnelState::Starting:
  case TunnelState::Listening:
    m_status.state = TunnelState::Stopping;
    m_status.failure = TunnelFailure::None;
    m_stopAskedAt = now;
    m_stopAskedAgain = false;
    return m_running ? Command::Kill : Command::None;
  }
  return Command::None;
}

Command TunnelStateMachine::tick(qint64 now)
{
  m_now = now;
  switch (m_status.state) {
  case TunnelState::Idle:
  case TunnelState::Failed:
    return Command::None;

  case TunnelState::Backoff:
    return now >= m_restartAt ? launch(now) : Command::None;

  case TunnelState::Stopping:
    if (m_running && !m_stopAskedAgain && now - m_stopAskedAt >= m_policy.stopTimeoutMs) {
      m_stopAskedAgain = true;
      return Command::Kill;
    }
    return Command::None;

  case TunnelState::Listening:
    if (now - m_listeningSince >= m_policy.stableAfterMs) {
      m_pauseCount = 0;
      m_quickExits = 0;
    }
    return Command::None;

  case TunnelState::Starting:
    break;
  }

  if (now - m_startedAt >= m_policy.readyTimeoutMs) {
    return fail(TunnelFailure::StartTimedOut, true);
  }

  if (m_firstOpenProbeAt >= 0) {
    if (m_running && now - m_firstOpenProbeAt >= m_policy.confirmMs) {
      m_status.state = TunnelState::Listening;
      m_listeningSince = now;
    }
    return Command::None;
  }

  const bool probeDue = m_listenerLogged || now - m_startedAt >= m_policy.probeStartMs;
  if (probeDue && (m_lastProbeAt < 0 || now - m_lastProbeAt >= m_policy.probeIntervalMs)) {
    m_lastProbeAt = now;
    return Command::ProbePort;
  }
  return Command::None;
}

} // namespace deskflow::tunnel
