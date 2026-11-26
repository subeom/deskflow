/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/VersionChecker.h"

#include "common/VersionInfo.h"
#include "common/UrlConstants.h"
#include "common/Settings.h"
#include "gui/UpdateIntegrity.h"
#include "gui/UpdateInstaller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimeZone>
#include <QTimer>

namespace {

constexpr int kMaximumRateWaitSeconds = 24 * 60 * 60;

deskflow::gui::UpdateEnvironment currentUpdateEnvironment()
{
  QString platform = QStringLiteral("unsupported");
  bool supported = false;
#if defined(Q_OS_MACOS)
  platform = QStringLiteral("macos");
  supported = true;
#elif defined(Q_OS_WIN)
  platform = QStringLiteral("windows");
  supported = true;
#elif defined(Q_OS_LINUX)
  platform = QStringLiteral("linux");
  supported = true;
#endif

  bool packageManaged = false;
  bool destinationWritable = false;
  bool automaticInstallEligible = false;
  const auto serviceMode =
      Settings::value(Settings::Core::ProcessMode).value<Settings::ProcessMode>() ==
      Settings::ProcessMode::Service;
#if defined(Q_OS_MACOS)
  const auto executablePath = QCoreApplication::applicationFilePath();
  const bool packageOwned =
      deskflow::gui::isMacUpdatePackageManaged(executablePath);
  const auto evidence =
      deskflow::gui::evaluateMacUpdateInstall(
          executablePath, packageOwned, serviceMode
      );
  packageManaged = evidence.packageManaged;
  destinationWritable = evidence.bundleWritable && evidence.parentWritable;
  automaticInstallEligible =
      deskflow::gui::isMacUpdateInstallEligible(evidence);
#endif
  return {
      platform, QSysInfo::currentCpuArchitecture(), packageManaged, destinationWritable,
      serviceMode, supported, automaticInstallEligible
  };
}

QString updateCachePath()
{
  return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
      .filePath(QStringLiteral("updates"));
}

bool sameAsset(
    const std::optional<deskflow::gui::UpdateAsset> &left,
    const std::optional<deskflow::gui::UpdateAsset> &right
)
{
  if (left.has_value() != right.has_value()) {
    return false;
  }
  return !left || (left->name == right->name && left->downloadUrl == right->downloadUrl &&
                   left->digest == right->digest && left->size == right->size);
}

bool samePlan(const deskflow::gui::UpdatePlan &left, const deskflow::gui::UpdatePlan &right)
{
  return left.releaseVersion == right.releaseVersion &&
         left.tag == right.tag && left.releaseUrl == right.releaseUrl &&
         left.mode == right.mode && sameAsset(left.installer, right.installer) &&
         sameAsset(left.checksums, right.checksums);
}

QByteArray sanitizedReleaseSnapshot(const deskflow::gui::UpdateRelease &release)
{
  QJsonArray assets;
  for (const auto &asset : release.assets) {
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
      {QStringLiteral("tag_name"), release.tag},
      {QStringLiteral("html_url"), QString::fromUtf8(release.pageUrl.toEncoded(QUrl::FullyEncoded))},
      {QStringLiteral("assets"), assets},
  })
      .toJson(QJsonDocument::Compact);
}

QDateTime epoch()
{
  return QDateTime::fromSecsSinceEpoch(0, QTimeZone::UTC);
}

} // namespace

VersionChecker::VersionChecker(QObject *parent)
    : VersionChecker(
          std::make_unique<deskflow::gui::QtUpdateNetwork>(),
          UpdateEnvironmentProvider([] { return currentUpdateEnvironment(); }),
          QString::fromLatin1(kVersion), updateCachePath(),
          deskflow::gui::createPlatformUpdateVerifier(), 2'000, parent
      )
{
}

VersionChecker::VersionChecker(
    std::unique_ptr<deskflow::gui::IUpdateNetwork> network,
    UpdateEnvironmentProvider environmentProvider, QString cacheRoot,
    std::unique_ptr<deskflow::gui::IUpdateVerifier> verifier, int retryDelayMs, QObject *parent
)
    : VersionChecker(
          std::move(network), std::move(environmentProvider),
          QString::fromLatin1(kVersion), std::move(cacheRoot),
          std::move(verifier), retryDelayMs, parent
      )
{
}

