/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace deskflow::gui {

namespace {

constexpr auto kDigestPattern = R"(sha256:[0-9a-fA-F]{64})";
constexpr auto kMaximumAutomaticInstallerBytes = 512LL * 1024 * 1024;
constexpr auto kMaximumAutomaticChecksumBytes = 2LL * 1024 * 1024;

struct ParsedVersion
{
  int major = 0;
  int minor = 0;
  int patch = 0;
  int stage = 0;
};

int stageRank(const QString &stage)
{
  const auto normalized = stage.toLower();
  if (normalized.isEmpty() || normalized == QStringLiteral("stable")) {
    return INT_MAX;
  }
  if (normalized.startsWith(QStringLiteral("rc"))) {
    return 2 + normalized.mid(2).toInt();
  }
  if (normalized == QStringLiteral("beta")) {
    return 1;
  }
  return 0;
}

std::optional<ParsedVersion> parseVersion(const QString &version)
{
  static const QRegularExpression pattern(
      QStringLiteral(R"(^v?(\d+)\.(\d+)\.(\d+)(?:\.\d+)?(?:-([A-Za-z]+)(\d*)?)?$)")
  );
  const auto match = pattern.match(version.trimmed());
  if (!match.hasMatch()) {
    return std::nullopt;
  }

  const auto stageName = match.captured(4);
  const auto stageNumber = match.captured(5);
  QString stage = stageName;
  if (stageName.startsWith(QStringLiteral("rc"), Qt::CaseInsensitive) && !stageNumber.isEmpty()) {
    stage += stageNumber;
  }

  return ParsedVersion{
      match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt(), stageRank(stage)
  };
}

bool isSafeAssetName(const QString &name)
{
  static const QRegularExpression pattern(QStringLiteral(R"(^[A-Za-z0-9][A-Za-z0-9._+-]*$)"));
  return pattern.match(name).hasMatch();
}

bool isOfficialAssetUrl(const QString &url, const QString &tag, const QString &name)
{
  return url == QStringLiteral("https://github.com/deskflow/deskflow/releases/download/%1/%2").arg(tag, name);
}

std::optional<UpdateAsset> findAsset(const UpdateRelease &release, const QString &name)
{
  for (const auto &asset : release.assets) {
    if (asset.name == name) {
      return asset;
    }
  }
  return std::nullopt;
}

} // namespace

UpdateReleaseParseResult parseGitHubRelease(const QByteArray &json)
{
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return {{}, QStringLiteral("invalid GitHub release JSON: %1").arg(parseError.errorString())};
  }

  const auto root = document.object();
  if (root.value(QStringLiteral("draft")).toBool() || root.value(QStringLiteral("prerelease")).toBool()) {
    return {{}, QStringLiteral("latest GitHub release is not a stable release")};
  }

  const auto tag = root.value(QStringLiteral("tag_name")).toString();
  const auto parsedVersion = parseVersion(tag);
  if (!tag.startsWith(QStringLiteral("v")) || !parsedVersion.has_value()) {
    return {{}, QStringLiteral("invalid stable release tag")};
  }

  const auto pageUrl = root.value(QStringLiteral("html_url")).toString();
  if (pageUrl != QStringLiteral("https://github.com/deskflow/deskflow/releases/tag/%1").arg(tag)) {
    return {{}, QStringLiteral("release page is not the official Deskflow release URL")};
  }

  const auto assetsJson = root.value(QStringLiteral("assets")).toArray();
  QList<UpdateAsset> assets;
  QSet<QString> names;
  for (const auto &assetValue : assetsJson) {
    if (!assetValue.isObject()) {
      continue;
    }
    const auto asset = assetValue.toObject();
    const auto name = asset.value(QStringLiteral("name")).toString();
    const auto url = asset.value(QStringLiteral("browser_download_url")).toString();
    const auto size = asset.value(QStringLiteral("size")).toInteger();
    const auto digest = asset.value(QStringLiteral("digest")).toString();
    if (!isSafeAssetName(name) || !isOfficialAssetUrl(url, tag, name) || size <= 0 || names.contains(name)) {
      return {{}, QStringLiteral("release contains an invalid or duplicate asset")};
    }
    names.insert(name);
    assets.append({name, url, digest, size});
  }

  if (assets.isEmpty()) {
    return {{}, QStringLiteral("stable release contains no assets")};
  }

  UpdateRelease release;
  release.tag = tag;
  release.version = tag.mid(1);
  release.pageUrl = pageUrl;
  release.assets = assets;
  return {release, {}};
}

