/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/TunnelSupervisor.h"

#include "common/TunnelAddress.h"
#include "gui/tunnel/CloudflaredLaunch.h"
#include "gui/tunnel/CloudflaredLogParser.h"
#include "gui/tunnel/OrphanRecord.h"
#include "gui/tunnel/QtPortProber.h"
#include "gui/tunnel/QtTunnelProcess.h"
#include "gui/tunnel/SecretStoreFactory.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPointer>
#include <QThread>
#include <QTimer>

#include <algorithm>

namespace deskflow::tunnel {

namespace {

constexpr int kProbeTimeoutMs = 500;
constexpr int kWakeProbeTimeoutMs = 1000;

} // namespace

TunnelSupervisor::TunnelSupervisor(Dependencies dependencies, QObject *parent)
    : QObject(parent),
      m_deps(std::move(dependencies)),
      m_machine(m_deps.policy)
{
  if (!m_deps.clock) {
    const auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    m_deps.clock = [timer] { return timer->elapsed(); };
  }
  if (!m_deps.locate) {
    m_deps.locate = [](const QString &configuredPath) { return locateCloudflared(configuredPath); };
  }

  if (m_deps.tickIntervalMs > 0) {
    m_timer = new QTimer(this);
    m_timer->setInterval(m_deps.tickIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &TunnelSupervisor::tick);
  }
}

TunnelSupervisor::~TunnelSupervisor()
{
  // nothing of the tunnel stays behind the supervisor
  if (m_process) {
    m_process->disconnect(this);
    delete m_process;
    m_process = nullptr;
    removeOrphanRecord(m_deps.orphanFile);
  }
  wipeToken();
}

TunnelSupervisor::Dependencies TunnelSupervisor::systemDependencies()
{
  Dependencies deps;
  deps.store = createSystemSecretStore();
  deps.processFactory = QtTunnelProcess::factory();
  deps.prober = std::make_shared<QtPortProber>();
  deps.inspector = createSystemProcessInspector();
  deps.orphanFile = defaultOrphanRecordPath();
  deps.baseEnvironment = QProcessEnvironment::systemEnvironment();
  return deps;
}

TunnelStatus TunnelSupervisor::status() const
{
  return m_machine.status();
}

bool TunnelSupervisor::isProcessRunning() const
{
  return m_process != nullptr && m_process->isRunning();
}

qint64 TunnelSupervisor::advanceClock()
{
  const auto real = m_deps.clock();
  if (m_lastReal >= 0) {
    const auto delta = real - m_lastReal;
    if (delta > 0) {
      m_now += std::min(delta, kMaxStepMs);
    }
    if (delta > kWakeGapMs) {
      m_wakeSeen = true;
    }
  }
  m_lastReal = real;
  return m_now;
}

void TunnelSupervisor::log(const QString &line)
{
  Q_EMIT logLine(m_redactor.redact(line));
}

void TunnelSupervisor::setPort(int port)
{
  if (m_port != port) {
    m_port = port;
    Q_EMIT activePortChanged(port);
  }
}

void TunnelSupervisor::wipeToken()
{
  m_token.wipe();
  m_redactor.clear();
}

void TunnelSupervisor::settle()
{
  const auto state = m_machine.status().state;
  if (state == TunnelState::Idle || state == TunnelState::Failed) {
    setPort(0);
    if (m_process == nullptr) {
      wipeToken();
    }
  }
}

void TunnelSupervisor::publish()
{
  const auto status = m_machine.status();
  const auto previous = m_published.state;
  const bool changed = !m_publishedOnce || !(status == m_published);
  m_published = status;
  m_publishedOnce = true;

  if (m_timer != nullptr) {
    const bool wanted = status.state != TunnelState::Idle && status.state != TunnelState::Failed;
    if ((wanted || m_process != nullptr) && !m_timer->isActive()) {
      m_lastReal = -1;
      m_timer->start();
    } else if (!wanted && m_process == nullptr && m_timer->isActive()) {
      m_timer->stop();
    }
  }

  if (changed) {
    Q_EMIT statusChanged(status);
  }
  if (status.state == TunnelState::Listening && previous != TunnelState::Listening) {
    if (!m_machine.listenerLineSeen()) {
      log(QStringLiteral(
          "cloudflared did not log its listener line; accepted because the port is open and the process stayed up"
      ));
    }
    Q_EMIT listening();
  }
  if (status.state == TunnelState::Failed) {
    if (!m_failedEmitted) {
      m_failedEmitted = true;
      Q_EMIT failed(status.failure);
    }
  } else {
    m_failedEmitted = false;
  }
}

bool TunnelSupervisor::start(const TunnelRequest &request)
{
  const auto state = m_machine.status().state;
  if (state == TunnelState::Starting || state == TunnelState::Listening || state == TunnelState::Backoff) {
    return false;
  }
  if (state == TunnelState::Stopping || m_process != nullptr) {
    // the old process is on its way out: the start follows when it is gone
    m_pending = request;
    return false;
  }

  m_pending.reset();
  m_request = request;
  m_portRetried = false;
  m_portRetryPending = false;
  const auto now = advanceClock();
  m_machine.setTarget(request.hostname, request.preferredPort);

  const auto reject = [this](TunnelFailure failure) {
    log(failureText(failure));
    execute(m_machine.rejectStart(failure));
    m_failedEmitted = false;
    settle();
    publish();
    return false;
  };

  if (!request.tlsEnabled) {
    return reject(TunnelFailure::TlsRequired);
  }
  if (!isValidHostname(request.hostname)) {
    return reject(TunnelFailure::InvalidHostname);
  }

  const auto location = m_deps.locate(request.configuredPath);
  if (!location.found()) {
    return reject(TunnelFailure::CloudflaredMissing);
  }
  // the path that is run is the path that was found: a name without a directory would be looked up again in PATH
  if (!QDir::isAbsolutePath(location.path)) {
    log(QStringLiteral("cloudflared was found as \"%1\", which is not an absolute path: not run").arg(location.path));
    return reject(TunnelFailure::CloudflaredMissing);
  }
  m_program = location.path;

  StoreResult stored;
  auto token = m_deps.store ? m_deps.store->load(&stored) : std::nullopt;
  if (!token.has_value() || !token->isComplete()) {
    if (!stored.ok && !stored.notFound && !stored.detail.isEmpty()) {
      log(QStringLiteral("The Service Token could not be read: %1").arg(stored.detail));
    }
    return reject(TunnelFailure::CredentialsMissing);
  }
  m_token = *token;
  token->wipe();
  m_redactor.clear();
  m_redactor.addSecret(m_token.secret);
  m_redactor.addSecret(m_token.clientId);

  if (!m_deps.orphanFile.isEmpty() && m_deps.inspector) {
    const auto cleanup = cleanUpOrphan(m_deps.orphanFile, *m_deps.inspector);
    switch (cleanup.outcome) {
    case OrphanOutcome::Ended:
      log(QStringLiteral("Ended a cloudflared that an earlier run left behind (process %1)").arg(cleanup.record.pid));
      break;
    case OrphanOutcome::EndFailed:
      log(QStringLiteral("A cloudflared that an earlier run left behind could not be ended (process %1)")
              .arg(cleanup.record.pid));
      break;
    case OrphanOutcome::Different:
      log(QStringLiteral(
          "The record of an earlier cloudflared was found, but its process number belongs to another "
          "program now: left alone"
      ));
      break;
    case OrphanOutcome::NoRecord:
    case OrphanOutcome::NotRunning:
      break;
    }
  }

  int port = request.preferredPort;
  if (!m_deps.prober->canListen(port)) {
    port = m_deps.prober->findFreePort();
    if (port <= 0) {
      return reject(TunnelFailure::PortBusy);
    }
    log(QStringLiteral("Local port %1 is busy, using port %2 instead").arg(request.preferredPort).arg(port));
  }

  setPort(port);
  m_machine.setTarget(request.hostname, port);
  execute(m_machine.start(now));
  publish();
  return true;
}

void TunnelSupervisor::stop()
{
  m_pending.reset();
  m_portRetryPending = false;
  execute(m_machine.stop(advanceClock()));
  settle();
  publish();
}

bool TunnelSupervisor::stopAndWait(int timeoutMs)
{
  stop();

  const auto waitForProcess = [this](int ms) {
    QElapsedTimer waited;
    waited.start();
    while (m_process != nullptr && waited.elapsed() < ms) {
      tick();
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
      QThread::msleep(5);
    }
  };

  waitForProcess(timeoutMs);
  if (m_process != nullptr) {
    m_process->kill();
    waitForProcess(1000);
  }

  if (m_process != nullptr) {
    // it never reported its end: it is dropped, and the object ends it
    releaseProcess(true);
    removeOrphanRecord(m_deps.orphanFile);
    execute(m_machine.processExited(-1, advanceClock()));
    settle();
    publish();
  }
  return m_process == nullptr;
}

void TunnelSupervisor::tick()
{
  const auto now = advanceClock();
  const bool wake = std::exchange(m_wakeSeen, false);

  execute(m_machine.tick(now));

  // a process that was asked to go after a failure (the machine does not wait for it) is ended when it stays
  if (m_process != nullptr && m_terminateSentAt >= 0 && !m_killSent &&
      m_machine.status().state != TunnelState::Stopping && now - m_terminateSentAt >= m_deps.policy.stopTimeoutMs) {
    m_killSent = true;
    m_process->kill();
  }

  publish();
  if (wake) {
    verifyAfterWake();
  }
}

void TunnelSupervisor::notifyServerConnected()
{
  execute(m_machine.serverConnected(advanceClock()));
  publish();
}

void TunnelSupervisor::verifyAfterWake()
{
  if (m_machine.status().state != TunnelState::Listening || m_process == nullptr) {
    return;
  }

  const auto generation = m_generation;
  QPointer<TunnelSupervisor> self(this);
  m_deps.prober->probeOpen(m_port, kWakeProbeTimeoutMs, [self, generation](bool open) {
    if (!self || self->m_generation != generation || open || self->m_process == nullptr ||
        self->m_machine.status().state != TunnelState::Listening) {
      return;
    }
    // the process is ended, and the state machine starts it again after its short pause
    self->log(QStringLiteral("The local port is closed after the computer woke up: restarting cloudflared"));
    self->m_process->kill();
  });
}

void TunnelSupervisor::execute(Command command)
{
  switch (command) {
  case Command::None:
    break;
  case Command::Launch:
    launch();
    break;
  case Command::ProbePort:
    probePort();
    break;
  case Command::Kill:
    if (m_process == nullptr) {
      break;
    }
    if (m_terminateSentAt < 0) {
      m_terminateSentAt = m_now;
      m_process->terminate();
    } else {
      m_killSent = true;
      m_process->kill();
    }
    break;
  }
}

void TunnelSupervisor::launch()
{
  const LaunchRequest launchRequest{m_program, m_request.hostname, m_port};
  const auto spec = buildLaunchSpec(launchRequest, m_token, m_deps.baseEnvironment);
  if (!spec.has_value()) {
    log(failureText(TunnelFailure::CloudflaredMissing));
    execute(m_machine.launchFailed(m_now));
    settle();
    return;
  }

  ++m_generation;
  m_terminateSentAt = -1;
  m_killSent = false;

  m_process = m_deps.processFactory(this);
  connect(m_process, &ITunnelProcess::started, this, &TunnelSupervisor::onStarted);
  connect(m_process, &ITunnelProcess::startFailed, this, &TunnelSupervisor::onStartFailed);
  connect(m_process, &ITunnelProcess::outputLine, this, &TunnelSupervisor::onOutputLine);
  connect(m_process, &ITunnelProcess::finished, this, &TunnelSupervisor::onFinished);

  log(QStringLiteral("Starting: %1").arg(spec->commandLine()));
  m_process->start(*spec);
}

void TunnelSupervisor::probePort()
{
  const auto generation = m_generation;
  QPointer<TunnelSupervisor> self(this);
  m_deps.prober->probeOpen(m_port, kProbeTimeoutMs, [self, generation](bool open) {
    if (!self || self->m_generation != generation) {
      return;
    }
    self->execute(self->m_machine.portProbeFinished(open, self->advanceClock()));
    self->publish();
  });
}

void TunnelSupervisor::onStarted()
{
  const auto pid = m_process != nullptr ? m_process->processId() : 0;
  log(QStringLiteral("cloudflared is running (process %1)").arg(pid));

  if (m_deps.orphanFile.isEmpty() || !m_deps.inspector || pid <= 0) {
    return;
  }
  const auto info = m_deps.inspector->inspect(pid);
  if (!info.has_value() || info->path.isEmpty()) {
    return;
  }

  OrphanRecord record;
  record.pid = pid;
  record.startTimeMs = info->startTimeMs;
  record.path = info->path;
  record.hostname = m_request.hostname;
  record.port = m_port;
  writeOrphanRecord(m_deps.orphanFile, record);
}

void TunnelSupervisor::onStartFailed(const QString &detail)
{
  log(QStringLiteral("cloudflared could not be started: %1").arg(detail));
  releaseProcess(false);
  execute(m_machine.launchFailed(advanceClock()));
  settle();
  publish();
}

void TunnelSupervisor::onOutputLine(const QString &line)
{
  // the line is made safe first, and only the safe line is read, logged and shown
  const auto event = parseCloudflaredLine(m_redactor.redact(line));
  if (!event.message.isEmpty()) {
    Q_EMIT logLine(event.message);
  }
  if (event.kind == LogEventKind::OriginFailed) {
    ++m_originFailures;
    m_lastOriginFailure = event.problem;
  }
  if (event.kind == LogEventKind::BindFailed && !m_portRetried && m_process != nullptr &&
      (m_machine.status().state == TunnelState::Starting || m_machine.status().state == TunnelState::Listening)) {
    // A program of this computer took the port between the check and the listen of cloudflared (and may have answered
    // the probe in the meantime). Once, the start goes on with a port that is free: the failure is not told to the
    // machine, which would stop the start of the core; the process is ended and the retry follows when it is gone.
    m_portRetried = true;
    m_portRetryPending = true;
    execute(Command::Kill);
    publish();
    return;
  }
  execute(m_machine.logEvent(event, advanceClock()));
  publish();
}

void TunnelSupervisor::retryOnAnotherPort()
{
  m_portRetryPending = false;
  if (m_machine.status().state != TunnelState::Backoff) {
    // stopped by the user in the meantime, or the process ended for a reason that the machine already judged
    return;
  }
  const auto taken = m_port;
  const auto port = m_deps.prober->findFreePort();
  if (port > 0) {
    log(QStringLiteral("Local port %1 was taken by another program, using port %2 instead").arg(taken).arg(port));
    setPort(port);
    m_machine.setTarget(m_request.hostname, port);
    return;
  }
  log(failureText(TunnelFailure::PortBusy));
  execute(m_machine.rejectStart(TunnelFailure::PortBusy));
}

void TunnelSupervisor::onFinished(int exitCode)
{
  const auto stopping = m_machine.status().state == TunnelState::Stopping;
  if (!stopping) {
    log(QStringLiteral("cloudflared stopped (exit code %1)").arg(exitCode));
  }

  removeOrphanRecord(m_deps.orphanFile);
  releaseProcess(false);
  execute(m_machine.processExited(exitCode, advanceClock()));
  if (m_portRetryPending) {
    retryOnAnotherPort();
  }
  settle();
  publish();

  const auto state = m_machine.status().state;
  if (m_pending.has_value() && (state == TunnelState::Idle || state == TunnelState::Failed)) {
    const auto request = *m_pending;
    m_pending.reset();
    start(request);
  }
}

void TunnelSupervisor::releaseProcess(bool immediately)
{
  if (m_process == nullptr) {
    return;
  }
  m_process->disconnect(this);
  if (immediately) {
    delete m_process;
  } else {
    // the process is in the middle of telling that it ended
    m_process->deleteLater();
  }
  m_process = nullptr;
  m_terminateSentAt = -1;
  m_killSent = false;
}

} // namespace deskflow::tunnel
