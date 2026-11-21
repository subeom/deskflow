/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/MacUpdateInstaller.h"

#include "common/VersionInfo.h"
#include "gui/MacUpdateVerifier.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

#if defined(Q_OS_MACOS)
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/signal.h>
#endif

#include <cerrno>
#include <csignal>
#include <limits>
#include <optional>

namespace deskflow::gui {

namespace {

constexpr int kHelperReadyTimeoutMs = 5'000;
constexpr int kShutdownTimeoutMs = 120'000;
constexpr qint64 kMinimumInstallHeadroom = 128LL * 1024 * 1024;
constexpr qsizetype kMaximumReceiptBytes = 16 * 1024;
constexpr int kRelaunchTimeoutMs = 30'000;

QString currentStableVersion()
{
  const auto parts = QString::fromLatin1(kVersion).split(QLatin1Char('.'));
  return parts.size() < 3 ? QString() : parts.mid(0, 3).join(QLatin1Char('.'));
}

struct UpdateReceipt
{
  QString transactionId;
  QString token;
  QString manifestPath;
  qint64 parentPid = 0;
  QString parentStartIdentity;
  qint64 helperPid = 0;
  QString state;
};

class QProcessUpdateLauncher final : public IUpdateProcessLauncher
{
public:
  UpdateHelperLaunchResult launch(
      const QString &program, const QStringList &arguments
  ) override
  {
    qint64 processId = 0;
    if (!QProcess::startDetached(
            program, arguments, QFileInfo(program).absolutePath(), &processId
        )) {
      return {false, 0, QStringLiteral("failed to start detached update helper")};
    }
    return {true, processId, {}};
  }
};

QString updateProcessStartIdentity(qint64 processId)
{
#if defined(Q_OS_MACOS)
  if (processId <= 0 || processId > std::numeric_limits<pid_t>::max()) {
    return {};
  }
  proc_bsdinfo info{};
  const auto bytes = proc_pidinfo(
      static_cast<int>(processId), PROC_PIDTBSDINFO, 0, &info, sizeof(info)
  );
  if (bytes != sizeof(info)) {
    return {};
  }
  return QStringLiteral("%1:%2")
      .arg(QString::number(info.pbi_start_tvsec), QString::number(info.pbi_start_tvusec));
#else
  Q_UNUSED(processId)
  return {};
#endif
}

std::optional<QString> deskflowBundlePath(const QString &executablePath)
{
  const QFileInfo executable(executablePath);
  const auto macosDirectory = QFileInfo(executable.absolutePath());
  if (macosDirectory.fileName() != QStringLiteral("MacOS") ||
      executable.fileName() != QStringLiteral("Deskflow")) {
    return std::nullopt;
  }
  const auto contents = macosDirectory.absolutePath();
  if (QFileInfo(contents).fileName() != QStringLiteral("Contents")) {
    return std::nullopt;
  }
  const auto bundle = QFileInfo(contents).absolutePath();
  if (QFileInfo(bundle).fileName() != QStringLiteral("Deskflow.app")) {
    return std::nullopt;
  }
  return bundle;
}

bool isCanonicalInside(const QString &rootPath, const QString &path)
{
  const auto root = QFileInfo(rootPath).canonicalFilePath();
  const auto canonical = QFileInfo(path).canonicalFilePath();
  return !root.isEmpty() && !canonical.isEmpty() &&
         canonical.startsWith(root + QLatin1Char('/'));
}

bool regularReceipt(const QString &path)
{
  const QFileInfo info(path);
  if (info.isSymLink() || !info.isFile()) {
    return false;
  }
  return !info.permissions().testFlag(QFileDevice::WriteGroup) &&
         !info.permissions().testFlag(QFileDevice::WriteOther);
}

std::optional<UpdateReceipt> readReceipt(const QString &path)
{
  if (QFileInfo(path).fileName().size() < 1 || !regularReceipt(path)) {
    return std::nullopt;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > kMaximumReceiptBytes) {
    return std::nullopt;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return std::nullopt;
  }
  const auto object = document.object();
  UpdateReceipt receipt{
      object.value(QStringLiteral("transactionId")).toString(),
      object.value(QStringLiteral("token")).toString(),
      object.value(QStringLiteral("manifestPath")).toString(),
      object.value(QStringLiteral("parentPid")).toInteger(),
      object.value(QStringLiteral("parentStartIdentity")).toString(),
      object.value(QStringLiteral("helperPid")).toInteger(),
      object.value(QStringLiteral("state")).toString(),
  };
  if (object.value(QStringLiteral("schemaVersion")).toInteger() != 1 ||
      receipt.transactionId.isEmpty() || receipt.token.isEmpty() ||
      receipt.manifestPath.isEmpty() || receipt.parentPid <= 1 ||
      receipt.parentStartIdentity.isEmpty() || receipt.helperPid < 0 ||
      receipt.state.isEmpty()) {
    return std::nullopt;
  }
  return receipt;
}

bool writeReceipt(const QString &path, const UpdateReceipt &receipt)
{
  const QJsonObject object{
      {QStringLiteral("schemaVersion"), 1},
      {QStringLiteral("transactionId"), receipt.transactionId},
      {QStringLiteral("token"), receipt.token},
      {QStringLiteral("manifestPath"), receipt.manifestPath},
      {QStringLiteral("parentPid"), receipt.parentPid},
      {QStringLiteral("parentStartIdentity"), receipt.parentStartIdentity},
      {QStringLiteral("helperPid"), receipt.helperPid},
      {QStringLiteral("state"), receipt.state},
  };
  QSaveFile file(path);
  const auto data = QJsonDocument(object).toJson(QJsonDocument::Compact);
  if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() ||
      !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
      !file.commit()) {
    return false;
  }
  return true;
}

UpdateInstallResult commandFailure(
    const QString &description, const UpdateCommandResult &result
)
{
  const auto failure =
      updateCommandFailure(description, result);
  return {failure.ok, failure.error, {}};
}

QString combinedFailure(const QString &first, const QString &second)
{
  if (second.isEmpty()) {
    return first;
  }
  return first.isEmpty() ? second : QStringLiteral("%1; %2").arg(first, second);
}

class MacUpdateApplyOperations final : public IUpdateApplyOperations
{
public:
  MacUpdateApplyOperations()
      : m_verifier(createUpdateCommandRunner()), m_runner(createUpdateCommandRunner())
  {
  }

