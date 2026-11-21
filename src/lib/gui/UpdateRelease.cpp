/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <limits>

namespace deskflow::gui {

namespace {

constexpr qint64 kMaximumReleaseBytes = 2LL * 1024 * 1024;
constexpr qint64 kMaximumChecksumBytes = 2LL * 1024 * 1024;
constexpr qint64 kMaximumAutomaticInstallerBytes = 512LL * 1024 * 1024;
constexpr qsizetype kMaximumAssets = 128;
constexpr qsizetype kMaximumChecksumLines = 256;
constexpr qsizetype kMaximumChecksumLineBytes = 1024;

struct ParsedVersion
{
  quint64 major = 0;
  quint64 minor = 0;
  quint64 patch = 0;
};

std::optional<ParsedVersion> parseVersion(const QString &value, bool allowBuildComponent = false)
{
  static const QRegularExpression stablePattern(
      QStringLiteral(R"(^((?:0|[1-9][0-9]*))\.((?:0|[1-9][0-9]*))\.((?:0|[1-9][0-9]*))$)")
  );
  static const QRegularExpression currentPattern(
      QStringLiteral(
          R"(^((?:0|[1-9][0-9]*))\.((?:0|[1-9][0-9]*))\.((?:0|[1-9][0-9]*))(?:\.(?:0|[1-9][0-9]*))?$)"
      )
  );
  const auto match = (allowBuildComponent ? currentPattern : stablePattern).match(value);
  if (!match.hasMatch()) {
    return std::nullopt;
  }

  bool majorValid = false;
  bool minorValid = false;
  bool patchValid = false;
  const ParsedVersion parsed{
      match.captured(1).toULongLong(&majorValid),
      match.captured(2).toULongLong(&minorValid),
      match.captured(3).toULongLong(&patchValid),
  };
  if (!majorValid || !minorValid || !patchValid) {
    return std::nullopt;
  }
  return parsed;
}

bool isSafeAssetName(const QString &name)
{
  static const QRegularExpression pattern(QStringLiteral(R"(^[A-Za-z0-9][A-Za-z0-9._+-]*$)"));
  return name.size() <= 200 && pattern.match(name).hasMatch();
}

bool isExactUrl(const QUrl &url, const QString &expected)
{
  return url.isValid() && !url.isRelative() && url.toEncoded(QUrl::FullyEncoded) == expected.toUtf8();
}

QString invalidJson(const QString &reason)
{
  return QStringLiteral("invalid GitHub release JSON: %1").arg(reason);
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

bool isSupportedStableTag(const QString &tag)
{
  return tag.startsWith(QLatin1Char('v')) && parseVersion(tag.mid(1)).has_value();
}

bool checksumPayloadIsAscii(const QByteArray &payload)
{
  for (const auto byte : payload) {
    const auto value = static_cast<unsigned char>(byte);
    if (value == 0 || value > 0x7f) {
      return false;
    }
  }
  return true;
}

} // namespace

UpdateReleaseParseResult parseGitHubRelease(const QByteArray &json)
{
  if (json.size() > kMaximumReleaseBytes) {
    return {{}, invalidJson(QStringLiteral("response exceeds the 2 MiB limit"))};
  }

  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return {{}, invalidJson(parseError.errorString())};
  }

  const auto root = document.object();
  const auto draft = root.value(QStringLiteral("draft"));
  const auto prerelease = root.value(QStringLiteral("prerelease"));
  const auto tagValue = root.value(QStringLiteral("tag_name"));
  const auto pageValue = root.value(QStringLiteral("html_url"));
  const auto assetsValue = root.value(QStringLiteral("assets"));
  if (!draft.isBool() || !prerelease.isBool() || !tagValue.isString() || !pageValue.isString() ||
      !assetsValue.isArray()) {
    return {{}, invalidJson(QStringLiteral("required field has an unexpected type"))};
  }
  if (draft.toBool() || prerelease.toBool()) {
    return {{}, QStringLiteral("latest GitHub release is not stable")};
  }

  const auto tag = tagValue.toString();
  if (!isSupportedStableTag(tag)) {
    return {{}, QStringLiteral("invalid stable release tag")};
  }
  const auto expectedPage = QStringLiteral("https://github.com/subeom/deskflow/releases/tag/%1").arg(tag);
  if (pageValue.toString() != expectedPage) {
    return {{}, QStringLiteral("release page is not the official fork release URL")};
  }

  const auto assetsJson = assetsValue.toArray();
  if (assetsJson.isEmpty() || assetsJson.size() > kMaximumAssets) {
    return {{}, QStringLiteral("release asset count is outside the allowed range")};
  }

  QList<UpdateAsset> assets;
  QSet<QString> assetNames;
  for (const auto &assetValue : std::as_const(assetsJson)) {
    if (!assetValue.isObject()) {
      return {{}, QStringLiteral("release assets contain a non-object value")};
    }
    const auto assetObject = assetValue.toObject();
    const auto nameValue = assetObject.value(QStringLiteral("name"));
    const auto urlValue = assetObject.value(QStringLiteral("browser_download_url"));
    const auto sizeValue = assetObject.value(QStringLiteral("size"));
    const auto digestValue = assetObject.value(QStringLiteral("digest"));
    if (!nameValue.isString() || !urlValue.isString() || !sizeValue.isDouble() || !digestValue.isString()) {
      return {{}, QStringLiteral("release asset has an unexpected field type")};
    }

    const auto name = nameValue.toString();
    const auto size = sizeValue.toInteger(std::numeric_limits<qint64>::min());
    const auto digest = digestValue.toString();
    const auto expectedDownload =
        QStringLiteral("https://github.com/subeom/deskflow/releases/download/%1/%2").arg(tag, name);
    if (!isSafeAssetName(name) || assetNames.contains(name) || !isValidSha256Digest(digest) || size <= 0 ||
        size > kMaximumAutomaticInstallerBytes || !isExactUrl(QUrl(urlValue.toString()), expectedDownload)) {
      return {{}, QStringLiteral("release contains an invalid, duplicate, or oversized asset")};
    }
    assetNames.insert(name);
    assets.append({name, QUrl(expectedDownload), digest, size});
  }

