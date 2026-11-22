/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateInstallerTests.h"

#include "gui/MacUpdateInstaller.h"
#include "gui/UpdateCache.h"
#include "common/Settings.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <algorithm>
#include <cerrno>
#include <memory>

using deskflow::gui::IUpdateApplyOperations;
using deskflow::gui::IUpdateProcessLauncher;
using deskflow::gui::IUpdateVerifier;
using deskflow::gui::MacUpdateInstaller;
using deskflow::gui::StagedUpdate;
using deskflow::gui::UpdateHelperLaunchResult;
using deskflow::gui::UpdateInstallResult;
using deskflow::gui::UpdateVerifierResult;

namespace {

constexpr auto kInstallerName = "deskflow-2.0.6-macos-arm64.dmg";

QString digest(const QByteArray &data)
{
  return QStringLiteral("sha256:%1").arg(
      QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex())
  );
}

bool writeFile(const QString &path, const QByteArray &contents)
{
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}

class FakeVerifier final : public IUpdateVerifier
{
public:
  UpdateVerifierResult verify(const StagedUpdate &) override
  {
    ++verifyCalls;
    return result;
  }

  UpdateVerifierResult result{true, {}};
  int verifyCalls = 0;
};

class FakeLauncher final : public IUpdateProcessLauncher
{
public:
  UpdateHelperLaunchResult launch(
      const QString &program, const QStringList &arguments
  ) override
  {
    launchedProgram = program;
    launchedArguments = arguments;
    return result;
  }

  UpdateHelperLaunchResult result{true, 4243, {}};
  QString launchedProgram;
  QStringList launchedArguments;
};

class FakeApplyOperations final : public IUpdateApplyOperations
{
public:
  UpdateInstallResult waitForHelperReady(
      const QString &receiptPath, qint64 helperPid, int timeoutMs
  ) override
  {
    operations.append(QStringLiteral("helper-ready"));
    helperReceipt = receiptPath;
    observedPid = helperPid;
    observedTimeout = timeoutMs;
    QFile receipt(receiptPath);
    if (!receipt.open(QIODevice::ReadOnly)) {
      return {false, QStringLiteral("cannot read helper receipt"), receiptPath};
    }
    const auto object =
        QJsonDocument::fromJson(receipt.readAll()).object();
    helperPidBeforeClaim = object.value(QStringLiteral("helperPid")).toInteger();
    const auto claimed = deskflow::gui::claimUpdateReceipt(
        receiptPath, object.value(QStringLiteral("token")).toString(), helperPid,
        QStringLiteral("helper-start")
    );
    if (!claimed.ok) {
      return claimed;
    }
    return result(QStringLiteral("helper-ready"));
  }

  UpdateInstallResult waitForShutdown(
      const QString &receiptPath, qint64 parentPid, const QString &startIdentity, int timeoutMs
  ) override
  {
    operations.append(QStringLiteral("shutdown"));
    shutdownReceipt = receiptPath;
    parentPidSeen = parentPid;
    startIdentitySeen = startIdentity;
    observedTimeout = timeoutMs;
    return result(QStringLiteral("shutdown"));
  }

  deskflow::gui::UpdateParentState parentState(
      qint64, const QString &
  ) override
  {
    return observedParentState;
  }

  UpdateInstallResult waitForProcessExit(
      qint64 processId, const QString &startIdentity, int timeoutMs
  ) override
  {
    operations.append(QStringLiteral("parent-exit"));
    parentPidSeen = processId;
    startIdentitySeen = startIdentity;
    observedTimeout = timeoutMs;
    return result(QStringLiteral("parent-exit"));
  }

  UpdateInstallResult createWorkspace(
      deskflow::gui::UpdateHelperWorkspace &workspace
  ) override
  {
    operations.append(QStringLiteral("workspace"));
    workspace.rootPath = QDir(workspaceDirectory.path()).filePath(QStringLiteral("root"));
    workspace.mountPoint = QDir(workspace.rootPath).filePath(QStringLiteral("volume"));
    QDir().mkpath(workspace.mountPoint);
    return result(QStringLiteral("workspace"));
  }

  UpdateInstallResult verifyDiskImage(const StagedUpdate &) override
  {
    operations.append(QStringLiteral("verify-dmg"));
    return result(QStringLiteral("verify-dmg"));
  }

  UpdateInstallResult mountDiskImage(
      const QString &, const QString &mountPoint
  ) override
  {
    operations.append(QStringLiteral("mount"));
    mountedPath = mountPoint;
    return result(QStringLiteral("mount"));
  }

  UpdateInstallResult copyApplication(
      const QString &, const QString &candidate
  ) override
  {
    operations.append(QStringLiteral("copy"));
    candidatePath = candidate;
    return result(QStringLiteral("copy"));
  }

  UpdateInstallResult verifyApplication(
      const QString &, const QString &
  ) override
  {
    operations.append(QStringLiteral("verify-app"));
    ++verificationCalls;
    if (mutateIdentityDuringFinalVerification && verificationCalls == 2) {
      identityInode = 999;
    }
    return result(QStringLiteral("verify-app"));
  }