VersionChecker::VersionChecker(
    std::unique_ptr<deskflow::gui::IUpdateNetwork> network,
    UpdateEnvironmentProvider environmentProvider, QString runningVersion,
    QString cacheRoot, std::unique_ptr<deskflow::gui::IUpdateVerifier> verifier,
    int retryDelayMs, QObject *parent
)
    : QObject(parent),
      m_network(std::move(network)),
      m_environmentProvider(std::move(environmentProvider)),
      m_runningVersion(std::move(runningVersion)),
      m_cache(std::make_unique<deskflow::gui::UpdateCache>(std::move(cacheRoot))),
      m_verifier(std::move(verifier)),
      m_retryDelayMs(retryDelayMs)
{
}

VersionChecker::VersionChecker(
    std::unique_ptr<deskflow::gui::IUpdateNetwork> network,
    deskflow::gui::UpdateEnvironment environment, QString cacheRoot,
    std::unique_ptr<deskflow::gui::IUpdateVerifier> verifier, int retryDelayMs, QObject *parent
)
    : VersionChecker(
          std::move(network),
          UpdateEnvironmentProvider([environment] { return environment; }),
          QString::fromLatin1(kVersion), std::move(cacheRoot),
          std::move(verifier), retryDelayMs, parent
      )
{
}

VersionChecker::VersionChecker(
    std::unique_ptr<deskflow::gui::IUpdateNetwork> network,
    deskflow::gui::UpdateEnvironment environment, QString runningVersion,
    QString cacheRoot, std::unique_ptr<deskflow::gui::IUpdateVerifier> verifier,
    int retryDelayMs, QObject *parent
)
    : VersionChecker(
          std::move(network),
          UpdateEnvironmentProvider([environment] { return environment; }),
          std::move(runningVersion), std::move(cacheRoot),
          std::move(verifier), retryDelayMs, parent
      )
{
}

VersionChecker::~VersionChecker()
{
  cancel();
}

void VersionChecker::checkLatest(UpdateCheckReason)
{
  if (m_checking) {
    return;
  }
  m_staged.reset();
  m_plan.reset();
  m_sums.reset();
  m_release.reset();
  if (!refreshEnvironment()) {
    return;
  }
  if (!m_environment.supported) {
    setStatus({UpdateStatusSnapshot::Phase::Idle, {}, {}, {}, false});
    return;
  }

  const auto stableVersion = deskflow::gui::normalizeUpdateVersion(m_runningVersion);
  if (!stableVersion) {
    failCheck(QStringLiteral("the running Deskflow version is invalid"));
    return;
  }
  if (!m_cache->cleanup(*stableVersion)) {
    failCheck(QStringLiteral("the update cache could not be safely cleaned"));
    return;
  }
  m_cachedState = m_cache->loadState();
  if (m_cachedState && m_cachedState->retryAt > QDateTime::currentDateTimeUtc()) {
    const auto remaining = QDateTime::currentDateTimeUtc().secsTo(m_cachedState->retryAt);
    failCheck(
        QStringLiteral("GitHub rate limit is still active; retry in %1 seconds")
            .arg(qMax<qint64>(1, remaining))
    );
    return;
  }

  m_checking = true;
  m_metadataAttempt = 0;
  m_checksumAttempt = 0;
  setStatus({UpdateStatusSnapshot::Phase::CheckingRelease, {}, {}, {}, false});
  requestLatest();
}

void VersionChecker::cancel()
{
  ++m_generation;
  if (m_requestId != 0 && m_network) {
    m_network->cancel(m_requestId);
  }
  m_requestId = 0;
  m_plan.reset();
  m_sums.reset();
  m_staged.reset();
  m_release.reset();
  m_checking = false;
  setStatus({UpdateStatusSnapshot::Phase::Idle, {}, {}, {}, false});
}

bool VersionChecker::isChecking() const
{
  return m_checking;
}

const UpdateStatusSnapshot &VersionChecker::status() const
{
  return m_status;
}

const std::optional<deskflow::gui::StagedUpdate> &VersionChecker::staged() const
{
  return m_staged;
}

void VersionChecker::requestLatest()
{
  deskflow::gui::UpdateNetworkOptions options;
  options.maximumBytes = 2LL * 1024 * 1024;
  options.timeoutMs = 30'000;
  options.maximumTotalMs = 30'000;
  options.kind = deskflow::gui::UpdateRequestKind::Metadata;
  options.etag = m_cachedState ? m_cachedState->etag : QString();
  const auto generation = ++m_generation;
  m_requestId = m_network->get(
      QUrl(kUrlUpdateCheck), options,
      [this, generation](const auto &result) {
        if (deskflow::gui::isCurrentUpdateGeneration(m_generation, generation)) {
          handleLatestResponse(result);
        }
      }
  );
  if (m_requestId == 0) {
    failCheck(QStringLiteral("the update request was rejected by network policy"));
  }
}

