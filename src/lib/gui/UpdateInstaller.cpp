/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
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
  errno = 0;
  if (acl_valid(acl) != 0) {
    const auto errorNumber = errno;
    acl_free(acl);
    return {false, -1, false, false, errorNumber};
  }
  bool hasEntries = false;
  acl_entry_t entry = nullptr;
  errno = 0;
  int result = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);
  while (result == 0) {
    hasEntries = true;
    result = acl_get_entry(acl, ACL_NEXT_ENTRY, &entry);
  }
  const auto parseComplete = result == -1 && errno == EINVAL;
  const auto parseError = parseComplete ? 0 : errno;
  const auto released = acl_free(acl) == 0;
  const auto releaseError = released ? 0 : errno;
  return {
      parseComplete,
      result,
      hasEntries,
      released,
      parseError != 0 ? parseError : releaseError
  };
}

std::optional<QJsonObject> readPackageJson(
    const QString &path, bool &present, bool &ambiguous
)
{
  const QFileInfo info(path);
  present = info.exists() || info.isSymLink();
  if (!present) {
    return std::nullopt;
  }
  if (info.isSymLink() || !info.isFile() || info.size() > 1024 * 1024) {
    ambiguous = true;
    return std::nullopt;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    ambiguous = true;
    return std::nullopt;
  }
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    ambiguous = true;
    return std::nullopt;
  }
  return document.object();
}

void appendPackageArtifact(
    QList<MacUpdatePackageArtifact> &artifacts, const QString &path
)
{
  const QFileInfo info(path);
  if (!info.exists() && !info.isSymLink()) {
    return;
  }
  MacUpdatePackageArtifact artifact;
  artifact.path = info.absoluteFilePath();
  artifact.symbolicLink = info.isSymLink();
  artifact.canonicalTarget = info.canonicalFilePath();
  artifact.broken = artifact.symbolicLink &&
                    (artifact.canonicalTarget.isEmpty() || !info.exists());
  artifact.ambiguous =
      !artifact.symbolicLink &&
      (!info.isDir() || artifact.canonicalTarget.isEmpty());
  artifacts.append(std::move(artifact));
}

void appendPackageReceipt(
    MacUpdatePackageEnumeration &enumeration, const QString &packageRoot
)
{
  const auto metadataRoot = QDir(packageRoot).filePath(QStringLiteral(".metadata"));
  const auto receiptPath = QDir(metadataRoot).filePath(QStringLiteral("INSTALL_RECEIPT.json"));
  const auto configPath = QDir(metadataRoot).filePath(QStringLiteral("config.json"));
  bool receiptPresent = false;
  bool configPresent = false;
  bool ambiguous = false;
  const auto receipt = readPackageJson(receiptPath, receiptPresent, ambiguous);
  const auto config = readPackageJson(configPath, configPresent, ambiguous);
  if (!receiptPresent && !configPresent) {
    return;
  }
  if (ambiguous || !receipt || !config || !receiptPresent || !configPresent) {
    enumeration.artifacts.append({
        receiptPresent ? receiptPath : configPath,
        {},
        false,
        false,
        true,
        true
    });
    return;
  }

  const auto homebrewVersion =
      receipt->value(QStringLiteral("homebrew_version")).toString();
  const auto tap =
      receipt->value(QStringLiteral("source")).toObject()
          .value(QStringLiteral("tap")).toString();
  bool provesDeskflowApp = false;
  for (const auto uninstall :
       receipt->value(QStringLiteral("uninstall_artifacts")).toArray()) {
    const auto apps = uninstall.toObject().value(QStringLiteral("app")).toArray();
    for (const auto app : apps) {
      if (app.toString() == QStringLiteral("Deskflow.app")) {
        provesDeskflowApp = true;
      }
    }
  }
  const auto appDirectory =
      config->value(QStringLiteral("default")).toObject()
          .value(QStringLiteral("appdir")).toString();
  if (homebrewVersion.isEmpty() || tap.isEmpty() || !provesDeskflowApp ||
      appDirectory.isEmpty() || !QFileInfo(appDirectory).isAbsolute()) {
    enumeration.artifacts.append({receiptPath, {}, false, false, true, true});
    return;
  }

  const auto installedBundle =
      QDir(appDirectory).filePath(QStringLiteral("Deskflow.app"));
  const auto canonicalBundle = QFileInfo(installedBundle).canonicalFilePath();
  if (canonicalBundle.isEmpty()) {
    enumeration.artifacts.append({receiptPath, {}, false, false, true, true});
    return;
  }
  enumeration.artifacts.append({
      receiptPath,
      canonicalBundle,
      false,
      false,
      true,
      false
  });
}

