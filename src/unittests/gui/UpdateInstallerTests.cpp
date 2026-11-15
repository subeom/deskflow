/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateInstallerTests.h"

#include "gui/MacUpdateInstaller.h"
#include "gui/UpdateCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

class FakeUpdateProcessLauncher final : public deskflow::gui::IUpdateProcessLauncher
{
public:
  deskflow::gui::UpdateHelperLaunchResult launch(const QString &program, const QStringList &arguments) override
  {
    calls++;
    lastProgram = program;
    lastArguments = arguments;
    return result;
  }

  int calls = 0;
  QString lastProgram;
  QStringList lastArguments;
  deskflow::gui::UpdateHelperLaunchResult result{true, 1234, {}};
};

class FakeUpdateApplyOperations final : public deskflow::gui::IUpdateApplyOperations
{
public:
  deskflow::gui::UpdateInstallResult waitForProcessExit(qint64 pid, int timeoutMs) override
  {
    events.append(QStringLiteral("wait"));
    waitedPid = pid;
    waitedTimeout = timeoutMs;
    return {true, {}};
  }

  deskflow::gui::UpdateInstallResult verifyDiskImage(const deskflow::gui::StagedUpdate &staged) override
  {
    events.append(QStringLiteral("verify-image"));
    verifiedInstaller = staged.installerPath;
    return failVerify ? deskflow::gui::UpdateInstallResult{false, QStringLiteral("signature verification failed")}
                      : deskflow::gui::UpdateInstallResult{true, {}};
  }

  deskflow::gui::UpdateInstallResult mountDiskImage(const QString &dmg, const QString &mountPoint) override
  {
    events.append(QStringLiteral("mount"));
    mountedDmg = dmg;
    const auto app = QDir(mountPoint).filePath(QStringLiteral("Deskflow.app"));
    QDir().mkpath(app);
    QFile marker(QDir(app).filePath(QStringLiteral("marker")));
    if (!marker.open(QIODevice::WriteOnly)) {
      return {false, QStringLiteral("cannot create mounted marker")};
    }
    marker.write("new");
    marker.close();
    return {true, {}};
  }

  deskflow::gui::UpdateInstallResult copyApplication(const QString &mountedApp, const QString &destination) override
  {
    events.append(QStringLiteral("copy"));
    copiedSource = mountedApp;
    QDir().mkpath(destination);
    QFile marker(QDir(destination).filePath(QStringLiteral("marker")));
    if (!marker.open(QIODevice::WriteOnly)) {
      return {false, QStringLiteral("cannot create copied marker")};
    }
    marker.write("new");
    marker.close();
    return {true, {}};
  }

  deskflow::gui::UpdateInstallResult verifyApplication(const QString &path, const QString &expectedVersion) override
  {
    events.append(QStringLiteral("verify-app"));
    verifiedApp = path;
    verifiedAppVersion = expectedVersion;
    return QFileInfo::exists(QDir(path).filePath(QStringLiteral("marker")))
               ? deskflow::gui::UpdateInstallResult{true, {}}
               : deskflow::gui::UpdateInstallResult{false, QStringLiteral("missing copied app")};
  }

  deskflow::gui::UpdateInstallResult detachDiskImage(const QString &mountPoint) override
  {
    events.append(QStringLiteral("detach"));
    detachedMount = mountPoint;
    return {true, {}};
  }

  deskflow::gui::UpdateInstallResult rename(const QString &source, const QString &destination) override
  {
    events.append(QStringLiteral("rename"));
    if (!QDir().rename(source, destination)) {
      return {false, QStringLiteral("fake rename failed")};
    }
    return {true, {}};
  }

  void removeRecursively(const QString &path) override
  {
    events.append(QStringLiteral("remove"));
    QDir(path).removeRecursively();
  }

  deskflow::gui::UpdateInstallResult relaunch(const QString &bundlePath) override
  {
    events.append(QStringLiteral("launch"));
    launchedBundles.append(bundlePath);
    return failLaunch ? deskflow::gui::UpdateInstallResult{false, QStringLiteral("fake launch failed")}
                      : deskflow::gui::UpdateInstallResult{true, {}};
  }

  QStringList events;
  QString verifiedInstaller;
  QString mountedDmg;
  QString copiedSource;
  QString verifiedApp;
  QString verifiedAppVersion;
  QString detachedMount;
  QStringList launchedBundles;
  qint64 waitedPid = 0;
  int waitedTimeout = 0;
  bool failLaunch = false;
  bool failVerify = false;
};