void VersionChecker::requestChecksums()
{
  if (!refreshAutomaticPlan()) {
    return;
  }
  if (!m_plan || !m_plan->checksums) {
    failCheck(QStringLiteral("the update checksum plan is missing"));
    return;
  }

  const auto requestedPlan = *m_plan;
  deskflow::gui::UpdateNetworkOptions options;
  options.maximumBytes = requestedPlan.checksums->size;
  options.timeoutMs = 30'000;
  options.maximumTotalMs = 30'000;
  options.kind = deskflow::gui::UpdateRequestKind::Checksum;
  setStatus(
      {UpdateStatusSnapshot::Phase::DownloadingChecksums,
       requestedPlan.releaseVersion,
       requestedPlan.releaseUrl,
       {},
       false}
  );
  const auto generation = ++m_generation;
  m_requestId = m_network->get(
      requestedPlan.checksums->downloadUrl, options,
      [this, generation, requestedPlan](const auto &result) {
        if (deskflow::gui::isCurrentUpdateGeneration(m_generation, generation)) {
          handleChecksumsResponse(result, requestedPlan);
        }
      }
  );
  if (m_requestId == 0) {
    failCheck(QStringLiteral("the checksum request was rejected by network policy"));
  }
}

void VersionChecker::requestInstaller()
{
  if (!refreshAutomaticPlan()) {
    return;
  }
  if (!m_plan || !m_plan->installer) {
    failCheck(QStringLiteral("the update installer plan is missing"));
    return;
  }

  const auto requestedPlan = *m_plan;
  deskflow::gui::UpdateNetworkOptions options;
  options.maximumBytes = requestedPlan.installer->size;
  options.timeoutMs = 30'000;
  options.maximumTotalMs = 5 * 60'000;
  options.kind = deskflow::gui::UpdateRequestKind::Installer;
  setStatus(
      {UpdateStatusSnapshot::Phase::DownloadingInstaller,
       requestedPlan.releaseVersion,
       requestedPlan.releaseUrl,
       {},
       false}
  );
  const auto generation = ++m_generation;
  m_requestId = m_network->get(
      requestedPlan.installer->downloadUrl, options,
      [this, generation, requestedPlan](const auto &result) {
        if (deskflow::gui::isCurrentUpdateGeneration(m_generation, generation)) {
          handleInstallerResponse(result, requestedPlan);
        }
      }
  );
  if (m_requestId == 0) {
    failCheck(QStringLiteral("the installer request was rejected by network policy"));
  }
}

void VersionChecker::handleLatestResponse(const deskflow::gui::UpdateNetworkResult &result)
{
  m_requestId = 0;
  if (result.status == deskflow::gui::UpdateNetworkStatus::Cancelled) {
    finishCheck();
    return;
  }
  if (result.status == deskflow::gui::UpdateNetworkStatus::NotModified) {
    if (!m_cachedState || m_cachedState->sanitizedSnapshot.isEmpty()) {
      failCheck(QStringLiteral("GitHub returned not-modified but the cached release is invalid"));
      return;
    }
    processReleaseJson(m_cachedState->sanitizedSnapshot, result.etag.isEmpty() ? m_cachedState->etag : result.etag);
    return;
  }
  if (handleRetryableFailure(result, deskflow::gui::UpdateRequestKind::Metadata, m_metadataAttempt)) {
    scheduleMetadataRetry();
    return;
  }
  if (result.status == deskflow::gui::UpdateNetworkStatus::RateLimited) {
    saveRateLimitState(result.retryAfterSeconds);
    failCheck(
        QStringLiteral("GitHub rate limit reached; retry after %1 seconds")
            .arg(qBound(1, result.retryAfterSeconds, kMaximumRateWaitSeconds))
    );
    return;
  }
  if (result.status != deskflow::gui::UpdateNetworkStatus::Success) {
    failCheck(result.error.isEmpty() ? QStringLiteral("the update check failed") : result.error);
    return;
  }

  processReleaseJson(result.data, result.etag);
}