MacUpdatePackageEnumeration enumerateMacUpdatePackageArtifacts(
    const QString &
)
{
  MacUpdatePackageEnumeration enumeration;
  for (const auto &prefix :
       {QStringLiteral("/opt/homebrew"), QStringLiteral("/usr/local")}) {
    for (const auto &packageRoot :
         {QDir(prefix).filePath(QStringLiteral("Caskroom/deskflow")),
          QDir(prefix).filePath(QStringLiteral("Cellar/deskflow"))}) {
      const QFileInfo rootInfo(packageRoot);
      if (!rootInfo.exists() && !rootInfo.isSymLink()) {
        continue;
      }
      if (rootInfo.isSymLink() || !rootInfo.isDir()) {
        enumeration.artifacts.append({
            packageRoot, {}, rootInfo.isSymLink(), true, false, true
        });
        continue;
      }
      QDir packageDirectory(packageRoot);
      if (!packageDirectory.isReadable()) {
        enumeration.complete = false;
        continue;
      }
      const auto versions = packageDirectory.entryInfoList(
          QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name
      );
      if (versions.size() > 256) {
        enumeration.complete = false;
      }
      for (const auto &version : versions) {
        appendPackageArtifact(
            enumeration.artifacts,
            QDir(version.absoluteFilePath())
                .filePath(QStringLiteral("Deskflow.app"))
        );
      }
      if (packageRoot.contains(QStringLiteral("/Caskroom/"))) {
        appendPackageReceipt(enumeration, packageRoot);
      }
    }
  }
  return enumeration;
}

#endif

} // namespace

bool isMacUpdatePackageManagedPath(const QString &canonicalPath)
{
  const auto cleaned = QDir::cleanPath(canonicalPath);
  for (const auto &prefix :
       {QStringLiteral("/opt/homebrew"), QStringLiteral("/usr/local")}) {
    for (const auto &package :
         {QStringLiteral("Caskroom/deskflow"), QStringLiteral("Cellar/deskflow")}) {
      const auto root = QDir(prefix).filePath(package);
      if (cleaned.startsWith(root + QLatin1Char('/'))) {
        return true;
      }
    }
  }
  return false;
}

bool isMacUpdatePackageManaged(
    const QString &executablePath,
    MacUpdatePackageEnumerator packageEnumerator
)
{
#if !defined(Q_OS_MACOS)
  Q_UNUSED(executablePath)
  Q_UNUSED(packageEnumerator)
  return false;
#else
  const auto bundlePath = deskflowBundlePath(executablePath);
  if (!bundlePath) {
    return false;
  }
  const auto lexicalBundle = QDir::cleanPath(*bundlePath);
  const auto canonicalBundle = QFileInfo(*bundlePath).canonicalFilePath();
  if (isMacUpdatePackageManagedPath(lexicalBundle) ||
      isMacUpdatePackageManagedPath(canonicalBundle)) {
    return true;
  }
  if (canonicalBundle.isEmpty()) {
    return true;
  }

  const auto enumeration =
      packageEnumerator ? packageEnumerator(canonicalBundle)
                        : enumerateMacUpdatePackageArtifacts(canonicalBundle);
  if (!enumeration.complete) {
    return true;
  }
  bool ambiguous = false;
  for (const auto &artifact : enumeration.artifacts) {
    if (!isMacUpdatePackageManagedPath(artifact.path)) {
      continue;
    }
    if (artifact.broken || artifact.ambiguous) {
      ambiguous = true;
      continue;
    }
    if ((artifact.symbolicLink || artifact.receiptProof) &&
        QDir::cleanPath(artifact.canonicalTarget) == canonicalBundle) {
      return true;
    }
  }
  return ambiguous;
#endif
}

bool isMacUpdateAclTrustProven(const MacUpdateAclResult &result)
{
  const bool readProven =
      result.readSucceeded && !result.hasEntries && result.releaseSucceeded &&
      result.errorNumber == 0;
  const bool absenceProven =
      result.absenceProven && !result.readSucceeded &&
      result.entryResult == -1 && !result.hasEntries &&
      result.errorNumber == ENOENT;
  return readProven || absenceProven;
}

MacUpdateInstallEvidence evaluateMacUpdateInstall(
    const QString &executablePath, bool packageManaged, bool serviceMode,
    MacUpdateAclReader aclReader,
    MacUpdatePackageEnumerator packageEnumerator
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
      isMacUpdatePackageManaged(executablePath, packageEnumerator) ||
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
