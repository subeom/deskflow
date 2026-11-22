/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/MacUpdateVerifier.h"
#include "gui/UpdateRelease.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

namespace deskflow::gui {

namespace {

constexpr auto kBundleIdentifier = "org.deskflow.deskflow";
constexpr auto kLeafHash = "39b1c21df0ba75cdde058fcbb09a1ad38aef8055";
constexpr int kCommandTimeoutMs = 120'000;
constexpr qsizetype kMaximumCommandOutput = 64 * 1024;
constexpr qsizetype kMaximumUserError = 240;

class QProcessUpdateCommandRunner final : public IUpdateCommandRunner
{
public:
  UpdateCommandResult run(
      const QString &program, const QStringList &arguments, int timeoutMs
  ) override
  {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(timeoutMs)) {
      return {false, false, -1, {}, process.errorString().toUtf8()};
    }
    if (!process.waitForFinished(timeoutMs)) {
      process.kill();
      process.waitForFinished();
      return {true, true, -1, {}, QByteArrayLiteral("command timed out")};
    }
    return {
        true,
        false,
        process.exitCode(),
        process.readAllStandardOutput().left(kMaximumCommandOutput),
        process.readAllStandardError().left(kMaximumCommandOutput)
    };
  }
};

QByteArray commandOutput(const UpdateCommandResult &result)
{
  return (result.standardOutput + QByteArrayLiteral("\n") + result.standardError).trimmed();
}

QString plistValueOutput(const UpdateCommandResult &result)
{
  auto output = result.standardOutput;
  if (output.endsWith(QByteArrayLiteral("\r\n"))) {
    output.chop(2);
  } else if (output.endsWith('\n')) {
    output.chop(1);
  }
  return QString::fromUtf8(output);
}

bool isInside(const QString &path, const QString &root)
{
  const auto canonicalPath = QFileInfo(path).canonicalFilePath();
  const auto canonicalRoot = QFileInfo(root).canonicalFilePath();
  return !canonicalPath.isEmpty() && !canonicalRoot.isEmpty() &&
         canonicalPath.startsWith(canonicalRoot + QLatin1Char('/'));
}

bool hasLicenseAgreementResource(const QByteArray &resourceXml)
{
  return resourceXml.contains(QByteArrayLiteral("<key>LPic</key>"));
}

QStringList evidenceLines(const QByteArray &data)
{
  QStringList lines;
  for (const auto &line : data.split('\n')) {
    lines.append(QString::fromUtf8(line.trimmed()));
  }
  return lines;
}

QString rootRequirement()
{
  return QStringLiteral(
      R"(identifier "%1" and certificate leaf = H"%2")"
  ).arg(QString::fromLatin1(kBundleIdentifier), QString::fromLatin1(kLeafHash));
}

QString leafRequirement()
{
  return QStringLiteral(R"(certificate leaf = H"%1")").arg(QString::fromLatin1(kLeafHash));
}

QStringList rootApplications(const QString &mountPoint)
{
  QStringList applications;
  const auto entries =
      QDir(mountPoint).entryInfoList(
          QStringList{QStringLiteral("*.app")}, QDir::Dirs | QDir::NoDotAndDotDot
      );
  for (const auto &entry : entries) {
    applications.append(entry.absoluteFilePath());
  }
  return applications;
}

} // namespace

UpdateVerifierResult parseMacUpdateSignatureEvidence(const QByteArray &codesignEvidence)
{
  const auto lines = evidenceLines(codesignEvidence);
  const auto text = QString::fromUtf8(codesignEvidence);
  if (text.contains(QStringLiteral("Notarized Developer ID"), Qt::CaseInsensitive) ||
      text.contains(QStringLiteral("Developer ID Application:"), Qt::CaseInsensitive)) {
    return {false, QStringLiteral("update uses an unaccepted Apple Developer ID policy")};
  }

  const auto identifierPrefix = QStringLiteral("Identifier=");
  QStringList identifiers;
  for (const auto &line : lines) {
    if (line.startsWith(identifierPrefix)) {
      identifiers.append(line.mid(identifierPrefix.size()));
    }
  }
  if (identifiers.size() != 1 ||
      identifiers.first() != QString::fromLatin1(kBundleIdentifier)) {
    return {false, QStringLiteral("update has an unexpected macOS bundle identifier")};
  }

  const auto teamPrefix = QStringLiteral("TeamIdentifier=");
  for (const auto &line : lines) {
    if (line.startsWith(teamPrefix)) {
      const auto team = line.mid(teamPrefix.size());
      if (!team.isEmpty() && team != QStringLiteral("not set")) {
        return {false, QStringLiteral("update has an unexpected Apple team")};
      }
    }
  }

  if (!text.contains(
          QStringLiteral("designated => %1").arg(rootRequirement()),
          Qt::CaseInsensitive
      )) {
    return {false, QStringLiteral("update is not signed with the pinned Deskflow requirement")};
  }
  return {true, {}};
}

