/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/MacUpdateInstaller.h"

#include "gui/MacUpdateVerifier.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <csignal>
#include <limits>
#endif

#include <optional>

namespace deskflow::gui {

namespace {

class QProcessUpdateLauncher final : public IUpdateProcessLauncher
{
public:
  UpdateHelperLaunchResult launch(const QString &program, const QStringList &arguments) override
  {
    qint64 processId = 0;
    if (!QProcess::startDetached(program, arguments, QFileInfo(program).absolutePath(), &processId)) {
      return {false, 0, QStringLiteral("failed to start detached update helper")};
    }
    return {true, processId, {}};
  }
};

std::optional<QString> deskflowBundlePath(const QString &path)
{
  const QFileInfo executable(path);
  const auto macosPath = executable.absolutePath();
  if (QFileInfo(macosPath).fileName() != QStringLiteral("MacOS")) {
    return std::nullopt;
  }
  const auto contentsPath = QFileInfo(macosPath).absolutePath();
  if (QFileInfo(contentsPath).fileName() != QStringLiteral("Contents")) {
    return std::nullopt;
  }
  const auto bundlePath = QFileInfo(contentsPath).absolutePath();
  if (QFileInfo(bundlePath).fileName() != QStringLiteral("Deskflow.app") ||
      executable.fileName() != QStringLiteral("Deskflow")) {
    return std::nullopt;
  }
  return bundlePath;
}

bool isDeskflowGuiExecutable(const QString &path)
{
  return deskflowBundlePath(path).has_value();
}

bool isHomebrewManagedPath(const QString &path)
{
  const auto canonical = QFileInfo(path).canonicalFilePath();
  const auto absolute = QFileInfo(path).absoluteFilePath();
  return canonical.contains(QStringLiteral("/Caskroom/deskflow/")) ||
         absolute.contains(QStringLiteral("/Caskroom/deskflow/"));
}

bool isRegularStagedFile(const QString &path)
{
  const QFileInfo info(path);
  return !info.isSymLink() && info.isFile();
}

UpdateInstallResult installResult(const UpdateVerifierResult &result)
{
  return {result.ok, result.error};
}

class MacUpdateApplyOperations final : public IUpdateApplyOperations
{
public:
  MacUpdateApplyOperations() : m_verifier(createUpdateCommandRunner()), m_runner(createUpdateCommandRunner())
  {
  }

  UpdateInstallResult waitForProcessExit(qint64 processId, int timeoutMs) override
  {
#if defined(Q_OS_UNIX)
    if (processId <= 1 || processId > std::numeric_limits<pid_t>::max()) {
      return {false, QStringLiteral("invalid update parent process")};
    }

    QElapsedTimer timer;
    timer.start();
    while (true) {
      if (kill(static_cast<pid_t>(processId), 0) != 0) {
        if (errno == ESRCH) {
          return {true, {}};
        }
        if (errno != EPERM) {
          return {false, QStringLiteral("cannot inspect update parent process")};
        }
      }
      if (timer.elapsed() >= timeoutMs) {
        return {false, QStringLiteral("timed out waiting for Deskflow to exit")};
      }
      QThread::msleep(100);
    }
#else
    Q_UNUSED(processId)
    Q_UNUSED(timeoutMs)
    return {false, QStringLiteral("update helper is not supported on this platform")};
#endif
  }

  UpdateInstallResult verifyDiskImage(const StagedUpdate &staged) override
  {
    return installResult(m_verifier.verify(staged));
  }

  UpdateInstallResult mountDiskImage(const QString &dmg, const QString &mountPoint) override
  {
    const auto result = m_runner->run(
        QStringLiteral("/usr/bin/hdiutil"),
        {QStringLiteral("attach"), dmg, QStringLiteral("-readonly"), QStringLiteral("-nobrowse"),
         QStringLiteral("-noautoopen"), QStringLiteral("-mountpoint"), mountPoint},
        120'000
    );
    return commandResult(QStringLiteral("mounting update disk image"), result);
  }