  UpdateInstallResult waitForHelperReady(
      const QString &receiptPath, qint64 helperPid, int timeoutMs
  ) override
  {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
      const auto receipt = readReceipt(receiptPath);
      if (receipt && receipt->state == QStringLiteral("helper-ready") &&
          receipt->helperPid == helperPid) {
        return {true, {}, receiptPath};
      }
      QThread::msleep(50);
    }
    return {false, QStringLiteral("update helper did not become ready"), receiptPath};
  }

  UpdateInstallResult waitForShutdown(
      const QString &receiptPath, qint64 parentPid,
      const QString &startIdentity, int timeoutMs
  ) override
  {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
      const auto receipt = readReceipt(receiptPath);
      if (receipt && receipt->state == QStringLiteral("shutdown-complete") &&
          receipt->parentPid == parentPid &&
          receipt->parentStartIdentity == startIdentity) {
        return {true, {}, receiptPath};
      }
      QThread::msleep(50);
    }
    return {false, QStringLiteral("timed out waiting for Deskflow shutdown"), receiptPath};
  }

  UpdateInstallResult waitForProcessExit(
      qint64 processId, const QString &startIdentity, int timeoutMs
  ) override
  {
    if (processId <= 1 || processId > std::numeric_limits<pid_t>::max()) {
      return {false, QStringLiteral("invalid update parent process"), {}};
    }
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
      const auto identity = updateProcessStartIdentity(processId);
      if (identity.isEmpty()) {
        if (kill(static_cast<pid_t>(processId), 0) != 0 && errno == ESRCH) {
          return {true, {}, {}};
        }
      } else if (identity != startIdentity) {
        return {false, QStringLiteral("the update parent PID was reused"), {}};
      }
      QThread::msleep(100);
    }
    return {false, QStringLiteral("timed out waiting for Deskflow to exit"), {}};
  }

  UpdateInstallResult verifyDiskImage(const StagedUpdate &staged) override
  {
    const auto result = m_verifier.verify(staged);
    return {result.ok, result.error, staged.manifestPath};
  }

  UpdateInstallResult mountDiskImage(
      const QString &dmg, const QString &mountPoint
  ) override
  {
    const auto result = m_runner->run(
        QStringLiteral("/usr/bin/hdiutil"),
        {QStringLiteral("attach"), dmg, QStringLiteral("-readonly"),
         QStringLiteral("-nobrowse"), QStringLiteral("-noautoopen"),
         QStringLiteral("-mountpoint"), mountPoint},
        120'000
    );
    return commandFailure(QStringLiteral("mounting update disk image"), result);
  }

  UpdateInstallResult copyApplication(
      const QString &mountedApp, const QString &destination
  ) override
  {
    const QFileInfo source(mountedApp);
    const QFileInfo target(destination);
    if (source.isSymLink() || !source.isDir() ||
        source.fileName() != QStringLiteral("Deskflow.app") ||
        target.exists() || target.isSymLink()) {
      return {false, QStringLiteral("mounted update app path is invalid"), {}};
    }
    const auto result = m_runner->run(
        QStringLiteral("/usr/bin/ditto"), {source.absoluteFilePath(), destination},
        120'000
    );
    return commandFailure(QStringLiteral("copying update app bundle"), result);
  }

  UpdateInstallResult verifyApplication(
      const QString &path, const QString &expectedVersion
  ) override
  {
    const auto result = m_verifier.verifyApplication(path, expectedVersion);
    return {result.ok, result.error, {}};
  }

  UpdateInstallResult removeQuarantine(const QString &candidate) override
  {
    const auto remove = m_runner->run(
        QStringLiteral("/usr/bin/xattr"),
        {QStringLiteral("-dr"), QStringLiteral("com.apple.quarantine"), candidate},
        120'000
    );
    if (!updateCommandSucceeded(remove)) {
      return commandFailure(QStringLiteral("removing verified quarantine attribute"), remove);
    }
    const auto inspect = m_runner->run(
        QStringLiteral("/usr/bin/xattr"), {QStringLiteral("-l"), candidate}, 120'000
    );
    if (!updateCommandSucceeded(inspect) ||
        QString::fromUtf8(
            inspect.standardOutput + inspect.standardError
        ).contains(QStringLiteral("com.apple.quarantine"))) {
      return {
          false,
          QStringLiteral("verified quarantine attribute could not be removed"),
          {}
      };
    }
    return {true, {}, {}};
  }

  UpdateInstallResult detachDiskImage(const QString &mountPoint) override
  {
    const auto result = m_runner->run(
        QStringLiteral("/usr/bin/hdiutil"),
        {QStringLiteral("detach"), mountPoint, QStringLiteral("-force")},
        120'000
    );
    return commandFailure(QStringLiteral("unmounting update disk image"), result);
  }

  UpdateInstallResult rename(
      const QString &source, const QString &destination
  ) override
  {
    const QFileInfo target(destination);
    if (target.exists() || target.isSymLink() || !QDir().rename(source, destination)) {
      return {false, QStringLiteral("failed to rename update bundle"), {}};
    }
    return {true, {}, {}};
  }

  UpdateInstallResult relaunchAndWait(
      const QString &bundlePath, const QString &receiptPath,
      const QString &expectedVersion
  ) override
  {
    const auto executable = QDir(bundlePath).filePath(QStringLiteral("Contents/MacOS/Deskflow"));
    const auto transactionPath = QFileInfo(receiptPath).absolutePath();
    const auto token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto handshakePath =
        QDir(transactionPath).filePath(QStringLiteral("handshake-%1.json").arg(token));
    qint64 processId = 0;
    if (!QProcess::startDetached(
            executable,
            {QStringLiteral("--update-receipt=%1").arg(receiptPath),
             QStringLiteral("--update-handshake=%1").arg(handshakePath),
             QStringLiteral("--update-handshake-token=%1").arg(token)},
            bundlePath, &processId
        )) {
      return {false, QStringLiteral("failed to relaunch Deskflow"), receiptPath};
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kRelaunchTimeoutMs) {
      QFile handshake(handshakePath);
      if (handshake.open(QIODevice::ReadOnly) && handshake.size() <= kMaximumReceiptBytes) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(handshake.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
          const auto object = document.object();
          if (object.value(QStringLiteral("state")).toString() ==
                  QStringLiteral("ready") &&
              object.value(QStringLiteral("pid")).toInteger() == processId &&
              object.value(QStringLiteral("token")).toString() == token &&
              (expectedVersion.isEmpty() ||
               object.value(QStringLiteral("version")).toString() == expectedVersion)) {
            QFile::remove(handshakePath);
            return {true, {}, receiptPath};
          }
        }
      }
      if (kill(static_cast<pid_t>(processId), 0) != 0 && errno == ESRCH) {
        return {false, QStringLiteral("Deskflow exited before the ready handshake"), receiptPath};
      }
      QThread::msleep(100);
    }
    kill(static_cast<pid_t>(processId), SIGTERM);
    QFile::remove(handshakePath);
    return {false, QStringLiteral("Deskflow relaunch readiness timed out"), receiptPath};
  }

  bool removeRecursively(const QString &path) override
  {
    const QFileInfo info(path);
    if (info.isSymLink() || !info.isDir()) {
      return QFile::remove(path);
    }
    return QDir(path).removeRecursively();
  }

  MacUpdateVerifier m_verifier;
  std::unique_ptr<IUpdateCommandRunner> m_runner;
};

} // namespace

