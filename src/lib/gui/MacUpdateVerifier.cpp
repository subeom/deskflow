/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/MacUpdateVerifier.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>

namespace deskflow::gui {

std::unique_ptr<IUpdateVerifier> createPlatformUpdateVerifier()
{
#if defined(Q_OS_MACOS)
  return std::make_unique<MacUpdateVerifier>();
#else
  return {};
#endif
}

namespace {

constexpr auto kBundleIdentifier = "org.deskflow.deskflow";
constexpr auto kTeamIdentifier = "5QCJCX8C37";
constexpr auto kCommandTimeoutMs = 120'000;

QByteArray boundedOutput(const QByteArray &data)
{
  constexpr auto maximumOutput = 64 * 1024;
  return data.left(maximumOutput);
}

class QProcessUpdateCommandRunner final : public IUpdateCommandRunner
{
public:
  UpdateCommandResult run(const QString &program, const QStringList &arguments, int timeoutMs) override
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
        true, false, process.exitCode(), boundedOutput(process.readAllStandardOutput()),
        boundedOutput(process.readAllStandardError())
    };
  }
};

QByteArray commandOutput(const UpdateCommandResult &result)
{
  auto output = result.standardOutput + QByteArrayLiteral("\n") + result.standardError;
  return output.trimmed();
}

bool isInside(const QString &path, const QString &root)
{
  const auto canonicalPath = QFileInfo(path).canonicalFilePath();
  const auto canonicalRoot = QFileInfo(root).canonicalFilePath();
  return !canonicalPath.isEmpty() && !canonicalRoot.isEmpty() &&
         canonicalPath.startsWith(canonicalRoot + QLatin1Char('/'));
}

bool versionMatches(const QString &expected, const QString &bundleVersion)
{
  return bundleVersion == expected || bundleVersion == expected + QStringLiteral(".0");
}

QStringList evidenceLines(const QByteArray &evidence)
{
  QStringList lines;
  for (const auto &line : evidence.split('\n')) {
    lines.append(QString::fromUtf8(line.trimmed()));
  }
  return lines;
}

QStringList valuesFor(const QStringList &lines, const QString &prefix)
{
  QStringList values;
  for (const auto &line : lines) {
    if (line.startsWith(prefix)) {
      values.append(line.mid(prefix.size()));
    }
  }
  return values;
}

} // namespace

UpdateVerifierResult
parseMacUpdateSignatureEvidence(const QByteArray &codesignEvidence, const QByteArray &gatekeeperEvidence)
{
  const auto codesignLines = evidenceLines(codesignEvidence);
  const auto identifiers = valuesFor(codesignLines, QStringLiteral("Identifier="));
  if (identifiers.size() != 1 || identifiers.first() != QString::fromLatin1(kBundleIdentifier)) {
    return {false, QStringLiteral("update has an unexpected macOS bundle identifier")};
  }

  const auto teams = valuesFor(codesignLines, QStringLiteral("TeamIdentifier="));
  if (teams.size() != 1 || teams.first() != QString::fromLatin1(kTeamIdentifier)) {
    return {false, QStringLiteral("update is signed by an unexpected Apple team")};
  }

  const QRegularExpression developerIdAuthority(
      QStringLiteral(R"(^Authority=Developer ID Application: .+ \(%1\)$)")
          .arg(QRegularExpression::escape(QString::fromLatin1(kTeamIdentifier)))
  );
  bool hasDeveloperIdAuthority = false;
  for (const auto &line : codesignLines) {
    hasDeveloperIdAuthority = hasDeveloperIdAuthority || developerIdAuthority.match(line).hasMatch();
  }
  if (!hasDeveloperIdAuthority) {
    return {false, QStringLiteral("update is not signed with the Deskflow Developer ID")};
  }

  if (!evidenceLines(gatekeeperEvidence).contains(QStringLiteral("source=Notarized Developer ID"))) {
    return {false, QStringLiteral("update was not accepted as a notarized Developer ID app")};
  }
  return {true, {}};
}

MacUpdateVerifier::MacUpdateVerifier(std::unique_ptr<IUpdateCommandRunner> runner) : m_runner(std::move(runner))
{
  if (!m_runner) {
    m_runner = createUpdateCommandRunner();
  }
}

UpdateVerifierResult MacUpdateVerifier::verify(const StagedUpdate &staged)
{
  const QFileInfo installer(staged.installerPath);
  if (installer.isSymLink() || !installer.isFile() ||
      installer.suffix().compare(QStringLiteral("dmg"), Qt::CaseInsensitive) != 0) {
    return {false, QStringLiteral("staged update is not a regular macOS disk image")};
  }

  QTemporaryDir temporary(QStringLiteral("%1/deskflow-update-XXXXXX").arg(QDir::tempPath()));
  if (!temporary.isValid()) {
    return {false, QStringLiteral("cannot create temporary update verification directory")};
  }
  const auto mountPoint = QDir(temporary.path()).filePath(QStringLiteral("volume"));
  if (!QDir().mkpath(mountPoint)) {
    return {false, QStringLiteral("cannot create update disk image mount point")};
  }

  const auto attach = m_runner->run(
      QStringLiteral("/usr/bin/hdiutil"),
      {QStringLiteral("attach"), installer.absoluteFilePath(), QStringLiteral("-readonly"), QStringLiteral("-nobrowse"),
       QStringLiteral("-noautoopen"), QStringLiteral("-mountpoint"), mountPoint},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(attach)) {
    return updateCommandFailure(QStringLiteral("mounting update disk image"), attach);
  }

  const auto verification = verifyMounted(mountPoint, temporary.path(), staged.version);
  const auto detach = m_runner->run(
      QStringLiteral("/usr/bin/hdiutil"), {QStringLiteral("detach"), mountPoint, QStringLiteral("-force")},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(detach)) {
    const auto failure = updateCommandFailure(QStringLiteral("unmounting update disk image"), detach);
    if (verification.ok) {
      return failure;
    }
    return {false, QStringLiteral("%1; %2").arg(verification.error, failure.error)};
  }
  return verification;
}

