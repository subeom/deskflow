/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/QtPortProber.h"

#include "common/TunnelAddress.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <memory>

namespace deskflow::tunnel {

QtPortProber::QtPortProber(QObject *parent) : QObject(parent)
{
}

bool QtPortProber::canListen(int port)
{
  if (!isValidLocalPort(port)) {
    return false;
  }

  QTcpServer server;
  return server.listen(QHostAddress::LocalHost, static_cast<quint16>(port));
}

int QtPortProber::findFreePort()
{
  // the system picks: a listener on port 0 is given a free one
  QTcpServer server;
  if (!server.listen(QHostAddress::LocalHost, 0)) {
    return 0;
  }
  const int port = server.serverPort();
  return isValidLocalPort(port) ? port : 0;
}

void QtPortProber::probeOpen(int port, int timeoutMs, std::function<void(bool open)> done)
{
  auto *socket = new QTcpSocket(this);
  auto *timer = new QTimer(socket);
  timer->setSingleShot(true);

  // whichever comes first tells the result, and only that one
  const auto answered = std::make_shared<bool>(false);
  const auto finish = [socket, answered, done = std::move(done)](bool open) {
    if (*answered) {
      return;
    }
    *answered = true;
    socket->abort();
    socket->deleteLater();
    done(open);
  };

  connect(socket, &QTcpSocket::connected, socket, [finish] { finish(true); });
  connect(socket, &QAbstractSocket::errorOccurred, socket, [finish](QAbstractSocket::SocketError) { finish(false); });
  connect(timer, &QTimer::timeout, socket, [finish] { finish(false); });

  timer->start(timeoutMs);
  socket->connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
}

} // namespace deskflow::tunnel
