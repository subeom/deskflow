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
#include <QTimer>

#if defined(Q_OS_MACOS)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace deskflow::core::ipc {

static CoreIpcServer *s_instance = nullptr;

CoreIpcServer::CoreIpcServer(
    QObject *parent, std::chrono::milliseconds indicatorSubscriptionTimeout, const QString &serverName
)
    : IpcServer(parent, serverName.isEmpty() ? QString::fromLatin1(kCoreIpcName) : serverName, QStringLiteral("core")),
      m_inputIndicatorCapability(
          qEnvironmentVariable(deskflow::input::kInputIndicatorCapabilityEnvironment)
      ),
      m_inputIndicatorGuiPid(
          qEnvironmentVariableIntValue(deskflow::input::kInputIndicatorGuiPidEnvironment)
      ),
      m_inputIndicatorSubscriptionTimeout(indicatorSubscriptionTimeout)
{
  assert(s_instance == nullptr);
  s_instance = this;
}

CoreIpcServer::~CoreIpcServer()
{
  deskflow::input::setInputIndicatorSubscribed(false);
  if (s_instance == this) {
    s_instance = nullptr;
  }
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
    const auto value =
        parts.size() == 2 && isClientRegistered(clientSocket) && isAuthorizedIndicatorPeer(clientSocket)
            ? deskflow::input::decodeInputIndicatorSubscription(parts.at(1), m_inputIndicatorCapability)
            : std::nullopt;
    if (!value || m_inputIndicatorCapability.isEmpty()) {
      removeInputIndicatorSubscriber(clientSocket);
      writeToClientSocket(clientSocket, QStringLiteral("error=invalid input indicator subscription"));
      return;
    }

    if (*value) {
      renewInputIndicatorSubscription(clientSocket);
    } else {
      removeInputIndicatorSubscriber(clientSocket);
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
  removeInputIndicatorSubscriber(clientSocket);
}

bool CoreIpcServer::isAuthorizedIndicatorPeer(QLocalSocket *clientSocket) const
{
#if defined(Q_OS_MACOS)
  uid_t peerUser = static_cast<uid_t>(-1);
  gid_t peerGroup = static_cast<gid_t>(-1);
  pid_t peerPid = 0;
  socklen_t peerPidSize = sizeof(peerPid);
  return clientSocket && clientSocket->socketDescriptor() >= 0 &&
         getpeereid(static_cast<int>(clientSocket->socketDescriptor()), &peerUser, &peerGroup) == 0 &&
         peerUser == geteuid() &&
         getsockopt(
             static_cast<int>(clientSocket->socketDescriptor()), SOL_LOCAL, LOCAL_PEERPID, &peerPid, &peerPidSize
         ) == 0 &&
         peerPid > 0 && peerPid == static_cast<pid_t>(m_inputIndicatorGuiPid);
#else
  Q_UNUSED(clientSocket)
  return false;
#endif
}

void CoreIpcServer::renewInputIndicatorSubscription(QLocalSocket *clientSocket)
{
  m_inputIndicatorSubscribers.insert(clientSocket);
  auto timer = m_inputIndicatorSubscriptionTimers.value(clientSocket);
  if (!timer) {
    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, clientSocket] {
      removeInputIndicatorSubscriber(clientSocket);
    });
    m_inputIndicatorSubscriptionTimers.insert(clientSocket, timer);
  }
  timer->start(static_cast<int>(m_inputIndicatorSubscriptionTimeout.count()));
  deskflow::input::setInputIndicatorSubscribed(!m_inputIndicatorSubscribers.isEmpty());
}

void CoreIpcServer::removeInputIndicatorSubscriber(QLocalSocket *clientSocket)
{
  m_inputIndicatorSubscribers.remove(clientSocket);
  if (auto *timer = m_inputIndicatorSubscriptionTimers.take(clientSocket)) {
    timer->stop();
    timer->deleteLater();
  }
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
