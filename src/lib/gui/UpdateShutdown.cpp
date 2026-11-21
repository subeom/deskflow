/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateShutdown.h"

#include <QTimer>

namespace deskflow::gui {

UpdateShutdown::UpdateShutdown(
    int coreTimeoutMs, int tunnelTimeoutMs, QObject *parent
)
    : QObject(parent),
      m_coreTimer(new QTimer(this)),
      m_tunnelTimer(new QTimer(this))
{
  m_coreTimer->setSingleShot(true);
  m_coreTimer->setInterval(coreTimeoutMs);
  m_tunnelTimer->setSingleShot(true);
  m_tunnelTimer->setInterval(tunnelTimeoutMs);
  connect(m_coreTimer, &QTimer::timeout, this, [this] {
    fail(QStringLiteral("Deskflow sharing did not stop in time"));
  });
  connect(m_tunnelTimer, &QTimer::timeout, this, [this] {
    fail(QStringLiteral("the Cloudflare Tunnel did not stop in time"));
  });
}

void UpdateShutdown::begin(
    const UpdateShutdownTargets &targets, const QString &receiptPath
)
{
  if (m_active) {
    return;
  }
  if (!targets.stopCore || !targets.coreStopped || !targets.restoreCore ||
      !targets.stopTunnel || !targets.tunnelStopped || !targets.restoreTunnel ||
      !targets.markReceipt || receiptPath.isEmpty()) {
    Q_EMIT failed(QStringLiteral("the update shutdown coordinator is incomplete"));
    return;
  }

  m_targets = targets;
  m_receiptPath = receiptPath;
  m_restoreCore = !m_targets.coreStopped();
  m_restoreTunnel = !m_targets.tunnelStopped();
  m_active = true;
  m_waitingForCore = m_restoreCore;
  m_waitingForTunnel = false;

  if (!m_waitingForCore) {
    stopTunnel();
    return;
  }
  m_targets.stopCore();
  if (m_targets.coreStopped()) {
    m_waitingForCore = false;
    stopTunnel();
  } else {
    m_coreTimer->start();
  }
}

void UpdateShutdown::coreStateChanged()
{
  if (!m_active || !m_waitingForCore || !m_targets.coreStopped()) {
    return;
  }
  m_coreTimer->stop();
  m_waitingForCore = false;
  stopTunnel();
}

void UpdateShutdown::tunnelStateChanged()
{
  if (!m_active || !m_waitingForTunnel || !m_targets.tunnelStopped()) {
    return;
  }
  m_tunnelTimer->stop();
  m_waitingForTunnel = false;
  finish();
}

void UpdateShutdown::stopTunnel()
{
  if (!m_active) {
    return;
  }
  if (m_targets.tunnelStopped()) {
    finish();
    return;
  }
  m_waitingForTunnel = true;
  m_targets.stopTunnel();
  if (m_targets.tunnelStopped()) {
    m_waitingForTunnel = false;
    finish();
  } else {
    m_tunnelTimer->start();
  }
}

void UpdateShutdown::finish()
{
  if (!m_active || m_waitingForCore || m_waitingForTunnel) {
    return;
  }
  m_coreTimer->stop();
  m_tunnelTimer->stop();
  const auto marked =
      m_targets.markReceipt(m_receiptPath, QStringLiteral("shutdown-complete"));
  if (!marked.ok) {
    fail(marked.error.isEmpty()
             ? QStringLiteral("the update shutdown receipt could not be written")
             : marked.error);
    return;
  }
  m_active = false;
  Q_EMIT completed();
}

void UpdateShutdown::fail(const QString &boundedReason)
{
  if (!m_active) {
    return;
  }
  m_active = false;
  m_waitingForCore = false;
  m_waitingForTunnel = false;
  m_coreTimer->stop();
  m_tunnelTimer->stop();
  m_targets.markReceipt(m_receiptPath, QStringLiteral("failed"));
  restore();
  Q_EMIT failed(boundedReason.left(240));
}

void UpdateShutdown::restore()
{
  if (m_restoreCore) {
    m_targets.restoreCore();
  }
  if (m_restoreTunnel) {
    m_targets.restoreTunnel();
  }
}

void UpdateShutdown::cancel()
{
  if (!m_active) {
    return;
  }
  fail(QStringLiteral("the update shutdown was cancelled"));
}

bool UpdateShutdown::isActive() const
{
  return m_active;
}

} // namespace deskflow::gui