  UpdateInstallResult copyApplication(const QString &mountedApp, const QString &destination) override
  {
    const QFileInfo appInfo(mountedApp);
    const QFileInfo destinationInfo(destination);
    if (appInfo.isSymLink() || !appInfo.isDir() || appInfo.fileName() != QStringLiteral("Deskflow.app") ||
        appInfo.canonicalPath() != QDir(QFileInfo(mountedApp).absolutePath()).canonicalPath() ||
        destinationInfo.isSymLink() || destinationInfo.exists()) {
      return {false, QStringLiteral("mounted update app path is invalid")};
    }
    const auto result = m_runner->run(QStringLiteral("/usr/bin/ditto"), {mountedApp, destination}, 120'000);
    return commandResult(QStringLiteral("copying update app bundle"), result);
  }

  UpdateInstallResult verifyApplication(const QString &path, const QString &expectedVersion) override
  {
    return installResult(m_verifier.verifyApplication(path, expectedVersion));
  }

  UpdateInstallResult detachDiskImage(const QString &mountPoint) override
  {
    const auto result = m_runner->run(
        QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), mountPoint, QStringLiteral("-force")}, 120'000
    );
    return commandResult(QStringLiteral("unmounting update disk image"), result);
  }

  UpdateInstallResult rename(const QString &source, const QString &destination) override
  {
    const QFileInfo destinationInfo(destination);
    if (destinationInfo.exists() || destinationInfo.isSymLink() || !QDir().rename(source, destination)) {
      return {false, QStringLiteral("failed to rename '%1' to '%2'").arg(source, destination)};
    }
    return {true, {}};
  }

  void removeRecursively(const QString &path) override
  {
    QDir directory(path);
    if (directory.exists() && !directory.removeRecursively()) {
      qWarning().noquote() << "failed to remove update path:" << path;
    }
  }

  UpdateInstallResult relaunch(const QString &bundlePath) override
  {
    return QProcess::startDetached(QStringLiteral("/usr/bin/open"), {QStringLiteral("-n"), bundlePath})
               ? UpdateInstallResult{true, {}}
               : UpdateInstallResult{false, QStringLiteral("failed to relaunch Deskflow")};
  }

private:
  static UpdateInstallResult commandResult(const QString &description, const UpdateCommandResult &result)
  {
    if (updateCommandSucceeded(result)) {
      return {true, {}};
    }
    return installResult(updateCommandFailure(description, result));
  }

  MacUpdateVerifier m_verifier;
  std::unique_ptr<IUpdateCommandRunner> m_runner;
};

QString combinedFailure(const QString &first, const QString &second)
{
  if (second.isEmpty()) {
    return first;
  }
  return first.isEmpty() ? second : QStringLiteral("%1; %2").arg(first, second);
}

} // namespace

MacUpdateInstaller::MacUpdateInstaller(
    QString executablePath, bool packageManaged, std::unique_ptr<IUpdateProcessLauncher> launcher,
    std::unique_ptr<IUpdateApplyOperations> operations
)
    : m_executablePath(std::move(executablePath)),
      m_packageManaged(packageManaged),
      m_launcher(std::move(launcher)),
      m_operations(std::move(operations))
{
  if (!m_launcher) {
    m_launcher = std::make_unique<QProcessUpdateLauncher>();
  }
  if (!m_operations) {
    m_operations = std::make_unique<MacUpdateApplyOperations>();
  }
}

