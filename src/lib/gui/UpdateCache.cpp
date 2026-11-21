/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateCache.h"

#include "gui/UpdateIntegrity.h"
#include "common/UrlConstants.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimeZone>
#include <QUuid>

#if defined(Q_OS_UNIX)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>

namespace deskflow::gui {

namespace {

constexpr qint64 kMaximumStateBytes = 2LL * 1024 * 1024;
constexpr qint64 kMaximumManifestBytes = 32LL * 1024;
constexpr qint64 kMaximumChecksumBytes = 2LL * 1024 * 1024;
constexpr qint64 kMaximumInstallerBytes = 512LL * 1024 * 1024;
constexpr qint64 kMaximumStageBytes = 768LL * 1024 * 1024;
constexpr qint64 kMaximumStageAgeSeconds = 30LL * 24 * 60 * 60;

struct LoadedStagedUpdate
{
  StagedUpdate staged;
  UpdateAsset installer;
  UpdateAsset checksums;
  QString repository;
  QString releaseUrl;
  QString selectedChecksum;
  QDateTime createdAt;
};

bool isSafeFileName(const QString &name)
{
  static const QRegularExpression pattern(QStringLiteral(R"(^[A-Za-z0-9][A-Za-z0-9._+-]*$)"));
  return !name.isEmpty() && name.size() <= 200 && pattern.match(name).hasMatch() &&
         QFileInfo(name).fileName() == name;
}

bool isSafeVersion(const QString &version)
{
  return normalizeUpdateVersion(version).has_value();
}

bool snapshotMatchesVersion(const QByteArray &snapshot, const QString &version)
{
  const auto normalizedVersion = normalizeUpdateVersion(version);
  const auto parsed = parseGitHubRelease(snapshot);
  return normalizedVersion && parsed.release &&
         parsed.release->version == *normalizedVersion;
}

bool isSafeStageId(const QString &stageId)
{
  static const QRegularExpression pattern(QStringLiteral(R"(^[A-Za-z0-9-]{1,128}$)"));
  return pattern.match(stageId).hasMatch();
}

bool hasSafeFilePermissions(const QFileInfo &info, bool privateFile)
{
  const auto permissions = info.permissions();
  if (permissions.testFlag(QFileDevice::WriteGroup) || permissions.testFlag(QFileDevice::WriteOther)) {
    return false;
  }
#if defined(Q_OS_UNIX)
  if (info.ownerId() != static_cast<quint32>(geteuid())) {
    return false;
  }
#else
  Q_UNUSED(privateFile)
#endif
  if (!privateFile) {
    return true;
  }
  return permissions.testFlag(QFileDevice::ReadOwner) && permissions.testFlag(QFileDevice::WriteOwner);
}

bool isSafeDirectory(const QString &path)
{
  const QFileInfo info(path);
  return !info.isSymLink() && info.isDir() && hasSafeFilePermissions(info, false);
}

QDateTime parseUtcDateTime(const QString &text)
{
  auto time = QDateTime::fromString(text, Qt::ISODateWithMs);
  if (time.isValid()) {
    time.setTimeZone(QTimeZone::UTC);
  }
  return time;
}

bool isRegularSingleLink(const QString &path)
{
  const QFileInfo info(path);
  if (info.isSymLink() || !info.isFile() || !hasSafeFilePermissions(info, true)) {
    return false;
  }
#if defined(Q_OS_UNIX)
  struct stat metadata {};
  return lstat(path.toLocal8Bit().constData(), &metadata) == 0 && S_ISREG(metadata.st_mode) &&
         metadata.st_nlink == 1;
#else
  return true;
#endif
}

std::optional<QByteArray> readBoundedRegularFile(const QString &path, qint64 maximumBytes)
{
  if (!isRegularSingleLink(path)) {
    return std::nullopt;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > maximumBytes) {
    return std::nullopt;
  }
  const auto data = file.read(maximumBytes + 1);
  if (data.size() > maximumBytes) {
    return std::nullopt;
  }
  return data;
}

bool writeFile(const QString &path, const QByteArray &data)
{
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
    return false;
  }
  if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) || !file.commit()) {
    return false;
  }
  return true;
}

