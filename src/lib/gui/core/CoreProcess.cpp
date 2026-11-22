/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2024 - 2025 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreProcess.h"

#include "common/ExitCodes.h"
#include "gui/core/CoreLogText.h"
#include "gui/core/ServiceQuit.h"
#include "gui/ipc/CoreIpcClient.h"
#include "gui/ipc/DaemonIpcClient.h"
#include "gui/tunnel/ITunnelGate.h"
#include "gui/tunnel/TunnelStartGuard.h"
#include "gui/tunnel/TunnelTypes.h"

#if defined(Q_OS_MACOS)
#include "OSXHelpers.h"
#include <mach/mach_time.h>
#endif

#ifdef Q_OS_LINUX
#include <signal.h>
#include <sys/prctl.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMetaEnum>
#include <QMutexLocker>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QRegularExpression>

namespace deskflow::gui {

namespace {

#if defined(Q_OS_MACOS)
bool isFreshInputIndicatorActivity(const deskflow::input::InputIndicatorActivity &activity)
{
  mach_timebase_info_data_t timebase{};
  if (mach_timebase_info(&timebase) != KERN_SUCCESS || timebase.numer == 0) {
    return false;
  }

  constexpr uint64_t kMaximumAgeNanoseconds = 300ULL * 1000000ULL;
  const auto maximumAge =
      kMaximumAgeNanoseconds * static_cast<uint64_t>(timebase.denom) / static_cast<uint64_t>(timebase.numer);
  return deskflow::input::isInputIndicatorActivityFresh(activity, mach_absolute_time(), maximumAge);
}
#else
bool isFreshInputIndicatorActivity(const deskflow::input::InputIndicatorActivity &)
{
  return false;
}
#endif

} // namespace

const int kRetryDelay = 1000;
// how long the quit waits for the service to be reached, and then for the stop of the service core to be written to it
const int kQuitConnectWaitMs = 1000;
const int kQuitFlushMs = 1000;
const auto kLineSplitRegex = QRegularExpression("\r|\n|\r\n");
constexpr int kInputIndicatorSubscriptionRenewalMs = 20'000;

QString inputIndicatorCapability()
{
  QByteArray bytes;
  bytes.reserve(deskflow::input::kInputIndicatorCapabilityLength / 2);
  auto *random = QRandomGenerator::system();
  while (bytes.size() < deskflow::input::kInputIndicatorCapabilityLength / 2) {
    bytes.append(static_cast<char>(random->bounded(256)));
  }
  return QString::fromLatin1(bytes.toHex());
}

QString CoreProcess::processModeToString(const Settings::ProcessMode mode)
{
  return QVariant::fromValue(mode).toString().toLower();
}

QString CoreProcess::processStateToString(const CoreProcess::ProcessState state)
{
  return QVariant::fromValue(state).toString().toLower();
}

/**
 * @brief Wraps options that contain spaces in quotes
 *
 * Useful to handle things like paths with spaces (e.g. "C:\Program Files").
 *
 * Can also be used to create a representation of a command that can be pasted
 * into a terminal.
 */
QString CoreProcess::makeQuotedArgs(const QString &app, const QStringList &args)
{
  QStringList command = {app};
  command.append(args);

  static const auto quote = QStringLiteral("\"");
  static const auto space = QStringLiteral(" ");
  QStringList quoted;
  for (const auto &item : std::as_const(command)) {
    auto temp = item.simplified();
    if (const auto wrapped = (temp.startsWith(quote) && temp.endsWith(quote)); temp.contains(space) && !wrapped) {
      temp = QStringLiteral("%1%2%1").arg(quote, temp);
    }
    quoted.append(temp);
  }

  return quoted.join(space);
}

/**
 * @brief If IPv6, ensures the IP is surround in square brackets.
 */
QString CoreProcess::wrapIpv6(const QString &address)
{
  static const auto colon = QStringLiteral(":");
  static const auto openBracket = QStringLiteral("[");
  static const auto closeBracket = QStringLiteral("]");

  if (!address.contains(colon) || address.isEmpty()) {
    return address;
  }

  QString wrapped = address;

  if (!address.startsWith(openBracket)) {
    wrapped.prepend(openBracket);
  }

  if (!address.endsWith(closeBracket)) {
    wrapped.append(closeBracket);
  }

  return wrapped;
}

//
// CoreProcess
//

CoreProcess::CoreProcess(const ServerConfig &serverConfig, DaemonClientFactory daemonClientFactory)
    : m_serverConfig(serverConfig),
      m_daemonIpcClient{daemonClientFactory ? daemonClientFactory(this) : new ipc::DaemonIpcClient(this)}
{
  m_appPath = QStringLiteral("%1/%2").arg(QCoreApplication::applicationDirPath(), kCoreBinName);
  m_inputIndicatorCapability = inputIndicatorCapability();
  if (!QFile::exists(m_appPath)) {
    qCritical("core server binary does not exist");
  }

  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::connected, this, &CoreProcess::daemonIpcClientConnected);
  connect(
      m_daemonIpcClient, &ipc::DaemonIpcClient::connectionFailed, this, &CoreProcess::daemonIpcClientConnectionFailed
  );
  connect(m_daemonIpcClient, &ipc::DaemonIpcClient::logPathReceived, this, &CoreProcess::setupDaemonLogTail);