MacUpdateInstaller::MacUpdateInstaller(
    QString executablePath, QString cacheRoot, bool packageManaged, qint64 processId,
    QString processStartIdentity, std::unique_ptr<IUpdateProcessLauncher> launcher,
    std::unique_ptr<IUpdateApplyOperations> operations,
    std::unique_ptr<IUpdateVerifier> verifier
)
    : m_executablePath(std::move(executablePath)),
      m_cacheRoot(QDir::cleanPath(QFileInfo(cacheRoot).absoluteFilePath())),
      m_packageManaged(packageManaged),
      m_processId(processId > 0 ? processId : QCoreApplication::applicationPid()),
      m_processStartIdentity(
          processStartIdentity.isEmpty()
              ? updateProcessStartIdentity(m_processId)
              : std::move(processStartIdentity)
      ),
      m_launcher(std::move(launcher)),
      m_operations(std::move(operations)),
      m_verifier(std::move(verifier))
{
  if (!m_launcher) {
    m_launcher = std::make_unique<QProcessUpdateLauncher>();
  }
  if (!m_operations) {
    m_operations = std::make_unique<MacUpdateApplyOperations>();
  }
  if (!m_verifier) {
    m_verifier = std::make_unique<MacUpdateVerifier>();
  }
}

UpdateInstallResult MacUpdateInstaller::preflight(const StagedUpdate &staged)
{
  if (m_packageManaged) {
    return {false, QStringLiteral("package-managed Deskflow must be updated manually"), {}};
  }
  const auto bundle = deskflowBundlePath(m_executablePath);
  if (!bundle) {
    return {false, QStringLiteral("update helper is not running from Deskflow.app"), {}};
  }
  const QFileInfo bundleInfo(*bundle);
  if (bundleInfo.isSymLink() || !bundleInfo.isDir() ||
      bundleInfo.absoluteFilePath().startsWith(QStringLiteral("/Volumes/")) ||
      !bundleInfo.isWritable() ||
      !QFileInfo(bundleInfo.dir().absolutePath()).isWritable() ||
      bundleInfo.canonicalFilePath() != bundleInfo.absoluteFilePath()) {
    return {false, QStringLiteral("Deskflow app bundle is not directly writable"), {}};
  }
  if (!isCanonicalInside(m_cacheRoot, staged.manifestPath) ||
      !isCanonicalInside(m_cacheRoot, staged.installerPath) ||
      m_processStartIdentity.isEmpty()) {
    return {false, QStringLiteral("update stage or process identity is unsafe"), {}};
  }
  const auto loaded = UpdateCache::load(staged.manifestPath);
  if (!loaded || loaded->installerPath != staged.installerPath) {
    return {false, QStringLiteral("staged update manifest is invalid"), {}};
  }
  const QFileInfo installer(staged.installerPath);
  const auto requiredSpace = qint64(installer.size()) * 2 + kMinimumInstallHeadroom;
  const QStorageInfo storage(bundleInfo.dir().absolutePath());
  if (!storage.isValid() || storage.bytesAvailable() < requiredSpace) {
    return {false, QStringLiteral("not enough free space to install the update"), {}};
  }
  const auto verification = m_verifier->verify(staged);
  if (!verification.ok) {
    return {false, verification.error, staged.manifestPath};
  }
  return {true, {}, {}};
}

