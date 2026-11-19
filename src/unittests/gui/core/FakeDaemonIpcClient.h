/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/ipc/DaemonIpcClient.h"

#include <QStringList>
#include <QTimer>

// A stand-in of the client of the service (the daemon): it has no socket and talks to nobody, it only does what the
// test tells it and writes down what it was asked to send.
class FakeDaemonIpcClient : public deskflow::gui::ipc::DaemonIpcClient
{
public:
  enum class Behaviour
  {
    ConnectsAtOnce,
    ConnectsLater, //!< after connectDelayMs, when connectToServer() was called
    NeverConnects,
    FailsAtOnce //!< connectionFailed() at once
  };

  explicit FakeDaemonIpcClient(QObject *parent = nullptr) : DaemonIpcClient(parent)
  {
  }

  Behaviour behaviour = Behaviour::ConnectsAtOnce;
  int connectDelayMs = 100;
  bool flushResult = true;

  int connectCalls = 0;
  int flushCalls = 0;
  int droppedMessages = 0; //!< sent while not connected, which the real client drops
  QStringList sent;        //!< what was sent while connected, in order

  void setConnected(bool connected)
  {
    m_connected = connected;
  }

  bool isConnected() const override
  {
    return m_connected;
  }

  void connectToServer() override
  {
    ++connectCalls;
    switch (behaviour) {
    case Behaviour::ConnectsAtOnce:
      m_connected = true;
      Q_EMIT connected();
      break;
    case Behaviour::ConnectsLater:
      QTimer::singleShot(connectDelayMs, this, [this] {
        m_connected = true;
        Q_EMIT connected();
      });
      break;
    case Behaviour::NeverConnects:
      break;
    case Behaviour::FailsAtOnce:
      Q_EMIT connectionFailed();
      break;
    }
  }

  //! The service answers late: what a connection that was waited for does when it comes
  void connectNow()
  {
    m_connected = true;
    Q_EMIT connected();
  }

  bool flush(int) override
  {
    ++flushCalls;
    return m_connected && flushResult;
  }

  int count(const QString &prefix) const
  {
    int found = 0;
    for (const auto &message : sent) {
      found += message.startsWith(prefix) ? 1 : 0;
    }
    return found;
  }

protected:
  void sendMessage(const QString &message) override
  {
    if (!m_connected) {
      ++droppedMessages;
      return;
    }
    sent << message;
  }

private:
  bool m_connected = false;
};