  UpdateInstallResult removeQuarantine(const QString &) override
  {
    operations.append(QStringLiteral("quarantine"));
    if (corruptReceiptBeforeNewLaunch && !shutdownReceipt.isEmpty()) {
      QFile::remove(shutdownReceipt);
      QDir().mkpath(shutdownReceipt);
    }
    if (mutateParentBeforeRename && !parentPathToMutate.isEmpty()) {
      QFile::setPermissions(
          parentPathToMutate,
          QFileInfo(parentPathToMutate).permissions() | QFileDevice::WriteOther
      );
    }
    return result(QStringLiteral("quarantine"));
  }

  UpdateInstallResult detachDiskImage(const QString &) override
  {
    operations.append(QStringLiteral("detach"));
    return result(QStringLiteral("detach"));
  }

  UpdateInstallResult rename(const QString &source, const QString &destination) override
  {
    const auto detail = QStringLiteral("rename:%1:%2").arg(
        QFileInfo(source).fileName(), QFileInfo(destination).fileName()
    );
    operations.append(detail);
    renameCalls.append({source, destination});
    QString failureKey;
    if (QFileInfo(source).fileName().contains(QStringLiteral("update-new"))) {
      failureKey = QStringLiteral("rename-new");
    } else if (QFileInfo(source).fileName() == QStringLiteral("Deskflow.app")) {
      failureKey = QStringLiteral("rename-old");
    } else {
      failureKey = QStringLiteral("rename-restore");
    }
    return result(failureKey);
  }

  UpdateInstallResult relaunchAndWait(
      const QString &bundlePath, const QString &receiptPath, const QString &expectedVersion
  ) override
  {
    const auto failureKey = relaunchCount++ == 0
                                ? QStringLiteral("launch-new")
                                : QStringLiteral("launch-old");
    operations.append(QStringLiteral("launch:%1").arg(QFileInfo(bundlePath).fileName()));
    relaunchedBundles.append(bundlePath);
    QFile receipt(receiptPath);
    if (relaunchedBundles.size() == 1 && receipt.open(QIODevice::ReadOnly)) {
      stateBeforeNewLaunch =
          QJsonDocument::fromJson(receipt.readAll())
              .object()
              .value(QStringLiteral("state"))
              .toString();
    }
    relaunchReceipt = receiptPath;
    relaunchVersion = expectedVersion;
    if (corruptReceiptOnNewLaunch && relaunchedBundles.size() == 1) {
      QFile::remove(receiptPath);
      QDir().mkpath(receiptPath);
    }
    return result(failureKey);
  }

  bool removeRecursively(const QString &path) override
  {
    operations.append(QStringLiteral("remove:%1").arg(QFileInfo(path).fileName()));
    removed.append(path);
    return !failures.contains(QStringLiteral("remove"));
  }

  std::optional<deskflow::gui::UpdateFileIdentity> captureIdentity(
      const QString &path
  ) override
  {
    Q_UNUSED(path)
    operations.append(QStringLiteral("identity"));
    ++identityCalls;
    if (identityMismatchAt == identityCalls) {
      identityInode = 999;
    }
    return deskflow::gui::UpdateFileIdentity{true, 1, identityInode, 10};
  }

  UpdateInstallResult result(const QString &operation)
  {
    if (!failures.contains(operation)) {
      return {true, {}};
    }
    return {false, QStringLiteral("injected failure: %1").arg(operation)};
  }

  QStringList operations;
  QSet<QString> failures;
  QString helperReceipt;
  QString shutdownReceipt;
  QString relaunchReceipt;
  QString relaunchVersion;
  QString mountedPath;
  QString candidatePath;
  QString startIdentitySeen;
  qint64 observedPid = 0;
  qint64 parentPidSeen = 0;
  int observedTimeout = 0;
  QList<QPair<QString, QString>> renameCalls;
  QStringList relaunchedBundles;
  QStringList removed;
  int relaunchCount = 0;
  bool corruptReceiptOnNewLaunch = false;
  bool corruptReceiptBeforeNewLaunch = false;
  bool mutateParentBeforeRename = false;
  deskflow::gui::UpdateParentState observedParentState =
      deskflow::gui::UpdateParentState::Running;
  QString parentPathToMutate;
  QString stateBeforeNewLaunch;
  qint64 helperPidBeforeClaim = -1;
  int identityCalls = 0;
  int identityMismatchAt = -1;
  int verificationCalls = 0;
  quint64 identityInode = 42;
  bool mutateIdentityDuringFinalVerification = false;
  QTemporaryDir workspaceDirectory;
};

struct Fixture
{
  QString executable;
  QString bundle;
  QString cacheRoot;
  QString manifestPath;
  QString parentStartIdentity;
  deskflow::gui::StagedUpdate staged;
  qint64 parentPid = 4242;
};

