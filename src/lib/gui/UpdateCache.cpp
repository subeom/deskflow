/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateCache.h"

#include "gui/UpdateIntegrity.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace deskflow::gui {

namespace {

std::optional<QByteArray> readFile(const QString &path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  return file.readAll();
}

bool writeFile(const QString &path, const QByteArray &data)
{
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
    return false;
  }
  return file.commit();
}

void removeDirectory(const QString &path)
{
  QDir directory(path);
  if (directory.exists() && !directory.removeRecursively()) {
    qWarning().noquote() << "failed to remove invalid update stage:" << path;
  }
}

bool safeFileName(const QString &name)
{
  return !name.isEmpty() && QFileInfo(name).fileName() == name && !name.contains(QLatin1Char('/')) &&
         !name.contains(QLatin1Char('\\')) && name != QStringLiteral(".") && name != QStringLiteral("..");
}

struct LoadedStagedUpdate
{
  StagedUpdate staged;
  UpdateAsset installer;
  QString releaseUrl;
  std::optional<UpdateAsset> checksums;
};

std::optional<LoadedStagedUpdate> loadManifest(const QString &manifestPath)
{
  const QFileInfo manifestInfo(manifestPath);
  if (manifestInfo.isSymLink() || !manifestInfo.isFile() ||
      manifestInfo.fileName() != QStringLiteral("manifest.json")) {
    return std::nullopt;
  }

  const auto manifestData = readFile(manifestInfo.absoluteFilePath());
  if (!manifestData) {
    return std::nullopt;
  }

  QJsonParseError parseError;
  const auto manifest = QJsonDocument::fromJson(*manifestData, &parseError);
  if (parseError.error != QJsonParseError::NoError || !manifest.isObject()) {
    return std::nullopt;
  }
  const auto object = manifest.object();
  const auto version = object.value(QStringLiteral("version")).toString();
  const auto installerName = object.value(QStringLiteral("installer")).toString();
  const auto digest = object.value(QStringLiteral("digest")).toString();
  const auto size = object.value(QStringLiteral("size")).toInteger();
  const auto releaseUrl = object.value(QStringLiteral("releaseUrl")).toString();
  static const QRegularExpression safeVersion(QStringLiteral(R"(^\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?$)"));
  if (!safeVersion.match(version).hasMatch() || !safeFileName(installerName) || !isValidSha256Digest(digest) ||
      size <= 0 || releaseUrl != QStringLiteral("https://github.com/deskflow/deskflow/releases/tag/v%1").arg(version) ||
      manifestInfo.dir().dirName() != version) {
    return std::nullopt;
  }

  const auto installerPath = manifestInfo.absoluteDir().filePath(installerName);
  const QFileInfo installerInfo(installerPath);
  const auto expectedInstallerPath = QDir(manifestInfo.dir().canonicalPath()).filePath(installerName);
  if (installerInfo.isSymLink() || !installerInfo.isFile() ||
      installerInfo.canonicalFilePath() != expectedInstallerPath) {
    return std::nullopt;
  }
  const auto installerData = readFile(installerInfo.absoluteFilePath());
  if (!installerData) {
    return std::nullopt;
  }

  std::optional<UpdateAsset> checksums;
  const auto checksumDigest = object.value(QStringLiteral("checksumDigest")).toString();
  const auto checksumSize = object.value(QStringLiteral("checksumSize")).toInteger();
  if (!checksumDigest.isEmpty()) {
    if (!isValidSha256Digest(checksumDigest) || checksumSize <= 0) {
      return std::nullopt;
    }
    checksums = UpdateAsset{QStringLiteral("sums.txt"), {}, checksumDigest, checksumSize};
  }

  std::optional<QByteArray> sums;
  const auto sumsPath = manifestInfo.absoluteDir().filePath(QStringLiteral("sums.txt"));
  if (checksums) {
    const QFileInfo sumsInfo(sumsPath);
    if (sumsInfo.isSymLink() || !sumsInfo.isFile() ||
        sumsInfo.canonicalFilePath() != QDir(manifestInfo.dir().canonicalPath()).filePath(QStringLiteral("sums.txt"))) {
      return std::nullopt;
    }
    const auto sumsData = readFile(sumsInfo.absoluteFilePath());
    if (!sumsData || !verifyUpdatePayload(*checksums, *sumsData, std::nullopt).ok) {
      return std::nullopt;
    }
    sums = *sumsData;
  } else if (QFileInfo::exists(sumsPath)) {
    return std::nullopt;
  }

  const UpdateAsset installer{installerName, {}, digest, size};
  if (!verifyUpdatePayload(installer, *installerData, sums).ok) {
    return std::nullopt;
  }

  return LoadedStagedUpdate{
      {version, installerName, installerInfo.absoluteFilePath(), manifestInfo.absoluteFilePath()},
      installer,
      releaseUrl,
      checksums
  };
}

} // namespace

UpdateCache::UpdateCache(QString rootPath) : m_rootPath(std::move(rootPath))
{
}

