/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateCache.h"
#include "gui/UpdateNetwork.h"
#include "gui/UpdateRelease.h"
#include "gui/UpdateVerifier.h"

#include <QObject>
#include <QUrl>

#include <memory>
#include <optional>

enum class UpdateCheckReason
{
  Startup,
  Manual
};

struct UpdateStatusSnapshot
{
  enum class Phase
  {
    Idle,
    CheckingRelease,
    UpToDate,
    ManualAvailable,
    DownloadingChecksums,
    DownloadingInstaller,
    Verifying,
    Staged,
    Confirming,
    Applying,
    RollingBack,
    Failed
  };

  Phase phase = Phase::Idle;
  QString version;
  QUrl releaseUrl;
  QString userError;
  bool installable = false;

  bool operator==(const UpdateStatusSnapshot &) const = default;
};

class VersionChecker : public QObject
{
  Q_OBJECT
public:
  explicit VersionChecker(QObject *parent = nullptr);
  VersionChecker(
      std::unique_ptr<deskflow::gui::IUpdateNetwork> network,
      deskflow::gui::UpdateEnvironment environment, QString cacheRoot,
      std::unique_ptr<deskflow::gui::IUpdateVerifier> verifier, int retryDelayMs = 0,
      QObject *parent = nullptr
  );
  ~VersionChecker() override;

  void checkLatest(UpdateCheckReason reason = UpdateCheckReason::Startup);
  void cancel();
  bool isChecking() const;
  const UpdateStatusSnapshot &status() const;
  const std::optional<deskflow::gui::StagedUpdate> &staged() const;

Q_SIGNALS:
  void statusChanged(const UpdateStatusSnapshot &status);
  void manualUpdateAvailable(const QUrl &releaseUrl, const QString &version);
  void stagedUpdateAvailable(const deskflow::gui::StagedUpdate &staged);
  void updateError(const QString &message);
  void updateFound(const QString &version);

private:
  void requestLatest();
  void requestChecksums();
  void requestInstaller();
  void handleLatestResponse(const deskflow::gui::UpdateNetworkResult &result);
  void handleChecksumsResponse(const deskflow::gui::UpdateNetworkResult &result);
  void handleInstallerResponse(const deskflow::gui::UpdateNetworkResult &result);
  void processReleaseJson(const QByteArray &json, const QString &etag);
  void publishStagedUpdate(const deskflow::gui::StagedUpdate &staged);
  void scheduleMetadataRetry();
  void scheduleChecksumRetry();
  bool handleRetryableFailure(
      const deskflow::gui::UpdateNetworkResult &result,
      deskflow::gui::UpdateRequestKind kind, int &attempt
  );
  void saveRateLimitState(int retryAfterSeconds);
  void setStatus(const UpdateStatusSnapshot &status);
  void finishCheck();
  void failCheck(const QString &error);

  std::unique_ptr<deskflow::gui::IUpdateNetwork> m_network;
  deskflow::gui::UpdateEnvironment m_environment;
  std::unique_ptr<deskflow::gui::UpdateCache> m_cache;
  std::unique_ptr<deskflow::gui::IUpdateVerifier> m_verifier;
  UpdateStatusSnapshot m_status;
  std::optional<deskflow::gui::UpdateCacheState> m_cachedState;
  std::optional<deskflow::gui::UpdatePlan> m_plan;
  std::optional<deskflow::gui::StagedUpdate> m_staged;
  std::optional<QByteArray> m_sums;
  quint64 m_requestId = 0;
  quint64 m_generation = 0;
  int m_metadataAttempt = 0;
  int m_checksumAttempt = 0;
  int m_retryDelayMs = 0;
  bool m_checking = false;
};

Q_DECLARE_METATYPE(UpdateStatusSnapshot)
