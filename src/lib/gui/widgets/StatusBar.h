/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QStatusBar>

#include "common/Enums.h"
#include "gui/UpdateCache.h"

#include <optional>

class QPushButton;
class QLabel;

using ProcessState = deskflow::core::ProcessState;
using ConnectionState = deskflow::core::ConnectionState;

class StatusBar : public QStatusBar
{
  Q_OBJECT
public:
  explicit StatusBar(QWidget *parent = nullptr);
  void setStatus(ConnectionState connectionState, ProcessState processState, bool isServer);
  void setServerClients(const QStringList &clients);
  void setSecurityIconVisible(bool visible);
  void setConnectionInterval(int newInterval);
  bool securityIconVisible() const;
  void updateSecurityInfo(bool encrypted);
  void setSecurityIcon(bool encrypted);
  void setSecurityLevel(const QString &securityLevel);
  void setBtnFingerprintVisible(bool visible);
  void updateFound(const QString &version, const QString &releaseUrl);
  void updateStaged(const deskflow::gui::StagedUpdate &staged);
  void updateFailed(const QString &message);

Q_SIGNALS:
  void requestShowMyFingerprints();
  void requestUpdateVersion(const QString &releaseUrl);
  void requestUpdateInstall(const deskflow::gui::StagedUpdate &staged);

protected:
  void changeEvent(QEvent *e) override;

private:
  enum class UpdateButtonState
  {
    Hidden,
    Manual,
    Staged,
    Failed
  };

  void updateButton();
  void updateText();
  void updateTimerLabel();
  QPushButton *m_btnFingerprint = nullptr;
  QLabel *m_lblSecurityIcon = nullptr;
  QLabel *m_lblStatus = nullptr;
  QPushButton *m_btnUpdate = nullptr;
  bool m_encrypted = false;
  QString m_securityLevel;
  int m_connectionInterval = -1;
  QTimer *m_retryTimer = nullptr;
  UpdateButtonState m_updateButtonState = UpdateButtonState::Hidden;
  QString m_updateVersion;
  QString m_updateReleaseUrl;
  QString m_updateError;
  std::optional<deskflow::gui::StagedUpdate> m_stagedUpdate;
};
