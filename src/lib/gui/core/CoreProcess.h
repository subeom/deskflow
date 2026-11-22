/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2024 - 2025 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Enums.h"
#include "common/Settings.h"
#include "deskflow/ipc/InputIndicatorIpc.h"
#include "gui/FileTail.h"
#include "gui/config/ServerConfig.h"

#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QTimer>

#include <functional>

struct MainWindowTestAccess;
struct CoreProcessInputIndicatorTestAccess;

namespace deskflow::tunnel {
class ITunnelGate;
class TunnelStartGuard;
} // namespace deskflow::tunnel

namespace deskflow::gui {

namespace ipc {
class CoreIpcClient;
class DaemonIpcClient;
} // namespace ipc

class CoreProcess : public QObject
{
  //! The test of the main window sets the state that the core would report (it never starts the core)
  friend struct ::MainWindowTestAccess;
  friend struct ::CoreProcessInputIndicatorTestAccess;
  using ConnectionState = deskflow::core::ConnectionState;
  using ProcessMode = Settings::ProcessMode;
  using ProcessState = deskflow::core::ProcessState;
  Q_OBJECT

public:
  enum class Error
  {
    AddressMissing,
    StartFailed
  };

  //! Makes the client of the service (the daemon); a test gives one that talks to nobody
  using DaemonClientFactory = std::function<ipc::DaemonIpcClient *(QObject *parent)>;

  explicit CoreProcess(const ServerConfig &serverConfig, DaemonClientFactory daemonClientFactory = {});

  void start(std::optional<ProcessMode> processMode = std::nullopt);
  void stop(std::optional<ProcessMode> processMode = std::nullopt);
  void restart();
  void cleanup();
  void applyLogLevel();
  void clearSettings();
  void retryDaemon();
  void setInputIndicatorEnabled(bool enabled);

  // Cloudflare Tunnel: the client core waits for the tunnel to listen before it starts, and the tunnel is let go when
  // the core stops (see gui/tunnel/TunnelStartGuard.h). Without a gate nothing of this is used.
  void setTunnelGate(deskflow::tunnel::ITunnelGate *gate);
  //! The client core needs the tunnel (a tunnel that is on, in client mode)
  bool isTunnelRequired() const;

  // getters
  Settings::CoreMode mode() const
  {
    return m_mode;
  }
  QString secureSocketVersion() const
  {
    return m_secureSocketVersion;
  }
  bool isStarted() const
  {
    return m_processState == ProcessState::Started;
  }
  ProcessState processState() const
  {
    return m_processState;
  }
  ConnectionState connectionState() const
  {
    return m_connectionState;
  }

  // setters
  void setAddress(const QString &address)
  {
    m_address = correctedAddress(address);
  }
  void setMode(Settings::CoreMode mode);

Q_SIGNALS:
  void error(deskflow::gui::CoreProcess::Error error);
  void logLine(const QString &line);
  void connectionStateChanged(deskflow::core::ConnectionState state);
  void processStateChanged(deskflow::core::ProcessState state);
  void secureSocket(bool enabled);
  void daemonIpcClientConnectionFailed();
  void connectedClientsChanged(const QStringList &clients);
  void securityLevelChanged(QString securityLevel);
  void unrecognisedClient(const QString &clientName);
  void connectionRefused(deskflow::core::ConnectionRefusal reason);
  void retryIn(int seconds);
  void peerFingerprint(const QString &fingerprint);
  void missingKeyboardLayouts(const QString &layouts);
  void inputIndicatorActivityReceived(const deskflow::input::InputIndicatorActivity &activity);
  void inputIndicatorLocalChanged(bool local);
  void inputIndicatorActivityInvalidated();
  void inputIndicatorStateInvalidated();

private Q_SLOTS:
  void onProcessFinished(int exitCode, QProcess::ExitStatus);
  void onProcessReadyReadStandardOutput();
  void onProcessReadyReadStandardError();
  void onCoreIpcMessageReceived(const QString &command, const QString &args);
  void daemonIpcClientConnected();

private:
  void startForegroundProcess(const QStringList &args);
  void startProcessFromDaemon();
  void stopForegroundProcess();
  void stopProcessFromDaemon();
  QPair<bool, QString> persistServerConfig() const;
  void setConnectionState(ConnectionState state);
  void setProcessState(ProcessState state);
  bool checkSecureSocket(const QString &line);
  void handleLogLines(const QString &text);
  QString correctedAddress(const QString &address) const;
  void setupDaemonLogTail(const QString &logPath);
  void resetInputIndicatorState();
  void sendInputIndicatorSubscription();
  void checkExistingProcess();
  static QString makeQuotedArgs(const QString &app, const QStringList &args);
  static QString processModeToString(const Settings::ProcessMode mode);
  static QString processStateToString(const CoreProcess::ProcessState state);
  static QString wrapIpv6(const QString &address);

  const ServerConfig &m_serverConfig;
  QString m_address;
  ProcessState m_processState = ProcessState::Stopped;
  ConnectionState m_connectionState = ConnectionState::Disconnected;
  Settings::CoreMode m_mode = Settings::CoreMode::None;
  QMutex m_processMutex;
  QString m_secureSocketVersion;
  std::optional<ProcessMode> m_lastProcessMode = std::nullopt;
  QTimer m_retryTimer;
  deskflow::gui::ipc::CoreIpcClient *m_coreIpcClient = nullptr;
  deskflow::gui::ipc::DaemonIpcClient *m_daemonIpcClient = nullptr;
  FileTail *m_daemonFileTail = nullptr;
  QProcess *m_process = nullptr;
  QString m_appPath;
  deskflow::tunnel::TunnelStartGuard *m_tunnelGuard = nullptr;
  deskflow::tunnel::ITunnelGate *m_tunnelGate = nullptr;
  std::optional<ProcessMode> m_tunnelPendingMode;
  bool m_inputIndicatorEnabled = false;
  bool m_inputIndicatorLocal = false;
  uint64_t m_lastInputIndicatorSequence = 0;
  // the program is ending (Cloudflare Tunnel, service mode): nothing is started or queued any more
  bool m_quitting = false;
  QMetaObject::Connection m_pendingDaemonStart;
  QMetaObject::Connection m_pendingDaemonStop;
};

} // namespace deskflow::gui
