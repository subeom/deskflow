/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/VersionChecker.h"

#include <QObject>

namespace deskflow::gui {

enum class UpdateAction
{
  None,
  OpenRelease,
  ConfirmInstall,
  Retry
};

} // namespace deskflow::gui

Q_DECLARE_METATYPE(deskflow::gui::UpdateAction)

namespace deskflow::gui {

struct UpdateStatusView
{
  QString version;
  QString label;
  QString toolTip;
  bool enabled = false;
  bool visible = false;
  bool installable = false;
  UpdateAction action = UpdateAction::None;

  bool operator==(const UpdateStatusView &) const = default;
};

class UpdateStatusModel final : public QObject
{
  Q_OBJECT
public:
  explicit UpdateStatusModel(QObject *parent = nullptr);

  void setStatus(const UpdateStatusSnapshot &status);
  void retranslate();
  const UpdateStatusView &view() const;
  const UpdateStatusSnapshot &status() const;

Q_SIGNALS:
  void changed();

private:
  UpdateStatusSnapshot m_status;
  UpdateStatusView m_view;
};

} // namespace deskflow::gui