  return {
      UpdateRelease{
          tag,
          tag.mid(1),
          QUrl(expectedPage),
          std::move(assets),
      },
      {},
  };
}

std::optional<UpdatePlan> planUpdate(const UpdateRelease &release, const UpdateEnvironment &environment)
{
  if (!environment.supported) {
    return std::nullopt;
  }
  const auto platform = environment.platform.toLower();
  if (platform != QStringLiteral("macos") && platform != QStringLiteral("windows") &&
      platform != QStringLiteral("linux")) {
    return std::nullopt;
  }

  UpdatePlan plan;
  plan.releaseVersion = release.version;
  plan.tag = release.tag;
  plan.releaseUrl = release.pageUrl;

  if (platform == QStringLiteral("windows") || platform == QStringLiteral("linux") || environment.packageManaged ||
      environment.serviceMode || !environment.destinationWritable ||
      !environment.automaticInstallEligible) {
    plan.mode = UpdateInstallMode::Manual;
    return plan;
  }

  const auto architecture = environment.architecture.toLower();
  QString packageArchitecture;
  if (architecture == QStringLiteral("arm64") || architecture == QStringLiteral("aarch64")) {
    packageArchitecture = QStringLiteral("arm64");
  } else if (architecture == QStringLiteral("x86_64") || architecture == QStringLiteral("amd64")) {
    packageArchitecture = QStringLiteral("x86_64");
  } else {
    plan.mode = UpdateInstallMode::Manual;
    return plan;
  }

  const auto installerName =
      QStringLiteral("deskflow-%1-macos-%2.dmg").arg(release.version, packageArchitecture);
  const auto installer = findAsset(release, installerName);
  const auto checksums = findAsset(release, QStringLiteral("SHA256SUMS.txt"));
  if (!installer || !checksums || installer->size > kMaximumAutomaticInstallerBytes ||
      checksums->size > kMaximumChecksumBytes) {
    plan.mode = UpdateInstallMode::Manual;
    return plan;
  }

  plan.mode = UpdateInstallMode::AutomaticMac;
  plan.installer = installer;
  plan.checksums = checksums;
  return plan;
}

bool isNewerUpdateVersion(const QString &current, const QString &candidate)
{
  const auto currentVersion = parseVersion(current, true);
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
  return candidateVersion->patch > currentVersion->patch;
}

bool isValidSha256Digest(const QString &digest)
{
  static const QRegularExpression pattern(QStringLiteral(R"(^sha256:[0-9a-fA-F]{64}$)"));
  return pattern.match(digest).hasMatch();
}

UpdateChecksumResult parseChecksumManifest(const QByteArray &payload, const QString &expectedAssetName)
{
  if (!isSafeAssetName(expectedAssetName)) {
    return {{}, QStringLiteral("checksum asset name is unsafe")};
  }
  if (payload.isEmpty() || payload.size() > kMaximumChecksumBytes || !checksumPayloadIsAscii(payload)) {
    return {{}, QStringLiteral("checksum file has invalid encoding or size")};
  }

  const bool hasFinalNewline = payload.endsWith('\n');
  auto rawLines = payload.split('\n');
  if (hasFinalNewline) {
    rawLines.removeLast();
  }
  if (rawLines.isEmpty() || rawLines.size() > kMaximumChecksumLines) {
    return {{}, QStringLiteral("checksum file line count is outside the allowed range")};
  }

  static const QRegularExpression checksumLine(
      QStringLiteral(R"(^([0-9a-fA-F]{64})(?:  \*?| \*)([A-Za-z0-9][A-Za-z0-9._+-]*)$)")
  );
  QSet<QString> names;
  std::optional<QString> expectedChecksum;
  for (qsizetype index = 0; index < rawLines.size(); ++index) {
    auto line = rawLines.at(index);
    if (line.isEmpty() || line.size() > kMaximumChecksumLineBytes) {
      return {{}, QStringLiteral("checksum file contains an empty or oversized line")};
    }
    if (line.endsWith('\r')) {
      if (index == rawLines.size() - 1 && !hasFinalNewline) {
        return {{}, QStringLiteral("checksum file contains a carriage return without LF")};
      }
      line.chop(1);
    }
    if (line.contains('\r')) {
      return {{}, QStringLiteral("checksum file contains an invalid carriage return")};
    }

    const auto match = checksumLine.match(QString::fromLatin1(line));
    if (!match.hasMatch()) {
      return {{}, QStringLiteral("checksum file contains a malformed line")};
    }
    const auto name = match.captured(2);
    if (names.contains(name)) {
      return {{}, QStringLiteral("checksum file contains a duplicate asset")};
    }
    names.insert(name);
    if (name == expectedAssetName) {
      expectedChecksum = match.captured(1).toLower();
    }
  }

  if (!expectedChecksum) {
    return {{}, QStringLiteral("checksum file does not contain the selected asset")};
  }
  return {*expectedChecksum, {}};
}

} // namespace deskflow::gui
