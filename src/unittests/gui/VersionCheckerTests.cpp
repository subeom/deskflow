/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "VersionCheckerTests.h"

#include "gui/UpdateCache.h"
#include "gui/UpdateRelease.h"
#include "gui/UpdateVerifier.h"
#include "gui/VersionChecker.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>

using deskflow::gui::IUpdateNetwork;
using deskflow::gui::IUpdateVerifier;
using deskflow::gui::UpdateCache;
using deskflow::gui::UpdateCacheState;
using deskflow::gui::UpdateEnvironment;
using deskflow::gui::UpdateNetworkCallback;
using deskflow::gui::UpdateNetworkOptions;
using deskflow::gui::UpdateNetworkResult;
using deskflow::gui::UpdateNetworkStatus;
using deskflow::gui::UpdateVerifierResult;

namespace {

constexpr auto kInstallerName = "deskflow-2.0.6-macos-arm64.dmg";
constexpr auto kRunningVersion = "2.0.5";

QString digest(const QByteArray &payload)
{
  return QStringLiteral("sha256:%1").arg(
      QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex())
  );
}

QByteArray installerPayload()
{
  return QByteArrayLiteral("synthetic verified update");
}

QByteArray checksumPayload(const QByteArray &installer = installerPayload())
{
  return QStringLiteral("%1  %2\n").arg(digest(installer).mid(7), QString::fromLatin1(kInstallerName)).toUtf8();
}

QByteArray releaseJson(const QString &version = QStringLiteral("2.0.6"), bool includeArm = true)
{
  const auto tag = QStringLiteral("v%1").arg(version);
  QJsonArray assets;
  if (includeArm) {
    assets.append(QJsonObject{
        {"name", QString::fromLatin1(kInstallerName)},
        {"browser_download_url",
         QStringLiteral("https://github.com/subeom/deskflow/releases/download/%1/%2")
             .arg(tag, QString::fromLatin1(kInstallerName))},
        {"size", installerPayload().size()},
        {"digest", digest(installerPayload())},
    });
  }
  const auto sums = checksumPayload();
  assets.append(QJsonObject{
      {"name", QStringLiteral("SHA256SUMS.txt")},
      {"browser_download_url",
       QStringLiteral("https://github.com/subeom/deskflow/releases/download/%1/SHA256SUMS.txt").arg(tag)},
      {"size", sums.size()},
      {"digest", digest(sums)},
  });
  return QJsonDocument(QJsonObject{
      {"draft", false},
      {"prerelease", false},
      {"tag_name", tag},
      {"html_url", QStringLiteral("https://github.com/subeom/deskflow/releases/tag/%1").arg(tag)},
      {"assets", assets},
  })
      .toJson(QJsonDocument::Compact);
}

struct FakeCall
{
  quint64 id = 0;
  QUrl url;
  UpdateNetworkOptions options;
  UpdateNetworkCallback callback;
};

class FakeUpdateNetwork final : public IUpdateNetwork
{
public:
  quint64 get(
      const QUrl &url, const UpdateNetworkOptions &options, UpdateNetworkCallback callback
  ) override
  {
    const auto id = nextId++;
    calls.append({id, url, options, std::move(callback)});
    return id;
  }

  void cancel(quint64 id) override
  {
    cancelled.append(id);
  }

  void cancelAll() override
  {
    const auto ids = cancelled;
    Q_UNUSED(ids)
    for (const auto &call : std::as_const(calls)) {
      cancelled.append(call.id);
    }
  }

  void complete(qsizetype index, const UpdateNetworkResult &result)
  {
    calls.at(index).callback(result);
  }

  QList<FakeCall> calls;
  QList<quint64> cancelled;
  quint64 nextId = 1;
};

class FakeUpdateVerifier final : public IUpdateVerifier
{
public:
  explicit FakeUpdateVerifier(bool accepted) : result{accepted, accepted ? QString() : QStringLiteral("wrong signature")}
  {
  }

