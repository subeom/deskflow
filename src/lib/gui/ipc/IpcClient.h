/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class QLocalSocket;

namespace deskflow::gui::ipc {

class IpcClient : public QObject
{
  Q_OBJECT

  // Represents underlying socket state and whether the server responded to the hello message.
  enum class State
  {
    Unconnected,
    Connecting,
    Connected,
    Disconnecting,
  };

public:
  explicit IpcClient(QObject *parent, const QString &socketName, const QString &typeName);
  // (the four that are virtual are what a test replaces, to talk to nobody)
  virtual void connectToServer();
  void disconnectFromServer();

  virtual bool isConnected() const
  {
    return m_state == State::Connected;
  }

  //! Writes what was sent and not yet written, waiting up to \p msecs: for a message that is sent right before the
  //! program ends. False when it was not written in time or the client is not connected.
  virtual bool flush(int msecs);

Q_SIGNALS:
  void connected();
  void connectionFailed();
  void serverShutdown();
  void versionMismatch();

private Q_SLOTS:
  void handleDisconnected();
  void handleErrorOccurred();
  void handleReadyRead();

protected:
  virtual void processCommand(const QString &command, const QStringList &parts)
  {
    Q_UNUSED(command)
    Q_UNUSED(parts)
  }

  virtual void sendMessage(const QString &message);

private:
  void attemptConnection();
  void handleHandshakeMessage(const QStringList &parts);
  void handleConnectionSuccess();
  void handleConnectionFailure();

  QLocalSocket *m_socket;
  State m_state{State::Unconnected};
  QString m_socketName;
  QByteArray m_readBuffer;
  int m_retryCount{0};
  QString m_typeName;
};

} // namespace deskflow::gui::ipc