deskflow::gui::StagedUpdate writeStagedUpdate(QTemporaryDir &directory)
{
  const auto installerPath = directory.filePath(QStringLiteral("deskflow-1.28.0-macos-arm64.dmg"));
  const auto manifestPath = directory.filePath(QStringLiteral("manifest.json"));
  QFile installer(installerPath);
  QFile manifest(manifestPath);
  if (!installer.open(QIODevice::WriteOnly) || installer.write("dmg") != 3 || !manifest.open(QIODevice::WriteOnly) ||
      manifest.write("{}") != 2) {
    return {};
  }
  installer.close();
  manifest.close();
  return {QStringLiteral("1.28.0"), QFileInfo(installerPath).fileName(), installerPath, manifestPath};
}

deskflow::gui::StagedUpdate stageVerifiedUpdate(QTemporaryDir &directory, const QByteArray &payload)
{
  const auto hash = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex();
  deskflow::gui::UpdatePlan plan;
  plan.releaseVersion = QStringLiteral("1.28.0");
  plan.releaseUrl = QStringLiteral("https://github.com/deskflow/deskflow/releases/tag/v1.28.0");
  plan.installer = deskflow::gui::UpdateAsset{
      QStringLiteral("deskflow-1.28.0-macos-arm64.dmg"),
      QStringLiteral("https://github.com/deskflow/deskflow/releases/download/v1.28.0/deskflow.dmg"),
      QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash)), payload.size()
  };
  plan.mode = deskflow::gui::UpdateInstallMode::Automatic;
  deskflow::gui::UpdateCache cache(QDir(directory.path()).filePath(QStringLiteral("updates")));
  const auto staged = cache.stage(plan, payload);
  return staged.ok ? staged.staged : deskflow::gui::StagedUpdate{};
}

QString writeMarker(const QString &bundlePath, const QByteArray &contents)
{
  const auto markerPath = QDir(bundlePath).filePath(QStringLiteral("marker"));
  QDir().mkpath(bundlePath);
  QFile marker(markerPath);
  if (!marker.open(QIODevice::WriteOnly)) {
    return {};
  }
  marker.write(contents);
  marker.close();
  return markerPath;
}

QByteArray readMarker(const QString &bundlePath)
{
  QFile marker(QDir(bundlePath).filePath(QStringLiteral("marker")));
  if (!marker.open(QIODevice::ReadOnly)) {
    return {};
  }
  return marker.readAll();
}

QString guiExecutable()
{
  return QStringLiteral("/Applications/Deskflow.app/Contents/MacOS/Deskflow");
}

} // namespace

void UpdateInstallerTests::launch_requiresExplicitConfirmation()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  auto launcher = std::make_unique<FakeUpdateProcessLauncher>();
  auto *fake = launcher.get();
  deskflow::gui::MacUpdateInstaller installer(guiExecutable(), false, std::move(launcher));

  const auto result = installer.start(writeStagedUpdate(directory), false);
  QVERIFY(!result.ok);
  QCOMPARE(fake->calls, 0);
}

void UpdateInstallerTests::launch_refusesPackageManagedHomebrewApp()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  auto launcher = std::make_unique<FakeUpdateProcessLauncher>();
  auto *fake = launcher.get();
  deskflow::gui::MacUpdateInstaller installer(guiExecutable(), true, std::move(launcher));

  const auto result = installer.start(writeStagedUpdate(directory), true);
  QVERIFY(!result.ok);
  QCOMPARE(fake->calls, 0);
}

void UpdateInstallerTests::launch_startsDetachedHelperWithManifestAndParentPid()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto staged = writeStagedUpdate(directory);
  auto launcher = std::make_unique<FakeUpdateProcessLauncher>();
  auto *fake = launcher.get();
  deskflow::gui::MacUpdateInstaller installer(guiExecutable(), false, std::move(launcher));

  const auto result = installer.start(staged, true);
  QVERIFY2(result.ok, qPrintable(result.error));
  QCOMPARE(fake->calls, 1);
  QCOMPARE(fake->lastProgram, guiExecutable());
  const QStringList expectedArguments{
      QStringLiteral("--apply-update"), staged.manifestPath, QStringLiteral("--update-parent-pid"),
      QString::number(QCoreApplication::applicationPid())
  };
  QCOMPARE(fake->lastArguments, expectedArguments);
}