  UpdateVerifierResult verify(const deskflow::gui::StagedUpdate &) override
  {
    ++calls;
    return result;
  }

  UpdateVerifierResult result;
  int calls = 0;
};

UpdateEnvironment directMac(const QString &architecture = QStringLiteral("arm64"))
{
  return {QStringLiteral("macos"), architecture, false, true};
}

UpdateNetworkResult success(const QByteArray &data, const QString &etag = QString())
{
  return {
      UpdateNetworkStatus::Success,
      200,
      data,
      QUrl(),
      {},
      etag,
      0
  };
}

} // namespace

void VersionCheckerTests::checkLatest_deduplicatesInFlightAndUsesPinnedEndpoint()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  checker.checkLatest();
  checker.checkLatest();
  QCOMPARE(fakeNetwork->calls.size(), 1);
  QCOMPARE(
      fakeNetwork->calls.first().url,
      QUrl(QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest"))
  );
  QVERIFY(checker.isChecking());
  fakeNetwork->complete(0, success(releaseJson()));
  QCOMPARE(fakeNetwork->calls.size(), 2);
}

void VersionCheckerTests::checkLatest_upToDateUsesOnlyBoundedCachedSnapshotOn304()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto root = QDir(temporary.path()).filePath(QStringLiteral("updates"));
  UpdateCache cache(root);
  UpdateCacheState state{
      1,
      QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest"),
      QStringLiteral("\"stable-etag\""),
      releaseJson(QStringLiteral("2.0.5")),
      QStringLiteral("2.0.5"),
      QDateTime::fromSecsSinceEpoch(1'000, QTimeZone::UTC),
      QDateTime::currentDateTimeUtc()
  };
  QVERIFY(cache.saveState(state));

  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion), root,
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  checker.checkLatest();
  QCOMPARE(fakeNetwork->calls.first().options.etag, state.etag);
  fakeNetwork->complete(
      0,
      {UpdateNetworkStatus::NotModified, 304, {}, QUrl(), {}, state.etag, 0}
  );
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::UpToDate);
  QCOMPARE(fakeNetwork->calls.size(), 1);
}

void VersionCheckerTests::checkLatest_futureRateLimitUsesValidSnapshotWithoutRequest()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto root = QDir(temporary.path()).filePath(QStringLiteral("updates"));
  const auto retryAt = QDateTime::currentDateTimeUtc().addSecs(60);
  UpdateCacheState state{
      1,
      QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest"),
      QStringLiteral("\"stable-etag\""),
      releaseJson(QString::fromLatin1(kRunningVersion)),
      QString::fromLatin1(kRunningVersion),
      retryAt,
      QDateTime::currentDateTimeUtc()
  };
  QVERIFY(UpdateCache(root).saveState(state));

  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion), root,
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  QSignalSpy updateErrors(&checker, &VersionChecker::updateError);

  checker.checkLatest();

  QVERIFY(fakeNetwork->calls.isEmpty());
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::UpToDate);
  QVERIFY(checker.status().userError.isEmpty());
  QVERIFY(!checker.isChecking());
  QCOMPARE(updateErrors.size(), 0);
  const auto persisted = UpdateCache(root).loadState();
  QVERIFY(persisted);
  QCOMPARE(persisted->retryAt, retryAt);
}

