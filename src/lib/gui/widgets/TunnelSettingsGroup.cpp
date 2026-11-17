/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/widgets/TunnelSettingsGroup.h"

#include "common/Constants.h"
#include "common/Settings.h"
#include "common/TunnelAddress.h"
#include "gui/tunnel/SecretStoreFactory.h"
#include "gui/tunnel/TunnelTypes.h"

#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

#include <initializer_list>

namespace deskflow::gui {

using namespace deskflow::tunnel;

namespace {

// the end of a Client ID, enough to tell two apart and not the whole of it; the ".access" that every Client ID ends in
// is the same for all of them and is left out
QString idTail(const QString &clientId)
{
  constexpr int kTailLength = 6;
  const auto suffix = QStringLiteral(".access");
  const auto base = clientId.endsWith(suffix) ? clientId.chopped(suffix.size()) : clientId;
  return base.size() > kTailLength ? base.right(kTailLength) : base;
}

} // namespace

TunnelSettingsGroup::TunnelSettingsGroup(TunnelSettingsDependencies dependencies, QWidget *parent)
    : QGroupBox(parent),
      m_dependencies(std::move(dependencies)),
      m_store(m_dependencies.secretStore ? m_dependencies.secretStore : createSystemSecretStore()),
      m_enabled(new QCheckBox(this)),
      m_note(new QLabel(this)),
      m_startNote(new QLabel(this)),
      m_tlsNote(new QLabel(this)),
      m_hostnameLabel(new QLabel(this)),
      m_hostname(new QLineEdit(this)),
      m_portLabel(new QLabel(this)),
      m_port(new QSpinBox(this)),
      m_pathLabel(new QLabel(this)),
      m_path(new QLineEdit(this)),
      m_browse(new QPushButton(this)),
      m_found(new QLabel(this)),
      m_installLabel(new QLabel(this)),
      m_install(new QLineEdit(this)),
      m_clientIdLabel(new QLabel(this)),
      m_clientId(new QLineEdit(this)),
      m_secretLabel(new QLabel(this)),
      m_secret(new QLineEdit(this)),
      m_credentialStatus(new QLabel(this)),
      m_remove(new QPushButton(this)),
      m_error(new QLabel(this))
{
  setObjectName(QStringLiteral("groupCloudflareTunnel"));
  m_enabled->setObjectName(QStringLiteral("cbTunnelEnabled"));
  m_note->setObjectName(QStringLiteral("lblTunnelNote"));
  m_startNote->setObjectName(QStringLiteral("lblTunnelStartNote"));
  m_tlsNote->setObjectName(QStringLiteral("lblTunnelTlsNote"));
  m_hostname->setObjectName(QStringLiteral("leTunnelHostname"));
  m_port->setObjectName(QStringLiteral("sbTunnelLocalPort"));
  m_path->setObjectName(QStringLiteral("leCloudflaredPath"));
  m_browse->setObjectName(QStringLiteral("btnCloudflaredBrowse"));
  m_found->setObjectName(QStringLiteral("lblCloudflaredFound"));
  m_install->setObjectName(QStringLiteral("leCloudflaredInstall"));
  m_clientId->setObjectName(QStringLiteral("leTokenClientId"));
  m_secret->setObjectName(QStringLiteral("leTokenSecret"));
  m_credentialStatus->setObjectName(QStringLiteral("lblCredentialStatus"));
  m_remove->setObjectName(QStringLiteral("btnRemoveCredentials"));
  m_error->setObjectName(QStringLiteral("lblTunnelError"));

  // lines that do not wrap and are cut at the width of the group instead of widening the dialog
  for (auto *label : {m_note, m_startNote, m_tlsNote, m_found, m_credentialStatus, m_error}) {
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  }
  m_error->setStyleSheet(QStringLiteral("color: #c0392b;"));
  m_error->setVisible(false);
  m_install->setReadOnly(true);

  m_hostname->setPlaceholderText(QStringLiteral("mbp.example.com"));
  m_port->setRange(kMinLocalPort, kMaxLocalPort);
  m_port->setValue(kDefaultLocalPort);
  m_port->setGroupSeparatorShown(false);

  // the Secret is typed here and goes nowhere else: no echo, no suggestions, no history
  m_secret->setEchoMode(QLineEdit::Password);
  m_secret->setInputMethodHints(Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
  m_secret->setClearButtonEnabled(false);
  m_clientId->setInputMethodHints(Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);

  m_installLabel->setBuddy(m_install);
  m_hostnameLabel->setBuddy(m_hostname);
  m_portLabel->setBuddy(m_port);
  m_pathLabel->setBuddy(m_path);
  m_clientIdLabel->setBuddy(m_clientId);
  m_secretLabel->setBuddy(m_secret);

  auto *layout = new QGridLayout(this);
  layout->addWidget(m_enabled, 0, 0, 1, 3);
  layout->addWidget(m_note, 1, 0, 1, 3);
  layout->addWidget(m_startNote, 2, 0, 1, 3);
  layout->addWidget(m_tlsNote, 3, 0, 1, 3);
  layout->addWidget(m_hostnameLabel, 4, 0);
  layout->addWidget(m_hostname, 4, 1, 1, 2);
  layout->addWidget(m_portLabel, 5, 0);
  layout->addWidget(m_port, 5, 1);
  layout->addWidget(m_pathLabel, 6, 0);
  layout->addWidget(m_path, 6, 1);
  layout->addWidget(m_browse, 6, 2);
  layout->addWidget(m_found, 7, 1, 1, 2);
  layout->addWidget(m_installLabel, 8, 0);
  layout->addWidget(m_install, 8, 1, 1, 2);
  layout->addWidget(m_clientIdLabel, 9, 0);
  layout->addWidget(m_clientId, 9, 1, 1, 2);
  layout->addWidget(m_secretLabel, 10, 0);
  layout->addWidget(m_secret, 10, 1);
  layout->addWidget(m_remove, 10, 2);
  layout->addWidget(m_credentialStatus, 11, 0, 1, 3);
  layout->addWidget(m_error, 12, 0, 1, 3);
  layout->setColumnStretch(1, 1);

  // the secure store is asked when the tunnel is turned on and when the fields of the token are used, and not before
  connect(m_enabled, &QCheckBox::toggled, this, [this](bool on) {
    if (on) {
      ensureCredentialsLoaded();
    }
    edited();
  });
  connect(m_hostname, &QLineEdit::textChanged, this, &TunnelSettingsGroup::edited);
  connect(m_port, &QSpinBox::valueChanged, this, &TunnelSettingsGroup::edited);
  connect(m_path, &QLineEdit::textChanged, this, &TunnelSettingsGroup::edited);
  for (auto *field : {m_clientId, m_secret}) {
    field->installEventFilter(this);
    connect(field, &QLineEdit::textChanged, this, [this] {
      ensureCredentialsLoaded();
      edited();
    });
  }
  connect(m_browse, &QPushButton::clicked, this, &TunnelSettingsGroup::browseForCloudflared);
  connect(m_remove, &QPushButton::clicked, this, &TunnelSettingsGroup::removeCredentials);

  retranslate();
  load();
}

void TunnelSettingsGroup::retranslate()
{
  setTitle(tr("Cloudflare Tunnel"));
  m_enabled->setText(tr("Connect through Cloudflare Tunnel"));
  m_hostnameLabel->setText(tr("Tunnel hostname:"));
  m_portLabel->setText(tr("Local port:"));
  m_port->setToolTip(tr("The port on this computer that cloudflared listens on. The client connects to it."));
  m_pathLabel->setText(tr("cloudflared:"));
  m_installLabel->setText(tr("Install with:"));
  m_path->setPlaceholderText(tr("Automatic"));
  m_browse->setText(tr("Browse..."));
  m_clientIdLabel->setText(tr("Service Token Client ID:"));
  m_secretLabel->setText(tr("Service Token Secret:"));
  m_remove->setText(tr("Remove credentials"));
  updateStatusLines();
}

void TunnelSettingsGroup::changeEvent(QEvent *event)
{
  QGroupBox::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    retranslate();
  }
}

bool TunnelSettingsGroup::tlsEnabled() const
{
  return m_dependencies.tlsEnabled ? m_dependencies.tlsEnabled()
                                   : Settings::value(Settings::Security::TlsEnabled).toBool();
}

QString TunnelSettingsGroup::typedHostname() const
{
  return normalizedHostname(m_hostname->text());
}

QString TunnelSettingsGroup::typedPath() const
{
  return m_path->text().trimmed();
}

void TunnelSettingsGroup::load()
{
  const QSignalBlocker blockEnabled(m_enabled);
  const QSignalBlocker blockHostname(m_hostname);
  const QSignalBlocker blockPort(m_port);
  const QSignalBlocker blockPath(m_path);
  const QSignalBlocker blockClientId(m_clientId);
  const QSignalBlocker blockSecret(m_secret);

  m_enabled->setChecked(Settings::value(Settings::Tunnel::Enabled).toBool());
  m_hostname->setText(normalizedHostname(Settings::value(Settings::Tunnel::Hostname).toString()));
  const auto port = localPortFromSetting(Settings::value(Settings::Tunnel::LocalPort));
  m_port->setValue(port != 0 ? port : kDefaultLocalPort);
  m_path->setText(Settings::value(Settings::Tunnel::CloudflaredPath).toString().trimmed());

  // what the store was asked before is kept: it is asked once for the life of the dialog, and only when there is a
  // reason (the tunnel is on, or the fields of the token are used). The Secret is not asked for, and it is never put
  // into a field.
  m_clientId->setText(m_storedClientId.value_or(QString()));
  m_secret->clear();

  showError(QString());
  if (m_enabled->isChecked()) {
    ensureCredentialsLoaded();
  }
  updateControls();
  updateStatusLines();
}

void TunnelSettingsGroup::ensureCredentialsLoaded()
{
  if (m_credentialsLoaded) {
    return;
  }
  m_credentialsLoaded = true;

  StoreResult result;
  m_storedClientId = m_store->storedClientId(&result);
  m_storeProblem = result.ok || result.notFound ? QString() : result.detail;

  // what the user has typed is not replaced by what is stored
  if (m_storedClientId.has_value() && m_clientId->text().trimmed().isEmpty()) {
    const QSignalBlocker blockClientId(m_clientId);
    m_clientId->setText(*m_storedClientId);
  }
  updateControls();
  updateStatusLines();
}

bool TunnelSettingsGroup::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::FocusIn && (watched == m_clientId || watched == m_secret)) {
    ensureCredentialsLoaded();
  }
  return QGroupBox::eventFilter(watched, event);
}

