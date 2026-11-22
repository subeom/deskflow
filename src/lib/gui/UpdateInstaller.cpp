/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSysInfo>

#include <cerrno>

#if defined(Q_OS_MACOS)
#include "gui/MacUpdateInstaller.h"
#include <sys/acl.h>
#include <unistd.h>
#endif

namespace deskflow::gui {

namespace {

QString currentUpdateCacheRoot()
{
  return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
      .filePath(QStringLiteral("updates"));
}

#if defined(Q_OS_MACOS)
std::optional<QString> deskflowBundlePath(const QString &executablePath)
{
  const QFileInfo executable(executablePath);
  const auto macosDirectory = QFileInfo(executable.absolutePath());
  if (macosDirectory.fileName() != QStringLiteral("MacOS") ||
      executable.fileName() != QStringLiteral("Deskflow")) {
    return std::nullopt;
  }
  const auto contentsPath = macosDirectory.absolutePath();
  if (QFileInfo(contentsPath).fileName() != QStringLiteral("Contents")) {
    return std::nullopt;
  }
  const auto bundlePath = QFileInfo(contentsPath).absolutePath();
  if (QFileInfo(bundlePath).fileName() != QStringLiteral("Deskflow.app")) {
    return std::nullopt;
  }
  return bundlePath;
}

MacUpdateAclResult readMacUpdateAcl(const QString &path)
{
  if (!QFileInfo::exists(path)) {
    return {false, -1, false, false, ENOENT};
  }
  acl_t acl = acl_get_file(path.toLocal8Bit().constData(), ACL_TYPE_EXTENDED);
  if (acl == nullptr) {
    const auto errorNumber = errno;
    const bool absenceProven = errorNumber == ENOENT && QFileInfo::exists(path);
    return {false, -1, false, false, errorNumber, absenceProven};
  }
  bool hasEntries = false;
  acl_entry_t entry = nullptr;
  int result = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);
  while (result == 1) {
    hasEntries = true;
    result = acl_get_entry(acl, ACL_NEXT_ENTRY, &entry);
  }
  const auto parseError = result == -1 ? errno : 0;
  const auto released = acl_free(acl) == 0;
  const auto releaseError = released ? 0 : errno;
  return {true, result, hasEntries, released, parseError != 0 ? parseError : releaseError};
}

#endif

} // namespace

bool isMacUpdatePackageManagedPath(const QString &canonicalPath)
{
  const auto cleaned = QDir::cleanPath(canonicalPath);
  return cleaned.contains(QStringLiteral("/Caskroom/deskflow/")) ||
         cleaned.contains(QStringLiteral("/Cellar/deskflow/"));
}

bool isMacUpdatePackageManaged(const QString &executablePath)
{
  return isMacUpdatePackageManagedPath(
      QFileInfo(executablePath).canonicalFilePath()
  );
}

bool isMacUpdateAclTrustProven(const MacUpdateAclResult &result)
{
  const bool readProven =
      result.readSucceeded && result.entryResult == 0 &&
      !result.hasEntries && result.releaseSucceeded &&
      result.errorNumber == 0;
  const bool absenceProven =
      result.absenceProven && !result.readSucceeded &&
      result.entryResult == -1 && !result.hasEntries &&
      result.errorNumber == ENOENT;
  return readProven || absenceProven;
}

