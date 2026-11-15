/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "VersionChecker.h"

#include "common/UrlConstants.h"
#include "common/VersionInfo.h"
#include "gui/UpdateIntegrity.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSysInfo>

namespace {

deskflow::gui::UpdateEnvironment currentUpdateEnvironment()
{
  bool packageManaged = false;
#if defined(Q_OS_MACOS)
  const auto bundlePath = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
  const auto canonicalBundlePath = QFileInfo(bundlePath).canonicalFilePath();
  packageManaged = canonicalBundlePath.contains(QStringLiteral("/Caskroom/deskflow/")) ||
                   QFileInfo::exists(QStringLiteral("/opt/homebrew/Caskroom/deskflow")) ||
                   QFileInfo::exists(QStringLiteral("/usr/local/Caskroom/deskflow"));
#endif

  QString platform = QStringLiteral("linux");
#if defined(Q_OS_MACOS)
  platform = QStringLiteral("macos");
#elif defined(Q_OS_WIN)
  platform = QStringLiteral("windows");
#endif
  return {platform, QSysInfo::currentCpuArchitecture(), packageManaged};
}

QString updateCachePath()
{
  return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("updates"));
}

} // namespace

VersionChecker::VersionChecker(QObject *parent)
    : VersionChecker(
          std::make_unique<deskflow::gui::QtUpdateNetwork>(), currentUpdateEnvironment(), updateCachePath(),
          deskflow::gui::createPlatformUpdateVerifier(), parent
      )
{
}

VersionChecker::VersionChecker(
    std::unique_ptr<deskflow::gui::IUpdateNetwork> network, deskflow::gui::UpdateEnvironment environment,
    QObject *parent
)
    : VersionChecker(std::move(network), std::move(environment), {}, {}, parent)
{
}

VersionChecker::VersionChecker(
    std::unique_ptr<deskflow::gui::IUpdateNetwork> network, deskflow::gui::UpdateEnvironment environment,
    QString cacheRoot, std::unique_ptr<deskflow::gui::IUpdateVerifier> verifier, QObject *parent
)
    : QObject(parent),
      m_network(std::move(network)),
      m_environment(std::move(environment)),
      m_cache(std::make_unique<deskflow::gui::UpdateCache>(std::move(cacheRoot))),
      m_verifier(std::move(verifier))
{
}

void VersionChecker::checkLatest()
{
  if (m_checking) {
    return;
  }

  const QUrl url(kUrlUpdateCheck);
  deskflow::gui::UpdateNetworkOptions options;
  options.maximumBytes = 2 * 1024 * 1024;
  m_checking = true;
  const auto generation = ++m_generation;
  m_requestId = m_network->get(url, options, [this, generation](const auto &result) {
    if (generation == m_generation) {
      handleLatestResponse(result);
    }
  });
}

void VersionChecker::cancel()
{
  if (m_requestId != 0) {
    m_network->cancel(m_requestId);
  }
  ++m_generation;
  m_requestId = 0;
  finishCheck();
}

bool VersionChecker::isChecking() const
{
  return m_checking;
}

void VersionChecker::handleLatestResponse(const deskflow::gui::UpdateNetworkResult &result)
{
  if (!handleNetworkResult(result, QStringLiteral("update check failed"))) {
    return;
  }

  const auto parsed = deskflow::gui::parseGitHubRelease(result.data);
  if (!parsed.release) {
    failCheck(parsed.error);
    return;
  }

  const auto plan = deskflow::gui::planUpdate(
      *parsed.release, m_environment.platform, m_environment.architecture, m_environment.packageManaged
  );
  if (!plan) {
    failCheck(QStringLiteral("no verified update asset for this platform"));
    return;
  }
  if (!deskflow::gui::isNewerUpdateVersion(kVersion, parsed.release->version)) {
    finishCheck();
    return;
  }
  if (plan->mode == deskflow::gui::UpdateInstallMode::Manual) {
    Q_EMIT updateFound(parsed.release->version, plan->releaseUrl);
    finishCheck();
    return;
  }
  if (!plan->installer || !m_verifier) {
    failCheck(QStringLiteral("automatic update verification is unavailable on this platform"));
    return;
  }

  m_plan = *plan;
  if (const auto staged = m_cache->find(*plan)) {
    publishStagedUpdate(*staged);
  } else if (plan->checksums) {
    requestChecksums();
  } else {
    requestInstaller();
  }
}