bool isMacUpdateBundleVersion(const QString &expected, const QString &bundle)
{
  const auto normalizedExpected = normalizeUpdateVersion(expected);
  const auto normalizedBundle = normalizeUpdateVersion(bundle);
  return normalizedExpected && normalizedBundle && *normalizedExpected == *normalizedBundle;
}

std::unique_ptr<IUpdateCommandRunner> createUpdateCommandRunner()
{
  return std::make_unique<QProcessUpdateCommandRunner>();
}

MacUpdateVerifier::MacUpdateVerifier(std::unique_ptr<IUpdateCommandRunner> runner)
    : m_runner(std::move(runner))
{
  if (!m_runner) {
    m_runner = createUpdateCommandRunner();
  }
}

UpdateVerifierResult MacUpdateVerifier::verify(const StagedUpdate &staged)
{
  if (!normalizeUpdateVersion(staged.version)) {
    return {false, QStringLiteral("staged update has an invalid version")};
  }
  const QFileInfo dmg(staged.installerPath);
  if (dmg.isSymLink() || !dmg.isFile() ||
      dmg.suffix().compare(QStringLiteral("dmg"), Qt::CaseInsensitive) != 0) {
    return {false, QStringLiteral("staged update is not a regular macOS disk image")};
  }

  const auto resources = m_runner->run(
      QStringLiteral("/usr/bin/hdiutil"),
      {QStringLiteral("udifderez"), QStringLiteral("-xml"), dmg.absoluteFilePath()},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(resources)) {
    return updateCommandFailure(
        QStringLiteral("inspecting update disk image resources"), resources
    );
  }
  if (hasLicenseAgreementResource(resources.standardOutput)) {
    return {false, QStringLiteral("update disk image contains a software license agreement resource")};
  }

  QTemporaryDir temporary(QStringLiteral("%1/deskflow-update-verify-XXXXXX").arg(QDir::tempPath()));
  if (!temporary.isValid()) {
    return {false, QStringLiteral("cannot create temporary update verification directory")};
  }
  const auto mountPoint = QDir(temporary.path()).filePath(QStringLiteral("volume"));
  if (!QDir().mkpath(mountPoint)) {
    return {false, QStringLiteral("cannot create update disk image mount point")};
  }

  const auto attach = m_runner->run(
      QStringLiteral("/usr/bin/hdiutil"),
      {QStringLiteral("attach"), dmg.absoluteFilePath(), QStringLiteral("-readonly"),
       QStringLiteral("-nobrowse"), QStringLiteral("-noautoopen"),
       QStringLiteral("-mountpoint"), mountPoint},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(attach)) {
    return updateCommandFailure(QStringLiteral("mounting update disk image"), attach);
  }

  const auto verification = verifyMounted(mountPoint, temporary.path(), staged.version);
  const auto detach = m_runner->run(
      QStringLiteral("/usr/bin/hdiutil"),
      {QStringLiteral("detach"), mountPoint, QStringLiteral("-force")},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(detach)) {
    const auto detachFailure =
        updateCommandFailure(QStringLiteral("unmounting update disk image"), detach);
    if (!verification.ok) {
      return {
          false,
          QStringLiteral("%1; %2")
              .arg(verification.error, detachFailure.error)
              .left(kMaximumUserError)
      };
    }
    return detachFailure;
  }
  return verification;
}

UpdateVerifierResult MacUpdateVerifier::verifyMounted(
    const QString &mountPoint, const QString &temporaryDirectory, const QString &expectedVersion
)
{
  const auto applications = rootApplications(mountPoint);
  if (applications.size() != 1 ||
      QFileInfo(applications.first()).fileName() != QStringLiteral("Deskflow.app")) {
    return {false, QStringLiteral("update disk image must contain exactly Deskflow.app at its root")};
  }
  const QFileInfo mountedApp(applications.first());
  if (mountedApp.isSymLink() || !mountedApp.isDir() ||
      !isInside(mountedApp.absoluteFilePath(), mountPoint)) {
    return {false, QStringLiteral("mounted Deskflow.app is not a contained app directory")};
  }

  const auto copiedParent = QDir(temporaryDirectory).filePath(QStringLiteral("copied"));
  if (!QDir().mkpath(copiedParent)) {
    return {false, QStringLiteral("cannot create temporary copied app directory")};
  }
  const auto copiedApp = QDir(copiedParent).filePath(QStringLiteral("Deskflow.app"));
  const auto copy = m_runner->run(
      QStringLiteral("/usr/bin/ditto"),
      {mountedApp.absoluteFilePath(), copiedApp},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(copy)) {
    return updateCommandFailure(QStringLiteral("copying update app bundle"), copy);
  }
  const QFileInfo copiedInfo(copiedApp);
  if (copiedInfo.isSymLink() || !copiedInfo.isDir() ||
      !isInside(copiedApp, temporaryDirectory)) {
    return {false, QStringLiteral("copied update app bundle escaped its temporary directory")};
  }
  return verifyApplication(copiedInfo.absoluteFilePath(), expectedVersion);
}