bool createFixture(QTemporaryDir &temporary, Fixture &fixture)
{
  fixture.bundle = QDir(temporary.path()).filePath(QStringLiteral("Deskflow.app"));
  fixture.executable =
      QDir(fixture.bundle).filePath(QStringLiteral("Contents/MacOS/Deskflow"));
  if (!QDir().mkpath(QFileInfo(fixture.executable).absolutePath()) ||
      !writeFile(fixture.executable, QByteArrayLiteral("old gui"))) {
    return false;
  }
  fixture.cacheRoot = QDir(temporary.path()).filePath(QStringLiteral("updates"));
  const QByteArray installer = QByteArrayLiteral("synthetic DMG");
  const auto sums =
      QStringLiteral("%1  %2\n").arg(digest(installer).mid(7), QString::fromLatin1(kInstallerName)).toUtf8();
  deskflow::gui::UpdatePlan plan{
      QStringLiteral("2.0.6"),
      QStringLiteral("v2.0.6"),
      QUrl(QStringLiteral("https://github.com/subeom/deskflow/releases/tag/v2.0.6")),
      deskflow::gui::UpdateInstallMode::AutomaticMac,
      deskflow::gui::UpdateAsset{
          QString::fromLatin1(kInstallerName),
          QUrl(),
          digest(installer),
          installer.size()
      },
      deskflow::gui::UpdateAsset{
          QStringLiteral("SHA256SUMS.txt"),
          QUrl(),
          digest(sums),
          sums.size()
      }
  };
  deskflow::gui::UpdateCache cache(fixture.cacheRoot);
  const auto stage = cache.stage(plan, installer, sums);
  if (!stage.ok) {
    return false;
  }
  fixture.staged = stage.staged;
  fixture.manifestPath = stage.staged.manifestPath;
  fixture.parentStartIdentity = QStringLiteral("parent-start-identity");
  return true;
}

qsizetype indexStartingWith(const QStringList &values, const QString &prefix)
{
  for (qsizetype index = 0; index < values.size(); ++index) {
    if (values.at(index).startsWith(prefix)) {
      return index;
    }
  }
  return -1;
}

std::unique_ptr<MacUpdateInstaller> parentInstaller(
    const Fixture &fixture, FakeLauncher *&launcher, FakeApplyOperations *&operations
)
{
  auto launcherOwner = std::make_unique<FakeLauncher>();
  launcher = launcherOwner.get();
  auto operationsOwner = std::make_unique<FakeApplyOperations>();
  operations = operationsOwner.get();
  auto verifier = std::make_unique<FakeVerifier>();
  return std::make_unique<MacUpdateInstaller>(
      fixture.executable, fixture.cacheRoot, false, fixture.parentPid,
      fixture.parentStartIdentity, std::move(launcherOwner), std::move(operationsOwner),
      std::move(verifier),
      [](const QString &) {
        return deskflow::gui::MacUpdateAclResult{true, 0, false, true, 0};
      }
  );
}

std::unique_ptr<MacUpdateInstaller> helperInstaller(
    const Fixture &fixture, FakeApplyOperations *&operations
)
{
  auto operationsOwner = std::make_unique<FakeApplyOperations>();
  operations = operationsOwner.get();
  return std::make_unique<MacUpdateInstaller>(
      fixture.executable, fixture.cacheRoot, false, 4243, QStringLiteral("helper-start"),
      std::make_unique<FakeLauncher>(), std::move(operationsOwner),
      std::make_unique<FakeVerifier>(),
      [](const QString &) {
        return deskflow::gui::MacUpdateAclResult{true, 0, false, true, 0};
      }
  );
}

} // namespace

void UpdateInstallerTests::launch_requiresExplicitConfirmationAndHelperReady()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *operations = nullptr;
  auto installer = parentInstaller(fixture, launcher, operations);
  QVERIFY(!installer->start(fixture.staged, false).ok);
  operations->failures.insert(QStringLiteral("helper-ready"));
  QVERIFY(!installer->start(fixture.staged, true).ok);
  operations->failures.clear();
  const auto started = installer->start(fixture.staged, true);
  QVERIFY2(started.ok, qPrintable(started.error));
  QVERIFY(!started.helperToken.isEmpty());
}

void UpdateInstallerTests::launch_refusesPackageManagedSymlinkAndWrongExecutable()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *operations = nullptr;
  auto installer = parentInstaller(fixture, launcher, operations);
  QVERIFY(installer->start(fixture.staged, true).ok);

  MacUpdateInstaller packageManaged(
      fixture.executable, fixture.cacheRoot, true, fixture.parentPid,
      fixture.parentStartIdentity, std::make_unique<FakeLauncher>(),
      std::make_unique<FakeApplyOperations>(), std::make_unique<FakeVerifier>()
  );
  QVERIFY(!packageManaged.start(fixture.staged, true).ok);

  MacUpdateInstaller wrongExecutable(
      QDir(temporary.path()).filePath(QStringLiteral("deskflow")), fixture.cacheRoot, false,
      fixture.parentPid, fixture.parentStartIdentity, std::make_unique<FakeLauncher>(),
      std::make_unique<FakeApplyOperations>(), std::make_unique<FakeVerifier>()
  );
  QVERIFY(!wrongExecutable.start(fixture.staged, true).ok);

  Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Service);
  QVERIFY(!installer->start(fixture.staged, true).ok);
  Settings::setValue(Settings::Core::ProcessMode, Settings::ProcessMode::Desktop);

  const auto originalPermissions = QFileInfo(temporary.path()).permissions();
  QVERIFY(QFile::setPermissions(
      temporary.path(), originalPermissions | QFileDevice::WriteOther
  ));
  QVERIFY(!installer->start(fixture.staged, true).ok);
  QVERIFY(QFile::setPermissions(temporary.path(), originalPermissions));

  const auto linkedBundle =
      QDir(temporary.path()).filePath(QStringLiteral("Deskflow-linked.app"));
  QVERIFY(QFile::link(fixture.bundle, linkedBundle));
  MacUpdateInstaller symlinked(
      QDir(linkedBundle).filePath(QStringLiteral("Contents/MacOS/Deskflow")),
      fixture.cacheRoot, false, fixture.parentPid, fixture.parentStartIdentity,
      std::make_unique<FakeLauncher>(), std::make_unique<FakeApplyOperations>(),
      std::make_unique<FakeVerifier>()
  );
  QVERIFY(!symlinked.start(fixture.staged, true).ok);
}