void VersionChecker::handleChecksumsResponse(const deskflow::gui::UpdateNetworkResult &result)
{
  if (!handleNetworkResult(result, QStringLiteral("update checksum download failed"))) {
    return;
  }
  if (!m_plan || !m_plan->checksums) {
    failCheck(QStringLiteral("update checksum plan is missing"));
    return;
  }

  const auto verification = deskflow::gui::verifyUpdatePayload(*m_plan->checksums, result.data, std::nullopt);
  if (!verification.ok) {
    failCheck(verification.error);
    return;
  }
  m_sums = result.data;
  requestInstaller();
}

void VersionChecker::handleInstallerResponse(const deskflow::gui::UpdateNetworkResult &result)
{
  if (!handleNetworkResult(result, QStringLiteral("update installer download failed"))) {
    return;
  }
  if (!m_plan) {
    failCheck(QStringLiteral("update plan is missing"));
    return;
  }

  const auto staged = m_cache->stage(*m_plan, result.data, m_sums);
  if (!staged.ok) {
    failCheck(staged.error);
    return;
  }
  publishStagedUpdate(staged.staged);
}

bool VersionChecker::handleNetworkResult(
    const deskflow::gui::UpdateNetworkResult &result, const QString &failureMessage
)
{
  m_requestId = 0;
  using deskflow::gui::UpdateNetworkStatus;
  if (result.status == UpdateNetworkStatus::Cancelled || result.status == UpdateNetworkStatus::NotModified) {
    finishCheck();
    return false;
  }
  if (result.status == UpdateNetworkStatus::Success) {
    return true;
  }

  QString error = result.error;
  if (result.status == UpdateNetworkStatus::RateLimited && result.retryAfterSeconds > 0) {
    error += QStringLiteral(" (retry after %1 seconds)").arg(result.retryAfterSeconds);
  }
  failCheck(error.isEmpty() ? failureMessage : error);
  return false;
}

void VersionChecker::requestChecksums()
{
  if (!m_plan || !m_plan->checksums) {
    failCheck(QStringLiteral("update checksum plan is missing"));
    return;
  }

  deskflow::gui::UpdateNetworkOptions options;
  options.maximumBytes = m_plan->checksums->size;
  const auto generation = m_generation;
  m_requestId = m_network->get(QUrl(m_plan->checksums->downloadUrl), options, [this, generation](const auto &result) {
    if (generation == m_generation) {
      handleChecksumsResponse(result);
    }
  });
}

void VersionChecker::requestInstaller()
{
  if (!m_plan || !m_plan->installer) {
    failCheck(QStringLiteral("update installer plan is missing"));
    return;
  }

  deskflow::gui::UpdateNetworkOptions options;
  options.maximumBytes = m_plan->installer->size;
  const auto generation = m_generation;
  m_requestId = m_network->get(QUrl(m_plan->installer->downloadUrl), options, [this, generation](const auto &result) {
    if (generation == m_generation) {
      handleInstallerResponse(result);
    }
  });
}

void VersionChecker::publishStagedUpdate(const deskflow::gui::StagedUpdate &staged)
{
  const auto verification = m_verifier->verify(staged);
  if (!verification.ok) {
    const bool discarded = m_cache->discard(staged);
    if (!discarded) {
      qCritical().noquote() << "failed to discard an invalid staged update:" << staged.installerPath;
    }
    failCheck(
        discarded
            ? (verification.error.isEmpty() ? QStringLiteral("staged update verification failed") : verification.error)
            : QStringLiteral("%1; failed to discard the staged update")
                  .arg(
                      verification.error.isEmpty() ? QStringLiteral("staged update verification failed")
                                                   : verification.error
                  )
    );
    return;
  }

  Q_EMIT updateStaged(staged);
  finishCheck();
}

void VersionChecker::finishCheck()
{
  m_requestId = 0;
  m_checking = false;
  m_plan.reset();
  m_sums.reset();
}

void VersionChecker::failCheck(const QString &error)
{
  Q_EMIT updateError(error);
  finishCheck();
}
