/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateCache.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace deskflow::gui {

struct UpdateInstallResult
{
  bool ok = false;
  QString error;
  QString receiptPath;
  QString helperToken;
};

struct UpdateHelperWorkspace
{
  QString rootPath;
  QString mountPoint;
};

struct UpdateFileIdentity
{
  bool ok = false;
  quint64 device = 0;
  quint64 inode = 0;
  qint64 size = 0;

  bool operator==(const UpdateFileIdentity &) const = default;
};

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
  virtual UpdateHelperLaunchResult launch(
      const QString &program, const QStringList &arguments
  ) = 0;
};

class IUpdateApplyOperations
{
public:
  virtual ~IUpdateApplyOperations() = default;

  virtual UpdateInstallResult waitForHelperReady(
      const QString &receiptPath, qint64 helperPid, int timeoutMs
  ) = 0;
  virtual UpdateInstallResult waitForShutdown(
      const QString &receiptPath, qint64 parentPid,
      const QString &startIdentity, int timeoutMs
  ) = 0;
  virtual UpdateInstallResult waitForProcessExit(
      qint64 processId, const QString &startIdentity, int timeoutMs
  ) = 0;
  virtual UpdateInstallResult createWorkspace(
      UpdateHelperWorkspace &workspace
  ) = 0;
  virtual UpdateInstallResult verifyDiskImage(const StagedUpdate &staged) = 0;
  virtual UpdateInstallResult mountDiskImage(
      const QString &dmg, const QString &mountPoint
  ) = 0;
  virtual UpdateInstallResult copyApplication(
      const QString &mountedApp, const QString &destination
  ) = 0;
  virtual UpdateInstallResult verifyApplication(
      const QString &path, const QString &expectedVersion
  ) = 0;
  virtual UpdateInstallResult removeQuarantine(const QString &candidate) = 0;
  virtual UpdateInstallResult detachDiskImage(const QString &mountPoint) = 0;
  virtual UpdateInstallResult rename(
      const QString &source, const QString &destination
  ) = 0;
  virtual UpdateInstallResult relaunchAndWait(
      const QString &bundlePath, const QString &receiptPath,
      const QString &expectedVersion
  ) = 0;
  virtual bool removeRecursively(const QString &path) = 0;
  virtual std::optional<UpdateFileIdentity> captureIdentity(
      const QString &path
  ) = 0;
};

class IUpdateInstaller
{
public:
  virtual ~IUpdateInstaller() = default;
  virtual UpdateInstallResult preflight(const StagedUpdate &staged) = 0;
  virtual UpdateInstallResult start(
      const StagedUpdate &staged, bool explicitConfirmation
  ) = 0;
};

std::unique_ptr<IUpdateInstaller> createPlatformUpdateInstaller();
UpdateInstallResult applyPlatformUpdate(
    const QString &manifestPath, qint64 parentPid,
    const QString &parentStartIdentity, const QString &receiptPath,
    const QString &helperToken
);
UpdateInstallResult markUpdateReceiptState(
    const QString &receiptPath, const QString &state
);
UpdateInstallResult claimUpdateReceipt(
    const QString &receiptPath, const QString &token, qint64 helperPid,
    const QString &helperStartIdentity
);

} // namespace deskflow::gui