void UpdateInstallerTests::launch_bindsManifestReceiptPidAndProcessStartIdentity()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *operations = nullptr;
  auto installer = parentInstaller(fixture, launcher, operations);
  const auto result = installer->start(fixture.staged, true);
  QVERIFY(result.ok);
  QVERIFY(!result.helperToken.isEmpty());
  QCOMPARE(launcher->launchedProgram, fixture.executable);
  QVERIFY(launcher->launchedArguments.contains(
      QStringLiteral("--apply-update=%1").arg(fixture.manifestPath)
  ));
  QVERIFY(launcher->launchedArguments.contains(
      QStringLiteral("--update-parent-pid=%1").arg(fixture.parentPid)
  ));
  QVERIFY(launcher->launchedArguments.contains(
      QStringLiteral("--update-parent-start=parent-start-identity")
  ));
  QVERIFY(launcher->launchedArguments.contains(
      QStringLiteral("--update-receipt=%1").arg(result.receiptPath)
  ));
  QVERIFY(launcher->launchedArguments.contains(
      QStringLiteral("--update-helper-token=%1").arg(result.helperToken)
  ));
  QVERIFY(QFileInfo(result.receiptPath).isFile());
  QCOMPARE(operations->helperReceipt, result.receiptPath);
  QCOMPARE(operations->helperPidBeforeClaim, 0);
}

void UpdateInstallerTests::launch_helperSelfClaimsPidTokenAndStartIdentity()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *operations = nullptr;
  auto installer = parentInstaller(fixture, launcher, operations);
  const auto result = installer->start(fixture.staged, true);
  QVERIFY2(result.ok, qPrintable(result.error));
  QCOMPARE(operations->helperPidBeforeClaim, 0);
  QVERIFY(!result.helperToken.isEmpty());
  QCOMPARE(operations->observedPid, launcher->result.processId);
}

void UpdateInstallerTests::apply_waitsForParentExitAndShutdownReceipt()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  const auto result =
      helper->apply(
          fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
          start.receiptPath, start.helperToken
      );
  QVERIFY(result.ok);
  const auto shutdownIndex = operations->operations.indexOf(QStringLiteral("shutdown"));
  const auto exitIndex = operations->operations.indexOf(QStringLiteral("parent-exit"));
  const auto renameIndex =
      indexStartingWith(operations->operations, QStringLiteral("rename:Deskflow.app"));
  QVERIFY(shutdownIndex >= 0 && exitIndex > shutdownIndex && renameIndex > exitIndex);
  QCOMPARE(operations->parentPidSeen, fixture.parentPid);
  QCOMPARE(operations->startIdentitySeen, fixture.parentStartIdentity);
}

void UpdateInstallerTests::apply_reverifiesCopiesAndAtomicallySwaps()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  QVERIFY(helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  ).ok);
  QVERIFY(operations->operations.contains(QStringLiteral("verify-dmg")));
  QVERIFY(operations->operations.contains(QStringLiteral("verify-app")));
  QCOMPARE(operations->operations.filter(QRegularExpression(QStringLiteral("^rename:"))).size(), 2);
  QVERIFY(operations->operations.indexOf(QStringLiteral("detach")) <
          indexStartingWith(
              operations->operations, QStringLiteral("rename:Deskflow.app")
          ));
  QCOMPARE(operations->stateBeforeNewLaunch, QStringLiteral("installed-ready"));
}

void UpdateInstallerTests::apply_removesQuarantineOnlyAfterCandidateVerification()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->failures.insert(QStringLiteral("quarantine"));
  QVERIFY(!helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  ).ok);
  QVERIFY(operations->operations.indexOf(QStringLiteral("verify-app")) <
          operations->operations.indexOf(QStringLiteral("quarantine")));
  QVERIFY(operations->operations.filter(QRegularExpression(QStringLiteral("^rename:"))).isEmpty());
}

