/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateCache.h"

#include "gui/UpdateIntegrity.h"

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

bool safeFileName(const QString &name)
{
  return !name.isEmpty() && QFileInfo(name).fileName() == name && !name.contains(QLatin1Char('/')) &&
         !name.contains(QLatin1Char('\\')) && name != QStringLiteral(".") && name != QStringLiteral("..");
}

std::optional<QByteArray> existingSums(const QString &path)
{
  if (!QFileInfo::exists(path)) {
    return std::nullopt;
  }
  return readFile(path);
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
  const auto manifestData = readFile(manifestPath);
  if (!manifestData) {
    return std::nullopt;
  }

  QJsonParseError parseError;
  const auto manifest = QJsonDocument::fromJson(*manifestData, &parseError);
  if (parseError.error != QJsonParseError::NoError || !manifest.isObject()) {
    return std::nullopt;
  }
  const auto object = manifest.object();
  if (object.value(QStringLiteral("version")).toString() != plan.releaseVersion ||
      object.value(QStringLiteral("installer")).toString() != plan.installer->name ||
      object.value(QStringLiteral("digest")).toString() != plan.installer->digest ||
      object.value(QStringLiteral("size")).toInteger() != plan.installer->size ||
      object.value(QStringLiteral("releaseUrl")).toString() != plan.releaseUrl) {
    return std::nullopt;
  }

  const auto installerPath = QDir(directory).filePath(plan.installer->name);
  const auto installerData = readFile(installerPath);
  if (!installerData) {
    return std::nullopt;
  }
  const auto sums = existingSums(QDir(directory).filePath(QStringLiteral("sums.txt")));
  if (!verifyUpdatePayload(*plan.installer, *installerData, sums).ok) {
    return std::nullopt;
  }

  return StagedUpdate{plan.releaseVersion, plan.installer->name, installerPath, manifestPath};
}

UpdateStageResult
UpdateCache::stage(const UpdatePlan &plan, const QByteArray &installer, const std::optional<QByteArray> &sums)
{
  if (!plan.installer || !safeFileName(plan.installer->name)) {
    return {false, {}, QStringLiteral("invalid update asset name")};
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
    existingDirectory.removeRecursively();
  }
  if (!root.mkpath(directory)) {
    return {false, {}, QStringLiteral("cannot create update stage directory")};
  }

  const auto installerPath = QDir(directory).filePath(plan.installer->name);
  if (!writeFile(installerPath, installer)) {
    QDir(directory).removeRecursively();
    return {false, {}, QStringLiteral("failed to publish staged installer")};
  }

  if (sums) {
    if (!writeFile(QDir(directory).filePath(QStringLiteral("sums.txt")), *sums)) {
      QDir(directory).removeRecursively();
      return {false, {}, QStringLiteral("failed to publish staged checksum file")};
    }
  }

  const QJsonObject manifest{
      {QStringLiteral("version"), plan.releaseVersion},   {QStringLiteral("installer"), plan.installer->name},
      {QStringLiteral("digest"), plan.installer->digest}, {QStringLiteral("size"), plan.installer->size},
      {QStringLiteral("releaseUrl"), plan.releaseUrl},
  };
  const auto manifestPath = QDir(directory).filePath(QStringLiteral("manifest.json"));
  if (!writeFile(manifestPath, QJsonDocument(manifest).toJson(QJsonDocument::Compact))) {
    QDir(directory).removeRecursively();
    return {false, {}, QStringLiteral("failed to publish update manifest")};
  }

  const auto staged = find(plan);
  if (!staged) {
    QDir(directory).removeRecursively();
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
