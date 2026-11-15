/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateRelease.h"

#include <QMetaType>
#include <QString>

#include <optional>

namespace deskflow::gui {

struct StagedUpdate
{
  QString version;
  QString installerName;
  QString installerPath;
  QString manifestPath;
};

struct UpdateStageResult
{
  bool ok = false;
  StagedUpdate staged;
  QString error;
};

class UpdateCache
{
public:
  explicit UpdateCache(QString rootPath);

  std::optional<StagedUpdate> find(const UpdatePlan &plan) const;
  static std::optional<StagedUpdate> load(const QString &manifestPath);
  UpdateStageResult
  stage(const UpdatePlan &plan, const QByteArray &installer, const std::optional<QByteArray> &sums = std::nullopt);
  bool discard(const StagedUpdate &staged);

private:
  QString stageDirectory(const UpdatePlan &plan) const;
  bool isInsideCache(const QString &path) const;

  QString m_rootPath;
};

} // namespace deskflow::gui

Q_DECLARE_METATYPE(deskflow::gui::StagedUpdate)