void UpdateInstallerTests::apply_stagesVerifiesAndAtomicallyReplaces()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto staged = stageVerifiedUpdate(directory, QByteArray("verified dmg"));
  QVERIFY(!staged.manifestPath.isEmpty());

  const auto bundle = QDir(directory.path()).filePath(QStringLiteral("Applications/Deskflow.app"));
  QVERIFY(!writeMarker(bundle, "old").isEmpty());
  const auto executable = QDir(bundle).filePath(QStringLiteral("Contents/MacOS/Deskflow"));
  QVERIFY(QDir().mkpath(QFileInfo(executable).absolutePath()));

  auto operations = std::make_unique<FakeUpdateApplyOperations>();
  auto *fake = operations.get();
  deskflow::gui::MacUpdateInstaller installer(
      executable, false, std::unique_ptr<deskflow::gui::IUpdateProcessLauncher>{}, std::move(operations)
  );

  const auto result = installer.apply(staged.manifestPath, 99);
  QVERIFY2(result.ok, qPrintable(result.error));
  QCOMPARE(fake->events.first(), QStringLiteral("wait"));
  QCOMPARE(readMarker(bundle), QByteArray("new"));
  QCOMPARE(fake->waitedPid, qint64{99});
  QVERIFY(fake->waitedTimeout > 0);
  QCOMPARE(fake->verifiedInstaller, staged.installerPath);
  QCOMPARE(fake->mountedDmg, staged.installerPath);
  QVERIFY(fake->verifiedApp.endsWith(QStringLiteral(".app")));
  QCOMPARE(fake->verifiedAppVersion, staged.version);
  QVERIFY(fake->events.contains(QStringLiteral("detach")));
  QVERIFY(fake->events.indexOf(QStringLiteral("detach")) < fake->events.indexOf(QStringLiteral("rename")));
  QCOMPARE(fake->launchedBundles, QStringList{bundle});
  QVERIFY(!QFileInfo::exists(QFileInfo(staged.manifestPath).absolutePath()));
}

void UpdateInstallerTests::apply_rollsBackAndRelaunchesOldAppWhenNewLaunchFails()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto staged = stageVerifiedUpdate(directory, QByteArray("verified dmg"));
  QVERIFY(!staged.manifestPath.isEmpty());

  const auto bundle = QDir(directory.path()).filePath(QStringLiteral("Applications/Deskflow.app"));
  QVERIFY(!writeMarker(bundle, "old").isEmpty());
  const auto executable = QDir(bundle).filePath(QStringLiteral("Contents/MacOS/Deskflow"));
  QVERIFY(QDir().mkpath(QFileInfo(executable).absolutePath()));

  auto operations = std::make_unique<FakeUpdateApplyOperations>();
  operations->failLaunch = true;
  auto *fake = operations.get();
  deskflow::gui::MacUpdateInstaller installer(
      executable, false, std::unique_ptr<deskflow::gui::IUpdateProcessLauncher>{}, std::move(operations)
  );

  const auto result = installer.apply(staged.manifestPath, 99);
  QVERIFY(!result.ok);
  QCOMPARE(readMarker(bundle), QByteArray("old"));
  QCOMPARE(fake->launchedBundles.size(), 2);
  QCOMPARE(fake->events.count(QStringLiteral("remove")), 1);
  QVERIFY(QFileInfo::exists(staged.manifestPath));
}

void UpdateInstallerTests::apply_discardsStageWhenReverificationFails()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto staged = stageVerifiedUpdate(directory, QByteArray("verified dmg"));
  QVERIFY(!staged.manifestPath.isEmpty());

  const auto bundle = QDir(directory.path()).filePath(QStringLiteral("Applications/Deskflow.app"));
  QVERIFY(!writeMarker(bundle, "old").isEmpty());
  const auto executable = QDir(bundle).filePath(QStringLiteral("Contents/MacOS/Deskflow"));
  QVERIFY(QDir().mkpath(QFileInfo(executable).absolutePath()));

  auto operations = std::make_unique<FakeUpdateApplyOperations>();
  operations->failVerify = true;
  auto *fake = operations.get();
  deskflow::gui::MacUpdateInstaller installer(
      executable, false, std::unique_ptr<deskflow::gui::IUpdateProcessLauncher>{}, std::move(operations)
  );

  QVERIFY(!installer.apply(staged.manifestPath, 99).ok);
  QVERIFY(!QFileInfo::exists(staged.manifestPath));
  QCOMPARE(readMarker(bundle), QByteArray("old"));
  QCOMPARE(fake->launchedBundles, QStringList{bundle});
}

QTEST_MAIN(UpdateInstallerTests)