UpdateVerifierResult MacUpdateVerifier::verifyMounted(
    const QString &mountPoint, const QString &temporaryDirectory, const QString &expectedVersion
)
{
  const auto mountedApp = QDir(mountPoint).filePath(QStringLiteral("Deskflow.app"));
  const QFileInfo mountedAppInfo(mountedApp);
  if (mountedAppInfo.isSymLink() || !mountedAppInfo.isDir() || !isInside(mountedApp, mountPoint)) {
    return {false, QStringLiteral("update disk image does not contain Deskflow.app at its root")};
  }

  const auto copiedParent = QDir(temporaryDirectory).filePath(QStringLiteral("copied"));
  if (!QDir().mkpath(copiedParent)) {
    return {false, QStringLiteral("cannot create temporary copied app directory")};
  }
  const auto copiedApp = QDir(copiedParent).filePath(QStringLiteral("Deskflow.app"));
  const auto copy = m_runner->run(
      QStringLiteral("/usr/bin/ditto"), {mountedAppInfo.absoluteFilePath(), copiedApp}, kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(copy)) {
    return updateCommandFailure(QStringLiteral("copying update app bundle"), copy);
  }

  const QFileInfo copiedAppInfo(copiedApp);
  if (copiedAppInfo.isSymLink() || !copiedAppInfo.isDir() || !isInside(copiedApp, temporaryDirectory)) {
    return {false, QStringLiteral("copied update app bundle is not contained in the staging area")};
  }

  return verifyApplication(copiedAppInfo.absoluteFilePath(), expectedVersion);
}

UpdateVerifierResult
MacUpdateVerifier::verifyApplication(const QString &applicationPath, const QString &expectedVersion)
{
  const QFileInfo applicationInfo(applicationPath);
  if (applicationInfo.isSymLink() || !applicationInfo.isDir()) {
    return {false, QStringLiteral("update app bundle is not a regular directory")};
  }
  const auto verifySignature = m_runner->run(
      QStringLiteral("/usr/bin/codesign"),
      {QStringLiteral("--verify"), QStringLiteral("--deep"), QStringLiteral("--strict"), QStringLiteral("--verbose=2"),
       applicationInfo.absoluteFilePath()},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(verifySignature)) {
    return updateCommandFailure(QStringLiteral("verifying update code signature"), verifySignature);
  }

  if (!expectedVersion.isEmpty()) {
    const auto infoPlist = QDir(applicationInfo.absoluteFilePath()).filePath(QStringLiteral("Contents/Info.plist"));
    const QFileInfo infoPlistInfo(infoPlist);
    if (infoPlistInfo.isSymLink() || !infoPlistInfo.isFile() ||
        !isInside(infoPlist, applicationInfo.absoluteFilePath())) {
      return {false, QStringLiteral("update app bundle has no contained Info.plist")};
    }
    const auto version = m_runner->run(
        QStringLiteral("/usr/bin/plutil"),
        {QStringLiteral("-extract"), QStringLiteral("CFBundleShortVersionString"), QStringLiteral("raw"),
         QStringLiteral("-o"), QStringLiteral("-"), infoPlistInfo.absoluteFilePath()},
        kCommandTimeoutMs
    );
    if (!updateCommandSucceeded(version)) {
      return updateCommandFailure(QStringLiteral("reading update app version"), version);
    }
    if (!versionMatches(expectedVersion, QString::fromUtf8(commandOutput(version)))) {
      return {false, QStringLiteral("update app version does not match the selected release")};
    }
  }

  const auto displaySignature = m_runner->run(
      QStringLiteral("/usr/bin/codesign"),
      {QStringLiteral("--display"), QStringLiteral("--verbose=4"), applicationInfo.absoluteFilePath()},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(displaySignature)) {
    return updateCommandFailure(QStringLiteral("reading update code signature"), displaySignature);
  }

  const auto assess = m_runner->run(
      QStringLiteral("/usr/sbin/spctl"),
      {QStringLiteral("--assess"), QStringLiteral("--no-cache"), QStringLiteral("--type"), QStringLiteral("execute"),
       QStringLiteral("--verbose=4"), applicationInfo.absoluteFilePath()},
      kCommandTimeoutMs
  );
  if (!updateCommandSucceeded(assess)) {
    return updateCommandFailure(QStringLiteral("assessing update notarization"), assess);
  }

  return parseMacUpdateSignatureEvidence(commandOutput(displaySignature), commandOutput(assess));
}

std::unique_ptr<IUpdateCommandRunner> createUpdateCommandRunner()
{
  return std::make_unique<QProcessUpdateCommandRunner>();
}

bool updateCommandSucceeded(const UpdateCommandResult &result)
{
  return result.started && !result.timedOut && result.exitCode == 0;
}

UpdateVerifierResult updateCommandFailure(const QString &description, const UpdateCommandResult &result)
{
  if (result.timedOut) {
    return {false, QStringLiteral("%1 timed out").arg(description)};
  }
  const auto output = QString::fromUtf8(commandOutput(result));
  return {
      false, output.isEmpty() ? QStringLiteral("%1 failed").arg(description)
                              : QStringLiteral("%1 failed: %2").arg(description, output.left(240))
  };
}

} // namespace deskflow::gui