  connect(&m_retryTimer, &QTimer::timeout, this, [this] {
    if (m_processState == ProcessState::RetryPending) {
      start();
    } else {
      qDebug("retry cancelled, process state is not retry pending");
    }
  });
  m_inputIndicatorSubscriptionTimer.setInterval(kInputIndicatorSubscriptionRenewalMs);
  connect(
      &m_inputIndicatorSubscriptionTimer, &QTimer::timeout, this, &CoreProcess::sendInputIndicatorSubscription
  );
}

void CoreProcess::onProcessReadyReadStandardOutput()
{
  if (m_process) {
    handleLogLines(m_process->readAllStandardOutput());
  }
}

void CoreProcess::onProcessReadyReadStandardError()
{
  if (m_process) {
    handleLogLines(m_process->readAllStandardError());
  }
}

void CoreProcess::daemonIpcClientConnected()
{
  applyLogLevel();
  m_daemonIpcClient->requestLogPath();
}

void CoreProcess::checkExistingProcess()
{
  qInfo("checking existing core");

  auto *client = new ipc::CoreIpcClient(this);
  connect(client, &ipc::CoreIpcClient::connected, this, [client] {
    qInfo("existing core has matching version, leaving it running");
    client->deleteLater();
  });
  connect(client, &ipc::CoreIpcClient::versionMismatch, this, [client] {
    qInfo("existing core has mismatched version, asking it to stop");
    client->sendStop();
  });
  connect(client, &ipc::CoreIpcClient::serverShutdown, this, [this, client] {
    qInfo("existing core stopped successfully");
    client->deleteLater();
    setProcessState(ProcessState::RetryPending);
    m_retryTimer.setSingleShot(true);
    m_retryTimer.start(kRetryDelay);
  });
  connect(client, &ipc::CoreIpcClient::connectionFailed, this, [client] {
    qCritical("could not contact existing core");
    client->deleteLater();
  });
  client->connectToServer();
}

void CoreProcess::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
  using enum ProcessState;
  setConnectionState(ConnectionState::Disconnected);

  if (m_retryTimer.isActive()) {
    m_retryTimer.stop();
  }

  if (exitCode != s_exitSuccess) {
    setProcessState(Stopped);
    if (exitCode == s_exitDuplicate) {
      checkExistingProcess();
      return;
    }
    qWarning("desktop process exited with code: %d", exitCode);
    return;
  }

  qDebug("desktop process exited normally");

  if (const auto wasStarted = m_processState == Started; wasStarted) {
    qDebug("desktop process was running, retrying in %d ms", kRetryDelay);
    setProcessState(RetryPending);
    m_retryTimer.setSingleShot(true);
    m_retryTimer.start(kRetryDelay);
  } else {
    setProcessState(Stopped);
  }
}

