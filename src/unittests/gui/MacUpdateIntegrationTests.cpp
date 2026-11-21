/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacUpdateIntegrationTests.h"

#include "gui/MacUpdateVerifier.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

using deskflow::gui::IUpdateCommandRunner;
using deskflow::gui::UpdateCommandResult;

namespace {

constexpr auto kEvidence =
    "Identifier=org.deskflow.deskflow\n"
    "Authority=Deskflow Local Signing\n"
    "designated => identifier \"org.deskflow.deskflow\" and certificate leaf = "
    "H\"39b1c21df0ba75cdde058fcbb09a1ad38aef8055\"\n"
    "TeamIdentifier=not set\n";

bool writeFile(const QString &path, const QByteArray &contents)
{
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}

UpdateCommandResult runReal(const QString &program, const QStringList &arguments, int timeoutMs)
{
  QProcess process;
  process.start(program, arguments);
  if (!process.waitForStarted(timeoutMs)) {
    return {false, false, -1, {}, process.errorString().toUtf8()};
  }
  if (!process.waitForFinished(timeoutMs)) {
    process.kill();
    process.waitForFinished();
    return {true, true, -1, {}, QByteArrayLiteral("timed out")};
  }
  return {
      true,
      false,
      process.exitCode(),
      process.readAllStandardOutput(),
      process.readAllStandardError()
  };
}

class SyntheticDmgRunner final : public IUpdateCommandRunner
{
public:
  UpdateCommandResult run(
      const QString &program, const QStringList &arguments, int timeoutMs
  ) override
  {
    if (program == QStringLiteral("/usr/bin/hdiutil") ||
        program == QStringLiteral("/usr/bin/ditto")) {
      return runReal(program, arguments, timeoutMs);
    }
    if (program == QStringLiteral("/usr/bin/plutil")) {
      if (arguments.at(1) == QStringLiteral("CFBundleIdentifier")) {
        return {true, false, 0, QByteArrayLiteral("org.deskflow.deskflow"), {}};
      }
      return {true, false, 0, QByteArrayLiteral("2.0.6"), {}};
    }
    if (program == QStringLiteral("/usr/bin/codesign")) {
      if (arguments.contains(QStringLiteral("-d"))) {
        return {true, false, 0, {}, QByteArray(kEvidence)};
      }
      return {true, false, 0, {}, {}};
    }
    return {false, false, -1, {}, QByteArrayLiteral("unexpected command")};
  }
};

} // namespace

void MacUpdateIntegrationTests::verifier_mountsSyntheticDmgCopiesChecksAndDetaches()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const auto source = QDir(temporary.path()).filePath(QStringLiteral("source"));
  const auto app = QDir(source).filePath(QStringLiteral("Deskflow.app"));
  QVERIFY(QDir().mkpath(QDir(app).filePath(QStringLiteral("Contents/MacOS"))));
  QVERIFY(writeFile(
      QDir(app).filePath(QStringLiteral("Contents/Info.plist")),
      QByteArrayLiteral(
          "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict>"
          "<key>CFBundleIdentifier</key><string>org.deskflow.deskflow</string>"
          "<key>CFBundleShortVersionString</key><string>2.0.6</string>"
          "<key>CFBundleVersion</key><string>2.0.6</string>"
          "</dict></plist>"
      )
  ));
  QVERIFY(writeFile(QDir(app).filePath(QStringLiteral("Contents/MacOS/Deskflow")), QByteArrayLiteral("gui")));
  QVERIFY(writeFile(QDir(app).filePath(QStringLiteral("Contents/MacOS/deskflow-core")), QByteArrayLiteral("core")));

  const auto dmg = QDir(temporary.path()).filePath(QStringLiteral("fixture.dmg"));
  const auto created = runReal(
      QStringLiteral("/usr/bin/hdiutil"),
      {QStringLiteral("create"), QStringLiteral("-volname"), QStringLiteral("Deskflow"),
       QStringLiteral("-srcfolder"), source, QStringLiteral("-format"), QStringLiteral("UDZO"),
       QStringLiteral("-ov"), dmg},
      120'000
  );
  QVERIFY2(created.started && !created.timedOut && created.exitCode == 0, created.standardError.constData());
  QVERIFY(QFileInfo(dmg).isFile());

  deskflow::gui::MacUpdateVerifier verifier(std::make_unique<SyntheticDmgRunner>());
  deskflow::gui::StagedUpdate staged{
      QStringLiteral("2.0.6"),
      QStringLiteral("fixture.dmg"),
      QFileInfo(dmg).absoluteFilePath(),
      QDir(temporary.path()).filePath(QStringLiteral("manifest.json"))
  };
  const auto result = verifier.verify(staged);
  QVERIFY2(result.ok, qPrintable(result.error));
  QVERIFY(QFileInfo(dmg).isFile());
}

QTEST_MAIN(MacUpdateIntegrationTests)
