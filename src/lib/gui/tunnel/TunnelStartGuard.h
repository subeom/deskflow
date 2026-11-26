/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/ITunnelGate.h"

#include <QObject>

#include <functional>

namespace deskflow::tunnel {

/**
 * Puts the start of the client core behind the tunnel: the process of the core asks it first thing when it is told to
 * start, and when the tunnel is needed and does not listen yet, the start is postponed (the guard asks the tunnel to
 * start and starts the core when it listens). All of it lives here, so that the process of the core only holds the
 * guard and the order (tunnel first, core second; core stopped first, tunnel second) is tested without a process.
 */
class TunnelStartGuard : public QObject
{
  Q_OBJECT

public:
  struct Actions
  {
    //! the start of the core is postponed: show that it is starting
    std::function<void()> waiting;
    //! the tunnel listens: start the core now (the guard lets this start through)
    std::function<void()> resume;
    //! the tunnel failed, while the core waited for it (\p wasWaiting) or after the core started
    std::function<void(TunnelFailure failure, bool wasWaiting)> failed;
  };

  TunnelStartGuard(ITunnelGate *gate, Actions actions, QObject *parent = nullptr);

  /**
   * Call it first thing in the start of the core. True: this start is postponed and the core is started again (through
   * Actions::resume) when the tunnel listens. False: start the core now.
   */
  bool intercept(bool clientMode);
  bool isWaiting() const
  {
    return m_waiting;
  }

  //! The core is stopped (or a start that was waiting is cancelled): the tunnel is not needed any more
  void coreStopped();

private:
  void onListening();
  void onFailed(TunnelFailure failure);

  ITunnelGate *m_gate;
  Actions m_actions;
  bool m_waiting = false;
  bool m_resuming = false;
};

} // namespace deskflow::tunnel