void CoreProcess::applyLogLevel()
{
  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>();
  if (processMode == ProcessMode::Service) {
    qDebug() << "setting daemon log level:" << Settings::logLevelText();
    m_daemonIpcClient->sendLogLevel(Settings::logLevelText());
  }
}

void CoreProcess::startForegroundProcess(const QStringList &args)
{
  using enum ProcessState;

  if (m_processState != Starting) {
    qCritical("not starting core desktop process, unexpected process state");
    return;
  }

  // only make quoted args for printing the command for convenience; so that the
  // core command can be easily copy/pasted to the terminal for testing.
  const auto quoted = makeQuotedArgs(m_appPath, args);
  qInfo("running command: %s", qPrintable(quoted));

#ifdef Q_OS_LINUX
  m_process->setChildProcessModifier([] {
    // the core process becomes orphaned when the gui process exits abruptly (e.g. with kill -9),
    // so ensure the os also kills the core when that happens to the gui.
    prctl(PR_SET_PDEATHSIG, SIGTERM);
  });
#endif

#ifdef Q_OS_MACOS
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(
      QLatin1String(deskflow::input::kInputIndicatorCapabilityEnvironment), m_inputIndicatorCapability
  );
  environment.insert(
      QLatin1String(deskflow::input::kInputIndicatorGuiPidEnvironment),
      QString::number(QCoreApplication::applicationPid())
  );
  m_process->setProcessEnvironment(environment);
#endif

  m_process->start(m_appPath, args);

  if (m_process->waitForStarted()) {
    setProcessState(Started);
  } else {
    setProcessState(Stopped);
    Q_EMIT error(Error::StartFailed);
  }
}

void CoreProcess::startProcessFromDaemon()
{
  if (m_processState != ProcessState::Starting) {
    qCritical("not starting core process from daemon, unexpected process state");
    return;
  }

  const auto configFile = Settings::settingsFile();
  qInfo("sending start to daemon (config file: %s)", qPrintable(configFile));

  auto sendStart = [this, configFile] {
    if (m_quitting) {
      return;
    }
    m_daemonIpcClient->sendConfigFile(configFile);
    m_daemonIpcClient->sendStartProcess();
    setProcessState(ProcessState::Started);
  };

  if (m_daemonIpcClient->isConnected()) {
    sendStart();
  } else {
    m_pendingDaemonStart = connect(
        m_daemonIpcClient, &ipc::DaemonIpcClient::connected, this, sendStart,
        static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection)
    );
    m_daemonIpcClient->connectToServer();
  }
}

void CoreProcess::stopForegroundProcess()
{
  if (m_processState != ProcessState::Stopping) {
    qCritical("not stopping core desktop process, unexpected process state");
    return;
  }

  if (!m_process) {
    qCritical("not stopping core desktop process, no process to stop");
    return;
  }

  qInfo("stopping core desktop process");
  if (m_coreIpcClient && m_coreIpcClient->isConnected()) {
    qDebug("sending stop command to core process");
    m_coreIpcClient->sendStop();

    // Fallback: if the core doesn't confirm shutdown via IPC within the
    // timeout, force-terminate it so we never hang in the Stopping state.
    QTimer::singleShot(kRetryDelay, this, [this] {
      if (m_processState != ProcessState::Stopping) {
        return;
      }

      qWarning("core process did not confirm graceful stop in time, forcing terminate");

      if (m_coreIpcClient) {
        m_coreIpcClient->disconnectFromServer();
        m_coreIpcClient->deleteLater();
        m_coreIpcClient = nullptr;
      }

      if (m_process && m_process->state() == QProcess::ProcessState::Running) {
        m_process->terminate();
      }
    });
    return;
  }

  if (m_process->state() == QProcess::ProcessState::Running) {
    qDebug("process is running, terminating");
    m_process->terminate();
  } else {
    qDebug("process is not running, skipping terminate");
  }
}

