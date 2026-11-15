/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateNetwork.h"
#include "gui/UpdateRelease.h"
#include "gui/UpdateVerifier.h"

#include <QObject>
#include <QString>

#include <memory>

namespace deskflow::gui {
struct UpdateEnvironment
{
  QString platform;
  QString architecture;
  bool packageManaged = false;
};
} // namespace deskflow::gui

class VersionChecker : public QObject
{
  Q_OBJECT
public:
  explicit VersionChecker(QObject *parent = nullptr);
  VersionChecker(
      std::unique_ptr<deskflow::gui::IUpdateNetwork> network, deskflow::gui::UpdateEnvironment environment,
      QObject *parent = nullptr
  );
  VersionChecker(
      std::unique_ptr<deskflow::gui::IUpdateNetwork> network, deskflow::gui::UpdateEnvironment environment,
      QString cacheRoot, std::unique_ptr<deskflow::gui::IUpdateVerifier> verifier, QObject *parent = nullptr
  );

  void checkLatest();
  void cancel();
  bool isChecking() const;
Q_SIGNALS:
  void updateFound(const QString &version, const QString &releaseUrl);
  void updateStaged(const deskflow::gui::StagedUpdate &staged);
  void updateError(const QString &message);

private:
  void handleLatestResponse(const deskflow::gui::UpdateNetworkResult &result);
  void handleChecksumsResponse(const deskflow::gui::UpdateNetworkResult &result);
  void handleInstallerResponse(const deskflow::gui::UpdateNetworkResult &result);
  bool handleNetworkResult(const deskflow::gui::UpdateNetworkResult &result, const QString &failureMessage);
  void requestChecksums();
  void requestInstaller();
  void publishStagedUpdate(const deskflow::gui::StagedUpdate &staged);
  void finishCheck();
  void failCheck(const QString &error);

  std::unique_ptr<deskflow::gui::IUpdateNetwork> m_network;
  deskflow::gui::UpdateEnvironment m_environment;
  std::unique_ptr<deskflow::gui::UpdateCache> m_cache;
  std::unique_ptr<deskflow::gui::IUpdateVerifier> m_verifier;
  std::optional<deskflow::gui::UpdatePlan> m_plan;
  std::optional<QByteArray> m_sums;
  quint64 m_requestId = 0;
  quint64 m_generation = 0;
  bool m_checking = false;
};
