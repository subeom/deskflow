/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace deskflow::gui {

enum class UpdateInstallMode
{
  Manual,
  AutomaticMac
};

struct UpdateAsset
{
  QString name;
  QUrl downloadUrl;
  QString digest;
  qint64 size = 0;
};

struct UpdateRelease
{
  QString tag;
  QString version;
  QUrl pageUrl;
  QList<UpdateAsset> assets;
};

struct UpdateEnvironment
{
  QString platform;
  QString architecture;
  bool packageManaged = false;
  bool destinationWritable = false;
  bool serviceMode = false;
  bool supported = true;
  bool automaticInstallEligible = true;
};

struct UpdatePlan
{
  QString releaseVersion;
  QString tag;
  QUrl releaseUrl;
  UpdateInstallMode mode = UpdateInstallMode::Manual;
  std::optional<UpdateAsset> installer;
  std::optional<UpdateAsset> checksums;
};

struct UpdateReleaseParseResult
{
  std::optional<UpdateRelease> release;
  QString error;
};

struct UpdateChecksumResult
{
  std::optional<QString> checksum;
  QString error;
};

UpdateReleaseParseResult parseGitHubRelease(const QByteArray &json);
std::optional<UpdatePlan> planUpdate(const UpdateRelease &release, const UpdateEnvironment &environment);
bool isNewerUpdateVersion(const QString &current, const QString &candidate);
bool isValidSha256Digest(const QString &digest);
UpdateChecksumResult parseChecksumManifest(const QByteArray &payload, const QString &expectedAssetName);

} // namespace deskflow::gui