void TunnelSettingsGroup::updateControls()
{
  m_enabled->setEnabled(m_writable && (tlsEnabled() || m_enabled->isChecked()));
  for (QWidget *widget : std::initializer_list<QWidget *>{m_hostname, m_port, m_path, m_browse, m_clientId, m_secret}) {
    widget->setEnabled(m_writable);
  }
  m_remove->setEnabled(m_writable && m_storedClientId.has_value());
}

void TunnelSettingsGroup::setWritable(bool writable)
{
  m_writable = writable;
  updateControls();
}

void TunnelSettingsGroup::updateStatusLines()
{
  // every text is a line, and what does not fit is in the tool tip
  const auto setLine = [](QLabel *label, const QString &text, const QString &tip = {}) {
    label->setText(text);
    label->setToolTip(tip.isEmpty() ? text : tip);
  };

  setLine(m_note, tr("Your saved server address is kept for when the tunnel is off."));

  if (kTunnelSupervisorAvailable) {
    setLine(
        m_startNote, tr("Deskflow starts cloudflared with the client and stops it with the client."),
        tr("While this is on, the client connects to 127.0.0.1 on the local port, where the cloudflared that Deskflow "
           "starts listens. Starting and stopping the client starts and stops cloudflared: you do not start it "
           "yourself.")
    );
  } else {
    setLine(
        m_startNote, tr("This version does not start cloudflared: start it yourself."),
        tr("While this is on, the client connects to 127.0.0.1 on the local port. This version does not start "
           "cloudflared for you: start it yourself with the same hostname and port.")
    );
  }

  m_tlsNote->setVisible(!tlsEnabled());
  setLine(m_tlsNote, failureText(TunnelFailure::TlsRequired));

  const auto locate =
      m_dependencies.locate ? m_dependencies.locate : [](const QString &path) { return locateCloudflared(path); };
  const auto location = locate(typedPath());
  m_installLabel->setVisible(!location.found());
  m_install->setVisible(!location.found());
  if (location.found()) {
    const auto where = QDir::toNativeSeparators(location.path);
    setLine(
        m_found, location.configuredPathUnusable ? tr("The path above cannot be run. Found instead: %1").arg(where)
                                                 : tr("Found: %1").arg(where)
    );
  } else {
    setLine(m_found, tr("cloudflared was not found."), failureText(TunnelFailure::CloudflaredMissing));
    m_install->setText(cloudflaredInstallHint(currentPlatform()));
  }

  const auto appName = QString::fromLatin1(kAppName);
  if (!m_credentialsLoaded) {
    setLine(
        m_credentialStatus, tr("Credentials are checked when the tunnel is turned on"),
        tr("The secure store is asked only when the tunnel is turned on or when you go into the Client ID and Secret "
           "fields, so that opening this window never asks the system for access.")
    );
  } else if (!m_storeProblem.isEmpty()) {
    setLine(m_credentialStatus, tr("Credentials unreadable: %1").arg(m_storeProblem));
  } else if (m_storedClientId.has_value()) {
    if (m_store->isPersistent()) {
      setLine(
          m_credentialStatus, tr("Stored in %1 (Client ID ...%2)").arg(m_store->name(), idTail(*m_storedClientId)),
          tr("The Service Token is kept in %1 and is read when the tunnel is started.").arg(m_store->name())
      );
    } else {
      setLine(
          m_credentialStatus, tr("In memory only, forgotten at exit (Client ID ...%1)").arg(idTail(*m_storedClientId)),
          tr("This system has no secure storage: the Service Token is kept only while %1 runs.").arg(appName)
      );
    }
  } else if (m_store->isPersistent()) {
    setLine(m_credentialStatus, tr("No credentials stored"));
  } else {
    setLine(
        m_credentialStatus, tr("No credentials stored (no secure storage on this system)"),
        tr("This system has no secure storage: the Secret is kept only while %1 runs.").arg(appName)
    );
  }

  m_secret->setPlaceholderText(m_storedClientId.has_value() ? tr("Stored (not shown)") : QString());
}

