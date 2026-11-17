/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/CloudflaredLocator.h"
#include "gui/tunnel/ISecretStore.h"

#include <QGroupBox>

#include <functional>
#include <memory>
#include <optional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace deskflow::gui {

//! Deskflow runs cloudflared itself (see gui/tunnel/TunnelSupervisor.h). When it does not, the group tells the user to
//! start it.
inline constexpr bool kTunnelSupervisorAvailable = true;

//! What the group asks of its surroundings; every part is replaced in tests so that no keychain, file system or dialog
//! of the system is used
struct TunnelSettingsDependencies
{
  //! null: the secure store of the system
  std::shared_ptr<deskflow::tunnel::ISecretStore> secretStore;
  //! null: the real file system and PATH
  std::function<deskflow::tunnel::CloudflaredLocation(const QString &configuredPath)> locate;
  //! null: asks with a message box
  std::function<bool()> confirmRemoval;
  //! null: the setting of Deskflow's TLS
  std::function<bool()> tlsEnabled;
};

/**
 * The "Cloudflare Tunnel" group of the dialog that configures the client: whether to connect through a tunnel, its
 * hostname, the local port, where cloudflared is and the Service Token (Client ID and Secret). It writes the settings
 * of the group "tunnel" and only what changed, so a user who never touches it leaves the configuration file as it was.
 * The Secret is typed into a masked field, goes to the secure store of the system and is never shown again: the field
 * is emptied after it is saved and the group only ever shows the Client ID. The saved server address is not touched.
 *
 * Every line of text is a line that does not wrap: a window does not grow its height for wrapped text, so the group
 * would be squeezed. What does not fit the width is cut, and is all in the tool tip.
 */
class TunnelSettingsGroup : public QGroupBox
{
  Q_OBJECT

public:
  explicit TunnelSettingsGroup(TunnelSettingsDependencies dependencies = {}, QWidget *parent = nullptr);

  //! Shows what is stored: the settings, and the Client ID of the stored token
  void load();
  //! Whether anything shown differs from what is stored (a Secret that was typed is a difference)
  bool isModified() const;
  //! Whether the settings shown are the defaults (the stored token is not a setting)
  bool isDefault() const;
  //! The settings to their defaults; the token that is stored stays and the Secret field is emptied
  void resetToDefault();
  void setWritable(bool writable);

  /**
   * What is wrong with what is entered, in words for the user; empty when it can be saved. Nothing is written by
   * asking.
   */
  QString validationProblem() const;
  //! Shows \p text under the group as the reason that nothing was saved (an empty text hides it)
  void showError(const QString &text);

  /**
   * Stores the token when a Secret was typed and writes the settings that changed. False when the token could not be
   * stored (the reason is shown, and no setting is written). Call it when validationProblem() is empty.
   */
  bool save();

Q_SIGNALS:
  void changed();

protected:
  void changeEvent(QEvent *event) override;

private:
  void retranslate();
  void updateControls();
  void updateStatusLines();
  void edited();
  void browseForCloudflared();
  void removeCredentials();

  bool tlsEnabled() const;
  QString typedHostname() const;
  QString typedPath() const;

  TunnelSettingsDependencies m_dependencies;
  std::shared_ptr<deskflow::tunnel::ISecretStore> m_store;
  std::optional<QString> m_storedClientId;
  QString m_storeProblem;
  bool m_writable = true;

  QCheckBox *m_enabled;
  QLabel *m_note;
  QLabel *m_startNote;
  QLabel *m_tlsNote;
  QLabel *m_hostnameLabel;
  QLineEdit *m_hostname;
  QLabel *m_portLabel;
  QSpinBox *m_port;
  QLabel *m_pathLabel;
  QLineEdit *m_path;
  QPushButton *m_browse;
  QLabel *m_found;
  QLabel *m_installLabel;
  QLineEdit *m_install;
  QLabel *m_clientIdLabel;
  QLineEdit *m_clientId;
  QLabel *m_secretLabel;
  QLineEdit *m_secret;
  QLabel *m_credentialStatus;
  QPushButton *m_remove;
  QLabel *m_error;
};

} // namespace deskflow::gui
