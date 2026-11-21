/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateInstallerTests.h"

#include "gui/MacUpdateInstaller.h"
#include "gui/UpdateCache.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <algorithm>
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
    return result(QStringLiteral("verify-app"));
  }

  UpdateInstallResult removeQuarantine(const QString &) override
  {
    operations.append(QStringLiteral("quarantine"));
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
      std::move(verifier)
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
      std::make_unique<FakeVerifier>()
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
  QVERIFY(QFileInfo(result.receiptPath).isFile());
  QCOMPARE(operations->helperReceipt, result.receiptPath);
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
      helper->apply(fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity, start.receiptPath);
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
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity, start.receiptPath
  ).ok);
  QVERIFY(operations->operations.contains(QStringLiteral("verify-dmg")));
  QVERIFY(operations->operations.contains(QStringLiteral("verify-app")));
  QCOMPARE(operations->operations.filter(QRegularExpression(QStringLiteral("^rename:"))).size(), 2);
  QVERIFY(operations->operations.indexOf(QStringLiteral("detach")) <
          indexStartingWith(
              operations->operations, QStringLiteral("rename:Deskflow.app")
          ));
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
      fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity, start.receiptPath
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
        fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity, start.receiptPath
    ).ok);
    QVERIFY(std::ranges::any_of(operations->operations, [](const QString &operation) {
      return operation.contains(QStringLiteral("update-backup")) &&
             operation.endsWith(QStringLiteral(":Deskflow.app"));
    }));
    QVERIFY(operations->relaunchedBundles.contains(fixture.bundle));
  }
}

void UpdateInstallerTests::apply_rollsBackWhenSuccessReceiptCannotBePublished()
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
      helper->apply(fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity, start.receiptPath);
  QVERIFY(!result.ok);
  QVERIFY(result.error.contains(QStringLiteral("cannot publish successful update receipt")));
  QCOMPARE(operations->relaunchCount, 2);
  QVERIFY(std::ranges::any_of(operations->operations, [](const QString &operation) {
    return operation.contains(QStringLiteral("update-backup")) &&
           operation.endsWith(QStringLiteral(":Deskflow.app"));
  }));
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
      helper->apply(fixture.manifestPath, fixture.parentPid, fixture.parentStartIdentity, start.receiptPath);
  QVERIFY(!result.ok);
  QVERIFY(!operations->removed.contains(fixture.bundle));
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
      fixture.manifestPath, fixture.parentPid + 1, fixture.parentStartIdentity, start.receiptPath
  ).ok);
  QVERIFY(operations->operations.isEmpty());
}

QTEST_MAIN(UpdateInstallerTests)
