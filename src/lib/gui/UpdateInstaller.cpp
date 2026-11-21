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

#if defined(Q_OS_MACOS)
#include "gui/MacUpdateInstaller.h"
#endif

namespace deskflow::gui {

namespace {

QString currentUpdateCacheRoot()
{
  return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
      .filePath(QStringLiteral("updates"));
}

#if defined(Q_OS_MACOS)
bool isPackageManaged(const QString &executablePath)
{
  const QFileInfo executable(executablePath);
  const auto canonical = executable.canonicalFilePath();
  return canonical.contains(QStringLiteral("/Caskroom/deskflow/")) ||
         canonical.contains(QStringLiteral("/Cellar/")) ||
         QFileInfo::exists(QStringLiteral("/opt/homebrew/Caskroom/deskflow")) ||
         QFileInfo::exists(QStringLiteral("/usr/local/Caskroom/deskflow"));
}
#endif

} // namespace

std::unique_ptr<IUpdateInstaller> createPlatformUpdateInstaller()
{
#if defined(Q_OS_MACOS)
  const auto executable = QCoreApplication::applicationFilePath();
  return std::make_unique<MacUpdateInstaller>(
      executable, currentUpdateCacheRoot(), isPackageManaged(executable)
  );
#else
  return {};
#endif
}

UpdateInstallResult applyPlatformUpdate(
    const QString &manifestPath, qint64 parentPid,
    const QString &parentStartIdentity, const QString &receiptPath
)
{
#if defined(Q_OS_MACOS)
  const auto executable = QCoreApplication::applicationFilePath();
  MacUpdateInstaller installer(
      executable, currentUpdateCacheRoot(), isPackageManaged(executable)
  );
  return installer.apply(manifestPath, parentPid, parentStartIdentity, receiptPath);
#else
  Q_UNUSED(manifestPath)
  Q_UNUSED(parentPid)
  Q_UNUSED(parentStartIdentity)
  Q_UNUSED(receiptPath)
  return {false, QStringLiteral("automatic update installation is not supported"), {}};
#endif
}

} // namespace deskflow::gui
