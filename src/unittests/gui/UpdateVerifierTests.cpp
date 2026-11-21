/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateVerifierTests.h"

#include "gui/MacUpdateVerifier.h"

#include <QFile>
#include <QDir>
#include <QTemporaryDir>

#include <algorithm>
#include <memory>

using deskflow::gui::IUpdateCommandRunner;
using deskflow::gui::MacUpdateVerifier;
using deskflow::gui::UpdateCommandResult;

namespace {

constexpr auto kPinnedRequirement =
    R"(designated => identifier "org.deskflow.deskflow" and certificate leaf = H"39b1c21df0ba75cdde058fcbb09a1ad38aef8055")";

QByteArray evidence(const QByteArray &requirement = QByteArray(kPinnedRequirement))
{
  return QByteArrayLiteral("Identifier=org.deskflow.deskflow\nAuthority=Deskflow Local Signing\n") +
         requirement + QByteArrayLiteral("\nTeamIdentifier=not set\n");
}

bool writeFile(const QString &path, const QByteArray &contents)
{
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}

class EvidenceRunner final : public IUpdateCommandRunner
{
public:
  UpdateCommandResult run(
      const QString &program, const QStringList &arguments, int
  ) override
  {
    commands.append({program, arguments});
    if (program.endsWith(QStringLiteral("/plutil"))) {
      const auto &field = arguments.at(1);
      if (field == QStringLiteral("CFBundleIdentifier")) {
        return {true, false, 0, QByteArrayLiteral("org.deskflow.deskflow"), {}};
      }
      if (field == QStringLiteral("CFBundleShortVersionString") ||
          field == QStringLiteral("CFBundleVersion")) {
        return {true, false, 0, QByteArrayLiteral("2.0.6"), {}};
      }
    }
    if (program.endsWith(QStringLiteral("/codesign")) && arguments.contains(QStringLiteral("-d"))) {
      return {true, false, 0, {}, evidenceOutput};
    }
    if (fail) {
      return {true, false, 1, {}, QByteArray(300, 'x')};
    }
    return {true, false, 0, {}, {}};
  }

  QList<QPair<QString, QStringList>> commands;
  QByteArray evidenceOutput = evidence();
  bool fail = false;
};

bool appFixture(QTemporaryDir &temporary, QString &path)
{
  const auto app = QDir(temporary.path()).filePath(QStringLiteral("Deskflow.app"));
  if (!QDir().mkpath(QDir(app).filePath(QStringLiteral("Contents/MacOS"))) ||
      !writeFile(QDir(app).filePath(QStringLiteral("Contents/Info.plist")), QByteArrayLiteral("plist")) ||
      !writeFile(QDir(app).filePath(QStringLiteral("Contents/MacOS/Deskflow")), QByteArrayLiteral("gui")) ||
      !writeFile(QDir(app).filePath(QStringLiteral("Contents/MacOS/deskflow-core")), QByteArrayLiteral("core"))) {
    return false;
  }
  path = app;
  return true;
}

} // namespace

void UpdateVerifierTests::signatureEvidence_acceptsPinnedForkIdentity()
{
  const auto result = deskflow::gui::parseMacUpdateSignatureEvidence(evidence());
  QVERIFY2(result.ok, qPrintable(result.error));
}

void UpdateVerifierTests::signatureEvidence_rejectsWrongBundleWrongLeafAdHocAndDeveloperId()
{
  const QList<QByteArray> invalid{
      QByteArrayLiteral(
          "Identifier=org.deskflow.other\n"
          "designated => identifier \"org.deskflow.other\" and certificate leaf = "
          "H\"39b1c21df0ba75cdde058fcbb09a1ad38aef8055\"\n"
      ),
      QByteArray(
          "Identifier=org.deskflow.deskflow\n"
          "designated => identifier \"org.deskflow.deskflow\" and certificate leaf = "
          "H\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"\n"
      ),
      QByteArrayLiteral("Identifier=org.deskflow.deskflow\ndesignated => cdhash H\"deadbeef\"\n"),
      evidence() + QByteArrayLiteral(
                       "Authority=Developer ID Application: Deskflow (5QCJCX8C37)\n"
                       "TeamIdentifier=5QCJCX8C37\n"
                   ),
  };
  for (const auto &payload : invalid)
    QVERIFY(!deskflow::gui::parseMacUpdateSignatureEvidence(payload).ok);
}

void UpdateVerifierTests::signatureEvidence_rejectsNotarizationOnlyAndMissingCore()
{
  const auto notarized = evidence() + QByteArrayLiteral("source=Notarized Developer ID\n");
  QVERIFY(!deskflow::gui::parseMacUpdateSignatureEvidence(notarized).ok);

  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QString app;
  QVERIFY(appFixture(temporary, app));
  QVERIFY(QFile::remove(QDir(app).filePath(QStringLiteral("Contents/MacOS/deskflow-core"))));
  MacUpdateVerifier verifier(std::make_unique<EvidenceRunner>());
  QVERIFY(!verifier.verifyApplication(app, QStringLiteral("2.0.6")).ok);
}

void UpdateVerifierTests::bundleVersion_acceptsStableOrZeroFourthComponentOnly()
{
  const auto accepted = QStringLiteral("2.0.5");
  QVERIFY(deskflow::gui::isMacUpdateBundleVersion(accepted, QStringLiteral("2.0.5")));
  QVERIFY(deskflow::gui::isMacUpdateBundleVersion(accepted, QStringLiteral("2.0.5.0")));
  for (const auto rejected : {
           QStringLiteral("2.0.5.1"),
           QStringLiteral("2.0.5.0.0"),
           QStringLiteral("v2.0.5"),
           QStringLiteral("2.0.5-rc1"),
           QStringLiteral("2.0.5 "),
           QStringLiteral("2.0"),
           QStringLiteral("02.0.5")}) {
    QVERIFY(!deskflow::gui::isMacUpdateBundleVersion(accepted, rejected));
  }
  QVERIFY(!deskflow::gui::isMacUpdateBundleVersion(
      QStringLiteral("2.0.5-rc1"), QStringLiteral("2.0.5.0")
  ));
}

void UpdateVerifierTests::verifierApplication_checksVersionBundleAndNestedCore()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QString app;
  QVERIFY(appFixture(temporary, app));
  auto runner = std::make_unique<EvidenceRunner>();
  auto *rawRunner = runner.get();
  MacUpdateVerifier verifier(std::move(runner));
  const auto result = verifier.verifyApplication(app, QStringLiteral("2.0.6"));
  QVERIFY2(result.ok, qPrintable(result.error));
  QVERIFY(std::ranges::any_of(rawRunner->commands, [](const auto &command) {
    return command.second.contains(QStringLiteral("--test-requirement==certificate leaf = "
                                                  "H\"39b1c21df0ba75cdde058fcbb09a1ad38aef8055\""));
  }));
  QVERIFY(!verifier.verifyApplication(app, QStringLiteral("2.0.7")).ok);
}

void UpdateVerifierTests::verifierApplication_boundsCommandFailures()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QString app;
  QVERIFY(appFixture(temporary, app));
  auto runner = std::make_unique<EvidenceRunner>();
  runner->fail = true;
  MacUpdateVerifier verifier(std::move(runner));
  const auto result = verifier.verifyApplication(app, QStringLiteral("2.0.6"));
  QVERIFY(!result.ok);
  QVERIFY(result.error.size() <= 240);
}

QTEST_MAIN(UpdateVerifierTests)