void VersionChecker::handleChecksumsResponse(
    const deskflow::gui::UpdateNetworkResult &result,
    const deskflow::gui::UpdatePlan &requestedPlan
)
{
  m_requestId = 0;
  if (!refreshAutomaticPlan()) {
    return;
  }
  if (!samePlan(*m_plan, requestedPlan)) {
    m_sums.reset();
    requestChecksums();
    return;
  }
  if (handleRetryableFailure(result, deskflow::gui::UpdateRequestKind::Checksum, m_checksumAttempt)) {
    scheduleChecksumRetry();
    return;
  }
  if (result.status != deskflow::gui::UpdateNetworkStatus::Success) {
    failCheck(
        result.error.isEmpty() ? QStringLiteral("the update checksum download failed")
                              : result.error
    );
    return;
  }
  if (!m_plan || !m_plan->checksums) {
    failCheck(QStringLiteral("the update checksum plan is missing"));
    return;
  }

  const auto verification =
      deskflow::gui::verifyUpdatePayload(*m_plan->checksums, result.data);
  if (!verification.ok) {
    failCheck(verification.error);
    return;
  }
  m_sums = result.data;
  requestInstaller();
}

void VersionChecker::handleInstallerResponse(
    const deskflow::gui::UpdateNetworkResult &result,
    const deskflow::gui::UpdatePlan &requestedPlan
)
{
  m_requestId = 0;
  if (!refreshAutomaticPlan()) {
    return;
  }
  if (!samePlan(*m_plan, requestedPlan)) {
    m_sums.reset();
    requestChecksums();
    return;
  }
  if (result.status != deskflow::gui::UpdateNetworkStatus::Success) {
    failCheck(
        result.error.isEmpty() ? QStringLiteral("the update installer download failed")
                              : result.error
    );
    return;
  }
  if (!m_plan || !m_plan->installer || !m_sums) {
    failCheck(QStringLiteral("the update installer plan or checksums are missing"));
    return;
  }

  setStatus(
      {UpdateStatusSnapshot::Phase::Verifying,
       m_plan->releaseVersion,
       m_plan->releaseUrl,
       {},
       false}
  );
  const auto staged = m_cache->stage(*m_plan, result.data, m_sums);
  if (!staged.ok) {
    failCheck(staged.error);
    return;
  }
  publishStagedUpdate(staged.staged);
}

void VersionChecker::processReleaseJson(const QByteArray &json, const QString &etag)
{
  const auto parsed = deskflow::gui::parseGitHubRelease(json);
  if (!parsed.release) {
    failCheck(parsed.error);
    return;
  }

  const auto now = QDateTime::currentDateTimeUtc();
  m_cachedState = deskflow::gui::UpdateCacheState{
      1,
      kUrlUpdateCheck,
      etag,
      sanitizedReleaseSnapshot(*parsed.release),
      parsed.release->version,
      epoch(),
      now,
  };
  if (!m_cache->saveState(*m_cachedState)) {
    failCheck(QStringLiteral("the update release state could not be saved safely"));
    return;
  }

  m_release = *parsed.release;
  if (!refreshAutomaticPlan()) {
    return;
  }

  if (const auto cached = m_cache->find(*m_plan)) {
    publishStagedUpdate(*cached);
    return;
  }

  requestChecksums();
}

bool VersionChecker::refreshEnvironment()
{
  if (!m_environmentProvider) {
    failCheck(QStringLiteral("the update environment provider is unavailable"));
    return false;
  }
  m_environment = m_environmentProvider();
  return true;
}

bool VersionChecker::refreshAutomaticPlan()
{
  if (!m_release) {
    failCheck(QStringLiteral("the update release is missing"));
    return false;
  }
  if (!refreshEnvironment()) {
    return false;
  }

  const auto stableVersion = deskflow::gui::normalizeUpdateVersion(m_runningVersion);
  if (!stableVersion) {
    failCheck(QStringLiteral("the running Deskflow version is invalid"));
    return false;
  }
  if (!deskflow::gui::isNewerUpdateVersion(*stableVersion, m_release->version)) {
    setStatus(
        {UpdateStatusSnapshot::Phase::UpToDate, {}, m_release->pageUrl, {}, false}
    );
    finishCheck();
    return false;
  }

  const auto plan = deskflow::gui::planUpdate(*m_release, m_environment);
  if (!plan || plan->mode == deskflow::gui::UpdateInstallMode::Manual) {
    publishManualUpdate();
    return false;
  }
  if (!m_verifier || !plan->installer || !plan->checksums) {
    failCheck(QStringLiteral("automatic macOS update verification is unavailable"));
    return false;
  }
  m_plan = *plan;
  return true;
}