std::optional<UpdatePlan>
planUpdate(const UpdateRelease &release, const QString &platform, const QString &architecture, bool packageManaged)
{
  const auto normalizedPlatform = platform.toLower();
  const auto normalizedArchitecture = architecture.toLower();
  QString architectureName;
  if (normalizedArchitecture == QStringLiteral("x86_64") || normalizedArchitecture == QStringLiteral("amd64")) {
    architectureName = QStringLiteral("x86_64");
  } else if (normalizedArchitecture == QStringLiteral("arm64") || normalizedArchitecture == QStringLiteral("aarch64")) {
    architectureName = QStringLiteral("arm64");
  } else {
    return std::nullopt;
  }

  UpdatePlan plan;
  plan.releaseVersion = release.version;
  plan.releaseUrl = release.pageUrl;
  plan.mode = UpdateInstallMode::Manual;
  plan.checksums = findAsset(release, QStringLiteral("sums.txt"));

  if (normalizedPlatform == QStringLiteral("macos")) {
    const auto installer =
        findAsset(release, QStringLiteral("deskflow-%1-macos-%2.dmg").arg(release.version, architectureName));
    if (!installer || !isValidSha256Digest(installer->digest)) {
      return std::nullopt;
    }
    plan.installer = *installer;
    if (!packageManaged) {
      plan.mode = UpdateInstallMode::Automatic;
    }
  } else if (normalizedPlatform == QStringLiteral("windows")) {
    const auto windowsArchitecture =
        architectureName == QStringLiteral("arm64") ? QStringLiteral("arm64") : QStringLiteral("x64");
    const auto installer =
        findAsset(release, QStringLiteral("deskflow-%1-win-%2.msi").arg(release.version, windowsArchitecture));
    if (!installer || !isValidSha256Digest(installer->digest)) {
      return std::nullopt;
    }
    plan.installer = *installer;
  } else if (normalizedPlatform != QStringLiteral("linux")) {
    return std::nullopt;
  }

  if (plan.checksums && !isValidSha256Digest(plan.checksums->digest)) {
    return std::nullopt;
  }
  if (plan.mode == UpdateInstallMode::Automatic &&
      (plan.installer->size > kMaximumAutomaticInstallerBytes ||
       (plan.checksums && plan.checksums->size > kMaximumAutomaticChecksumBytes))) {
    return std::nullopt;
  }
  return plan;
}

bool isNewerUpdateVersion(const QString &current, const QString &candidate)
{
  const auto currentVersion = parseVersion(current);
  const auto candidateVersion = parseVersion(candidate);
  if (!currentVersion || !candidateVersion) {
    return false;
  }

  if (candidateVersion->major != currentVersion->major) {
    return candidateVersion->major > currentVersion->major;
  }
  if (candidateVersion->minor != currentVersion->minor) {
    return candidateVersion->minor > currentVersion->minor;
  }
  if (candidateVersion->patch != currentVersion->patch) {
    return candidateVersion->patch > currentVersion->patch;
  }
  return candidateVersion->stage > currentVersion->stage;
}

bool isValidSha256Digest(const QString &digest)
{
  static const QRegularExpression pattern(QStringLiteral("^%1$").arg(kDigestPattern));
  return pattern.match(digest).hasMatch();
}

} // namespace deskflow::gui
