/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateInstaller.h"

#include <QObject>
#include <QString>

#include <functional>

class QTimer;

namespace deskflow::gui {

struct UpdateShutdownTargets
{
  std::function<void()> stopCore;
  std::function<bool()> coreStopped;
  std::function<void()> restoreCore;
  std::function<void()> stopTunnel;
  std::function<bool()> tunnelStopped;
  std::function<void()> restoreTunnel;
  std::function<UpdateInstallResult(const QString &, const QString &)> markReceipt;
};

class UpdateShutdown final : public QObject
{
  Q_OBJECT
public:
  explicit UpdateShutdown(
      int coreTimeoutMs = 15'000, int tunnelTimeoutMs = 10'000, QObject *parent = nullptr
  );

  void begin(const UpdateShutdownTargets &targets, const QString &receiptPath);
  void coreStateChanged();
  void tunnelStateChanged();
  void cancel();
  bool isActive() const;

Q_SIGNALS:
  void completed();
  void failed(const QString &boundedReason);

private:
  void stopTunnel();
  void finish();
  void fail(const QString &boundedReason);
  void restore();

  UpdateShutdownTargets m_targets;
  QString m_receiptPath;
  QTimer *m_coreTimer = nullptr;
  QTimer *m_tunnelTimer = nullptr;
  bool m_active = false;
  bool m_restoreCore = false;
  bool m_restoreTunnel = false;
  bool m_waitingForCore = false;
  bool m_waitingForTunnel = false;
};

} // namespace deskflow::gui