void TunnelSettingsGroup::edited()
{
  updateControls();
  updateStatusLines();
  showError(QString());
  Q_EMIT changed();
}

void TunnelSettingsGroup::showError(const QString &text)
{
  m_error->setText(text);
  m_error->setToolTip(text);
  m_error->setVisible(!text.isEmpty());
}

bool TunnelSettingsGroup::isModified() const
{
  const auto storedPort = localPortFromSetting(Settings::value(Settings::Tunnel::LocalPort));
  const auto currentId = m_storedClientId.value_or(QString());
  return m_enabled->isChecked() != Settings::value(Settings::Tunnel::Enabled).toBool() ||
         typedHostname() != normalizedHostname(Settings::value(Settings::Tunnel::Hostname).toString()) ||
         m_port->value() != (storedPort != 0 ? storedPort : kDefaultLocalPort) ||
         typedPath() != Settings::value(Settings::Tunnel::CloudflaredPath).toString().trimmed() ||
         m_clientId->text().trimmed() != currentId || !m_secret->text().isEmpty();
}

bool TunnelSettingsGroup::isDefault() const
{
  return !m_enabled->isChecked() && typedHostname().isEmpty() && m_port->value() == kDefaultLocalPort &&
         typedPath().isEmpty();
}

void TunnelSettingsGroup::resetToDefault()
{
  m_enabled->setChecked(false);
  m_hostname->clear();
  m_port->setValue(kDefaultLocalPort);
  m_path->clear();
  // the token is not a setting: it stays, and only what was typed is dropped
  m_clientId->setText(m_storedClientId.value_or(QString()));
  m_secret->clear();
}