void UpdateInstallerTests::apply_rollsBackForRenameLaunchAndHandshakeFailure()
{
  for (const auto &failure : {QStringLiteral("launch-new"), QStringLiteral("rename-new")}) {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    Fixture fixture;
    QVERIFY(createFixture(temporary, fixture));
    FakeLauncher *launcher = nullptr;
    FakeApplyOperations *parentOperations = nullptr;
    auto parent = parentInstaller(fixture, launcher, parentOperations);
    const auto start = parent->start(fixture.staged, true);
    QVERIFY(start.ok);

    FakeApplyOperations *operations = nullptr;
    auto helper = helperInstaller(fixture, operations);
    operations->failures.insert(failure);
    QVERIFY(!helper->apply(
        fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
        start.receiptPath, start.helperToken
    ).ok);
    QVERIFY(std::ranges::any_of(operations->operations, [](const QString &operation) {
      return operation.contains(QStringLiteral("update-backup")) &&
             operation.endsWith(QStringLiteral(":Deskflow.app"));
    }));
    QVERIFY(operations->relaunchedBundles.contains(fixture.bundle));
  }
}

void UpdateInstallerTests::apply_keepsCandidateWhenFinalReceiptCannotBePublished()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY2(start.ok, qPrintable(start.error));

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->corruptReceiptOnNewLaunch = true;
  const auto result =
      helper->apply(
          fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
          start.receiptPath, start.helperToken
      );
  QVERIFY(result.ok);
  QCOMPARE(operations->relaunchCount, 1);
  QVERIFY(!operations->operations.contains(QStringLiteral("launch-old")));
}

void UpdateInstallerTests::apply_keepsOldBackupWhenRollbackRelaunchFails()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->failures.insert(QStringLiteral("launch-new"));
  operations->failures.insert(QStringLiteral("rename-restore"));
  operations->failures.insert(QStringLiteral("launch-old"));
  const auto result =
      helper->apply(
          fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
          start.receiptPath, start.helperToken
      );
  QVERIFY(!result.ok);
  QVERIFY(!operations->removed.contains(fixture.bundle));
}

void UpdateInstallerTests::apply_relaunchesOldForEveryPostParentExitFailure()
{
  for (const auto failure : {
           QStringLiteral("manifest"),
           QStringLiteral("verify-dmg"),
           QStringLiteral("workspace"),
           QStringLiteral("mount"),
           QStringLiteral("copy"),
           QStringLiteral("verify-app"),
           QStringLiteral("quarantine"),
           QStringLiteral("detach"),
           QStringLiteral("rename-old"),
           QStringLiteral("rename-new"),
       }) {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    Fixture fixture;
    QVERIFY(createFixture(temporary, fixture));
    FakeLauncher *launcher = nullptr;
    FakeApplyOperations *parentOperations = nullptr;
    auto parent = parentInstaller(fixture, launcher, parentOperations);
    const auto start = parent->start(fixture.staged, true);
    QVERIFY2(start.ok, qPrintable(start.error));

    FakeApplyOperations *operations = nullptr;
    auto helper = helperInstaller(fixture, operations);
    if (failure == QStringLiteral("manifest")) {
      QVERIFY(writeFile(fixture.manifestPath, QByteArrayLiteral("{}")));
    } else {
      operations->failures.insert(failure);
    }
    const auto result = helper->apply(
        fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
        start.receiptPath, start.helperToken
    );
    QVERIFY2(!result.ok, qPrintable(failure));
    QVERIFY2(
        operations->relaunchCount >= 1,
        qPrintable(failure + QStringLiteral(" did not relaunch the old app"))
    );
  }

  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);
  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->corruptReceiptBeforeNewLaunch = true;
  QVERIFY(!helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  ).ok);
  QVERIFY(operations->relaunchCount >= 1);
}

void UpdateInstallerTests::apply_doesNotRelaunchOldBeforeParentExit()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->failures.insert(QStringLiteral("shutdown"));
  QVERIFY(!helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  ).ok);
  QCOMPARE(operations->relaunchCount, 0);
}

void UpdateInstallerTests::apply_shutdownFailureWithExitedParentRelaunchesOld()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->failures.insert(QStringLiteral("shutdown"));
  operations->observedParentState =
      deskflow::gui::UpdateParentState::OriginalExited;
  const auto result = helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  );
  QVERIFY(!result.ok);
  QCOMPARE(operations->relaunchCount, 1);
  QVERIFY(!operations->operations.contains(QStringLiteral("verify-dmg")));
}

void UpdateInstallerTests::apply_shutdownFailureWithReusedPidRelaunchesOld()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->failures.insert(QStringLiteral("shutdown"));
  operations->observedParentState =
      deskflow::gui::UpdateParentState::PidReused;
  QVERIFY(!helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  ).ok);
  QCOMPARE(operations->relaunchCount, 1);
}

void UpdateInstallerTests::apply_processExitPidReuseContinuesSafely()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->failures.insert(QStringLiteral("parent-exit"));
  operations->observedParentState =
      deskflow::gui::UpdateParentState::PidReused;
  const auto result = helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  );
  QVERIFY(result.ok);
  QCOMPARE(operations->relaunchCount, 1);
}

void UpdateInstallerTests::apply_missingReceiptWithExitedParentRelaunchesOld()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);
  QVERIFY(QFile::remove(start.receiptPath));

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->observedParentState =
      deskflow::gui::UpdateParentState::OriginalExited;
  const auto result = helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  );
  QVERIFY(!result.ok);
  QCOMPARE(operations->relaunchCount, 1);
}

