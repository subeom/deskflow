/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/CloudflaredLocator.h"
#include "gui/tunnel/IPortProber.h"
#include "gui/tunnel/ISecretStore.h"
#include "gui/tunnel/ITunnelProcess.h"
#include "gui/tunnel/ProcessInspector.h"
#include "gui/tunnel/RepeatCollapser.h"
#include "gui/tunnel/SecretRedactor.h"
#include "gui/tunnel/ServiceToken.h"
#include "gui/tunnel/TunnelStateMachine.h"
#include "gui/tunnel/TunnelTypes.h"

#include <QObject>
#include <QProcessEnvironment>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class QTimer;

namespace deskflow::tunnel {

//! What the tunnel is asked to be
struct TunnelRequest
{
  QString hostname;
  int preferredPort = 24800;
  QString configuredPath; //!< the path of cloudflared that the user set; empty: look for it
  bool tlsEnabled = true; //!< Deskflow's TLS: the tunnel is refused without it

  bool operator==(const TunnelRequest &other) const = default;
};

/**
 * Runs `cloudflared access tcp` for as long as it is wanted. It checks what the start needs (TLS, the host name,
 * cloudflared, the Service Token, a leftover cloudflared of an earlier run, a free local port), starts the process with
 * the token in its environment and nowhere else, follows it with the state machine (which says when to probe the port,
 * when to end the process, when to start it again) and tells what happens through signals.
 *
 * Everything the supervisor touches is given to it (the secret store, the way to find cloudflared, the way to start a
 * process, the ports, the process inspector, the clock), so a test runs all of it with fakes and nothing of the system.
 *
 * It is not thread safe: it lives in the thread of the window.
 *
 * What is written to the log has gone through the redactor (the token, and the shapes of the headers and variables that
 * carry it); the Service Token is kept in memory only while the tunnel is wanted and wiped when the process is gone.
 */
class TunnelSupervisor : public QObject
{
  Q_OBJECT

public:
  struct Dependencies
  {
    std::shared_ptr<ISecretStore> store;
    //! The place of cloudflared; the argument is the path the user set
    std::function<CloudflaredLocation(const QString &configuredPath)> locate;
    TunnelProcessFactory processFactory;
    std::shared_ptr<IPortProber> prober;
    std::shared_ptr<IProcessInspector> inspector;
    //! Where the record of the running cloudflared is kept; empty: no record, so no cleanup of a leftover process
    QString orphanFile;
    //! A steady clock in milliseconds
    std::function<qint64()> clock;
    //! The environment of this process, the base of that of cloudflared
    QProcessEnvironment baseEnvironment;
    TunnelPolicy policy;
    //! The pause between two ticks of the timer of the supervisor; 0: no timer, the caller calls tick()
    int tickIntervalMs = 100;
  };

  /**
   * The longest step of the clock of the tunnel at one tick. A longer time between two ticks means the computer was
   * asleep (or the program was held): it is not counted, so that the time out of a start does not run out in a sleep.
   */
  static constexpr qint64 kMaxStepMs = 1000;
  //! A time between two ticks that is taken for a sleep: the local port is checked again
  static constexpr qint64 kWakeGapMs = 3000;

  explicit TunnelSupervisor(Dependencies dependencies, QObject *parent = nullptr);
  ~TunnelSupervisor() override;

  //! The real secret store, locator, process, ports, inspector and clock
  static Dependencies systemDependencies();

  /**
   * Starts the tunnel. False when it is not started: it runs or starts already (start it again after stop()), or the
   * start was refused (then the status is Failed with the reason). A start while the tunnel is stopping is done when it
   * has stopped.
   */
  bool start(const TunnelRequest &request);
  //! Ends the process (it is asked to go and, when it stays, ended) and does not start it again
  void stop();
  /**
   * stop(), and waits until the process is gone; ends it when it does not go within \p timeoutMs. For the end of the
   * program: nothing stays behind. True when the process is gone.
   */
  bool stopAndWait(int timeoutMs = 4000);

  //! One step of the clock; the timer calls it. Public for tests that run without a timer.
  void tick();
  //! The client got a connection through the tunnel: a problem that cloudflared reported is over
  void notifyServerConnected();
  //! The computer woke up, or the network changed: checks that the local port is still open (it restarts the process
  //! when it is not). The supervisor calls it itself when it sees a gap in the ticks.
  void verifyAfterWake();

  TunnelStatus status() const;
  bool isProcessRunning() const;
  //! The port that cloudflared listens on, or will listen on once started; 0 when the tunnel is not started
  int activePort() const
  {
    return m_port;
  }
  const TunnelRequest &request() const
  {
    return m_request;
  }
  //! How many connections through the tunnel cloudflared said it could not open, since the supervisor was made. A test
  //! of the connection compares the count before and after, so that an old problem is not taken for a new one.
  quint64 originFailureCount() const
  {
    return m_originFailures;
  }
  //! Why the last of them failed
  TunnelFailure lastOriginFailure() const
  {
    return m_lastOriginFailure;
  }

Q_SIGNALS:
  void statusChanged(const deskflow::tunnel::TunnelStatus &status);
  //! A line for the log; already redacted. What cloudflared wrote and what the supervisor did, in one stream
  void logLine(const QString &line);
  //! The tunnel became ready: its local port is open
  void listening();
  void failed(deskflow::tunnel::TunnelFailure failure);
  //! The port of the tunnel was chosen (it is not the preferred one when that was busy) or is gone (0)
  void activePortChanged(int port);

private:
  using Command = TunnelStateMachine::Command;

  void execute(Command command);
  void launch();
  void probePort();
  void onStarted();
  void onStartFailed(const QString &detail);
  void onOutputLine(const QString &line);
  void onFinished(int exitCode);
  void retryOnAnotherPort();
  void releaseProcess(bool immediately);
  void wipeToken();
  void settle();
  void setPort(int port);
  void log(const QString &line);
  void publish();
  qint64 advanceClock();

  Dependencies m_deps;
  TunnelStateMachine m_machine;
  QTimer *m_timer = nullptr;

  TunnelRequest m_request;
  std::optional<TunnelRequest> m_pending;
  ServiceToken m_token;
  SecretRedactor m_redactor;
  ITunnelProcess *m_process = nullptr;
  QString m_program;
  int m_port = 0;

  qint64 m_now = 0;
  qint64 m_lastReal = -1;
  bool m_wakeSeen = false;
  bool m_failedEmitted = false;
  qint64 m_terminateSentAt = -1;
  bool m_killSent = false;
  quint64 m_generation = 0;
  quint64 m_originFailures = 0;
  //! a program took the local port before cloudflared could listen: the start goes on once on a free port
  bool m_portRetried = false;
  bool m_portRetryPending = false;
  TunnelFailure m_lastOriginFailure = TunnelFailure::None;
  //! the same failure of the connection, again and again while the tunnel of the server is down, is not in the log each
  //! time
  RepeatCollapser m_repeats;
  TunnelStatus m_published;
  bool m_publishedOnce = false;
};

} // namespace deskflow::tunnel