UpdateInstallResult MacUpdateInstaller::start(
    const StagedUpdate &staged, bool explicitConfirmation
)
{
  if (!explicitConfirmation) {
    return {false, QStringLiteral("update installation was not confirmed"), {}};
  }
  const auto checked = preflight(staged);
  if (!checked.ok) {
    return checked;
  }

  const auto transactions = QDir(m_cacheRoot).filePath(QStringLiteral("transactions"));
  if (!QDir().mkpath(transactions) ||
      !QFile::setPermissions(
          transactions,
          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
      )) {
    return {false, QStringLiteral("cannot create update transaction directory"), {}};
  }
  const auto transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const auto receiptPath =
      QDir(transactions).filePath(QStringLiteral("%1.json").arg(transactionId));
  UpdateReceipt receipt{
      transactionId,
      QUuid::createUuid().toString(QUuid::WithoutBraces),
      QFileInfo(staged.manifestPath).absoluteFilePath(),
      m_processId,
      m_processStartIdentity,
      0,
      QStringLiteral("awaiting-helper"),
  };
  if (!writeReceipt(receiptPath, receipt)) {
    return {false, QStringLiteral("cannot create update transaction receipt"), {}};
  }

  const auto launched = m_launcher->launch(
      m_executablePath,
      {QStringLiteral("--apply-update=%1").arg(staged.manifestPath),
       QStringLiteral("--update-parent-pid=%1").arg(m_processId),
       QStringLiteral("--update-parent-start=%1").arg(m_processStartIdentity),
       QStringLiteral("--update-receipt=%1").arg(receiptPath)}
  );
  if (!launched.started) {
    receipt.state = QStringLiteral("failed");
    writeReceipt(receiptPath, receipt);
    return {
        false,
        launched.error.isEmpty() ? QStringLiteral("failed to start update helper")
                                 : launched.error,
        receiptPath
    };
  }
  receipt.helperPid = launched.processId;
  if (!writeReceipt(receiptPath, receipt)) {
    return {false, QStringLiteral("cannot record update helper process"), receiptPath};
  }
  const auto ready =
      m_operations->waitForHelperReady(receiptPath, launched.processId, kHelperReadyTimeoutMs);
  if (!ready.ok) {
    receipt.state = QStringLiteral("failed");
    writeReceipt(receiptPath, receipt);
    return {false, ready.error, receiptPath};
  }
  return {true, {}, receiptPath};
}