UpdateInstallResult MacUpdateInstaller::start(const StagedUpdate &staged, bool confirmed)
{
  if (!confirmed) {
    return {false, QStringLiteral("update installation was not confirmed")};
  }
  if (m_packageManaged || isHomebrewManagedPath(m_executablePath)) {
    return {false, QStringLiteral("Homebrew-managed Deskflow must be updated with Homebrew")};
  }
  if (!isDeskflowGuiExecutable(m_executablePath)) {
    return {false, QStringLiteral("update helper is not running from Deskflow.app")};
  }
  if (!isRegularStagedFile(staged.manifestPath) || !isRegularStagedFile(staged.installerPath) ||
      QFileInfo(staged.manifestPath).fileName() != QStringLiteral("manifest.json") ||
      QFileInfo(staged.installerPath).fileName() != staged.installerName ||
      QFileInfo(staged.manifestPath).absolutePath() != QFileInfo(staged.installerPath).absolutePath()) {
    return {false, QStringLiteral("staged update files are invalid")};
  }

  const QStringList arguments{
      QStringLiteral("--apply-update"), staged.manifestPath, QStringLiteral("--update-parent-pid"),
      QString::number(QCoreApplication::applicationPid())
  };
  const auto launched = m_launcher->launch(m_executablePath, arguments);
  if (!launched.started) {
    return {false, launched.error.isEmpty() ? QStringLiteral("failed to start update helper") : launched.error};
  }
  return {true, {}};
}

UpdateInstallResult MacUpdateInstaller::apply(const QString &manifestPath, qint64 parentProcessId)
{
  if (m_packageManaged || isHomebrewManagedPath(m_executablePath)) {
    return {false, QStringLiteral("Homebrew-managed Deskflow must be updated with Homebrew")};
  }
  if (parentProcessId <= 1 || parentProcessId == QCoreApplication::applicationPid()) {
    return {false, QStringLiteral("invalid update parent process")};
  }

  const auto bundlePath = deskflowBundlePath(m_executablePath);
  if (!bundlePath) {
    return {false, QStringLiteral("update helper is not running from Deskflow.app")};
  }
  const QFileInfo bundleInfo(*bundlePath);
  if (bundleInfo.isSymLink() || !bundleInfo.isDir()) {
    return {false, QStringLiteral("Deskflow app bundle cannot be updated")};
  }
  const auto failAndRelaunch = [this, &bundleInfo](const UpdateInstallResult &failure) {
    const auto relaunched = m_operations->relaunch(bundleInfo.absoluteFilePath());
    return UpdateInstallResult{false, combinedFailure(failure.error, relaunched.ok ? QString() : relaunched.error)};
  };

  constexpr auto processExitTimeoutMs = 120'000;
  const auto wait = m_operations->waitForProcessExit(parentProcessId, processExitTimeoutMs);
  if (!wait.ok) {
    return wait;
  }

  const auto staged = UpdateCache::load(manifestPath);
  if (!staged) {
    return failAndRelaunch({false, QStringLiteral("staged update manifest or payload is invalid")});
  }

  const auto verified = m_operations->verifyDiskImage(*staged);
  if (!verified.ok) {
    m_operations->removeRecursively(QFileInfo(staged->manifestPath).absolutePath());
    return failAndRelaunch(verified);
  }

  QTemporaryDir temporary(QStringLiteral("%1/deskflow-update-apply-XXXXXX").arg(QDir::tempPath()));
  if (!temporary.isValid()) {
    return failAndRelaunch({false, QStringLiteral("cannot create update helper workspace")});
  }
  const auto mountPoint = QDir(temporary.path()).filePath(QStringLiteral("volume"));
  if (!QDir().mkpath(mountPoint)) {
    return failAndRelaunch({false, QStringLiteral("cannot create update helper mount point")});
  }

  const auto uniqueId = QUuid::createUuid().toString(QUuid::Id128);
  const auto candidate = bundleInfo.dir().filePath(QStringLiteral(".Deskflow-update-new-%1.app").arg(uniqueId));
  const auto backup = bundleInfo.dir().filePath(QStringLiteral(".Deskflow-update-backup-%1.app").arg(uniqueId));
  const auto mounted = m_operations->mountDiskImage(staged->installerPath, mountPoint);
  if (!mounted.ok) {
    return failAndRelaunch(mounted);
  }

  const auto mountedApp = QDir(mountPoint).filePath(QStringLiteral("Deskflow.app"));
  auto prepared = m_operations->copyApplication(mountedApp, candidate);
  bool candidateVerificationFailed = false;
  if (prepared.ok) {
    prepared = m_operations->verifyApplication(candidate, staged->version);
    candidateVerificationFailed = !prepared.ok;
  }
  const auto detached = m_operations->detachDiskImage(mountPoint);
  if (!detached.ok || !prepared.ok) {
    m_operations->removeRecursively(candidate);
    if (candidateVerificationFailed) {
      m_operations->removeRecursively(QFileInfo(staged->manifestPath).absolutePath());
    }
    return failAndRelaunch({false, combinedFailure(prepared.error, detached.ok ? QString() : detached.error)});
  }

  const auto movedOld = m_operations->rename(bundleInfo.absoluteFilePath(), backup);
  if (!movedOld.ok) {
    m_operations->removeRecursively(candidate);
    return failAndRelaunch(movedOld);
  }

  const auto movedNew = m_operations->rename(candidate, bundleInfo.absoluteFilePath());
  if (!movedNew.ok) {
    const auto restored = m_operations->rename(backup, bundleInfo.absoluteFilePath());
    m_operations->removeRecursively(candidate);
    const auto relaunched = m_operations->relaunch(bundleInfo.absoluteFilePath());
    return {
        false, combinedFailure(
                   combinedFailure(movedNew.error, restored.ok ? QString() : restored.error),
                   relaunched.ok ? QString() : relaunched.error
               )
    };
  }

  const auto launched = m_operations->relaunch(bundleInfo.absoluteFilePath());
  if (!launched.ok) {
    const auto movedNewAside = m_operations->rename(bundleInfo.absoluteFilePath(), candidate);
    const auto restored = m_operations->rename(backup, bundleInfo.absoluteFilePath());
    m_operations->removeRecursively(candidate);
    const auto relaunchedOld = m_operations->relaunch(bundleInfo.absoluteFilePath());
    return {
        false, combinedFailure(
                   combinedFailure(
                       combinedFailure(launched.error, movedNewAside.ok ? QString() : movedNewAside.error),
                       restored.ok ? QString() : restored.error
                   ),
                   relaunchedOld.ok ? QString() : relaunchedOld.error
               )
    };
  }

  m_operations->removeRecursively(backup);
  m_operations->removeRecursively(QFileInfo(staged->manifestPath).absolutePath());
  return {true, {}};
}