bool isCanonicalInside(const QString &rootPath, const QString &path)
{
  const auto root = QFileInfo(rootPath).canonicalFilePath();
  const auto canonical = QFileInfo(path).canonicalFilePath();
  return !root.isEmpty() && !canonical.isEmpty() && canonical.startsWith(root + QLatin1Char('/'));
}

QString stageRootForManifest(const QString &manifestPath)
{
  const auto versionDirectory = QFileInfo(manifestPath).absolutePath();
  const auto stagesDirectory = QFileInfo(versionDirectory).absolutePath();
  if (QFileInfo(stagesDirectory).fileName() != QStringLiteral("stages")) {
    return {};
  }
  return QFileInfo(stagesDirectory).absolutePath();
}

bool ensurePrivateDirectory(const QString &path, QString *error = nullptr)
{
  const auto existing = QFileInfo(path);
  if (existing.exists()) {
    if (existing.isSymLink() || !existing.isDir() || !hasSafeFilePermissions(existing, false)) {
      if (error) {
        *error = QStringLiteral("update cache directory is unsafe");
      }
      return false;
    }
    return true;
  }

  if (!QDir().mkpath(path)) {
    if (error) {
      *error = QStringLiteral("cannot create update cache directory");
    }
    return false;
  }
  if (!QFile::setPermissions(
          path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
      )) {
    if (error) {
      *error = QStringLiteral("cannot restrict update cache permissions");
    }
    return false;
  }
  return true;
}