void VersionCheckerTests::checkLatest_rateLimitUsesValidSnapshotWithoutError()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto root = QDir(temporary.path()).filePath(QStringLiteral("updates"));
  UpdateCacheState state{
      1,
      QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest"),
      QStringLiteral("\"stable-etag\""),
      releaseJson(QString::fromLatin1(kRunningVersion)),
      QString::fromLatin1(kRunningVersion),
      QDateTime::fromSecsSinceEpoch(1'000, QTimeZone::UTC),
      QDateTime::currentDateTimeUtc()
  };
  QVERIFY(UpdateCache(root).saveState(state));

  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion), root,
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  QSignalSpy updateErrors(&checker, &VersionChecker::updateError);
  checker.checkLatest();
  fakeNetwork->complete(
      0,
      {UpdateNetworkStatus::RateLimited, 429, {}, QUrl(), QStringLiteral("rate limited"), {}, 1}
  );
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::UpToDate);
  QVERIFY(checker.status().userError.isEmpty());
  QVERIFY(!checker.isChecking());
  QCOMPARE(updateErrors.size(), 0);
  const auto persisted = UpdateCache(root).loadState();
  QVERIFY(persisted);
  QVERIFY(persisted->retryAt > QDateTime::currentDateTimeUtc());
  QCOMPARE(persisted->sanitizedSnapshot, state.sanitizedSnapshot);
  QCOMPARE(persisted->snapshotVersion, state.snapshotVersion);

  checker.checkLatest(UpdateCheckReason::Manual);
  QCOMPARE(fakeNetwork->calls.size(), 1);
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::UpToDate);
}

void VersionCheckerTests::checkLatest_rateLimitWithoutCacheDefersOneCheck()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto root = QDir(temporary.path()).filePath(QStringLiteral("updates"));
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion), root,
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  QSignalSpy updateErrors(&checker, &VersionChecker::updateError);

  checker.checkLatest();
  fakeNetwork->complete(
      0,
      {UpdateNetworkStatus::RateLimited, 429, {}, QUrl(), QStringLiteral("rate limited"), {}, 1}
  );

  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::Idle);
  QVERIFY(checker.status().userError.isEmpty());
  QVERIFY(!checker.isChecking());
  QCOMPARE(updateErrors.size(), 0);
  const auto persisted = UpdateCache(root).loadState();
  QVERIFY(persisted);
  QVERIFY(persisted->retryAt > QDateTime::currentDateTimeUtc());
  QVERIFY(persisted->sanitizedSnapshot.isEmpty());

  checker.checkLatest(UpdateCheckReason::Manual);
  checker.checkLatest(UpdateCheckReason::Manual);
  QCOMPARE(fakeNetwork->calls.size(), 1);

  QTRY_COMPARE_WITH_TIMEOUT(fakeNetwork->calls.size(), 2, 3'000);
  QTest::qWait(100);
  QCOMPARE(fakeNetwork->calls.size(), 2);
  QVERIFY(checker.isChecking());
  fakeNetwork->complete(1, success(releaseJson(QString::fromLatin1(kRunningVersion))));
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::UpToDate);
  QCOMPARE(updateErrors.size(), 0);
}

void VersionCheckerTests::checkLatest_timeoutMakesOnlyOneMetadataRetry()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  checker.checkLatest();
  fakeNetwork->complete(
      0, {UpdateNetworkStatus::Timeout, 0, {}, QUrl(), QStringLiteral("timeout"), {}, 0}
  );
  QTRY_COMPARE(fakeNetwork->calls.size(), 2);
  fakeNetwork->complete(
      1, {UpdateNetworkStatus::Timeout, 0, {}, QUrl(), QStringLiteral("timeout"), {}, 0}
  );
  QCOMPARE(fakeNetwork->calls.size(), 2);
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::Failed);
}

