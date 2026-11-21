/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateCacheTests.h"

#include "gui/UpdateCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>

#include <optional>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

using deskflow::gui::UpdateCache;
using deskflow::gui::UpdateCacheState;
using deskflow::gui::UpdateInstallMode;
using deskflow::gui::UpdatePlan;

namespace {

QString digest(const QByteArray &data)
{
  return QStringLiteral("sha256:%1").arg(
      QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex())
  );
}

QByteArray installerPayload()
{
  return QByteArrayLiteral("synthetic signed DMG bytes");
}

QString installerName(const QString &version)
{
  return QStringLiteral("deskflow-%1-macos-arm64.dmg").arg(version);
}

QByteArray checksumPayload(
    const QByteArray &installer, const QString &version = QStringLiteral("2.0.6")
)
{
  return QStringLiteral("%1  %2\n").arg(digest(installer).mid(7), installerName(version)).toUtf8();
}

UpdatePlan planForVersion(
    const QString &version, const QByteArray &installer = installerPayload()
)
{
  const auto tag = QStringLiteral("v%1").arg(version);
  const auto sums = checksumPayload(installer, version);
  return {
      version,
      tag,
      QUrl(QStringLiteral("https://github.com/subeom/deskflow/releases/tag/%1").arg(tag)),
      UpdateInstallMode::AutomaticMac,
      deskflow::gui::UpdateAsset{
          installerName(version),
          QUrl(
              QStringLiteral("https://github.com/subeom/deskflow/releases/download/%1/%2")
                  .arg(tag, installerName(version))
          ),
          digest(installer),
          installer.size()
      },
      deskflow::gui::UpdateAsset{
          QStringLiteral("SHA256SUMS.txt"),
          QUrl(
              QStringLiteral(
                  "https://github.com/subeom/deskflow/releases/download/%1/SHA256SUMS.txt"
              ).arg(tag)
          ),
          digest(sums),
          sums.size()
      }
  };
}

UpdatePlan plan(const QByteArray &installer = installerPayload())
{
  return planForVersion(QStringLiteral("2.0.6"), installer);
}

QByteArray releaseSnapshot(const QString &version)
{
  const auto selectedPlan = planForVersion(version);
  QJsonArray assets;
  for (const auto &asset :
       {selectedPlan.installer.value(), selectedPlan.checksums.value()}) {
    assets.append(QJsonObject{
        {QStringLiteral("name"), asset.name},
        {QStringLiteral("browser_download_url"),
         QString::fromUtf8(asset.downloadUrl.toEncoded(QUrl::FullyEncoded))},
        {QStringLiteral("size"), asset.size},
        {QStringLiteral("digest"), asset.digest},
    });
  }
  return QJsonDocument(QJsonObject{
      {QStringLiteral("draft"), false},
      {QStringLiteral("prerelease"), false},
      {QStringLiteral("tag_name"), selectedPlan.tag},
      {QStringLiteral("html_url"),
       QString::fromUtf8(selectedPlan.releaseUrl.toEncoded(QUrl::FullyEncoded))},
      {QStringLiteral("assets"), assets},
  }).toJson(QJsonDocument::Compact);
}

std::optional<deskflow::gui::StagedUpdate>
stageVersion(UpdateCache &cache, const QString &version, const QByteArray &installer = installerPayload())
{
  const auto result = cache.stage(
      planForVersion(version, installer), installer, checksumPayload(installer, version)
  );
  if (!result.ok) {
    return std::nullopt;
  }
  return result.staged;
}

std::optional<deskflow::gui::StagedUpdate>
stage(UpdateCache &cache, const QByteArray &installer = installerPayload())
{
  return stageVersion(cache, QStringLiteral("2.0.6"), installer);
}

bool writeFile(const QString &path, const QByteArray &contents)
{
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}

} // namespace

