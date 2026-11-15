/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateInstaller.h"

#include <QStringList>

#include <memory>

namespace deskflow::gui {

struct UpdateHelperLaunchResult
{
  bool started = false;
  qint64 processId = 0;
  QString error;
};

class IUpdateProcessLauncher
{
public:
  virtual ~IUpdateProcessLauncher() = default;

  virtual UpdateHelperLaunchResult launch(const QString &program, const QStringList &arguments) = 0;
};

class IUpdateApplyOperations
{
public:
  virtual ~IUpdateApplyOperations() = default;

  virtual UpdateInstallResult waitForProcessExit(qint64 processId, int timeoutMs) = 0;
  virtual UpdateInstallResult verifyDiskImage(const StagedUpdate &staged) = 0;
  virtual UpdateInstallResult mountDiskImage(const QString &dmg, const QString &mountPoint) = 0;
  virtual UpdateInstallResult copyApplication(const QString &mountedApp, const QString &destination) = 0;
  virtual UpdateInstallResult verifyApplication(const QString &path, const QString &expectedVersion) = 0;
  virtual UpdateInstallResult detachDiskImage(const QString &mountPoint) = 0;
  virtual UpdateInstallResult rename(const QString &source, const QString &destination) = 0;
  virtual void removeRecursively(const QString &path) = 0;
  virtual UpdateInstallResult relaunch(const QString &bundlePath) = 0;
};

class MacUpdateInstaller final : public IUpdateInstaller
{
public:
  MacUpdateInstaller(
      QString executablePath, bool packageManaged, std::unique_ptr<IUpdateProcessLauncher> launcher = {},
      std::unique_ptr<IUpdateApplyOperations> operations = {}
  );

  UpdateInstallResult start(const StagedUpdate &staged, bool confirmed) override;
  UpdateInstallResult apply(const QString &manifestPath, qint64 parentProcessId);

private:
  QString m_executablePath;
  bool m_packageManaged = false;
  std::unique_ptr<IUpdateProcessLauncher> m_launcher;
  std::unique_ptr<IUpdateApplyOperations> m_operations;
};

} // namespace deskflow::gui
