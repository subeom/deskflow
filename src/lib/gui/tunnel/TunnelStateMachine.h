/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/CloudflaredLogParser.h"
#include "gui/tunnel/TunnelTypes.h"

#include <QtGlobal>

#include <vector>

namespace deskflow::tunnel {

//! The times and counts of the machine, in milliseconds; the defaults are what is used, a test makes them smaller
struct TunnelPolicy
{
  qint64 readyTimeoutMs = 8000; //!< from the start to the local port being open
  qint64 probeStartMs = 300;    //!< probing starts this long after the start, or when the listener is logged
  qint64 probeIntervalMs = 200; //!< between two probes
  qint64 confirmMs = 300;       //!< the process must still run this long after the first open probe
  qint64 stopTimeoutMs = 3000;  //!< after which a process that was asked to go is asked again
  qint64 stableAfterMs = 60000; //!< a process that listened this long starts the pauses over
  qint64 quickExitMs = 2000;    //!< a process that stops sooner than this after its start stopped at once
  int maxQuickExits = 5;        //!< in a row; then it is Failed
  std::vector<int> backoffSeconds = {1, 2, 4, 8, 16, 30, 60};
};

/**
 * What a cloudflared that the tunnel runs is doing, as a state machine without a clock, a process or a socket of its
 * own: whoever runs the process tells it what happened and when (in milliseconds of any steady clock), and it says what
 * to do next. That makes every path testable, and the process runner (a later step) a thin layer.
 *
 * Idle -> Starting -> Listening; a process that stops on its own goes to Backoff and is started again after a pause
 * that grows, and to Failed when it keeps stopping at once or when restarting cannot help (a login is asked for, the
 * port is busy); Stopping while the process is asked to go.
 */
class TunnelStateMachine
{
public:
  //! What the runner does next
  enum class Command
  {
    None,
    Launch,    //!< start cloudflared
    ProbePort, //!< try to open a connection to the local port and tell portProbeFinished()
    Kill       //!< stop the process (harder when it is asked again)
  };

  using Policy = TunnelPolicy;

  explicit TunnelStateMachine(Policy policy = {});

  //! Where the tunnel is meant to listen; the hostname is only for the status. Not while a process runs.
  void setTarget(const QString &hostname, int localPort);

  //! The status as of the last call (the seconds of a pause count from the time of that call)
  TunnelStatus status() const;
  bool processRunning() const
  {
    return m_running;
  }

  //! From Idle, Failed or Backoff: start. In any other state it is ignored.
  Command start(qint64 now);
  //! The start was refused before a process was made (no cloudflared, no token, TLS off): Failed with \p failure
  Command rejectStart(TunnelFailure failure);
  //! The process could not be started at all
  Command launchFailed(qint64 now);
  //! One line of what the process wrote, parsed
  Command logEvent(const LogEvent &event, qint64 now);
  Command portProbeFinished(bool open, qint64 now);
  Command processExited(int exitCode, qint64 now);
  //! The client got a connection through the tunnel to the server: a problem that cloudflared reported is over
  Command serverConnected(qint64 now);
  Command stop(qint64 now);
  //! Time passes; call it about every 100 ms while the tunnel is not Idle or Failed
  Command tick(qint64 now);

private:
  Command fail(TunnelFailure failure, bool killProcess);
  Command launch(qint64 now);
  int nextPauseSeconds();

  Policy m_policy;
  TunnelStatus m_status;
  qint64 m_now = 0;
  bool m_running = false;
  qint64 m_startedAt = 0;
  qint64 m_listeningSince = 0;
  qint64 m_lastProbeAt = -1;
  qint64 m_firstOpenProbeAt = -1;
  bool m_listenerLogged = false;
  qint64 m_restartAt = 0;
  qint64 m_stopAskedAt = 0;
  bool m_stopAskedAgain = false;
  int m_pauseCount = 0;
  int m_quickExits = 0;
};

} // namespace deskflow::tunnel