std::optional<StagedUpdate> UpdateCache::find(const UpdatePlan &plan) const
{
  if (!plan.installer || !safeFileName(plan.installer->name)) {
    return std::nullopt;
  }

  const auto directory = stageDirectory(plan);
  const auto manifestPath = QDir(directory).filePath(QStringLiteral("manifest.json"));
  const auto loaded = loadManifest(manifestPath);
  if (!loaded) {
    return std::nullopt;
  }

  if (loaded->staged.version != plan.releaseVersion || loaded->installer.name != plan.installer->name ||
      loaded->installer.digest != plan.installer->digest || loaded->installer.size != plan.installer->size ||
      loaded->releaseUrl != plan.releaseUrl || loaded->checksums.has_value() != plan.checksums.has_value()) {
    return std::nullopt;
  }
  if (loaded->checksums &&
      (loaded->checksums->digest != plan.checksums->digest || loaded->checksums->size != plan.checksums->size)) {
    return std::nullopt;
  }

  return loaded->staged;
}

std::optional<StagedUpdate> UpdateCache::load(const QString &manifestPath)
{
  const auto loaded = loadManifest(manifestPath);
  return loaded ? std::optional<StagedUpdate>{loaded->staged} : std::nullopt;
}

UpdateStageResult
UpdateCache::stage(const UpdatePlan &plan, const QByteArray &installer, const std::optional<QByteArray> &sums)
{
  if (!plan.installer || !safeFileName(plan.installer->name)) {
    return {false, {}, QStringLiteral("invalid update asset name")};
  }
  if (plan.checksums.has_value() != sums.has_value()) {
    return {
        false,
        {},
        plan.checksums ? QStringLiteral("release checksum file is missing")
                       : QStringLiteral("unexpected update checksum file")
    };
  }
  if (sums) {
    const auto checksumVerification = verifyUpdatePayload(*plan.checksums, *sums, std::nullopt);
    if (!checksumVerification.ok) {
      return {false, {}, QStringLiteral("update checksum file failed verification")};
    }
  }

  const auto verification = verifyUpdatePayload(*plan.installer, installer, sums);
  if (!verification.ok) {
    return {false, {}, verification.error};
  }

  if (const auto existing = find(plan)) {
    return {true, *existing, {}};
  }

  static const QRegularExpression safeVersion(QStringLiteral(R"(^\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?$)"));
  if (!safeVersion.match(plan.releaseVersion).hasMatch()) {
    return {false, {}, QStringLiteral("invalid update version")};
  }

  QDir root;
  if (!root.mkpath(m_rootPath)) {
    return {false, {}, QStringLiteral("cannot create update cache")};
  }
  const auto directory = stageDirectory(plan);
  QDir existingDirectory(directory);
  if (existingDirectory.exists()) {
    if (!existingDirectory.removeRecursively()) {
      return {false, {}, QStringLiteral("cannot replace invalid update stage")};
    }
  }
  if (!root.mkpath(directory)) {
    return {false, {}, QStringLiteral("cannot create update stage directory")};
  }

  const auto installerPath = QDir(directory).filePath(plan.installer->name);
  if (!writeFile(installerPath, installer)) {
    removeDirectory(directory);
    return {false, {}, QStringLiteral("failed to publish staged installer")};
  }

  if (sums) {
    if (!writeFile(QDir(directory).filePath(QStringLiteral("sums.txt")), *sums)) {
      removeDirectory(directory);
      return {false, {}, QStringLiteral("failed to publish staged checksum file")};
    }
  }

  const QJsonObject manifest{
      {QStringLiteral("version"), plan.releaseVersion},
      {QStringLiteral("installer"), plan.installer->name},
      {QStringLiteral("digest"), plan.installer->digest},
      {QStringLiteral("size"), plan.installer->size},
      {QStringLiteral("releaseUrl"), plan.releaseUrl},
      {QStringLiteral("checksumDigest"), plan.checksums ? plan.checksums->digest : QString()},
      {QStringLiteral("checksumSize"), plan.checksums ? plan.checksums->size : 0},
  };
  const auto manifestPath = QDir(directory).filePath(QStringLiteral("manifest.json"));
  if (!writeFile(manifestPath, QJsonDocument(manifest).toJson(QJsonDocument::Compact))) {
    removeDirectory(directory);
    return {false, {}, QStringLiteral("failed to publish update manifest")};
  }

  const auto staged = find(plan);
  if (!staged) {
    removeDirectory(directory);
    return {false, {}, QStringLiteral("staged update failed final verification")};
  }
  return {true, *staged, {}};
}

bool UpdateCache::discard(const StagedUpdate &staged)
{
  if (!isInsideCache(staged.installerPath) || !isInsideCache(staged.manifestPath)) {
    return false;
  }
  return QDir(QFileInfo(staged.manifestPath).absolutePath()).removeRecursively();
}

QString UpdateCache::stageDirectory(const UpdatePlan &plan) const
{
  return QDir(m_rootPath).filePath(plan.releaseVersion);
}

bool UpdateCache::isInsideCache(const QString &path) const
{
  const auto root = QDir(m_rootPath).canonicalPath();
  const auto canonical = QFileInfo(path).canonicalFilePath();
  return !root.isEmpty() && !canonical.isEmpty() &&
         (canonical == root || canonical.startsWith(root + QLatin1Char('/')));
}

} // namespace deskflow::gui