void VersionCheckerTests::checkLatest_manualPlatformsNeverDownload()
{
  auto serviceMac = directMac();
  serviceMac.serviceMode = true;
  for (const auto &environment : {
           UpdateEnvironment{QStringLiteral("windows"), QStringLiteral("arm64"), false, false},
           UpdateEnvironment{QStringLiteral("windows"), QStringLiteral("x86_64"), false, false},
           UpdateEnvironment{QStringLiteral("linux"), QStringLiteral("x86_64"), false, false},
           UpdateEnvironment{QStringLiteral("macos"), QStringLiteral("arm64"), true, true},
           serviceMac}) {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    auto network = std::make_unique<FakeUpdateNetwork>();
    auto *fakeNetwork = network.get();
    VersionChecker checker(
        std::move(network), environment, QString::fromLatin1(kRunningVersion),
        QDir(temporary.path()).filePath(QStringLiteral("updates")),
        std::make_unique<FakeUpdateVerifier>(true), 0
    );
    checker.checkLatest();
    fakeNetwork->complete(0, success(releaseJson()));
    QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::ManualAvailable);
    QCOMPARE(fakeNetwork->calls.size(), 1);
  }

  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto uncertain = directMac();
  uncertain.automaticInstallEligible = false;
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), uncertain, QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  checker.checkLatest();
  fakeNetwork->complete(0, success(releaseJson()));
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::ManualAvailable);
  QCOMPARE(fakeNetwork->calls.size(), 1);
}

void VersionCheckerTests::checkLatest_rejectsInvalidRunningVersionWithoutRequest()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), directMac(), QStringLiteral("2.0.5.17"),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );

  checker.checkLatest();
  QVERIFY(fakeNetwork->calls.isEmpty());
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::Failed);
}

void VersionCheckerTests::checkLatest_refreshesEnvironmentForEveryCheck()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto environment = directMac();
  environment.supported = false;
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), [&environment] { return environment; },
      QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );

  checker.checkLatest();
  QVERIFY(fakeNetwork->calls.isEmpty());
  environment = directMac();
  checker.checkLatest();
  QCOMPARE(fakeNetwork->calls.size(), 1);
  fakeNetwork->complete(0, success(releaseJson()));
  fakeNetwork->complete(1, success(checksumPayload()));
  fakeNetwork->complete(2, success(installerPayload()));
  QVERIFY(checker.staged());

  environment.supported = false;
  checker.checkLatest();
  QCOMPARE(fakeNetwork->calls.size(), 3);
  QVERIFY(!checker.staged());
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::Idle);
}

void VersionCheckerTests::checkLatest_rechecksEnvironmentBeforeChecksumRequest()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto environment = directMac();
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), [&environment] { return environment; },
      QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );

  checker.checkLatest();
  environment.packageManaged = true;
  fakeNetwork->complete(0, success(releaseJson()));

  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::ManualAvailable);
  QCOMPARE(
      checker.status().releaseUrl,
      QUrl(QStringLiteral("https://github.com/subeom/deskflow/releases/tag/v2.0.6"))
  );
  QCOMPARE(fakeNetwork->calls.size(), 1);
}

void VersionCheckerTests::checkLatest_rechecksEnvironmentBeforeAndAfterInstallerRequest()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto environment = directMac();
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), [&environment] { return environment; },
      QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );

  checker.checkLatest();
  fakeNetwork->complete(0, success(releaseJson()));
  QCOMPARE(fakeNetwork->calls.size(), 2);
  environment.serviceMode = true;
  fakeNetwork->complete(1, success(checksumPayload()));
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::ManualAvailable);
  QCOMPARE(fakeNetwork->calls.size(), 2);

  environment.serviceMode = false;
  checker.checkLatest();
  fakeNetwork->complete(2, success(releaseJson()));
  fakeNetwork->complete(3, success(checksumPayload()));
  QCOMPARE(fakeNetwork->calls.size(), 5);
  environment.supported = false;
  fakeNetwork->complete(4, success(installerPayload()));
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::ManualAvailable);
  QCOMPARE(
      checker.status().releaseUrl,
      QUrl(QStringLiteral("https://github.com/subeom/deskflow/releases/tag/v2.0.6"))
  );
  QVERIFY(!checker.staged());
}

void VersionCheckerTests::checkLatest_unsupportedEnvironmentDoesNotRequest()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto environment = directMac();
  environment.supported = false;
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), environment, QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  checker.checkLatest();
  QVERIFY(fakeNetwork->calls.isEmpty());
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::Idle);
  QVERIFY(!checker.isChecking());
}

