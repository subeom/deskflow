/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateRelease.h"

#include <QByteArray>
#include <QDateTime>
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

struct UpdateCacheState
{
  int schemaVersion = 1;
  QString endpoint;
  QString etag;
  QByteArray sanitizedSnapshot;
  QString snapshotVersion;
  QDateTime retryAt;
  QDateTime lastSuccessfulCheck;
};

class UpdateCache
{
public:
  explicit UpdateCache(QString rootPath);

  std::optional<StagedUpdate> find(const UpdatePlan &plan) const;
  static std::optional<StagedUpdate> load(const QString &manifestPath);
  UpdateStageResult stage(
      const UpdatePlan &plan, const QByteArray &installer,
      const std::optional<QByteArray> &checksums = std::nullopt
  );
  bool discard(const StagedUpdate &staged);

  std::optional<UpdateCacheState> loadState() const;
  bool saveState(const UpdateCacheState &state);
  bool cleanup(const QString &currentVersion);

private:
  QString m_rootPath;
};

} // namespace deskflow::gui

Q_DECLARE_METATYPE(deskflow::gui::StagedUpdate)