UpdateInstallResult MacUpdateInstaller::apply(
    const QString &manifestPath, qint64 parentPid,
    const QString &parentStartIdentity, const QString &receiptPath
)
{
  if (m_packageManaged) {
    return {false, QStringLiteral("package-managed Deskflow must be updated manually"), receiptPath};
  }
  const auto bundle = deskflowBundlePath(m_executablePath);
  if (!bundle || m_processId <= 1 || parentPid <= 1 || parentPid == m_processId ||
      parentStartIdentity.isEmpty()) {
    return {false, QStringLiteral("invalid update helper process"), receiptPath};
  }
  if (!isCanonicalInside(m_cacheRoot, manifestPath) ||
      !isCanonicalInside(m_cacheRoot, receiptPath)) {
    return {false, QStringLiteral("update transaction is outside the cache"), receiptPath};
  }
  auto receipt = readReceipt(receiptPath);
  if (!receipt || receipt->manifestPath != QFileInfo(manifestPath).absoluteFilePath() ||
      receipt->parentPid != parentPid ||
      receipt->parentStartIdentity != parentStartIdentity ||
      receipt->helperPid != m_processId) {
    return {false, QStringLiteral("update transaction receipt does not match"), receiptPath};
  }

  receipt->state = QStringLiteral("helper-ready");
  receipt->helperPid = m_processId;
  if (!writeReceipt(receiptPath, *receipt)) {
    return {false, QStringLiteral("cannot publish update helper readiness"), receiptPath};
  }

  const auto stopped = m_operations->waitForShutdown(
      receiptPath, parentPid, parentStartIdentity, kShutdownTimeoutMs
  );
  if (!stopped.ok) {
    receipt->state = QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    return {false, stopped.error, receiptPath};
  }
  const auto parentExited = m_operations->waitForProcessExit(
      parentPid, parentStartIdentity, kShutdownTimeoutMs
  );
  if (!parentExited.ok) {
    receipt->state = QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    return {false, parentExited.error, receiptPath};
  }

  const auto staged = UpdateCache::load(manifestPath);
  if (!staged || !isCanonicalInside(m_cacheRoot, staged->installerPath)) {
    receipt->state = QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    return {false, QStringLiteral("staged update is invalid"), receiptPath};
  }
  const auto verified = m_operations->verifyDiskImage(*staged);
  if (!verified.ok) {
    receipt->state = QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    const auto relaunched =
        m_operations->relaunchAndWait(*bundle, receiptPath, currentStableVersion());
    return {
        false,
        combinedFailure(verified.error, relaunched.ok ? QString() : relaunched.error),
        receiptPath
    };
  }

  QTemporaryDir temporary(
      QStringLiteral("%1/deskflow-update-apply-XXXXXX").arg(QDir::tempPath())
  );
  if (!temporary.isValid()) {
    receipt->state = QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    return {false, QStringLiteral("cannot create update helper workspace"), receiptPath};
  }
  const auto mountPoint = QDir(temporary.path()).filePath(QStringLiteral("volume"));
  if (!QDir().mkpath(mountPoint)) {
    return {false, QStringLiteral("cannot create update mount point"), receiptPath};
  }

  receipt->state = QStringLiteral("applying");
  writeReceipt(receiptPath, *receipt);
  const auto suffix = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const auto candidate =
      QFileInfo(*bundle).dir().filePath(QStringLiteral(".Deskflow-update-new-%1.app").arg(suffix));
  const auto backup =
      QFileInfo(*bundle).dir().filePath(QStringLiteral(".Deskflow-update-backup-%1.app").arg(suffix));
  const auto failedCandidate =
      QFileInfo(*bundle).dir().filePath(QStringLiteral(".Deskflow-update-failed-%1.app").arg(suffix));

  const auto mounted = m_operations->mountDiskImage(staged->installerPath, mountPoint);
  if (!mounted.ok) {
    receipt->state = QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    const auto relaunched =
        m_operations->relaunchAndWait(*bundle, receiptPath, currentStableVersion());
    return {
        false,
        combinedFailure(mounted.error, relaunched.ok ? QString() : relaunched.error),
        receiptPath
    };
  }

  const auto mountedApp = QDir(mountPoint).filePath(QStringLiteral("Deskflow.app"));
  auto prepared = m_operations->copyApplication(mountedApp, candidate);
  if (prepared.ok) {
    prepared = m_operations->verifyApplication(candidate, staged->version);
  }
  if (prepared.ok) {
    prepared = m_operations->removeQuarantine(candidate);
  }
  const auto detached = m_operations->detachDiskImage(mountPoint);
  if (!prepared.ok || !detached.ok) {
    m_operations->removeRecursively(candidate);
    receipt->state = QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    const auto relaunched =
        m_operations->relaunchAndWait(*bundle, receiptPath, currentStableVersion());
    return {
        false,
        combinedFailure(
            combinedFailure(prepared.error, detached.error),
            relaunched.ok ? QString() : relaunched.error
        ),
        receiptPath
    };
  }

  const auto movedOld = m_operations->rename(*bundle, backup);
  if (!movedOld.ok) {
    m_operations->removeRecursively(candidate);
    receipt->state = QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    const auto relaunched =
        m_operations->relaunchAndWait(*bundle, receiptPath, currentStableVersion());
    return {
        false,
        combinedFailure(movedOld.error, relaunched.ok ? QString() : relaunched.error),
        receiptPath
    };
  }

  const auto movedNew = m_operations->rename(candidate, *bundle);
  if (!movedNew.ok) {
    const auto restored = m_operations->rename(backup, *bundle);
    m_operations->removeRecursively(candidate);
    const auto relaunched =
        m_operations->relaunchAndWait(*bundle, receiptPath, currentStableVersion());
    receipt->state = restored.ok && relaunched.ok ? QStringLiteral("rolled-back")
                                                  : QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    return {
        false,
        combinedFailure(
            combinedFailure(movedNew.error, restored.error), relaunched.error
        ),
        receiptPath
    };
  }

  const auto newLaunch =
      m_operations->relaunchAndWait(*bundle, receiptPath, staged->version);
  if (!newLaunch.ok) {
    const auto movedNewAside = m_operations->rename(*bundle, failedCandidate);
    const auto restored = m_operations->rename(backup, *bundle);
    const auto relaunchedOld =
        m_operations->relaunchAndWait(*bundle, receiptPath, currentStableVersion());
    if (movedNewAside.ok && restored.ok && relaunchedOld.ok) {
      m_operations->removeRecursively(failedCandidate);
    }
    receipt->state = movedNewAside.ok && restored.ok && relaunchedOld.ok
                         ? QStringLiteral("rolled-back")
                         : QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    return {
        false,
        combinedFailure(
            combinedFailure(
                combinedFailure(newLaunch.error, movedNewAside.error), restored.error
            ),
            relaunchedOld.error
        ),
        receiptPath
    };
  }

  receipt->state = QStringLiteral("success");
  if (!writeReceipt(receiptPath, *receipt)) {
    const auto movedNewAside = m_operations->rename(*bundle, failedCandidate);
    const auto restored = m_operations->rename(backup, *bundle);
    const auto relaunchedOld =
        m_operations->relaunchAndWait(*bundle, receiptPath, currentStableVersion());
    if (movedNewAside.ok && restored.ok && relaunchedOld.ok) {
      m_operations->removeRecursively(failedCandidate);
    }
    receipt->state = movedNewAside.ok && restored.ok && relaunchedOld.ok
                         ? QStringLiteral("rolled-back")
                         : QStringLiteral("failed");
    writeReceipt(receiptPath, *receipt);
    return {
        false,
        combinedFailure(
            combinedFailure(
                QStringLiteral("cannot publish successful update receipt"),
                movedNewAside.error
            ),
            combinedFailure(restored.error, relaunchedOld.error)
        ),
        receiptPath
    };
  }
  m_operations->removeRecursively(backup);
  m_operations->removeRecursively(QFileInfo(staged->manifestPath).absolutePath());
  return {true, {}, receiptPath};
}

UpdateInstallResult markUpdateReceiptState(
    const QString &receiptPath, const QString &state
)
{
  auto receipt = readReceipt(receiptPath);
  if (!receipt || state.isEmpty()) {
    return {false, QStringLiteral("update transaction receipt is invalid"), receiptPath};
  }
  receipt->state = state;
  if (!writeReceipt(receiptPath, *receipt)) {
    return {false, QStringLiteral("cannot update transaction receipt"), receiptPath};
  }
  return {true, {}, receiptPath};
}

} // namespace deskflow::gui
