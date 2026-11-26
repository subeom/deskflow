/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientConfigDialog.h"
#include "ui_ClientConfigDialog.h"

#include "common/ConnectionDiagnosis.h"
#include "common/Settings.h"
#include "gui/widgets/SettingsDialogButtonBox.h"

#include <QPushButton>

namespace {

// what the client uses, which is the default when the settings file holds something that is not a time limit
double connectTimeoutSeconds()
{
  return deskflow::diagnosis::connectTimeoutFromSetting(Settings::value(Settings::Client::ConnectTimeout)).seconds;
}

} // namespace

ClientConfigDialog::ClientConfigDialog(QWidget *parent, deskflow::gui::TunnelSettingsDependencies tunnel)
    : QDialog(parent),
      ui(new Ui::ClientConfigDialog),
      m_buttonBox{new SettingsDialogButtonBox(this)},
      m_tunnelGroup{new deskflow::gui::TunnelSettingsGroup(std::move(tunnel), this)}
{
  ui->setupUi(this);
  layout()->addWidget(m_tunnelGroup);
  layout()->addWidget(m_buttonBox);
  ui->btnClearServerHistory->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-all")));
  initConnections();
  load();
  updateControls();
  setButtonBoxEnabledButtons();
}

ClientConfigDialog::~ClientConfigDialog()
{
  delete ui;
}

void ClientConfigDialog::changeEvent(QEvent *e)
{
  QDialog::changeEvent(e);
  if (e->type() == QEvent::LanguageChange)
    ui->retranslateUi(this);
}

void ClientConfigDialog::updateControls() const
{
  const auto writable = Settings::isWritable();
  ui->cbDynamicConnectTime->setEnabled(writable);
  ui->cbLanguageSync->setEnabled(writable);
  ui->sbConnectTimeout->setEnabled(writable);
  ui->cbXScrollInvert->setEnabled(writable);
  ui->sbXScrollScale->setEnabled(writable);
  ui->cbYScrollInvert->setEnabled(writable);
  ui->sbYScrollScale->setEnabled(writable);
  ui->btnClearServerHistory->setEnabled(
      writable && !Settings::value(Settings::Client::RecentRemoteHosts).toStringList().isEmpty()
  );
  m_tunnelGroup->setWritable(writable);
}

void ClientConfigDialog::initConnections() const
{
  connect(m_buttonBox, &SettingsDialogButtonBox::accepted, this, &ClientConfigDialog::save);
  connect(m_buttonBox, &SettingsDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_buttonBox, &SettingsDialogButtonBox::reset, this, &ClientConfigDialog::load);
  connect(m_buttonBox, &SettingsDialogButtonBox::restoreDefault, this, &ClientConfigDialog::resetToDefault);

  connect(
      ui->cbDynamicConnectTime, &QCheckBox::checkStateChanged, this, &ClientConfigDialog::setButtonBoxEnabledButtons
  );
  connect(ui->btnClearServerHistory, &QPushButton::clicked, this, &ClientConfigDialog::clearServerHistory);
  connect(ui->cbLanguageSync, &QCheckBox::checkStateChanged, this, &ClientConfigDialog::setButtonBoxEnabledButtons);
  connect(ui->sbConnectTimeout, &QDoubleSpinBox::valueChanged, this, &ClientConfigDialog::setButtonBoxEnabledButtons);
  connect(ui->cbYScrollInvert, &QCheckBox::checkStateChanged, this, &ClientConfigDialog::setButtonBoxEnabledButtons);
  connect(ui->sbYScrollScale, &QDoubleSpinBox::valueChanged, this, &ClientConfigDialog::setButtonBoxEnabledButtons);
  connect(ui->cbXScrollInvert, &QCheckBox::checkStateChanged, this, &ClientConfigDialog::setButtonBoxEnabledButtons);
  connect(ui->sbXScrollScale, &QDoubleSpinBox::valueChanged, this, &ClientConfigDialog::setButtonBoxEnabledButtons);
  connect(Settings::instance(), &Settings::settingsWritableChanged, this, &ClientConfigDialog::updateControls);
  connect(
      m_tunnelGroup, &deskflow::gui::TunnelSettingsGroup::changed, this, &ClientConfigDialog::setButtonBoxEnabledButtons
  );
}

bool ClientConfigDialog::isModified() const
{
  return (ui->cbDynamicConnectTime->isChecked() !=
          Settings::value(Settings::Client::DynamicConnectionRetry).toBool()) ||
         (ui->cbLanguageSync->isChecked() != Settings::value(Settings::Client::LanguageSync).toBool()) ||
         (ui->sbConnectTimeout->value() != connectTimeoutSeconds()) ||
         (ui->cbYScrollInvert->isChecked() != Settings::value(Settings::Client::InvertYScroll).toBool()) ||
         (ui->sbYScrollScale->value() != Settings::value(Settings::Client::YScrollScale).toDouble()) ||
         (ui->cbXScrollInvert->isChecked() != Settings::value(Settings::Client::InvertXScroll).toBool()) ||
         (ui->sbXScrollScale->value() != Settings::value(Settings::Client::XScrollScale).toDouble()) ||
         m_tunnelGroup->isModified();
}