std::unique_ptr<IUpdateInstaller> createPlatformUpdateInstaller()
{
#if defined(Q_OS_MACOS)
  const auto executable = QCoreApplication::applicationFilePath();
  const bool packageManaged = isHomebrewManagedPath(executable) ||
                              QFileInfo::exists(QStringLiteral("/opt/homebrew/Caskroom/deskflow")) ||
                              QFileInfo::exists(QStringLiteral("/usr/local/Caskroom/deskflow"));
  return std::make_unique<MacUpdateInstaller>(executable, packageManaged);
#else
  return {};
#endif
}

UpdateInstallResult applyPlatformUpdate(const QString &manifestPath, qint64 parentProcessId)
{
#if defined(Q_OS_MACOS)
  const auto executable = QCoreApplication::applicationFilePath();
  const bool packageManaged = isHomebrewManagedPath(executable) ||
                              QFileInfo::exists(QStringLiteral("/opt/homebrew/Caskroom/deskflow")) ||
                              QFileInfo::exists(QStringLiteral("/usr/local/Caskroom/deskflow"));
  MacUpdateInstaller installer(executable, packageManaged);
  return installer.apply(manifestPath, parentProcessId);
#else
  Q_UNUSED(manifestPath)
  Q_UNUSED(parentProcessId)
  return {false, QStringLiteral("automatic update installation is not supported on this platform")};
#endif
}

} // namespace deskflow::gui