void CoreProcess::stopProcessFromDaemon()
{
  if (m_processState != ProcessState::Stopping) {
    qCritical("not stopping core process from daemon, unexpected process state");
    return;
  }

  // the quit sends the stop itself (it waits for the service, which a queued send does not)
  if (m_quitting) {
    setProcessState(ProcessState::Stopped);
    return;
  }

  auto sendStop = [this] {
    m_daemonIpcClient->sendStopProcess();
    setProcessState(ProcessState::Stopped);
  };

  if (m_daemonIpcClient->isConnected()) {
    sendStop();
  } else {
    m_pendingDaemonStop = connect(
        m_daemonIpcClient, &ipc::DaemonIpcClient::connected, this, sendStop,
        static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection)
    );
    m_daemonIpcClient->connectToServer();
  }
}

void CoreProcess::handleLogLines(const QString &text)
{
  const auto lines = text.split(kLineSplitRegex);
  for (const auto &line : lines) {
    if (line.isEmpty()) {
      continue;
    }

#if defined(Q_OS_MACOS)
    // HACK: macOS 10.13.4+ spamming error lines in logs making them
    // impossible to read and debug; giving users a red herring.
    if (line.contains("calling TIS/TSM in non-main thread environment")) {
      continue;
    }

    // the core process is not allowed to show the permission prompt
    // (called "notification permission") and the notification log line is emitted from
    // deep inside cocoa code in the core binary to stdout, so it can't be sent over
    // ipc from the core to the gui and instead the gui has to parse the core output.
    static const QString needle = "OSX Notification: ";
    if (line.contains(needle) && line.contains('|')) {
      const int delimiterPosition = line.indexOf('|');
      const int start = line.indexOf(needle);
      const QString title = line.mid(start + needle.length(), delimiterPosition - start - needle.length());
      const QString body = line.mid(delimiterPosition + 1, line.length() - delimiterPosition);
      if (!showOSXNotification(title, body)) {
        qDebug("osx notification was not shown");
      }
    }
#endif

    // the one line of the hold of the pointer that a macOS server writes is shown in the language of the program
    Q_EMIT logLine(localizeCoreLogLine(line));
  }
}

