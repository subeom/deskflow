/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateCache.h"

#include <QString>
#include <QStringList>

#include <functional>
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

enum class UpdateParentState
{
  Running,
  OriginalExited,
  PidReused
};

struct UpdateFileIdentity
{
  bool ok = false;
  quint64 device = 0;
  quint64 inode = 0;
  qint64 size = 0;

  bool operator==(const UpdateFileIdentity &) const = default;
};

struct MacUpdateAclResult
{
  bool readSucceeded = false;
  int entryResult = -1;
  bool hasEntries = false;
  bool releaseSucceeded = false;
  int errorNumber = 0;
  bool absenceProven = false;
};

using MacUpdateAclReader = std::function<MacUpdateAclResult(const QString &)>;

struct MacUpdateInstallEvidence
{
  bool supported = false;
  bool desktopMode = false;
  bool directApp = false;
  bool canonicalPath = false;
  bool mountedVolume = false;
  bool packageManaged = false;
  bool appOwnedByEffectiveUser = false;
  bool parentTrusted = false;
  bool bundleWritable = false;
  bool parentWritable = false;
  bool aclTrustProven = false;
  bool architectureSupported = false;
  QString bundlePath;
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
  virtual UpdateParentState parentState(
      qint64 processId, const QString &startIdentity
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
MacUpdateInstallEvidence evaluateMacUpdateInstall(
    const QString &executablePath, bool packageManaged, bool serviceMode,
    MacUpdateAclReader aclReader = {}
);
bool isMacUpdateAclTrustProven(const MacUpdateAclResult &result);
bool isMacUpdateInstallEligible(const MacUpdateInstallEvidence &evidence);
bool isMacUpdatePackageManagedPath(const QString &canonicalPath);
bool isMacUpdatePackageManaged(const QString &executablePath);

} // namespace deskflow::gui
