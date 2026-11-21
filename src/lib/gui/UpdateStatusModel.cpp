/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateStatusModel.h"

namespace deskflow::gui {

UpdateStatusModel::UpdateStatusModel(QObject *parent) : QObject(parent)
{
}

void UpdateStatusModel::setStatus(const UpdateStatusSnapshot &status)
{
  UpdateStatusView view;
  view.version = status.version;
  using Phase = UpdateStatusSnapshot::Phase;
  switch (status.phase) {
  case Phase::Idle:
  case Phase::UpToDate:
    break;
  case Phase::CheckingRelease:
  case Phase::DownloadingChecksums:
  case Phase::DownloadingInstaller:
  case Phase::Verifying:
    view.visible = true;
    view.label = tr("Checking for updates...");
    view.toolTip = tr("Checking for updates...");
    break;
  case Phase::ManualAvailable:
    view.visible = true;
    view.enabled = true;
    view.label = tr("View update");
    view.toolTip = tr("Deskflow v%1 is available").arg(status.version);
    view.action = UpdateAction::OpenRelease;
    break;
  case Phase::Staged:
    view.visible = status.installable;
    view.enabled = status.installable;
    view.installable = status.installable;
    view.label = tr("Install update");
    view.toolTip =
        tr("Update v%1 is verified and ready to install").arg(status.version);
    view.action = status.installable ? UpdateAction::ConfirmInstall : UpdateAction::Retry;
    break;
  case Phase::Confirming:
  case Phase::Applying:
  case Phase::RollingBack:
    view.visible = true;
    view.label = tr("Installing update...");
    view.toolTip = tr("Installing update...");
    break;
  case Phase::Failed:
    view.visible = true;
    view.enabled = true;
    view.label = tr("Update failed");
    view.toolTip =
        status.userError.isEmpty() ? tr("The update could not be completed") : status.userError;
    view.action = UpdateAction::Retry;
    break;
  }

  if (m_status == status && m_view == view) {
    return;
  }
  m_status = status;
  m_view = view;
  Q_EMIT changed();
}

const UpdateStatusView &UpdateStatusModel::view() const
{
  return m_view;
}

const UpdateStatusSnapshot &UpdateStatusModel::status() const
{
  return m_status;
}

} // namespace deskflow::gui