void CoreProcess::start(std::optional<ProcessMode> processModeOption)
{
  if (m_processState == ProcessState::Started) {
    qCritical("not starting core process, already started");
    return;
  }

  if (m_mode == Settings::CoreMode::None) {
    qCritical("not starting core process, no core mode set");
    return;
  }

  // Cloudflare Tunnel in service mode: the program is ending, and a start that was asked for late must not run
  if (m_quitting) {
    qDebug("not starting core process, the program is ending");
    return;
  }

  // Cloudflare Tunnel: the client core starts when the tunnel listens (before the lock: the start is made again then)
  if (m_tunnelGuard != nullptr) {
    m_tunnelPendingMode = processModeOption;
    if (m_tunnelGuard->intercept(m_mode == Settings::CoreMode::Client)) {
      return;
    }
  }

  QMutexLocker locker(&m_processMutex);

  const auto currentMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  const auto processMode = processModeOption.value_or(currentMode);
  const auto coreMode = QVariant::fromValue(m_mode).toString().toLower();

  qInfo().noquote() << QString("starting %1 process (%2 mode)").arg(coreMode, processModeToString(processMode));

  setProcessState(ProcessState::Starting);

#ifdef Q_OS_MACOS
  requestOSXNotificationPermission();
#endif

  setConnectionState(ConnectionState::Connecting);

  if (processMode == ProcessMode::Desktop) {
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &CoreProcess::onProcessFinished, Qt::UniqueConnection);
    connect(
        m_process, &QProcess::readyReadStandardOutput, this, &CoreProcess::onProcessReadyReadStandardOutput,
        Qt::UniqueConnection
    );
    connect(
        m_process, &QProcess::readyReadStandardError, this, &CoreProcess::onProcessReadyReadStandardError,
        Qt::UniqueConnection
    );
  }

  QStringList args = {coreMode};

  if (m_mode == Settings::CoreMode::Server) {
    const auto [hasNeededPermissions, configFilename] = persistServerConfig();
    if (configFilename.isEmpty()) {
      qCritical("not starting core process, no server config file");
      setProcessState(ProcessState::Stopped);
      setConnectionState(ConnectionState::Disconnected);
      Q_EMIT error(Error::StartFailed);
      return;
    }
    if (!hasNeededPermissions) {
      setProcessState(ProcessState::Stopped);
      setConnectionState(ConnectionState::Disconnected);
      Q_EMIT error(Error::StartFailed);
      return;
    }
    qInfo("core config file: %s", qPrintable(configFilename));
  }

  qDebug().noquote() << "log level:" << Settings::logLevelText();

  if (Settings::value(Settings::Log::ToFile).toBool()) {
    const auto logFile = Settings::value(Settings::Log::File).toString();
    QDir(QFileInfo(logFile).absolutePath()).mkpath(".");
    qInfo().noquote() << "log file:" << logFile;
  }

  // Wired before the start calls so it catches Started from both sync (desktop) and async (service) paths.
  connect(
      this, &CoreProcess::processStateChanged, this,
      [this](ProcessState state) {
        if (state != ProcessState::Started) {
          return;
        }

        // Delay briefly to give the core process time to start its IPC server.
        QTimer::singleShot(kRetryDelay, this, [this] {
          if (m_processState != ProcessState::Started) {
            return;
          }

          if (m_coreIpcClient) {
            m_coreIpcClient->disconnectFromServer();
            m_coreIpcClient->deleteLater();
            m_coreIpcClient = nullptr;
          }

          m_coreIpcClient = new ipc::CoreIpcClient(this);

          // queued so a modal dialog opened by a handler can't block readyRead and strand later messages
          connect(
              m_coreIpcClient, &ipc::CoreIpcClient::commandReceived, this, &CoreProcess::onCoreIpcMessageReceived,
              Qt::QueuedConnection
          );
          connect(m_coreIpcClient, &ipc::CoreIpcClient::connected, this, [this] {
            qDebug("connected to core ipc server");
            sendInputIndicatorSubscription();
          });
          connect(m_coreIpcClient, &ipc::CoreIpcClient::connectionFailed, this, [this] {
            qWarning("failed to establish core ipc connection");
            resetInputIndicatorState();
          });
          connect(m_coreIpcClient, &ipc::CoreIpcClient::serverShutdown, this, [this, client = m_coreIpcClient] {
            qDebug("core ipc server shut down cleanly");
            client->deleteLater();
            if (m_coreIpcClient == client)
              m_coreIpcClient = nullptr;
            resetInputIndicatorState();
          });

          m_coreIpcClient->connectToServer();
        });
      },
      static_cast<Qt::ConnectionType>(Qt::SingleShotConnection | Qt::QueuedConnection)
  );

  if (processMode == ProcessMode::Desktop) {
    startForegroundProcess(args);
  } else if (processMode == ProcessMode::Service) {
    startProcessFromDaemon();
  }

  m_lastProcessMode = processMode;
}

void CoreProcess::stop(std::optional<ProcessMode> processModeOption)
{
  QMutexLocker locker(&m_processMutex);

  const auto currentMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  const auto processMode = processModeOption.value_or(currentMode);

  qInfo("stopping core process (%s mode)", qPrintable(processModeToString(processMode)));

  if (m_coreIpcClient && m_coreIpcClient->isConnected()) {
    m_coreIpcClient->sendInputIndicatorSubscription(false, m_inputIndicatorCapability);
  }
  resetInputIndicatorState();

  if (m_coreIpcClient && processMode != ProcessMode::Desktop) {
    m_coreIpcClient->disconnectFromServer();
    m_coreIpcClient->deleteLater();
    m_coreIpcClient = nullptr;
  }

  if (m_processState == ProcessState::Starting) {
    qDebug("core process is starting, cancelling");
    setProcessState(ProcessState::Stopped);
  } else if (m_processState != ProcessState::Stopped) {
    setProcessState(ProcessState::Stopping);

    if (processMode == ProcessMode::Service) {
      stopProcessFromDaemon();
    } else if (processMode == ProcessMode::Desktop) {
      stopForegroundProcess();
    }

  } else {
    qWarning("core process already stopped");
  }

  setConnectionState(ConnectionState::Disconnected);

  // Cloudflare Tunnel: the core is stopped first, the tunnel is let go second
  if (m_tunnelGuard != nullptr) {
    m_tunnelGuard->coreStopped();
  }
}