QString TunnelSettingsGroup::validationProblem() const
{
  if (m_enabled->isChecked() && !tlsEnabled()) {
    return failureText(TunnelFailure::TlsRequired);
  }
  if (m_enabled->isChecked() && !isValidHostname(typedHostname())) {
    return tr("Enter a valid tunnel hostname, for example mbp.example.com.");
  }

  const auto clientId = m_clientId->text().trimmed();
  const bool secretTyped = !m_secret->text().isEmpty();
  if (secretTyped && clientId.isEmpty()) {
    return tr("Enter the Client ID that goes with the Secret.");
  }
  if (!secretTyped && !clientId.isEmpty() && clientId != m_storedClientId.value_or(QString())) {
    return tr("Enter the Secret that goes with the Client ID.");
  }
  return {};
}

bool TunnelSettingsGroup::save()
{
  // the token first: when the secure store refuses it, nothing is written and the user is told why
  bool tokenSaved = false;
  if (!m_secret->text().isEmpty()) {
    const ServiceToken token(m_clientId->text().trimmed(), m_secret->text());
    const auto result = m_store->save(token);
    if (!result.ok) {
      showError(tr("The credentials could not be stored: %1").arg(result.detail));
      return false;
    }
    m_secret->clear();
    tokenSaved = true;
  }

  bool changedAnySetting = false;
  const auto write = [&changedAnySetting](const QString &key, const QVariant &now, const QVariant &then) {
    if (now == then) {
      return;
    }
    Settings::setValue(key, now);
    changedAnySetting = true;
  };

  // an empty text is the setting not being there, the same as in a configuration file that never had it
  const auto textOrUnset = [](const QString &text) { return text.isEmpty() ? QVariant() : QVariant(text); };
  const auto storedPort = localPortFromSetting(Settings::value(Settings::Tunnel::LocalPort));
  const auto storedHostname = normalizedHostname(Settings::value(Settings::Tunnel::Hostname).toString());
  const auto storedPath = Settings::value(Settings::Tunnel::CloudflaredPath).toString().trimmed();

  write(Settings::Tunnel::Enabled, m_enabled->isChecked(), Settings::value(Settings::Tunnel::Enabled).toBool());
  write(Settings::Tunnel::Hostname, textOrUnset(typedHostname()), textOrUnset(storedHostname));
  write(Settings::Tunnel::LocalPort, m_port->value(), storedPort != 0 ? storedPort : kDefaultLocalPort);
  write(Settings::Tunnel::CloudflaredPath, textOrUnset(typedPath()), textOrUnset(storedPath));

  // a port that was in use because the configured one was busy belongs to the configuration that was changed
  if (changedAnySetting && Settings::value(Settings::Tunnel::ActivePort).isValid()) {
    Settings::setValue(Settings::Tunnel::ActivePort);
  }

  // what is stored is looked at again only when it can have changed or has been looked at before: a dialog that was
  // accepted without anybody going to the token does not ask the store
  if (tokenSaved || m_credentialsLoaded) {
    m_credentialsLoaded = true;
    StoreResult result;
    m_storedClientId = m_store->storedClientId(&result);
    m_storeProblem = result.ok || result.notFound ? QString() : result.detail;
    m_clientId->setText(m_storedClientId.value_or(m_clientId->text().trimmed()));
  }
  showError(QString());
  updateControls();
  updateStatusLines();
  return true;
}