std::optional<LoadedStagedUpdate> loadManifest(const QString &manifestPath, bool requireStageRoot = true)
{
  if (QFileInfo(manifestPath).fileName() != QStringLiteral("manifest.json")) {
    return std::nullopt;
  }
  const auto rootPath = stageRootForManifest(manifestPath);
  if (requireStageRoot && rootPath.isEmpty()) {
    return std::nullopt;
  }
  const auto versionDirectory = QFileInfo(manifestPath).absolutePath();
  if (!isSafeDirectory(versionDirectory)) {
    return std::nullopt;
  }
  if (requireStageRoot) {
    const auto stagesDirectory = QFileInfo(versionDirectory).absolutePath();
    if (!isSafeDirectory(rootPath) || !isSafeDirectory(stagesDirectory)) {
      return std::nullopt;
    }
  }

  const auto manifestData = readBoundedRegularFile(manifestPath, kMaximumManifestBytes);
  if (!manifestData) {
    return std::nullopt;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(*manifestData, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return std::nullopt;
  }

  const auto object = document.object();
  const auto version = object.value(QStringLiteral("version")).toString();
  const auto tag = object.value(QStringLiteral("tag")).toString();
  const auto stageId = object.value(QStringLiteral("stageId")).toString();
  const auto repository = object.value(QStringLiteral("repository")).toString();
  const auto releaseUrl = object.value(QStringLiteral("releaseUrl")).toString();
  const auto installerName = object.value(QStringLiteral("installerName")).toString();
  const auto installerDigest = object.value(QStringLiteral("installerDigest")).toString();
  const auto installerSize = object.value(QStringLiteral("installerSize")).toInteger();
  const auto checksumName = object.value(QStringLiteral("checksumName")).toString();
  const auto checksumDigest = object.value(QStringLiteral("checksumDigest")).toString();
  const auto checksumSize = object.value(QStringLiteral("checksumSize")).toInteger();
  const auto selectedChecksum = object.value(QStringLiteral("selectedChecksum")).toString();
  const auto createdAt =
      parseUtcDateTime(object.value(QStringLiteral("createdAtUtc")).toString());

  const auto normalizedVersion = normalizeUpdateVersion(version);
  const auto expectedReleaseUrl =
      normalizedVersion
          ? QStringLiteral("https://github.com/subeom/deskflow/releases/tag/v%1")
                .arg(*normalizedVersion)
          : QString();
  if (object.value(QStringLiteral("schemaVersion")).toInteger() != 1 ||
      repository != QStringLiteral("subeom/deskflow") || !normalizedVersion ||
      tag != QStringLiteral("v%1").arg(*normalizedVersion) ||
      releaseUrl != expectedReleaseUrl ||
      !isSafeStageId(stageId) || !isSafeFileName(installerName) ||
      checksumName != QStringLiteral("SHA256SUMS.txt") || !isValidSha256Digest(installerDigest) ||
      !isValidSha256Digest(checksumDigest) || installerSize <= 0 || checksumSize <= 0 ||
      !QRegularExpression(QStringLiteral(R"(^[0-9a-f]{64}$)")).match(selectedChecksum).hasMatch() ||
      !createdAt.isValid() ||
      (requireStageRoot && QFileInfo(versionDirectory).fileName() != version)) {
    return std::nullopt;
  }

  const auto installerPath = QDir(versionDirectory).filePath(installerName);
  const auto checksumPath = QDir(versionDirectory).filePath(checksumName);
  if (requireStageRoot &&
      (!isCanonicalInside(rootPath, manifestPath) || !isCanonicalInside(rootPath, installerPath) ||
       !isCanonicalInside(rootPath, checksumPath))) {
    return std::nullopt;
  }
  const auto installerData = readBoundedRegularFile(installerPath, kMaximumStageBytes);
  const auto checksumData = readBoundedRegularFile(checksumPath, kMaximumChecksumBytes);
  if (!installerData || !checksumData) {
    return std::nullopt;
  }

  const UpdateAsset checksums{
      checksumName,
      QUrl(),
      checksumDigest,
      checksumSize,
  };
  if (!verifyUpdatePayload(checksums, *checksumData).ok) {
    return std::nullopt;
  }
  const UpdateAsset installer{
      installerName,
      QUrl(),
      installerDigest,
      installerSize,
  };
  if (!verifyUpdatePayload(installer, *installerData, *checksumData).ok ||
      selectedChecksum != installerDigest.mid(7).toLower()) {
    return std::nullopt;
  }

  return LoadedStagedUpdate{
      {version, installerName, QFileInfo(installerPath).absoluteFilePath(), QFileInfo(manifestPath).absoluteFilePath()},
      installer,
      checksums,
      repository,
      releaseUrl,
      selectedChecksum,
      createdAt,
  };
}

bool removePathInside(const QString &rootPath, const QString &path)
{
  if (!isCanonicalInside(rootPath, path)) {
    return false;
  }
  const QFileInfo info(path);
  if (info.isSymLink() || !info.isDir()) {
    return QFile::remove(path);
  }
  return QDir(path).removeRecursively();
}

qint64 stageBytes(const QString &path)
{
  qint64 total = 0;
  QDirIterator iterator(
      path, QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks,
      QDirIterator::Subdirectories
  );
  while (iterator.hasNext()) {
    iterator.next();
    total += iterator.fileInfo().size();
  }
  return total;
}

} // namespace

UpdateCache::UpdateCache(QString rootPath)
    : m_rootPath(QDir::cleanPath(QFileInfo(std::move(rootPath)).absoluteFilePath()))
{
}

std::optional<StagedUpdate> UpdateCache::find(const UpdatePlan &plan) const
{
  if (!plan.installer || !plan.checksums || !isSafeVersion(plan.releaseVersion) ||
      !isSafeFileName(plan.installer->name)) {
    return std::nullopt;
  }
  const auto manifestPath = QDir(m_rootPath).filePath(
      QStringLiteral("stages/%1/manifest.json").arg(plan.releaseVersion)
  );
  if (!isCanonicalInside(m_rootPath, manifestPath)) {
    return std::nullopt;
  }

  const auto loaded = loadManifest(manifestPath);
  if (!loaded || loaded->staged.version != plan.releaseVersion ||
      loaded->staged.installerName != plan.installer->name ||
      loaded->installer.digest != plan.installer->digest ||
      loaded->installer.size != plan.installer->size ||
      loaded->checksums.digest != plan.checksums->digest ||
      loaded->checksums.size != plan.checksums->size ||
      loaded->releaseUrl != plan.releaseUrl.toEncoded(QUrl::FullyEncoded)) {
    return std::nullopt;
  }
  return loaded->staged;
}