void CoreProcess::restart()
{
  qDebug("restarting core process");

  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();

  const bool waitForProcess = m_process && m_process->state() != QProcess::ProcessState::NotRunning;

  if (waitForProcess) {
    qInfo("waiting for current desktop core to exit before restarting");

    connect(
        m_process, &QProcess::finished, this,
        [this](int, QProcess::ExitStatus) {
          qInfo("desktop core exited, restarting");
          start();
        },
        Qt::SingleShotConnection
    );
  }

  if (m_lastProcessMode != std::nullopt && m_lastProcessMode != processMode) {
    const auto debugMessage =
        QStringLiteral("process mode changed to %1, stopping %2 process")
            .arg(processModeToString(processMode), processModeToString(m_lastProcessMode.value()));
    qDebug().noquote() << debugMessage;
    stop(m_lastProcessMode);
  } else {
    // in service mode: though there is technically no need to stop the service
    // before restarting it, it does make for cleaner process state tracking,
    // especially if something goes wrong with starting the service.
    stop();
  }

  if (!waitForProcess) {
    start();
  }
}

void CoreProcess::cleanup()
{
  qInfo("cleaning up core process");

  const auto isDesktop = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>() == ProcessMode::Desktop;
  const auto isRunning = m_processState == ProcessState::Started;
  if (isDesktop && isRunning) {
    stop();
  } else if (!isDesktop && m_tunnelGuard != nullptr && isTunnelRequired() &&
             (isRunning || m_processState == ProcessState::Starting)) {
    // Cloudflare Tunnel in service mode: the service core would keep retrying a tunnel that is gone with this program,
    // so it is stopped with it (the stop also clears the config of the service, so it does not start at boot either).
    // From here nothing is started or queued: a start that waits for the connection to the service is cancelled, so
    // that a connection that comes late cannot start the core while or after the program ends.
    m_quitting = true;
    disconnect(m_pendingDaemonStart);
    disconnect(m_pendingDaemonStop);

    // the service was told to start the core only when the state is Started; a start that was still waiting never
    // reached it
    const bool serviceHasTheCore = isRunning;
    stop();

    if (serviceHasTheCore) {
      const auto result = deliverStopToService(*m_daemonIpcClient, kQuitConnectWaitMs, kQuitFlushMs);
      if (result != StopDelivery::Delivered) {
        qWarning().noquote() << stopDeliveryWarning(result);
      }
    }
  }
}

void CoreProcess::setMode(Settings::CoreMode mode)
{
  // Cloudflare Tunnel: a start that waits for the tunnel is a start of the client. When the mode changes to another one
  // it is cancelled here, in the Starting state, which the window does not stop (it stops a core that has started); the
  // core is back to Stopped and Disconnected, and the tunnel is let go. Without a tunnel nothing waits, and this does
  // nothing.
  if (m_tunnelGuard != nullptr && m_tunnelGuard->isWaiting() && mode != Settings::CoreMode::Client) {
    stop();
  }
  m_mode = mode;
}

void CoreProcess::setTunnelGate(deskflow::tunnel::ITunnelGate *gate)
{
  delete m_tunnelGuard;
  m_tunnelGuard = nullptr;
  m_tunnelGate = gate;
  if (gate == nullptr) {
    return;
  }

  deskflow::tunnel::TunnelStartGuard::Actions actions;
  actions.waiting = [this] {
    setProcessState(ProcessState::Starting);
    setConnectionState(ConnectionState::Connecting);
  };
  actions.resume = [this] { start(m_tunnelPendingMode); };
  actions.failed = [this](deskflow::tunnel::TunnelFailure failure, bool wasWaiting) {
    qWarning().noquote() << "Cloudflare Tunnel failed:" << deskflow::tunnel::failureText(failure);
    if (wasWaiting) {
      // the core never started: the state goes back as a start that failed leaves it
      setProcessState(ProcessState::Stopped);
      setConnectionState(ConnectionState::Disconnected);
    } else if (m_processState != ProcessState::Stopped) {
      // the core would retry a tunnel that is gone for ever
      stop();
    }
  };
  m_tunnelGuard = new deskflow::tunnel::TunnelStartGuard(gate, actions, this);
}

