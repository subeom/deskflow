/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "IpcServer.h"

#include <QObject>
#include <QHash>
#include <QSet>

#include <chrono>

class QLocalSocket;
class QTimer;

namespace deskflow::core::ipc {

class CoreIpcServer : public IpcServer
{
  Q_OBJECT

public:
  explicit CoreIpcServer(
      QObject *parent, std::chrono::milliseconds indicatorSubscriptionTimeout = std::chrono::seconds{60}
  );

  static CoreIpcServer &instance();
  void broadcastInputIndicatorCommand(const QString &command, const QString &args = "");

private:
  void processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts) override;
  void clientDisconnected(QLocalSocket *clientSocket) override;
  bool isAuthorizedIndicatorPeer(QLocalSocket *clientSocket) const;
  void renewInputIndicatorSubscription(QLocalSocket *clientSocket);
  void removeInputIndicatorSubscriber(QLocalSocket *clientSocket);

  QSet<QLocalSocket *> m_inputIndicatorSubscribers;
  QHash<QLocalSocket *, QTimer *> m_inputIndicatorSubscriptionTimers;
  QString m_inputIndicatorCapability;
  qint64 m_inputIndicatorGuiPid = 0;
  std::chrono::milliseconds m_inputIndicatorSubscriptionTimeout;
};

} // namespace deskflow::core::ipc
