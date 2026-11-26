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

#include <ranges>

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
    commands.append({program, arguments});
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

  QList<QPair<QString, QStringList>> commands;
};

bool createAppSource(QTemporaryDir &temporary, QString &source, QString &app)
{
  source = QDir(temporary.path()).filePath(QStringLiteral("source"));
  app = QDir(source).filePath(QStringLiteral("Deskflow.app"));
  return QDir().mkpath(QDir(app).filePath(QStringLiteral("Contents/MacOS"))) &&
         writeFile(
             QDir(app).filePath(QStringLiteral("Contents/Info.plist")),
             QByteArrayLiteral(
                 "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict>"
                 "<key>CFBundleIdentifier</key><string>org.deskflow.deskflow</string>"
                 "<key>CFBundleShortVersionString</key><string>2.0.6</string>"
                 "<key>CFBundleVersion</key><string>2.0.6</string>"
                 "</dict></plist>"
             )
         ) &&
         writeFile(
             QDir(app).filePath(QStringLiteral("Contents/MacOS/Deskflow")),
             QByteArrayLiteral("gui")
         ) &&
         writeFile(
             QDir(app).filePath(QStringLiteral("Contents/MacOS/deskflow-core")),
             QByteArrayLiteral("core")
         );
}

UpdateCommandResult createSyntheticDmg(
    const QString &source, const QString &dmg
)
{
  return runReal(
      QStringLiteral("/usr/bin/hdiutil"),
      {QStringLiteral("create"), QStringLiteral("-volname"), QStringLiteral("Deskflow"),
       QStringLiteral("-srcfolder"), source, QStringLiteral("-format"), QStringLiteral("UDZO"),
       QStringLiteral("-ov"), dmg},
      120'000
  );
}

UpdateCommandResult addSyntheticSla(const QString &dmg, QTemporaryDir &temporary)
{
  const auto xml = QDir(temporary.path()).filePath(QStringLiteral("sla.xml"));
  const auto filler = QByteArray(72 * 1024, 'A');
  const auto resourceXml =
      QByteArrayLiteral(
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
          "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
          "<plist version=\"1.0\"><dict>\n"
          "<key>LPic</key><array><dict><key>Attributes</key><string>0x0000</string>"
          "<key>Data</key><data>"
          "AAIAEQADAAEAAAAAAAIAAAAIAAMAAAABAAQAAAAEAAUAAAAOAAYA"
          "AQAFAAcAAAAHAAgAAABHAAkAAAA0AAoAAQA1AAsAAQAgAAwAAAAR"
          "AA0AAABbAAQAAAAzAA8AAQAMABAAAAALAA4AAA=="
          "</data><key>ID</key><string>5000</string><key>Name</key><string></string></dict></array>\n"
          "<key>STR#</key><array><dict><key>Attributes</key><string>0x0000</string>"
          "<key>Data</key><data>AAkHRW5nbGlzaAVBZ3JlZQhEaXNhZ3JlZQVQcmludAdTYXZlLi4u</data>"
          "<key>ID</key><string>5002</string><key>Name</key><string>English</string></dict></array>\n"
          "<key>TEXT</key><array><dict><key>Attributes</key><string>0x0000</string>"
          "<key>Data</key><data>"
      ) +
      filler +
      QByteArrayLiteral(
          "</data><key>ID</key><string>5002</string><key>Name</key><string>English</string></dict></array>\n"
          "</dict></plist>\n"
      );
  if (!writeFile(
      xml, resourceXml
  )) {
    return {false, false, -1, {}, QByteArrayLiteral("cannot write synthetic SLA resources")};
  }
  return runReal(
      QStringLiteral("/usr/bin/hdiutil"),
      {QStringLiteral("udifrez"), dmg, QStringLiteral("-xml"), xml, QStringLiteral("-replaceall")},
      120'000
  );
}

} // namespace

void MacUpdateIntegrationTests::verifier_mountsSyntheticDmgCopiesChecksAndDetaches()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QString source;
  QString app;
  QVERIFY(createAppSource(temporary, source, app));

  const auto dmg = QDir(temporary.path()).filePath(QStringLiteral("fixture.dmg"));
  const auto created = createSyntheticDmg(source, dmg);
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

void MacUpdateIntegrationTests::verifier_rejectsSyntheticDmgWithSlaResourceBeforeMount()
{
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QString source;
  QString app;
  QVERIFY(createAppSource(temporary, source, app));

  const auto dmg = QDir(temporary.path()).filePath(QStringLiteral("fixture-sla.dmg"));
  const auto created = createSyntheticDmg(source, dmg);
  QVERIFY2(created.started && !created.timedOut && created.exitCode == 0, created.standardError.constData());
  const auto embedded = addSyntheticSla(dmg, temporary);
  QVERIFY2(embedded.started && !embedded.timedOut && embedded.exitCode == 0, embedded.standardError.constData());
  const auto inspected = runReal(
      QStringLiteral("/usr/bin/hdiutil"),
      {QStringLiteral("udifderez"), QStringLiteral("-xml"), dmg},
      120'000
  );
  QVERIFY2(inspected.started && !inspected.timedOut && inspected.exitCode == 0, inspected.standardError.constData());
  QVERIFY(inspected.standardOutput.size() > 64 * 1024);

  auto runner = std::make_unique<SyntheticDmgRunner>();
  auto *rawRunner = runner.get();
  deskflow::gui::MacUpdateVerifier verifier(std::move(runner));
  const deskflow::gui::StagedUpdate staged{
      QStringLiteral("2.0.6"),
      QStringLiteral("fixture-sla.dmg"),
      dmg,
      QDir(temporary.path()).filePath(QStringLiteral("manifest.json"))
  };
  const auto result = verifier.verify(staged);
  QVERIFY(!result.ok);
  QVERIFY(result.error.contains(QStringLiteral("license"), Qt::CaseInsensitive));
  QVERIFY(std::ranges::any_of(rawRunner->commands, [](const auto &command) {
    return command.first == QStringLiteral("/usr/bin/hdiutil") &&
           command.second.value(0) == QStringLiteral("udifderez");
  }));
  QVERIFY(std::ranges::none_of(rawRunner->commands, [](const auto &command) {
    return command.first == QStringLiteral("/usr/bin/hdiutil") &&
           command.second.value(0) == QStringLiteral("attach");
  }));
}

QTEST_MAIN(MacUpdateIntegrationTests)