bool CoreProcess::isTunnelRequired() const
{
  return m_tunnelGate != nullptr && m_mode == Settings::CoreMode::Client && m_tunnelGate->isRequired();
}

QPair<bool, QString> CoreProcess::persistServerConfig() const
{
  if (Settings::value(Settings::Server::ExternalConfig).toBool()) {
    return {Settings::isServerConfigFileReadable(), Settings::value(Settings::Server::ExternalConfigFile).toString()};
  }

  const auto configFilePath = Settings::defaultValue(Settings::Server::ExternalConfigFile).toString();
  QFile configFile(configFilePath);
  if (!configFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning() << "failed to open core config file for write:" << configFilePath;
    return {false, configFile.fileName()};
  }

  m_serverConfig.save(configFile);
  configFile.close();
  return {Settings::isServerConfigFileReadable(), configFile.fileName()};
}

void CoreProcess::setConnectionState(ConnectionState state)
{
  if (m_connectionState == state) {
    return;
  }

  m_connectionState = state;
  Q_EMIT connectionStateChanged(state);
}

void CoreProcess::setProcessState(ProcessState state)
{
  if (m_processState == state) {
    return;
  }

  qDebug(
      "core process state changed: %s -> %s", //
      qPrintable(processStateToString(m_processState)), qPrintable(processStateToString(state))
  );
  m_processState = state;
  if (state != ProcessState::Started) {
    resetInputIndicatorState();
  }
  Q_EMIT processStateChanged(state);
}

void CoreProcess::onCoreIpcMessageReceived(const QString &command, const QString &args)
{
  if (command == "connectionState") {
    const auto metaEnum = QMetaEnum::fromType<ConnectionState>();
    bool ok = false;
    const auto state = static_cast<ConnectionState>(metaEnum.keyToValue(args.toUtf8().constData(), &ok));
    if (!ok) {
      qWarning("core ipc got unknown connection state: %s", args.toUtf8().constData());
      return;
    }
    setConnectionState(state);
  } else if (command == "connectedClients") {
    const auto clients = args.isEmpty() ? QStringList() : args.split(",");
    Q_EMIT connectedClientsChanged(clients);
  } else if (command == "secureSocket") {
    Q_EMIT secureSocket(true);
    if (args != m_secureSocketVersion) {
      m_secureSocketVersion = args;
      Q_EMIT securityLevelChanged(args);
    }
  } else if (command == "unrecognisedClient") {
    Q_EMIT unrecognisedClient(args);
  } else if (command == "connectionRefused") {
    const auto metaEnum = QMetaEnum::fromType<deskflow::core::ConnectionRefusal>();
    bool ok = false;
    const auto reason =
        static_cast<deskflow::core::ConnectionRefusal>(metaEnum.keyToValue(args.toUtf8().constData(), &ok));
    if (ok) {
      Q_EMIT connectionRefused(reason);
    } else {
      qWarning("core ipc got unknown connection refusal: %s", args.toUtf8().constData());
    }
  } else if (command == "retryIn") {
    Q_EMIT retryIn(args.toInt());
  } else if (command == "peerFingerprint") {
    Q_EMIT peerFingerprint(args);
  } else if (command == "missingKeyboardLayouts") {
    Q_EMIT missingKeyboardLayouts(args);
  } else if (command == QLatin1String(deskflow::input::kInputIndicatorLocalCommand)) {
    const auto local = deskflow::input::decodeInputIndicatorBoolean(args);
    if (!local || !m_inputIndicatorEnabled) {
      Q_EMIT inputIndicatorStateInvalidated();
      return;
    }
    const bool changed = m_inputIndicatorLocal != *local;
    m_inputIndicatorLocal = *local;
    if (changed) {
      Q_EMIT inputIndicatorLocalChanged(*local);
    }
  } else if (command == QLatin1String(deskflow::input::kInputIndicatorActivityCommand)) {
    const auto activity = deskflow::input::decodeInputIndicatorActivity(args);
    if (!activity || !m_inputIndicatorEnabled || !isFreshInputIndicatorActivity(*activity) ||
        !deskflow::input::isInputIndicatorActivityNewer(*activity, m_lastInputIndicatorSequence)) {
      Q_EMIT inputIndicatorActivityInvalidated();
      return;
    }
    m_lastInputIndicatorSequence = activity->sequence;
    Q_EMIT inputIndicatorActivityReceived(*activity);
  }
}

