/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class UpdateShutdownTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void shutdown_waitsForCoreAndTunnelBeforeHelper();
  void shutdown_timeoutCancelsApplyAndRestoresRunningState();
  void shutdown_ignoresLateCoreAndTunnelSignalsAfterCancellation();
  void shutdown_writesCompleteReceiptOnlyOnce();
};
