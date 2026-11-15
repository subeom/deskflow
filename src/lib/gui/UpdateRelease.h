/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

namespace deskflow::gui {

enum class UpdateInstallMode
{
  Manual,
  Automatic
};

struct UpdateAsset
{
  QString name;
  QString downloadUrl;
  QString digest;
  qint64 size = 0;
};

struct UpdateRelease
{
  QString tag;
  QString version;
  QString pageUrl;
  QList<UpdateAsset> assets;
};

struct UpdateReleaseParseResult
{
  std::optional<UpdateRelease> release;
  QString error;
};

struct UpdatePlan
{
  QString releaseVersion;
  QString releaseUrl;
  std::optional<UpdateAsset> installer;
  std::optional<UpdateAsset> checksums;
  UpdateInstallMode mode = UpdateInstallMode::Manual;
};

UpdateReleaseParseResult parseGitHubRelease(const QByteArray &json);
std::optional<UpdatePlan>
planUpdate(const UpdateRelease &release, const QString &platform, const QString &architecture, bool packageManaged);
bool isNewerUpdateVersion(const QString &current, const QString &candidate);
bool isValidSha256Digest(const QString &digest);

} // namespace deskflow::gui