MacUpdateInstallEvidence evaluateMacUpdateInstall(
    const QString &executablePath, bool packageManaged, bool serviceMode,
    MacUpdateAclReader aclReader
)
{
#if !defined(Q_OS_MACOS)
  Q_UNUSED(executablePath)
  Q_UNUSED(packageManaged)
  Q_UNUSED(serviceMode)
  Q_UNUSED(aclReader)
  return {};
#else
  MacUpdateInstallEvidence evidence;
  evidence.supported = true;
  evidence.desktopMode = !serviceMode;
  const auto architecture = QSysInfo::currentCpuArchitecture().toLower();
  evidence.architectureSupported =
      architecture == QStringLiteral("arm64") ||
      architecture == QStringLiteral("aarch64") ||
      architecture == QStringLiteral("x86_64") ||
      architecture == QStringLiteral("amd64");

  const auto bundlePath = deskflowBundlePath(executablePath);
  if (!bundlePath) {
    return evidence;
  }
  const QFileInfo bundle(*bundlePath);
  const QFileInfo parent(bundle.dir().absolutePath());
  evidence.bundlePath = bundle.absoluteFilePath();
  evidence.packageManaged =
      packageManaged || bundle.isSymLink() ||
      isMacUpdatePackageManagedPath(bundle.canonicalFilePath()) ||
      isMacUpdatePackageManaged(executablePath) ||
      bundle.absoluteFilePath().startsWith(QStringLiteral("/opt/local/"));
  evidence.directApp = bundle.exists() && !bundle.isSymLink() && bundle.isDir();
  evidence.canonicalPath =
      bundle.canonicalFilePath() == bundle.absoluteFilePath() &&
      QDir::cleanPath(bundle.absoluteFilePath()) == bundle.absoluteFilePath();
  evidence.mountedVolume = bundle.absoluteFilePath().startsWith(QStringLiteral("/Volumes/"));
  evidence.appOwnedByEffectiveUser =
      bundle.ownerId() == static_cast<quint32>(geteuid());
  evidence.parentTrusted = isTrustedUpdateParentPermissions(
      geteuid(), parent.ownerId(), parent.group(),
      parent.permissions().testFlag(QFileDevice::WriteGroup),
      parent.permissions().testFlag(QFileDevice::WriteOther)
  );
  evidence.bundleWritable = bundle.isWritable();
  evidence.parentWritable = parent.isWritable();
  const auto readAcl = [&aclReader](const QString &path) {
    return aclReader ? aclReader(path) : readMacUpdateAcl(path);
  };
  evidence.aclTrustProven =
      isMacUpdateAclTrustProven(readAcl(bundle.absoluteFilePath())) &&
      isMacUpdateAclTrustProven(readAcl(parent.absoluteFilePath()));
  return evidence;
#endif
}

bool isMacUpdateInstallEligible(const MacUpdateInstallEvidence &evidence)
{
  return evidence.supported && evidence.desktopMode && evidence.directApp &&
         evidence.canonicalPath && !evidence.mountedVolume &&
         !evidence.packageManaged && evidence.appOwnedByEffectiveUser &&
         evidence.parentTrusted && evidence.bundleWritable &&
         evidence.parentWritable && evidence.aclTrustProven &&
         evidence.architectureSupported;
}

std::unique_ptr<IUpdateInstaller> createPlatformUpdateInstaller()
{
#if defined(Q_OS_MACOS)
  const auto executable = QCoreApplication::applicationFilePath();
  return std::make_unique<MacUpdateInstaller>(
      executable, currentUpdateCacheRoot(), isMacUpdatePackageManaged(executable)
  );
#else
  return {};
#endif
}

UpdateInstallResult applyPlatformUpdate(
    const QString &manifestPath, qint64 parentPid,
    const QString &parentStartIdentity, const QString &receiptPath,
    const QString &helperToken
)
{
#if defined(Q_OS_MACOS)
  const auto executable = QCoreApplication::applicationFilePath();
  MacUpdateInstaller installer(
      executable, currentUpdateCacheRoot(), isMacUpdatePackageManaged(executable)
  );
  return installer.apply(
      manifestPath, parentPid, parentStartIdentity, receiptPath, helperToken
  );
#else
  Q_UNUSED(manifestPath)
  Q_UNUSED(parentPid)
  Q_UNUSED(parentStartIdentity)
  Q_UNUSED(receiptPath)
  Q_UNUSED(helperToken)
  return {false, QStringLiteral("automatic update installation is not supported"), {}};
#endif
}

} // namespace deskflow::gui