bool ClientConfigDialog::isDefault() const
{
  return (ui->cbDynamicConnectTime->isChecked() ==
          Settings::defaultValue(Settings::Client::DynamicConnectionRetry).toBool()) &&
         (ui->cbLanguageSync->isChecked() == Settings::defaultValue(Settings::Client::LanguageSync).toBool()) &&
         (ui->sbConnectTimeout->value() == Settings::defaultValue(Settings::Client::ConnectTimeout).toDouble()) &&
         (ui->cbYScrollInvert->isChecked() == Settings::defaultValue(Settings::Client::InvertYScroll).toBool()) &&
         (ui->sbYScrollScale->value() == Settings::defaultValue(Settings::Client::YScrollScale).toDouble()) &&
         (ui->cbXScrollInvert->isChecked() == Settings::defaultValue(Settings::Client::InvertXScroll).toBool()) &&
         (ui->sbXScrollScale->value() == Settings::defaultValue(Settings::Client::XScrollScale).toDouble()) &&
         m_tunnelGroup->isDefault();
}

void ClientConfigDialog::setButtonBoxEnabledButtons() const
{
  const bool modified = isModified();
  m_buttonBox->enableSave(modified);
  m_buttonBox->enableReset(modified);
  m_buttonBox->enableRestoreDefaults(!isDefault());
}

void ClientConfigDialog::load()
{
  ui->cbDynamicConnectTime->setChecked(Settings::value(Settings::Client::DynamicConnectionRetry).toBool());
  ui->cbLanguageSync->setChecked(Settings::value(Settings::Client::LanguageSync).toBool());
  ui->sbConnectTimeout->setValue(connectTimeoutSeconds());
  ui->cbYScrollInvert->setChecked(Settings::value(Settings::Client::InvertYScroll).toBool());
  ui->sbYScrollScale->setValue(Settings::value(Settings::Client::YScrollScale).toDouble());
  ui->cbXScrollInvert->setChecked(Settings::value(Settings::Client::InvertXScroll).toBool());
  ui->sbXScrollScale->setValue(Settings::value(Settings::Client::XScrollScale).toDouble());
  m_tunnelGroup->load();
  setButtonBoxEnabledButtons();
}

void ClientConfigDialog::resetToDefault()
{
  ui->cbDynamicConnectTime->setChecked(Settings::defaultValue(Settings::Client::DynamicConnectionRetry).toBool());
  ui->cbLanguageSync->setChecked(Settings::defaultValue(Settings::Client::LanguageSync).toBool());
  ui->sbConnectTimeout->setValue(Settings::defaultValue(Settings::Client::ConnectTimeout).toDouble());
  ui->cbYScrollInvert->setChecked(Settings::defaultValue(Settings::Client::InvertYScroll).toBool());
  ui->sbYScrollScale->setValue(Settings::defaultValue(Settings::Client::YScrollScale).toDouble());
  ui->cbXScrollInvert->setChecked(Settings::defaultValue(Settings::Client::InvertXScroll).toBool());
  ui->sbXScrollScale->setValue(Settings::defaultValue(Settings::Client::XScrollScale).toDouble());
  m_tunnelGroup->resetToDefault();
}

void ClientConfigDialog::save()
{
  // before anything is written: what cannot be saved is said, and the dialog stays
  if (const auto problem = m_tunnelGroup->validationProblem(); !problem.isEmpty()) {
    m_tunnelGroup->showError(problem);
    return;
  }

  Settings::setValue(Settings::Client::DynamicConnectionRetry, ui->cbDynamicConnectTime->isChecked());
  Settings::setValue(Settings::Client::LanguageSync, ui->cbLanguageSync->isChecked());
  Settings::setValue(Settings::Client::ConnectTimeout, ui->sbConnectTimeout->value());
  Settings::setValue(Settings::Client::InvertYScroll, ui->cbYScrollInvert->isChecked());
  Settings::setValue(Settings::Client::YScrollScale, ui->sbYScrollScale->value());
  Settings::setValue(Settings::Client::InvertXScroll, ui->cbXScrollInvert->isChecked());
  Settings::setValue(Settings::Client::XScrollScale, ui->sbXScrollScale->value());
  if (!m_tunnelGroup->save()) {
    return;
  }
  QDialog::accept();
}

void ClientConfigDialog::clearServerHistory()
{
  Settings::setValue(Settings::Client::RecentRemoteHosts, QVariant());
  ui->btnClearServerHistory->setEnabled(false);
}
