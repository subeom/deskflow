/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "VersionChecker.h"

#include "common/UrlConstants.h"
#include "common/VersionInfo.h"

#include <QCoreApplication>
#include <QFileInfo>
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

} // namespace

VersionChecker::VersionChecker(QObject *parent)
    : VersionChecker(std::make_unique<deskflow::gui::QtUpdateNetwork>(), currentUpdateEnvironment(), parent)
{
}

VersionChecker::VersionChecker(
    std::unique_ptr<deskflow::gui::IUpdateNetwork> network, deskflow::gui::UpdateEnvironment environment,
    QObject *parent
)
    : QObject(parent),
      m_network(std::move(network)),
      m_environment(std::move(environment))
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
  m_requestId = m_network->get(url, options, [this](const auto &result) { handleLatestResponse(result); });
}

void VersionChecker::cancel()
{
  if (m_requestId != 0) {
    m_network->cancel(m_requestId);
  }
  m_requestId = 0;
  m_checking = false;
}

bool VersionChecker::isChecking() const
{
  return m_checking;
}

void VersionChecker::handleLatestResponse(const deskflow::gui::UpdateNetworkResult &result)
{
  m_checking = false;
  m_requestId = 0;

  using deskflow::gui::UpdateNetworkStatus;
  if (result.status == UpdateNetworkStatus::Cancelled) {
    return;
  }
  if (result.status == UpdateNetworkStatus::NotModified) {
    return;
  }
  if (result.status != UpdateNetworkStatus::Success) {
    QString error = result.error;
    if (result.status == UpdateNetworkStatus::RateLimited && result.retryAfterSeconds > 0) {
      error += QStringLiteral(" (retry after %1 seconds)").arg(result.retryAfterSeconds);
    }
    Q_EMIT updateError(error.isEmpty() ? QStringLiteral("update check failed") : error);
    return;
  }

  const auto parsed = deskflow::gui::parseGitHubRelease(result.data);
  if (!parsed.release) {
    Q_EMIT updateError(parsed.error);
    return;
  }

  const auto plan = deskflow::gui::planUpdate(
      *parsed.release, m_environment.platform, m_environment.architecture, m_environment.packageManaged
  );
  if (!plan) {
    Q_EMIT updateError(QStringLiteral("no verified update asset for this platform"));
    return;
  }
  if (!deskflow::gui::isNewerUpdateVersion(kVersion, parsed.release->version)) {
    return;
  }
  Q_EMIT updateFound(parsed.release->version);
}