std::optional<StagedUpdate> UpdateCache::load(const QString &manifestPath)
{
  const auto loaded = loadManifest(manifestPath);
  return loaded ? std::optional<StagedUpdate>{loaded->staged} : std::nullopt;
}

UpdateStageResult UpdateCache::stage(
    const UpdatePlan &plan, const QByteArray &installer,
    const std::optional<QByteArray> &checksums
)
{
  const auto normalizedVersion = normalizeUpdateVersion(plan.releaseVersion);
  const auto expectedReleaseUrl =
      normalizedVersion
          ? QStringLiteral("https://github.com/subeom/deskflow/releases/tag/v%1")
                .arg(*normalizedVersion)
          : QString();
  if (!plan.installer || !plan.checksums || !checksums || !normalizedVersion ||
      plan.mode != UpdateInstallMode::AutomaticMac || !isSafeVersion(plan.releaseVersion) ||
      plan.tag != QStringLiteral("v%1").arg(*normalizedVersion) ||
      plan.releaseUrl.toEncoded(QUrl::FullyEncoded) != expectedReleaseUrl.toUtf8() ||
      !isSafeFileName(plan.installer->name) || plan.checksums->name != QStringLiteral("SHA256SUMS.txt")) {
    return {false, {}, QStringLiteral("invalid update plan")};
  }
  if (plan.installer->size <= 0 || plan.installer->size > kMaximumInstallerBytes ||
      plan.checksums->size <= 0 || plan.checksums->size > kMaximumChecksumBytes) {
    return {false, {}, QStringLiteral("update asset exceeds the cache limit")};
  }

  const auto checksumVerification =
      verifyUpdatePayload(*plan.checksums, *checksums, std::nullopt);
  if (!checksumVerification.ok) {
    return {false, {}, checksumVerification.error};
  }
  const auto verification = verifyUpdatePayload(*plan.installer, installer, *checksums);
  if (!verification.ok) {
    return {false, {}, verification.error};
  }

  QString error;
  if (!ensurePrivateDirectory(m_rootPath, &error)) {
    return {false, {}, error};
  }
  const auto stagesPath = QDir(m_rootPath).filePath(QStringLiteral("stages"));
  if (!ensurePrivateDirectory(stagesPath, &error)) {
    return {false, {}, error};
  }

  if (const auto existing = find(plan)) {
    return {true, *existing, {}};
  }
  const auto finalDirectory = QDir(stagesPath).filePath(plan.releaseVersion);
  if (QFileInfo::exists(finalDirectory)) {
    return {false, {}, QStringLiteral("invalid existing update stage requires cleanup")};
  }

  const auto stageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const auto incomingDirectory =
      QDir(m_rootPath).filePath(QStringLiteral(".incoming-%1").arg(stageId));
  if (!ensurePrivateDirectory(incomingDirectory, &error)) {
    return {false, {}, error};
  }

  const auto fail = [&](const QString &message) {
    QDir(incomingDirectory).removeRecursively();
    return UpdateStageResult{false, {}, message};
  };
  const auto installerPath = QDir(incomingDirectory).filePath(plan.installer->name);
  if (!writeFile(installerPath, installer)) {
    return fail(QStringLiteral("failed to publish staged installer"));
  }
  if (!writeFile(QDir(incomingDirectory).filePath(QStringLiteral("SHA256SUMS.txt")), *checksums)) {
    return fail(QStringLiteral("failed to publish staged checksum manifest"));
  }

  const auto parsedChecksum = parseChecksumManifest(*checksums, plan.installer->name);
  if (!parsedChecksum.checksum) {
    return fail(parsedChecksum.error);
  }
  const QJsonObject manifest{
      {QStringLiteral("schemaVersion"), 1},
      {QStringLiteral("stageId"), stageId},
      {QStringLiteral("repository"), QStringLiteral("subeom/deskflow")},
      {QStringLiteral("version"), plan.releaseVersion},
      {QStringLiteral("tag"), plan.tag},
      {QStringLiteral("releaseUrl"), QString::fromUtf8(plan.releaseUrl.toEncoded(QUrl::FullyEncoded))},
      {QStringLiteral("installerName"), plan.installer->name},
      {QStringLiteral("installerDigest"), plan.installer->digest},
      {QStringLiteral("installerSize"), plan.installer->size},
      {QStringLiteral("checksumName"), QStringLiteral("SHA256SUMS.txt")},
      {QStringLiteral("checksumDigest"), plan.checksums->digest},
      {QStringLiteral("checksumSize"), plan.checksums->size},
      {QStringLiteral("selectedChecksum"), *parsedChecksum.checksum},
      {QStringLiteral("createdAtUtc"),
       QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
  };
  const auto incomingManifest =
      QDir(incomingDirectory).filePath(QStringLiteral("manifest.json"));
  if (!writeFile(
          incomingManifest, QJsonDocument(manifest).toJson(QJsonDocument::Compact)
      )) {
    return fail(QStringLiteral("failed to publish staged update manifest"));
  }
  if (!loadManifest(incomingManifest, false)) {
    return fail(QStringLiteral("incoming update stage failed verification"));
  }
  if (!QDir().rename(incomingDirectory, finalDirectory)) {
    return fail(QStringLiteral("failed to publish verified update stage"));
  }

  const auto published = find(plan);
  if (!published) {
    QDir(finalDirectory).removeRecursively();
    return {false, {}, QStringLiteral("published update stage failed final verification")};
  }
  return {true, *published, {}};
}

bool UpdateCache::discard(const StagedUpdate &staged)
{
  const auto loaded = load(staged.manifestPath);
  if (!loaded || loaded->installerPath != staged.installerPath ||
      loaded->manifestPath != staged.manifestPath || loaded->version != staged.version ||
      !isCanonicalInside(m_rootPath, staged.manifestPath)) {
    return false;
  }
  const auto stageDirectory = QFileInfo(staged.manifestPath).absolutePath();
  return removePathInside(m_rootPath, stageDirectory);
}

std::optional<UpdateCacheState> UpdateCache::loadState() const
{
  if (!isSafeDirectory(m_rootPath)) {
    return std::nullopt;
  }
  const auto path = QDir(m_rootPath).filePath(QStringLiteral("state.json"));
  if (!QFileInfo::exists(path)) {
    return std::nullopt;
  }
  const auto data = readBoundedRegularFile(path, kMaximumStateBytes);
  if (!data) {
    return std::nullopt;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(*data, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return std::nullopt;
  }
  const auto object = document.object();
  const auto endpoint = object.value(QStringLiteral("endpoint")).toString();
  const auto etag = object.value(QStringLiteral("etag")).toString();
  const auto snapshotText = object.value(QStringLiteral("sanitizedSnapshot")).toString();
  const auto snapshotVersion = object.value(QStringLiteral("snapshotVersion")).toString();
  const auto retryAt = parseUtcDateTime(object.value(QStringLiteral("retryAt")).toString());
  const auto lastCheck =
      parseUtcDateTime(object.value(QStringLiteral("lastSuccessfulCheck")).toString());
  const auto hasSnapshot = !snapshotText.isEmpty() || !snapshotVersion.isEmpty();
  if (object.value(QStringLiteral("schemaVersion")).toInteger() != 1 ||
      endpoint != kUrlUpdateCheck || etag.size() > 1024 ||
      snapshotText.toUtf8().size() > kMaximumStateBytes ||
      (hasSnapshot && (snapshotText.isEmpty() || snapshotVersion.isEmpty() ||
                       !isSafeVersion(snapshotVersion) ||
                       !snapshotMatchesVersion(snapshotText.toUtf8(), snapshotVersion))) ||
      !retryAt.isValid() || !lastCheck.isValid()) {
    return std::nullopt;
  }
  return UpdateCacheState{
      1,
      endpoint,
      etag,
      snapshotText.toUtf8(),
      snapshotVersion,
      retryAt,
      lastCheck,
  };
}

bool UpdateCache::saveState(const UpdateCacheState &state)
{
  QString error;
  if (!ensurePrivateDirectory(m_rootPath, &error) || state.schemaVersion != 1 ||
      state.endpoint != kUrlUpdateCheck || state.etag.size() > 1024 ||
      state.sanitizedSnapshot.size() > kMaximumStateBytes ||
      ((!state.sanitizedSnapshot.isEmpty() || !state.snapshotVersion.isEmpty()) &&
       (state.sanitizedSnapshot.isEmpty() || state.snapshotVersion.isEmpty() ||
        !isSafeVersion(state.snapshotVersion) ||
        !snapshotMatchesVersion(state.sanitizedSnapshot, state.snapshotVersion))) ||
      !state.retryAt.isValid() || !state.lastSuccessfulCheck.isValid()) {
    return false;
  }
  const auto path = QDir(m_rootPath).filePath(QStringLiteral("state.json"));
  if (QFileInfo::exists(path) && !isRegularSingleLink(path)) {
    return false;
  }
  const QJsonObject object{
      {QStringLiteral("schemaVersion"), state.schemaVersion},
      {QStringLiteral("endpoint"), state.endpoint},
      {QStringLiteral("etag"), state.etag},
      {QStringLiteral("sanitizedSnapshot"), QString::fromUtf8(state.sanitizedSnapshot)},
      {QStringLiteral("snapshotVersion"), state.snapshotVersion},
      {QStringLiteral("retryAt"), state.retryAt.toUTC().toString(Qt::ISODateWithMs)},
      {QStringLiteral("lastSuccessfulCheck"),
       state.lastSuccessfulCheck.toUTC().toString(Qt::ISODateWithMs)},
  };
  return writeFile(path, QJsonDocument(object).toJson(QJsonDocument::Compact));
}

bool UpdateCache::cleanup(const QString &currentVersion)
{
  QString error;
  if (!isSafeVersion(currentVersion) || !ensurePrivateDirectory(m_rootPath, &error)) {
    return false;
  }

  QDir root(m_rootPath);
  for (const auto &entry :
       root.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
    if (!entry.fileName().startsWith(QStringLiteral(".incoming-"))) {
      continue;
    }
    if (!removePathInside(m_rootPath, entry.absoluteFilePath())) {
      return false;
    }
  }

  const auto stagesPath = QDir(m_rootPath).filePath(QStringLiteral("stages"));
  if (!ensurePrivateDirectory(stagesPath, &error)) {
    return false;
  }
  struct ValidStage
  {
    QString path;
    LoadedStagedUpdate loaded;
  };
  QList<ValidStage> validStages;
  const auto now = QDateTime::currentDateTimeUtc();
  for (const auto &entry :
       QDir(stagesPath).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
    const auto path = entry.absoluteFilePath();
    auto loaded = loadManifest(QDir(path).filePath(QStringLiteral("manifest.json")));
    if (entry.isSymLink() || !entry.isDir() || !loaded ||
        !isNewerUpdateVersion(currentVersion, loaded->staged.version) ||
        loaded->createdAt.secsTo(now) > kMaximumStageAgeSeconds) {
      if (!removePathInside(m_rootPath, path)) {
        return false;
      }
      continue;
    }
    validStages.append({path, *loaded});
  }

  std::sort(validStages.begin(), validStages.end(), [](const ValidStage &left, const ValidStage &right) {
    return isNewerUpdateVersion(right.loaded.staged.version, left.loaded.staged.version);
  });
  for (qsizetype index = 1; index < validStages.size(); ++index) {
    if (!removePathInside(m_rootPath, validStages.at(index).path)) {
      return false;
    }
  }
  if (!validStages.isEmpty() && stageBytes(validStages.first().path) > kMaximumStageBytes) {
    return removePathInside(m_rootPath, validStages.first().path);
  }
  return true;
}

} // namespace deskflow::gui