void UpdateCacheTests::stage_publishesAtomicallyAndReloadReverifies()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  UpdateCache cache(QDir(temporary.path()).filePath(QStringLiteral("updates")));
  const auto selectedPlan = plan();
  const auto result = cache.stage(selectedPlan, installerPayload(), checksumPayload(installerPayload()));
  QVERIFY2(result.ok, qPrintable(result.error));
  QVERIFY(QFileInfo(result.staged.manifestPath).isFile());
  QVERIFY(QFileInfo(result.staged.installerPath).isFile());
  QVERIFY(!QFileInfo(result.staged.manifestPath).isSymLink());
  QVERIFY(!QFileInfo(result.staged.installerPath).isSymLink());

  const auto loaded = UpdateCache::load(result.staged.manifestPath);
  QVERIFY(loaded);
  QCOMPARE(loaded->version, QStringLiteral("2.0.6"));
  const auto found = cache.find(selectedPlan);
  QVERIFY(found);
  QCOMPARE(found->installerPath, result.staged.installerPath);
}

void UpdateCacheTests::stage_rejectsCorruptOrUnexpectedPayload()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  UpdateCache cache(QDir(temporary.path()).filePath(QStringLiteral("updates")));
  auto changed = installerPayload();
  changed[0] ^= 1;
  QVERIFY(!cache.stage(plan(), changed, checksumPayload(installerPayload())).ok);
  QVERIFY(!cache.stage(plan(), installerPayload(), QByteArrayLiteral("unexpected")).ok);
  QVERIFY(!cache.stage(plan(), installerPayload(), std::nullopt).ok);
}

void UpdateCacheTests::find_rejectsWrongPlanManifestDigestSizeVersionAndRepository()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  UpdateCache cache(QDir(temporary.path()).filePath(QStringLiteral("updates")));
  const auto staged = stage(cache);
  QVERIFY(staged);

  auto wrongDigest = plan();
  wrongDigest.installer->digest =
      QStringLiteral("sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  QVERIFY(!cache.find(wrongDigest));

  auto wrongSize = plan();
  wrongSize.installer->size++;
  QVERIFY(!cache.find(wrongSize));

  auto wrongVersion = plan();
  wrongVersion.releaseVersion = QStringLiteral("2.0.7");
  QVERIFY(!cache.find(wrongVersion));

  QFile manifest(staged->manifestPath);
  QVERIFY(manifest.open(QIODevice::ReadWrite));
  auto data = manifest.readAll();
  data.replace(QByteArrayLiteral("subeom/deskflow"), QByteArrayLiteral("deskflow/deskflow"));
  manifest.seek(0);
  QCOMPARE(manifest.write(data), data.size());
  QVERIFY(manifest.resize(data.size()));
  manifest.close();
  QVERIFY(!UpdateCache::load(staged->manifestPath));
}

void UpdateCacheTests::load_rejectsSymlinkRootStageManifestInstallerAndChecksum()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto realRoot = QDir(temporary.path()).filePath(QStringLiteral("real-updates"));
  UpdateCache realCache(realRoot);
  QVERIFY(stage(realCache));
  const auto linkRoot = QDir(temporary.path()).filePath(QStringLiteral("linked-updates"));
  QVERIFY(QFile::link(realRoot, linkRoot));
  UpdateCache linkedCache(linkRoot);
  QVERIFY(!stage(linkedCache));

  const auto staged = stage(realCache);
  QVERIFY(staged);
  const auto checksumsPath =
      QDir(QFileInfo(staged->manifestPath).absolutePath()).filePath(QStringLiteral("SHA256SUMS.txt"));

  for (const QString &path : {staged->manifestPath, staged->installerPath, checksumsPath}) {
    const auto outside = QDir(temporary.path()).filePath(QStringLiteral("outside-%1").arg(QFileInfo(path).fileName()));
    QVERIFY(QFile::rename(path, outside));
    QVERIFY(QFile::link(outside, path));
    QVERIFY(!UpdateCache::load(staged->manifestPath));
    QVERIFY(QFile::remove(path));
    QVERIFY(QFile::rename(outside, path));
  }
}

void UpdateCacheTests::load_rejectsEscapedPathsHardlinksAndUnsafeModes()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  UpdateCache cache(QDir(temporary.path()).filePath(QStringLiteral("updates")));
  const auto staged = stage(cache);
  QVERIFY(staged);