void UpdateInstallerTests::apply_revalidatesBundleParentAfterParentExit()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  QVERIFY(QFile::setPermissions(
      temporary.path(),
      QFileInfo(temporary.path()).permissions() | QFileDevice::WriteOther
  ));
  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  const auto result = helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  );
  QVERIFY(!result.ok);
  QVERIFY(result.error.contains(QStringLiteral("bundle")));
  QVERIFY(operations->relaunchCount >= 1);
}

void UpdateInstallerTests::apply_rechecksParentPermissionsImmediatelyBeforeRename()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->mutateParentBeforeRename = true;
  operations->parentPathToMutate = temporary.path();
  const auto result = helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  );
  QVERIFY(!result.ok);
  QVERIFY(result.error.contains(QStringLiteral("bundle")));
  QVERIFY(operations->operations.filter(
      QRegularExpression(QStringLiteral("^rename:"))
  ).isEmpty());
  QVERIFY(operations->relaunchCount >= 1);
}

void UpdateInstallerTests::pathPolicy_acceptsOnlyOwnedBundleAndSafeParentPermissions()
{
  QVERIFY(deskflow::gui::isTrustedUpdateBundleOwner(501, 501));
  QVERIFY(!deskflow::gui::isTrustedUpdateBundleOwner(501, 0));
  QVERIFY(deskflow::gui::isTrustedUpdateParentPermissions(
      501, 0, QStringLiteral("admin"), true, false
  ));
  QVERIFY(deskflow::gui::isTrustedUpdateParentPermissions(
      501, 0, QStringLiteral("wheel"), true, false
  ));
  QVERIFY(deskflow::gui::isTrustedUpdateParentPermissions(
      501, 501, QStringLiteral("staff"), true, false
  ) == false);
  QVERIFY(!deskflow::gui::isTrustedUpdateParentPermissions(
      501, 0, QStringLiteral("staff"), true, false
  ));
  QVERIFY(!deskflow::gui::isTrustedUpdateParentPermissions(
      501, 501, QStringLiteral("staff"), false, true
  ));
  QVERIFY(!deskflow::gui::isTrustedUpdateParentPermissions(
      501, 502, QStringLiteral("staff"), false, false
  ));

  deskflow::gui::MacUpdateInstallEvidence evidence;
  evidence.supported = true;
  evidence.desktopMode = true;
  evidence.directApp = true;
  evidence.canonicalPath = true;
  evidence.mountedVolume = false;
  evidence.packageManaged = false;
  evidence.appOwnedByEffectiveUser = true;
  evidence.parentTrusted = deskflow::gui::isTrustedUpdateParentPermissions(
      501, 0, QStringLiteral("admin"), true, false
  );
  evidence.bundleWritable = true;
  evidence.parentWritable = true;
  evidence.aclTrustProven = true;
  evidence.architectureSupported = true;
  QVERIFY(deskflow::gui::isMacUpdateInstallEligible(evidence));

  evidence.aclTrustProven = false;
  QVERIFY(!deskflow::gui::isMacUpdateInstallEligible(evidence));
  evidence.aclTrustProven = true;
  evidence.appOwnedByEffectiveUser = false;
  QVERIFY(!deskflow::gui::isMacUpdateInstallEligible(evidence));
  evidence.appOwnedByEffectiveUser = true;
  evidence.desktopMode = false;
  QVERIFY(!deskflow::gui::isMacUpdateInstallEligible(evidence));
}

