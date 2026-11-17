/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/core/ServiceQuit.h"

#include "gui/ipc/DaemonIpcClient.h"

#include <QEventLoop>
#include <QTimer>

namespace deskflow::gui {

StopDelivery deliverStopToService(ipc::DaemonIpcClient &client, int connectWaitMs, int flushMs)
{
  if (!client.isConnected()) {
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool failed = false;
    QObject::connect(&client, &ipc::DaemonIpcClient::connected, &loop, &QEventLoop::quit);
    QObject::connect(&client, &ipc::DaemonIpcClient::connectionFailed, &loop, [&failed, &loop] {
      failed = true;
      loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    client.connectToServer();
    // (a client can be connected, or fail, inside the call)
    if (!client.isConnected() && !failed) {
      timeout.start(connectWaitMs);
      loop.exec();
    }

    if (!client.isConnected()) {
      return StopDelivery::NotConnected;
    }
  }

  client.sendStopProcess();
  return client.flush(flushMs) ? StopDelivery::Delivered : StopDelivery::NotFlushed;
}

QString stopDeliveryWarning(StopDelivery result)
{
  switch (result) {
  case StopDelivery::Delivered:
    return {};
  case StopDelivery::NotConnected:
  case StopDelivery::NotFlushed:
    return QStringLiteral(
        "the service core was not told to stop: it keeps running without the Cloudflare Tunnel (retrying a local port "
        "that "
        "nothing listens on), and with start at boot on it starts again at boot. Stop it from the Deskflow service, "
        "and "
        "turn off start at boot for the service when using the tunnel"
    );
  }
  return {};
}

} // namespace deskflow::gui