bool CoreProcess::checkSecureSocket(const QString &line)
{
  static const QString tlsCheckString = "network encryption protocol: ";
  const auto index = line.indexOf(tlsCheckString, 0, Qt::CaseInsensitive);
  if (index == -1) {
    return false;
  }

  Q_EMIT secureSocket(true);
  if (const auto ssv = line.mid(index + tlsCheckString.size()); ssv != m_secureSocketVersion) {
    m_secureSocketVersion = ssv;
    Q_EMIT securityLevelChanged(ssv);
  }

  return true;
}

QString CoreProcess::correctedAddress(const QString &address) const
{
  return wrapIpv6(address.simplified());
}

void CoreProcess::setupDaemonLogTail(const QString &logPath)
{
  qDebug() << "daemon log path:" << logPath;

  if (QFileInfo logFile(logPath); !logFile.isFile()) {
    auto file = QFile(logPath);
    if (!file.open(QFile::ReadWrite)) {
      qCritical() << "daemon log path file can not be written:" << logPath;
      return;
    }
    file.write(""); // Create an empty file
  }

  if (m_daemonFileTail) {
    m_daemonFileTail->setWatchedFile(logPath);
  } else {
    m_daemonFileTail = new FileTail(logPath, this);
    connect(m_daemonFileTail, &FileTail::newLine, this, &CoreProcess::handleLogLines);
  }
}

void CoreProcess::clearSettings()
{
  const auto processMode = Settings::value(Settings::Core::ProcessMode).value<ProcessMode>();
  if (processMode == ProcessMode::Desktop) {
    qDebug("no core settings to clear in desktop mode");
    return;
  }

  if (processMode != ProcessMode::Service) {
    qCritical("not clearing core settings, unexpected process mode");
    return;
  }

  qInfo("clearing core settings through daemon");
  m_daemonIpcClient->sendClearSettings();
}

void CoreProcess::retryDaemon()
{
  m_daemonIpcClient->connectToServer();
}

void CoreProcess::setInputIndicatorEnabled(bool enabled)
{
  if (m_inputIndicatorEnabled == enabled) {
    return;
  }
  m_inputIndicatorEnabled = enabled;
  sendInputIndicatorSubscription();
  if (!enabled) {
    resetInputIndicatorState();
  }
}

void CoreProcess::sendInputIndicatorSubscription()
{
  if (m_coreIpcClient && m_coreIpcClient->isConnected()) {
    m_coreIpcClient->sendInputIndicatorSubscription(m_inputIndicatorEnabled, m_inputIndicatorCapability);
    if (m_inputIndicatorEnabled) {
      m_inputIndicatorSubscriptionTimer.start();
    } else {
      m_inputIndicatorSubscriptionTimer.stop();
    }
  } else {
    m_inputIndicatorSubscriptionTimer.stop();
  }
}

void CoreProcess::resetInputIndicatorState()
{
  m_inputIndicatorSubscriptionTimer.stop();
  m_lastInputIndicatorSequence = 0;
  if (m_inputIndicatorLocal) {
    m_inputIndicatorLocal = false;
    Q_EMIT inputIndicatorLocalChanged(false);
  }
  Q_EMIT inputIndicatorStateInvalidated();
}

} // namespace deskflow::gui