void UpdateInstallerTests::packagePolicy_usesCanonicalExecutablePathOnly()
{
  using deskflow::gui::isMacUpdatePackageManagedPath;
  using deskflow::gui::MacUpdatePackageArtifact;
  using deskflow::gui::MacUpdatePackageEnumeration;

  QVERIFY(!isMacUpdatePackageManagedPath(
      QStringLiteral("/Applications/Deskflow.app/Contents/MacOS/Deskflow")
  ));
  QVERIFY(isMacUpdatePackageManagedPath(
      QStringLiteral(
          "/opt/homebrew/Caskroom/deskflow/2.0.6/Deskflow.app/Contents/MacOS/Deskflow"
      )
  ));
  QVERIFY(isMacUpdatePackageManagedPath(
      QStringLiteral(
          "/opt/homebrew/Cellar/deskflow/2.0.6/Deskflow.app/Contents/MacOS/Deskflow"
      )
  ));
  QVERIFY(!isMacUpdatePackageManagedPath(
      QStringLiteral(
          "/Applications/Caskroom/deskflow-not-owned/Deskflow.app/Contents/MacOS/Deskflow"
      )
  ));
  QVERIFY(!isMacUpdatePackageManagedPath(
      QStringLiteral(
          "/Applications/Caskroom/deskflow/2.0.6/Deskflow.app/Contents/MacOS/Deskflow"
      )
  ));

  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture direct;
  QVERIFY(createFixture(temporary, direct));
  QVERIFY(QDir().mkpath(
      QDir(temporary.path()).filePath(QStringLiteral("stale/opt/homebrew/Caskroom/deskflow"))
  ));
  QVERIFY(!deskflow::gui::isMacUpdatePackageManaged(direct.executable));

  const auto directExecutable =
      QDir(direct.bundle).filePath(QStringLiteral("Contents/MacOS/Deskflow"));
  const auto reverseCaskArtifact =
      QDir(temporary.path()).filePath(
          QStringLiteral("opt/homebrew/Caskroom/deskflow/2.0.6/Deskflow.app")
      );
  QVERIFY(QDir().mkpath(QFileInfo(reverseCaskArtifact).absolutePath()));
  QVERIFY(QFile::link(direct.bundle, reverseCaskArtifact));

  QString observedRunningBundle;
  const auto enumerate = [&reverseCaskArtifact, &observedRunningBundle](
                             const QString &runningBundle
                         ) {
    observedRunningBundle = runningBundle;
    return MacUpdatePackageEnumeration{
        QList<MacUpdatePackageArtifact>{
            {QStringLiteral(
                 "/opt/homebrew/Caskroom/deskflow/2.0.6/Deskflow.app"
             ),
             QFileInfo(reverseCaskArtifact).canonicalFilePath(),
             true,
             false,
             false,
             false}
        },
        true
    };
  };
  QVERIFY(deskflow::gui::isMacUpdatePackageManaged(
      directExecutable, enumerate
  ));
  QCOMPARE(
      observedRunningBundle,
      QFileInfo(direct.bundle).canonicalFilePath()
  );

  QVERIFY(!deskflow::gui::isMacUpdatePackageManaged(
      directExecutable,
      [](const QString &) {
        return MacUpdatePackageEnumeration{
            QList<MacUpdatePackageArtifact>{
                {QStringLiteral("/opt/homebrew/Caskroom/deskflow/2.0.6/Deskflow.app"),
                 QStringLiteral("/Applications/Other.app"),
                 true,
                 false,
                 false,
                 false}
            },
            true
        };
      }
  ));
  QVERIFY(deskflow::gui::isMacUpdatePackageManaged(
      directExecutable,
      [](const QString &) {
        return MacUpdatePackageEnumeration{
            QList<MacUpdatePackageArtifact>{
                {QStringLiteral("/opt/homebrew/Caskroom/deskflow/2.0.6/Deskflow.app"),
                 QStringLiteral("/Applications/Other.app"),
                 true,
                 false,
                 false,
                 true}
            },
            true
        };
      }
  ));
  QVERIFY(deskflow::gui::isMacUpdatePackageManaged(
      directExecutable,
      [](const QString &) {
        return MacUpdatePackageEnumeration{
            QList<MacUpdatePackageArtifact>{
                {QStringLiteral("/opt/homebrew/Caskroom/deskflow/2.0.6/Deskflow.app"),
                 {},
                 true,
                 true,
                 false,
                 false}
            },
            true
        };
      }
  ));
  QVERIFY(deskflow::gui::isMacUpdatePackageManaged(
      directExecutable,
      [](const QString &) {
        return MacUpdatePackageEnumeration{
            {},
            false
        };
      }
  ));
  QVERIFY(deskflow::gui::isMacUpdatePackageManaged(
      directExecutable,
      [&direct](const QString &) {
        return MacUpdatePackageEnumeration{
            QList<MacUpdatePackageArtifact>{
                {QStringLiteral(
                     "/opt/homebrew/Caskroom/deskflow/.metadata/INSTALL_RECEIPT.json"
                 ),
                 QFileInfo(direct.bundle).canonicalFilePath(),
                 false,
                 false,
                 true,
                 false}
            },
            true
        };
      }
  ));

  const auto reverseEvidence = deskflow::gui::evaluateMacUpdateInstall(
      directExecutable,
      false,
      false,
      [](const QString &) {
        return deskflow::gui::MacUpdateAclResult{
            true, 0, false, true, 0, false
        };
      },
      enumerate
  );
  QVERIFY(reverseEvidence.packageManaged);
  QVERIFY(!deskflow::gui::isMacUpdateInstallEligible(reverseEvidence));
}