void VersionChecker::publishManualUpdate()
{
  if (!m_release) {
    failCheck(QStringLiteral("the update release is missing"));
    return;
  }
  const auto release = *m_release;
  m_plan.reset();
  setStatus(
      {UpdateStatusSnapshot::Phase::ManualAvailable,
       release.version,
       release.pageUrl,
       {},
       false}
  );
  Q_EMIT manualUpdateAvailable(release.pageUrl, release.version);
  Q_EMIT updateFound(release.version);
  finishCheck();
}

void VersionChecker::publishStagedUpdate(const deskflow::gui::StagedUpdate &staged)
{
  setStatus(
      {UpdateStatusSnapshot::Phase::Verifying,
       staged.version,
       m_plan ? m_plan->releaseUrl : QUrl(),
       {},
       false}
  );
  if (!m_verifier) {
    failCheck(QStringLiteral("the platform verifier is unavailable"));
    return;
  }
  const auto verification = m_verifier->verify(staged);
  if (!verification.ok) {
    const auto discarded = m_cache->discard(staged);
    const auto verificationError =
        verification.error.isEmpty() ? QStringLiteral("staged update verification failed")
                                     : verification.error;
    failCheck(
        discarded
            ? verificationError
            : QStringLiteral("%1; the invalid stage could not be discarded").arg(verificationError)
    );
    return;
  }

  m_staged = staged;
  const auto releaseUrl = m_plan ? m_plan->releaseUrl : QUrl();
  setStatus(
      {UpdateStatusSnapshot::Phase::Staged, staged.version, releaseUrl, {}, true}
  );
  Q_EMIT stagedUpdateAvailable(staged);
  Q_EMIT updateFound(staged.version);
  finishCheck();
}

void VersionChecker::scheduleMetadataRetry()
{
  ++m_metadataAttempt;
  const auto generation = ++m_generation;
  QTimer::singleShot(m_retryDelayMs, this, [this, generation] {
    if (deskflow::gui::isCurrentUpdateGeneration(m_generation, generation)) {
      requestLatest();
    }
  });
}

void VersionChecker::scheduleChecksumRetry()
{
  ++m_checksumAttempt;
  const auto generation = ++m_generation;
  QTimer::singleShot(m_retryDelayMs, this, [this, generation] {
    if (deskflow::gui::isCurrentUpdateGeneration(m_generation, generation)) {
      requestChecksums();
    }
  });
}

bool VersionChecker::handleRetryableFailure(
    const deskflow::gui::UpdateNetworkResult &result,
    deskflow::gui::UpdateRequestKind kind, int &attempt
)
{
  return attempt == 0 &&
         deskflow::gui::shouldRetryUpdateRequest(
             kind, result.status, attempt, result.httpStatus
         );
}

void VersionChecker::saveRateLimitState(int retryAfterSeconds)
{
  auto state = m_cachedState.value_or(deskflow::gui::UpdateCacheState{
      1,
      kUrlUpdateCheck,
      {},
      {},
      {},
      epoch(),
      epoch(),
  });
  state.retryAt =
      QDateTime::currentDateTimeUtc().addSecs(qBound(1, retryAfterSeconds, kMaximumRateWaitSeconds));
  m_cachedState = state;
  if (!m_cache->saveState(state)) {
    qWarning("failed to persist update rate-limit state");
  }
}

void VersionChecker::setStatus(const UpdateStatusSnapshot &status)
{
  if (m_status == status) {
    return;
  }
  m_status = status;
  Q_EMIT statusChanged(m_status);
}

void VersionChecker::finishCheck()
{
  m_requestId = 0;
  m_checking = false;
  m_plan.reset();
  m_sums.reset();
  m_release.reset();
}

void VersionChecker::failCheck(const QString &error)
{
  const auto releaseUrl =
      m_plan ? m_plan->releaseUrl : (m_release ? m_release->pageUrl : QUrl());
  const auto version =
      m_plan ? m_plan->releaseVersion : (m_release ? m_release->version : QString());
  setStatus({UpdateStatusSnapshot::Phase::Failed, version, releaseUrl, error, false});
  m_staged.reset();
  Q_EMIT updateError(error);
  finishCheck();
}
