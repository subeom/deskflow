/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcServer.h"

#include "InputIndicatorIpc.h"
#include "base/Log.h"
#include "common/Constants.h"

#include <QLocalSocket>

namespace deskflow::core::ipc {

static CoreIpcServer *s_instance = nullptr;

CoreIpcServer::CoreIpcServer(QObject *parent) : IpcServer(parent, kCoreIpcName, QStringLiteral("core"))
{
  assert(s_instance == nullptr);
  s_instance = this;
}

CoreIpcServer &CoreIpcServer::instance()
{
  assert(s_instance != nullptr);
  return *s_instance;
}

void CoreIpcServer::processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts)
{
  if (command == QStringLiteral("stop")) {
    LOG_DEBUG("core ipc server got stop message");
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    broadcastCommand(QStringLiteral("bye"));
    Q_EMIT stopProcessRequested();
    return;
  }
  if (command == QLatin1String(deskflow::input::kInputIndicatorSubscriptionCommand)) {
    const auto value = parts.size() >= 2
                           ? deskflow::input::decodeInputIndicatorBoolean(parts.at(1))
                           : std::nullopt;
    if (!value) {
      writeToClientSocket(clientSocket, QStringLiteral("error=invalid input indicator subscription"));
      return;
    }

    if (*value) {
      m_inputIndicatorSubscribers.insert(clientSocket);
    } else {
      m_inputIndicatorSubscribers.remove(clientSocket);
    }
    deskflow::input::setInputIndicatorSubscribed(!m_inputIndicatorSubscribers.isEmpty());
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    if (*value) {
      auto *target = clientSocket;
      writeToClientSocket(
          target,
          QStringLiteral("%1=%2")
              .arg(QLatin1String(deskflow::input::kInputIndicatorLocalCommand))
              .arg(deskflow::input::encodeInputIndicatorBoolean(deskflow::input::isInputIndicatorLocal()))
      );
    }
    return;
  }
  LOG_WARN("core ipc server got unknown command: %s", command.toUtf8().constData());
}

void CoreIpcServer::clientDisconnected(QLocalSocket *clientSocket)
{
  m_inputIndicatorSubscribers.remove(clientSocket);
  deskflow::input::setInputIndicatorSubscribed(!m_inputIndicatorSubscribers.isEmpty());
}

void CoreIpcServer::broadcastInputIndicatorCommand(const QString &command, const QString &args)
{
  if (m_inputIndicatorSubscribers.isEmpty()) {
    deskflow::input::setInputIndicatorSubscribed(false);
    return;
  }

  const auto message = args.isEmpty() ? command : QStringLiteral("%1=%2").arg(command, args);
  for (auto *clientSocket : std::as_const(m_inputIndicatorSubscribers)) {
    auto *target = clientSocket;
    writeToClientSocket(target, message);
    target->flush();
  }
}

} // namespace deskflow::core::ipc