UpdateVerifierResult MacUpdateVerifier::verifyApplication(
    const QString &applicationPath, const QString &expectedVersion
)
{
  const QFileInfo app(applicationPath);
  if (app.isSymLink() || !app.isDir() || app.fileName() != QStringLiteral("Deskflow.app")) {
    return {false, QStringLiteral("update app bundle is not a regular Deskflow.app directory")};
  }

  const auto contentsPath = QDir(app.absoluteFilePath()).filePath(QStringLiteral("Contents"));
  const auto infoPlist = QDir(contentsPath).filePath(QStringLiteral("Info.plist"));
  const QFileInfo plistInfo(infoPlist);
  if (plistInfo.isSymLink() || !plistInfo.isFile() ||
      !isInside(infoPlist, app.absoluteFilePath())) {
    return {false, QStringLiteral("update app bundle has no contained Info.plist")};
  }

  const auto guiExecutable =
      QDir(contentsPath).filePath(QStringLiteral("MacOS/Deskflow"));
  const auto coreExecutable =
      QDir(contentsPath).filePath(QStringLiteral("MacOS/deskflow-core"));
  for (const auto &executable : {guiExecutable, coreExecutable}) {
    const QFileInfo info(executable);
    if (info.isSymLink() || !info.isFile() ||
        !isInside(executable, app.absoluteFilePath())) {
      return {false, QStringLiteral("update app bundle has an unexpected nested executable")};
    }
  }

  const auto readPlist = [this, &infoPlist](const QString &field) {
    return m_runner->run(
        QStringLiteral("/usr/bin/plutil"),
        {QStringLiteral("-extract"), field, QStringLiteral("raw"),
         QStringLiteral("-o"), QStringLiteral("-"), infoPlist},
        kCommandTimeoutMs
    );
  };
  const auto bundleIdentifier = readPlist(QStringLiteral("CFBundleIdentifier"));
  if (!updateCommandSucceeded(bundleIdentifier)) {
    return updateCommandFailure(
        QStringLiteral("reading update bundle identifier"), bundleIdentifier
    );
  }
  if (plistValueOutput(bundleIdentifier) !=
      QString::fromLatin1(kBundleIdentifier)) {
    return {false, QStringLiteral("update bundle identifier is unexpected")};
  }

  if (!expectedVersion.isEmpty()) {
    for (const auto field :
         {QStringLiteral("CFBundleShortVersionString"), QStringLiteral("CFBundleVersion")}) {
      const auto version = readPlist(field);
      if (!updateCommandSucceeded(version) ||
          !isMacUpdateBundleVersion(
              expectedVersion, plistValueOutput(version)
          )) {
        if (!updateCommandSucceeded(version)) {
          return updateCommandFailure(QStringLiteral("reading update app version"), version);
        }
        return {false, QStringLiteral("update app version does not match the selected release")};
      }
    }
  }

  const auto rootVerify = m_runner->run(
      QStringLiteral("/usr/bin/codesign"),
      {QStringLiteral("--verify"), QStringLiteral("--deep"), QStringLiteral("--strict"),
       QStringLiteral("--test-requirement==%1").arg(rootRequirement()),
       app.absoluteFilePath()},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(rootVerify)) {
    return updateCommandFailure(QStringLiteral("verifying update code signature"), rootVerify);
  }

  for (const auto &executable : {guiExecutable, coreExecutable}) {
    const auto nestedVerify = m_runner->run(
        QStringLiteral("/usr/bin/codesign"),
        {QStringLiteral("--verify"), QStringLiteral("--strict"),
         QStringLiteral("--test-requirement==%1").arg(leafRequirement()), executable},
        kCommandTimeoutMs
    );
    if (!updateCommandSucceeded(nestedVerify)) {
      return updateCommandFailure(
          QStringLiteral("verifying nested update code signature"), nestedVerify
      );
    }
  }

  const auto evidence = m_runner->run(
      QStringLiteral("/usr/bin/codesign"),
      {QStringLiteral("-d"), QStringLiteral("-r-"), QStringLiteral("--verbose=4"),
       app.absoluteFilePath()},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(evidence)) {
    return updateCommandFailure(QStringLiteral("reading update code signature"), evidence);
  }
  return parseMacUpdateSignatureEvidence(commandOutput(evidence));
}

bool updateCommandSucceeded(const UpdateCommandResult &result)
{
  return result.started && !result.timedOut && result.exitCode == 0;
}

UpdateVerifierResult updateCommandFailure(
    const QString &description, const UpdateCommandResult &result
)
{
  if (result.timedOut) {
    return {false, QStringLiteral("%1 timed out").arg(description).left(kMaximumUserError)};
  }
  const auto output = QString::fromUtf8(commandOutput(result));
  const auto message =
      output.isEmpty()
          ? QStringLiteral("%1 failed").arg(description)
          : QStringLiteral("%1 failed: %2").arg(description, output);
  return {false, message.left(kMaximumUserError)};
}

} // namespace deskflow::gui
