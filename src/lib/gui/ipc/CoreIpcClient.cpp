/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcClient.h"

#include "common/Constants.h"
#include "deskflow/ipc/InputIndicatorIpc.h"

#include <QString>

namespace deskflow::gui::ipc {

CoreIpcClient::CoreIpcClient(QObject *parent) : IpcClient(parent, kCoreIpcName, QStringLiteral("core"))
{
  // do nothing
}

void CoreIpcClient::sendStop()
{
  sendMessage(QStringLiteral("stop"));
}

void CoreIpcClient::sendInputIndicatorSubscription(bool enabled)
{
  sendMessage(
      QStringLiteral("%1=%2")
          .arg(QLatin1String(deskflow::input::kInputIndicatorSubscriptionCommand))
          .arg(deskflow::input::encodeInputIndicatorBoolean(enabled))
  );
}

void CoreIpcClient::processCommand(const QString &command, const QStringList &parts)
{
  const auto args = parts.size() >= 2 ? parts.at(1) : QString();
  Q_EMIT commandReceived(command, args);
}

} // namespace deskflow::gui::ipc