void TunnelSettingsGroup::browseForCloudflared()
{
  const auto start = typedPath().isEmpty() ? QString() : QFileInfo(typedPath()).absolutePath();
  const auto chosen = QFileDialog::getOpenFileName(this, tr("Select cloudflared"), start);
  if (!chosen.isEmpty()) {
    m_path->setText(QDir::toNativeSeparators(chosen));
  }
}

void TunnelSettingsGroup::removeCredentials()
{
  const bool confirmed =
      m_dependencies.confirmRemoval
          ? m_dependencies.confirmRemoval()
          : QMessageBox::question(
                this, QString::fromLatin1(kAppName),
                tr("Remove the stored Service Token? The Secret cannot be shown again: it has to be copied from "
                   "Cloudflare once more."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No
            ) == QMessageBox::Yes;
  if (!confirmed) {
    return;
  }

  const auto result = m_store->remove();
  if (!result.ok) {
    showError(tr("The credentials could not be removed: %1").arg(result.detail));
    return;
  }

  m_storedClientId.reset();
  m_storeProblem.clear();
  m_credentialsLoaded = true;
  const QSignalBlocker blockClientId(m_clientId);
  const QSignalBlocker blockSecret(m_secret);
  m_clientId->clear();
  m_secret->clear();
  showError(QString());
  updateControls();
  updateStatusLines();
  Q_EMIT changed();
}

} // namespace deskflow::gui