void UpdateInstallerTests::aclPolicy_requiresSuccessfulEmptyAclRead()
{
  using deskflow::gui::MacUpdateAclResult;
  using deskflow::gui::isMacUpdateAclTrustProven;

  for (const auto error :
       {ENOENT, ENOTSUP, EOPNOTSUPP, EINVAL, EACCES, EPERM, EIO}) {
    QVERIFY(!isMacUpdateAclTrustProven(
        MacUpdateAclResult{false, -1, false, false, error}
    ));
  }
  QVERIFY(!isMacUpdateAclTrustProven(
      MacUpdateAclResult{true, -1, false, true, EIO}
  ));
  QVERIFY(!isMacUpdateAclTrustProven(
      MacUpdateAclResult{true, 0, true, true, 0}
  ));
  QVERIFY(!isMacUpdateAclTrustProven(
      MacUpdateAclResult{true, 0, false, false, EIO}
  ));
  QVERIFY(isMacUpdateAclTrustProven(
      MacUpdateAclResult{false, -1, false, false, ENOENT, true}
  ));
  QVERIFY(!isMacUpdateAclTrustProven(
      MacUpdateAclResult{false, -1, false, false, ENOTSUP, true}
  ));
  QVERIFY(isMacUpdateAclTrustProven(
      MacUpdateAclResult{true, 0, false, true, 0}
  ));

  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  for (const auto error :
       {ENOENT, ENOTSUP, EOPNOTSUPP, EINVAL, EACCES, EPERM, EIO}) {
    const auto evidence = deskflow::gui::evaluateMacUpdateInstall(
        fixture.executable, false, false,
        [error](const QString &) {
          return MacUpdateAclResult{false, -1, false, false, error};
        }
    );
    QVERIFY(!evidence.aclTrustProven);
    QVERIFY(!deskflow::gui::isMacUpdateInstallEligible(evidence));
  }
  const auto proven = deskflow::gui::evaluateMacUpdateInstall(
      fixture.executable, false, false,
      [](const QString &) {
        return MacUpdateAclResult{true, 0, false, true, 0};
      }
  );
  QVERIFY(proven.aclTrustProven);
  QVERIFY(deskflow::gui::isMacUpdateInstallEligible(proven));

  const auto actualAcl = deskflow::gui::evaluateMacUpdateInstall(
      fixture.executable, false, false
  );
  QVERIFY2(
      actualAcl.aclTrustProven,
      "an existing file with no extended ACL must be a proven safe absence"
  );
}

void UpdateInstallerTests::apply_rejectsCandidateIdentityChangeBeforeAndAfterRename()
{
  for (const int mismatchAt : {2, 3}) {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    Fixture fixture;
    QVERIFY(createFixture(temporary, fixture));
    FakeLauncher *launcher = nullptr;
    FakeApplyOperations *parentOperations = nullptr;
    auto parent = parentInstaller(fixture, launcher, parentOperations);
    const auto start = parent->start(fixture.staged, true);
    QVERIFY(start.ok);

    FakeApplyOperations *operations = nullptr;
    auto helper = helperInstaller(fixture, operations);
    operations->identityMismatchAt = mismatchAt;
    const auto result = helper->apply(
        fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
        start.receiptPath, start.helperToken
    );
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("identity")));
    QVERIFY(operations->relaunchCount >= 1);
  }
}

void UpdateInstallerTests::apply_rejectsIdentityMutationDuringFinalVerification()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  operations->mutateIdentityDuringFinalVerification = true;
  const auto result = helper->apply(
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  );
  QVERIFY(!result.ok);
  QVERIFY(result.error.contains(QStringLiteral("identity")));
  QVERIFY(operations->relaunchCount >= 1);
}

void UpdateInstallerTests::apply_rejectsMismatchedPidAndReceipt()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  Fixture fixture;
  QVERIFY(createFixture(temporary, fixture));
  FakeLauncher *launcher = nullptr;
  FakeApplyOperations *parentOperations = nullptr;
  auto parent = parentInstaller(fixture, launcher, parentOperations);
  const auto start = parent->start(fixture.staged, true);
  QVERIFY(start.ok);

  FakeApplyOperations *operations = nullptr;
  auto helper = helperInstaller(fixture, operations);
  QVERIFY(!helper->apply(
      fixture.manifestPath, fixture.parentPid + 1, fixture.parentStartIdentity,
      start.receiptPath, start.helperToken
  ).ok);
  QVERIFY(operations->operations.isEmpty());
}

void UpdateInstallerTests::handshake_acceptsOnlyMatchingPidTokenAndVersion()
{
  const auto json = [](qint64 pid, const QString &token, const QString &version) {
    return QJsonDocument(QJsonObject{
        {QStringLiteral("state"), QStringLiteral("ready")},
        {QStringLiteral("pid"), pid},
        {QStringLiteral("token"), token},
        {QStringLiteral("version"), version},
    }).toJson(QJsonDocument::Compact);
  };
  const auto token = QStringLiteral("one-use-token");
  QVERIFY(deskflow::gui::isValidUpdateHandshake(
      json(4243, token, QStringLiteral("2.0.6")), 4243, token, QStringLiteral("2.0.6")
  ));
  QVERIFY(deskflow::gui::isValidUpdateHandshake(
      json(4243, token, QStringLiteral("2.0.6.0")), 4243, token, QStringLiteral("2.0.6")
  ));
  QVERIFY(!deskflow::gui::isValidUpdateHandshake(
      json(4244, token, QStringLiteral("2.0.6")), 4243, token, QStringLiteral("2.0.6")
  ));
  QVERIFY(!deskflow::gui::isValidUpdateHandshake(
      json(4243, QStringLiteral("other"), QStringLiteral("2.0.6")),
      4243, token, QStringLiteral("2.0.6")
  ));
  QVERIFY(!deskflow::gui::isValidUpdateHandshake(
      json(4243, token, QStringLiteral("2.0.5")), 4243, token, QStringLiteral("2.0.6")
  ));
  QVERIFY(!deskflow::gui::isValidUpdateHandshake(
      json(4243, token, QStringLiteral("2.0.6.1")), 4243, token, QStringLiteral("2.0.6")
  ));
}

QTEST_MAIN(UpdateInstallerTests)
