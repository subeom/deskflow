/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateVerifierTests.h"

#include "gui/MacUpdateVerifier.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>

#include <memory>

namespace {

QByteArray validCodesignEvidence()
{
  return R"signature(
Executable=/private/tmp/Deskflow.app/Contents/MacOS/Deskflow
Identifier=org.deskflow.deskflow
Authority=Developer ID Application: Nick Bolton (5QCJCX8C37)
Authority=Developer ID Certification Authority
Authority=Apple Root CA
TeamIdentifier=5QCJCX8C37
)signature";
}

QByteArray validAssessment()
{
  return "/private/tmp/Deskflow.app: accepted\n"
         "source=Notarized Developer ID\n";
}

class FakeCommandRunner final : public deskflow::gui::IUpdateCommandRunner
{
public:
  deskflow::gui::UpdateCommandResult run(const QString &program, const QStringList &arguments, int timeoutMs) override
  {
    calls.append(Call{program, arguments, timeoutMs});

    if (program.endsWith(QStringLiteral("/hdiutil")) && arguments.first() == QStringLiteral("attach")) {
      const auto mountIndex = arguments.indexOf(QStringLiteral("-mountpoint"));
      if (mountIndex < 0) {
        return failure(QStringLiteral("missing mount point"));
      }
      mountPoint = arguments.at(mountIndex + 1);
      QDir().mkpath(QDir(mountPoint).filePath(QStringLiteral("Deskflow.app")));
      return success();
    }
    if (program.endsWith(QStringLiteral("/ditto")) && arguments.size() == 2) {
      copiedPath = arguments.at(1);
      QDir().mkpath(QDir(copiedPath).filePath(QStringLiteral("Contents")));
      QFile infoPlist(QDir(copiedPath).filePath(QStringLiteral("Contents/Info.plist")));
      if (!infoPlist.open(QIODevice::WriteOnly)) {
        return failure(QStringLiteral("cannot create Info.plist"));
      }
      infoPlist.close();
      return success();
    }
    if (program.endsWith(QStringLiteral("/codesign")) && arguments.contains(QStringLiteral("--display"))) {
      return {true, false, 0, codesignEvidence, {}};
    }
    if (program.endsWith(QStringLiteral("/spctl"))) {
      return {true, false, 0, assessment, {}};
    }
    if (program.endsWith(QStringLiteral("/plutil"))) {
      return {true, false, 0, bundleVersion.toUtf8() + QByteArrayLiteral("\n"), {}};
    }
    return success();
  }

  static deskflow::gui::UpdateCommandResult success()
  {
    return {true, false, 0, {}, {}};
  }

  static deskflow::gui::UpdateCommandResult failure(const QString &message)
  {
    return {false, false, -1, {}, message.toUtf8()};
  }

  struct Call
  {
    QString program;
    QStringList arguments;
    int timeoutMs = 0;
  };

  QList<Call> calls;
  QString mountPoint;
  QString copiedPath;
  QByteArray codesignEvidence = validCodesignEvidence();
  QByteArray assessment = validAssessment();
  QString bundleVersion = QStringLiteral("1.28.0");
};

deskflow::gui::StagedUpdate writeStagedUpdate(QTemporaryDir &directory)
{
  const auto installerPath = directory.filePath(QStringLiteral("deskflow-1.28.0-macos-arm64.dmg"));
  QFile installer(installerPath);
  if (!installer.open(QIODevice::WriteOnly) || installer.write("dmg") != 3) {
    return {};
  }
  installer.close();
  return {
      QStringLiteral("1.28.0"), QFileInfo(installerPath).fileName(), installerPath,
      directory.filePath(QStringLiteral("manifest.json"))
  };
}

bool hasCall(const QList<FakeCommandRunner::Call> &calls, const QString &program, const QString &argument)
{
  return std::any_of(calls.begin(), calls.end(), [&program, &argument](const auto &call) {
    return call.program.endsWith(program) && call.arguments.contains(argument);
  });
}

} // namespace

void UpdateVerifierTests::signatureEvidence_acceptsPinnedNotarizedDeveloperId()
{
  const auto result = deskflow::gui::parseMacUpdateSignatureEvidence(validCodesignEvidence(), validAssessment());
  QVERIFY2(result.ok, qPrintable(result.error));
}

