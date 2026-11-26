/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QStatusBar>

#include "common/Enums.h"
#include "gui/UpdateStatusModel.h"

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
  //! The new main window shows the same state in its own widgets and keeps this bar as their source, hidden
  void setHeadless(bool headless);
  void updateSecurityInfo(bool encrypted);
  void setSecurityIcon(bool encrypted);
  void setSecurityLevel(const QString &securityLevel);
  void setBtnFingerprintVisible(bool visible);
  void updateFound(const QString &version);
  void setUpdateStatus(const deskflow::gui::UpdateStatusView &status);

  //! What the bar says, for a second view of the same state (the main window of the new look): the sentence, the
  //! encryption, the fingerprint button and the version that can be updated to
  QString statusText() const;
  QString securityText() const;
  bool isEncrypted() const;
  bool fingerprintVisible() const;
  QString updateVersion() const;
  //! The texts of the buttons of the bar, in the language of the program
  QString fingerprintToolTip() const;
  QString updateButtonText() const;

Q_SIGNALS:
  void requestShowMyFingerprints();
  void requestUpdateVersion();
  //! Something that the getters above return changed (it may come several times for one change of state)
  void changed();
  void requestUpdateAction(deskflow::gui::UpdateAction action);

protected:
  void changeEvent(QEvent *e) override;

private:
  void updateText();
  void updateTimerLabel();
  QPushButton *m_btnFingerprint = nullptr;
  QLabel *m_lblSecurityIcon = nullptr;
  QLabel *m_lblStatus = nullptr;
  QPushButton *m_btnUpdate = nullptr;
  bool m_encrypted = false;
  bool m_headless = false;
  QString m_securityLevel;
  QString m_updateVersion;
  deskflow::gui::UpdateStatusView m_updateStatus;
  int m_connectionInterval = -1;
  QTimer *m_retryTimer = nullptr;
};