#if defined(Q_OS_UNIX)
  const auto hardlinkSource =
      QDir(temporary.path()).filePath(QStringLiteral("installer-hardlink-source"));
  QVERIFY(QFile::rename(staged->installerPath, hardlinkSource));
  QVERIFY(::link(
              hardlinkSource.toLocal8Bit().constData(),
              staged->installerPath.toLocal8Bit().constData()
          ) == 0);
  QVERIFY(!UpdateCache::load(staged->manifestPath));
  QVERIFY(QFile::remove(staged->installerPath));
  QVERIFY(QFile::rename(hardlinkSource, staged->installerPath));
#endif

  QVERIFY(QFile::setPermissions(
      staged->installerPath,
      QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteOther
  ));
  QVERIFY(!UpdateCache::load(staged->manifestPath));
  QVERIFY(QFile::setPermissions(staged->installerPath, QFile::ReadOwner | QFile::WriteOwner));

  deskflow::gui::StagedUpdate escaped{
      staged->version, staged->installerName, QDir(temporary.path()).filePath(QStringLiteral("outside.dmg")),
      staged->manifestPath
  };
  QVERIFY(!cache.discard(escaped));
}

void UpdateCacheTests::discard_removesOnlyAValidatedStageChild()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto root = QDir(temporary.path()).filePath(QStringLiteral("updates"));
  UpdateCache cache(root);
  const auto staged = stage(cache);
  QVERIFY(staged);
  QVERIFY(cache.discard(*staged));
  QVERIFY(!QFileInfo::exists(staged->installerPath));
  QVERIFY(QDir(root).exists());
}

void UpdateCacheTests::state_roundTripsEtagSnapshotAndRateLimitWithoutRawBody()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  UpdateCache cache(QDir(temporary.path()).filePath(QStringLiteral("updates")));
  const UpdateCacheState state{
      1,
      QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest"),
      QStringLiteral("\"release-etag\""),
      releaseSnapshot(QStringLiteral("2.0.6")),
      QStringLiteral("2.0.6.0"),
      QDateTime::fromSecsSinceEpoch(2'000'000'000, QTimeZone::UTC),
      QDateTime::fromSecsSinceEpoch(1'900'000'000, QTimeZone::UTC)
  };
  QVERIFY(cache.saveState(state));
  const auto loaded = cache.loadState();
  QVERIFY(loaded);
  QCOMPARE(loaded->etag, state.etag);
  QCOMPARE(loaded->sanitizedSnapshot, state.sanitizedSnapshot);
  QCOMPARE(loaded->snapshotVersion, state.snapshotVersion);
  QCOMPARE(loaded->retryAt, state.retryAt);
  QCOMPARE(loaded->lastSuccessfulCheck, state.lastSuccessfulCheck);

  auto mismatched = state;
  mismatched.snapshotVersion = QStringLiteral("2.0.7");
  QVERIFY(!cache.saveState(mismatched));
}

void UpdateCacheTests::cleanup_removesExpiredInvalidIncomingAndOlderStages()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto root = QDir(temporary.path()).filePath(QStringLiteral("updates"));
  UpdateCache cache(root);
  const auto older = stage(cache);
  const auto newest = stageVersion(cache, QStringLiteral("2.0.7"));
  QVERIFY(older);
  QVERIFY(newest);

  QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral(".incoming-dead"))));
  QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral("stages/invalid"))));
  QVERIFY(writeFile(
      QDir(root).filePath(QStringLiteral(".incoming-dead/partial")), QByteArrayLiteral("partial")
  ));
  QVERIFY(cache.cleanup(QStringLiteral("2.0.5")));
  QVERIFY(!QDir(QDir(root).filePath(QStringLiteral(".incoming-dead"))).exists());
  QVERIFY(!QDir(QDir(root).filePath(QStringLiteral("stages/invalid"))).exists());
  QVERIFY(!UpdateCache::load(older->manifestPath));
  QVERIFY(UpdateCache::load(newest->manifestPath));

  QVERIFY(cache.cleanup(QStringLiteral("2.0.7.0")));
  QVERIFY(!UpdateCache::load(newest->manifestPath));
}

QTEST_MAIN(UpdateCacheTests)