void VersionCheckerTests::checkLatest_directMacDownloadsChecksumsThenDmgThenVerifies()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  auto verifier = std::make_unique<FakeUpdateVerifier>(true);
  auto *fakeVerifier = verifier.get();
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::move(verifier), 0
  );
  checker.checkLatest();
  fakeNetwork->complete(0, success(releaseJson(), QStringLiteral("\"release-etag\"")));
  QCOMPARE(fakeNetwork->calls.size(), 2);
  QCOMPARE(fakeNetwork->calls.at(1).options.kind, deskflow::gui::UpdateRequestKind::Checksum);
  fakeNetwork->complete(1, success(checksumPayload()));
  QCOMPARE(fakeNetwork->calls.size(), 3);
  QCOMPARE(fakeNetwork->calls.at(2).options.kind, deskflow::gui::UpdateRequestKind::Installer);
  fakeNetwork->complete(2, success(installerPayload()));
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::Staged);
  QVERIFY(checker.staged());
  QCOMPARE(fakeVerifier->calls, 1);
}

void VersionCheckerTests::checkLatest_cancelInvalidatesLateNetworkCallbacks()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  checker.checkLatest();
  checker.cancel();
  fakeNetwork->complete(0, success(releaseJson()));
  QCOMPARE(fakeNetwork->calls.size(), 1);
  QVERIFY(!checker.isChecking());
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::Idle);
}

void VersionCheckerTests::checkLatest_discardsWrongIntegrityAndPlatformVerification()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  auto verifier = std::make_unique<FakeUpdateVerifier>(false);
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::move(verifier), 0
  );
  checker.checkLatest();
  fakeNetwork->complete(0, success(releaseJson()));
  fakeNetwork->complete(1, success(checksumPayload()));
  auto changed = installerPayload();
  changed[0] ^= 1;
  fakeNetwork->complete(2, success(changed));
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::Failed);
  QVERIFY(!checker.staged());
}

void VersionCheckerTests::checkLatest_missingExactArchitectureNeverFallsBack()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  VersionChecker checker(
      std::move(network), directMac(QStringLiteral("x86_64")),
      QString::fromLatin1(kRunningVersion),
      QDir(temporary.path()).filePath(QStringLiteral("updates")),
      std::make_unique<FakeUpdateVerifier>(true), 0
  );
  checker.checkLatest();
  fakeNetwork->complete(0, success(releaseJson(QStringLiteral("2.0.6"), true)));
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::ManualAvailable);
  QCOMPARE(fakeNetwork->calls.size(), 1);
}

void VersionCheckerTests::checkLatest_reusesOnlyAReverifiedStage()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto root = QDir(temporary.path()).filePath(QStringLiteral("updates"));
  const auto parsed = deskflow::gui::parseGitHubRelease(releaseJson());
  QVERIFY(parsed.release);
  const auto selected = deskflow::gui::planUpdate(*parsed.release, directMac());
  QVERIFY(selected);
  UpdateCache cache(root);
  QVERIFY(cache.stage(*selected, installerPayload(), checksumPayload()).ok);

  auto network = std::make_unique<FakeUpdateNetwork>();
  auto *fakeNetwork = network.get();
  auto verifier = std::make_unique<FakeUpdateVerifier>(true);
  auto *fakeVerifier = verifier.get();
  VersionChecker checker(
      std::move(network), directMac(), QString::fromLatin1(kRunningVersion), root,
      std::move(verifier), 0
  );
  checker.checkLatest();
  fakeNetwork->complete(0, success(releaseJson()));
  QCOMPARE(fakeNetwork->calls.size(), 1);
  QCOMPARE(fakeVerifier->calls, 1);
  QCOMPARE(checker.status().phase, UpdateStatusSnapshot::Phase::Staged);
}

QTEST_MAIN(VersionCheckerTests)