void UpdateVerifierTests::signatureEvidence_rejectsWrongIdentityTeamOrAuthority()
{
  auto evidence = validCodesignEvidence();
  auto result = deskflow::gui::parseMacUpdateSignatureEvidence(
      evidence.replace("Identifier=org.deskflow.deskflow", "Identifier=com.example.deskflow"), validAssessment()
  );
  QVERIFY(!result.ok);

  evidence = validCodesignEvidence();
  result = deskflow::gui::parseMacUpdateSignatureEvidence(
      evidence.replace("TeamIdentifier=5QCJCX8C37", "TeamIdentifier=AAAAAAAAAA"), validAssessment()
  );
  QVERIFY(!result.ok);

  evidence = validCodesignEvidence();
  result = deskflow::gui::parseMacUpdateSignatureEvidence(
      evidence.replace(
          "Authority=Developer ID Application: Nick Bolton (5QCJCX8C37)",
          "Authority=Apple Development: Nick Bolton (5QCJCX8C37)"
      ),
      validAssessment()
  );
  QVERIFY(!result.ok);
}

void UpdateVerifierTests::signatureEvidence_rejectsUnnotarizedAssessment()
{
  const auto result = deskflow::gui::parseMacUpdateSignatureEvidence(
      validCodesignEvidence(), "/private/tmp/Deskflow.app: accepted\nsource=Developer ID\n"
  );
  QVERIFY(!result.ok);
}

void UpdateVerifierTests::verifier_mountsCopiesChecksAndDetaches()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto staged = writeStagedUpdate(directory);
  QVERIFY(!staged.installerPath.isEmpty());

  auto runner = std::make_unique<FakeCommandRunner>();
  auto *fake = runner.get();
  deskflow::gui::MacUpdateVerifier verifier(std::move(runner));

  const auto result = verifier.verify(staged);
  QVERIFY2(result.ok, qPrintable(result.error));
  QVERIFY(hasCall(fake->calls, QStringLiteral("/hdiutil"), QStringLiteral("attach")));
  QVERIFY(hasCall(fake->calls, QStringLiteral("/ditto"), fake->copiedPath));
  QVERIFY(hasCall(fake->calls, QStringLiteral("/codesign"), QStringLiteral("--verify")));
  QVERIFY(hasCall(fake->calls, QStringLiteral("/spctl"), QStringLiteral("execute")));
  QVERIFY(hasCall(fake->calls, QStringLiteral("/hdiutil"), QStringLiteral("detach")));
  QVERIFY(!QFileInfo::exists(fake->mountPoint));
  QVERIFY(!QFileInfo::exists(fake->copiedPath));
}

void UpdateVerifierTests::verifier_detachesAndRemovesCopyWhenEvidenceFails()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto staged = writeStagedUpdate(directory);
  QVERIFY(!staged.installerPath.isEmpty());

  auto runner = std::make_unique<FakeCommandRunner>();
  runner->assessment = "/private/tmp/Deskflow.app: rejected\nsource=Developer ID\n";
  auto *fake = runner.get();
  deskflow::gui::MacUpdateVerifier verifier(std::move(runner));

  QVERIFY(!verifier.verify(staged).ok);
  QVERIFY(hasCall(fake->calls, QStringLiteral("/hdiutil"), QStringLiteral("detach")));
  QVERIFY(!QFileInfo::exists(fake->mountPoint));
  QVERIFY(!QFileInfo::exists(fake->copiedPath));
}

void UpdateVerifierTests::verifier_rejectsAnUnexpectedApplicationVersion()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto staged = writeStagedUpdate(directory);
  QVERIFY(!staged.installerPath.isEmpty());

  auto runner = std::make_unique<FakeCommandRunner>();
  runner->bundleVersion = QStringLiteral("1.27.0");
  auto *fake = runner.get();
  deskflow::gui::MacUpdateVerifier verifier(std::move(runner));

  QVERIFY(!verifier.verify(staged).ok);
  QVERIFY(hasCall(fake->calls, QStringLiteral("/hdiutil"), QStringLiteral("detach")));
}

void UpdateVerifierTests::platformVerifier_isAvailableOnlyOnMac()
{
  const auto verifier = deskflow::gui::createPlatformUpdateVerifier();
#if defined(Q_OS_MACOS)
  QVERIFY(verifier);
#else
  QVERIFY(!verifier);
#endif
}

QTEST_MAIN(UpdateVerifierTests)
