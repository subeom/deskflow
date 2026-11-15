/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateNetwork.h"
#include "gui/UpdateRelease.h"

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

  void checkLatest();
  void cancel();
  bool isChecking() const;
Q_SIGNALS:
  void updateFound(const QString &version);
  void updateError(const QString &message);

private:
  void handleLatestResponse(const deskflow::gui::UpdateNetworkResult &result);

  std::unique_ptr<deskflow::gui::IUpdateNetwork> m_network;
  deskflow::gui::UpdateEnvironment m_environment;
  quint64 m_requestId = 0;
  bool m_checking = false;
};
