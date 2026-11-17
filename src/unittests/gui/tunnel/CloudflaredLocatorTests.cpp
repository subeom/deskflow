/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CloudflaredLocatorTests.h"

#include "gui/tunnel/CloudflaredLocator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

using namespace deskflow::tunnel;
using Source = CloudflaredLocation::Source;

namespace {

// a file system that holds only these paths
FileCheck holding(const QStringList &paths)
{
  return [paths](const QString &path) { return paths.contains(path); };
}

} // namespace

void CloudflaredLocatorTests::knownLocations_areTheOnesOfTheInstallersOfEachSystem()
{
  QCOMPARE(
      cloudflaredKnownLocations(Platform::Windows),
      (QStringList{
          QStringLiteral("C:/Program Files (x86)/cloudflared/cloudflared.exe"),
          QStringLiteral("C:/Program Files/cloudflared/cloudflared.exe")
      })
  );
  QCOMPARE(
      cloudflaredKnownLocations(Platform::MacOS),
      (QStringList{QStringLiteral("/opt/homebrew/bin/cloudflared"), QStringLiteral("/usr/local/bin/cloudflared")})
  );
  QVERIFY(cloudflaredKnownLocations(Platform::Linux).contains(QStringLiteral("/usr/bin/cloudflared")));
}

void CloudflaredLocatorTests::fileName_hasTheExtensionOfWindowsOnly()
{
  QCOMPARE(cloudflaredFileName(Platform::Windows), QStringLiteral("cloudflared.exe"));
  QCOMPARE(cloudflaredFileName(Platform::MacOS), QStringLiteral("cloudflared"));
  QCOMPARE(cloudflaredFileName(Platform::Linux), QStringLiteral("cloudflared"));
}

void CloudflaredLocatorTests::locate_prefersTheConfiguredPathThenTheKnownPlacesThenPath()
{
  const auto configured = QStringLiteral("/Users/me/tools/cloudflared");
  const auto known = QStringLiteral("/opt/homebrew/bin/cloudflared");
  const auto onPath = QStringLiteral("/Users/me/bin/cloudflared");
  const auto pathVariable = QStringLiteral("/usr/bin:/Users/me/bin");

  auto all = locateCloudflared(configured, pathVariable, Platform::MacOS, holding({configured, known, onPath}));
  QCOMPARE(all.source, Source::Configured);
  QCOMPARE(all.path, configured);
  QVERIFY(!all.configuredPathUnusable);

  auto noConfigured = locateCloudflared(QString(), pathVariable, Platform::MacOS, holding({known, onPath}));
  QCOMPARE(noConfigured.source, Source::KnownLocation);
  QCOMPARE(noConfigured.path, known);

  auto onlyPath = locateCloudflared(QString(), pathVariable, Platform::MacOS, holding({onPath}));
  QCOMPARE(onlyPath.source, Source::Path);
  QCOMPARE(onlyPath.path, onPath);
  QVERIFY(onlyPath.found());

  // the second place of the installers, and the paths of Windows
  QCOMPARE(
      locateCloudflared(QString(), QString(), Platform::MacOS, holding({QStringLiteral("/usr/local/bin/cloudflared")}))
          .path,
      QStringLiteral("/usr/local/bin/cloudflared")
  );
  const auto x86 = QStringLiteral("C:/Program Files (x86)/cloudflared/cloudflared.exe");
  const auto x64 = QStringLiteral("C:/Program Files/cloudflared/cloudflared.exe");
  QCOMPARE(locateCloudflared(QString(), QString(), Platform::Windows, holding({x86, x64})).path, x86);
  QCOMPARE(locateCloudflared(QString(), QString(), Platform::Windows, holding({x64})).path, x64);
}

void CloudflaredLocatorTests::locate_aConfiguredPathThatCannotBeRunFallsBackAndSaysSo()
{
  const auto known = QStringLiteral("/opt/homebrew/bin/cloudflared");
  const auto location =
      locateCloudflared(QStringLiteral("/gone/cloudflared"), QString(), Platform::MacOS, holding({known}));
  QCOMPARE(location.source, Source::KnownLocation);
  QCOMPARE(location.path, known);
  QVERIFY(location.configuredPathUnusable);

  // blanks around the path are not a path
  const auto blank = locateCloudflared(QStringLiteral("   "), QString(), Platform::MacOS, holding({known}));
  QVERIFY(!blank.configuredPathUnusable);
}

void CloudflaredLocatorTests::locate_searchesTheDirectoriesOfPathInOrderAndNeverARelativeOne_data()
{
  QTest::addColumn<int>("platform");
  QTest::addColumn<QString>("pathVariable");
  QTest::addColumn<QStringList>("files");
  QTest::addColumn<QString>("expected");

  QTest::newRow("first-directory-wins") << int(Platform::Linux) << QStringLiteral("/a:/b")
                                        << QStringList{"/a/cloudflared", "/b/cloudflared"}
                                        << QStringLiteral("/a/cloudflared");
  QTest::newRow("empty-entries-are-skipped") << int(Platform::Linux) << QStringLiteral("::/b:")
                                             << QStringList{"/b/cloudflared"} << QStringLiteral("/b/cloudflared");
  QTest::newRow("a-dot-is-not-searched") << int(Platform::Linux) << QStringLiteral(".:/b")
                                         << QStringList{"./cloudflared", "/b/cloudflared"}
                                         << QStringLiteral("/b/cloudflared");
  QTest::newRow("a-relative-directory-is-not-searched")
      << int(Platform::MacOS) << QStringLiteral("bin:tools/bin")
      << QStringList{"bin/cloudflared", "tools/bin/cloudflared"} << QString();
  QTest::newRow("trailing-slash") << int(Platform::Linux) << QStringLiteral("/a/") << QStringList{"/a/cloudflared"}
                                  << QStringLiteral("/a/cloudflared");
  QTest::newRow("windows-separator-and-quotes")
      << int(Platform::Windows) << QStringLiteral("\"C:\\Tools With Space\";D:\\bin\\")
      << QStringList{"C:\\Tools With Space/cloudflared.exe"} << QStringLiteral("C:\\Tools With Space/cloudflared.exe");
  QTest::newRow("windows-skips-relative")
      << int(Platform::Windows) << QStringLiteral(".;bin;C:\\x")
      << QStringList{"./cloudflared.exe", "bin/cloudflared.exe", "C:\\x/cloudflared.exe"}
      << QStringLiteral("C:\\x/cloudflared.exe");
  QTest::newRow("a-colon-is-not-a-separator-on-windows")
      << int(Platform::Windows) << QStringLiteral("C:\\a;C:\\b") << QStringList{"C:\\b/cloudflared.exe"}
      << QStringLiteral("C:\\b/cloudflared.exe");
}

void CloudflaredLocatorTests::locate_searchesTheDirectoriesOfPathInOrderAndNeverARelativeOne()
{
  QFETCH(int, platform);
  QFETCH(QString, pathVariable);
  QFETCH(QStringList, files);
  QFETCH(QString, expected);

  const auto location = locateCloudflared(QString(), pathVariable, static_cast<Platform>(platform), holding(files));
  QCOMPARE(location.path, expected);
  QCOMPARE(location.found(), !expected.isEmpty());
}

void CloudflaredLocatorTests::locate_saysNotFoundWhenThereIsNothing()
{
  const auto location = locateCloudflared(QString(), QStringLiteral("/usr/bin"), Platform::MacOS, holding({}));
  QCOMPARE(location.source, Source::NotFound);
  QVERIFY(location.path.isEmpty());
  QVERIFY(!location.found());
  QVERIFY(!location.configuredPathUnusable);
}

void CloudflaredLocatorTests::locate_withTheSystemCheckDoesNotRunWhatIsNotAnExecutableFile()
{
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const auto data = dir.filePath(QStringLiteral("not-executable"));
  QFile file(data);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("x");
  file.close();
  QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));

  const auto check = systemFileCheck();
  QVERIFY(!check(dir.path()));
  QVERIFY(!check(dir.filePath(QStringLiteral("missing"))));
#ifndef Q_OS_WIN
  QVERIFY(!check(data));
  QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
  QVERIFY(check(data));
#endif
}

void CloudflaredLocatorTests::installHint_namesTheInstallerOfWindowsAndMac()
{
  QCOMPARE(cloudflaredInstallHint(Platform::Windows), QStringLiteral("winget install --id Cloudflare.cloudflared"));
  QCOMPARE(cloudflaredInstallHint(Platform::MacOS), QStringLiteral("brew install cloudflared"));
  QVERIFY(!cloudflaredInstallHint(Platform::Linux).isEmpty());
}

// A configured name without a directory can be looked up by the system in the PATH of the process, and the file that is
// found may not be the file that was checked (the working directory is not the PATH). The Service Token goes to
// whatever is started, so the identity of that file has to be one: only an absolute path is taken, and it is the path
// that is run.
void CloudflaredLocatorTests::aRelativeConfiguredPath_isRefusedAndNothingElseIsUsedInstead_data()
{
  QTest::addColumn<int>("platform");
  QTest::addColumn<QString>("configured");

  QTest::newRow("bare name") << int(Platform::MacOS) << QStringLiteral("cloudflared");
  QTest::newRow("dot slash") << int(Platform::Linux) << QStringLiteral("./cloudflared");
  QTest::newRow("a directory below") << int(Platform::MacOS) << QStringLiteral("bin/cloudflared");
  QTest::newRow("parent") << int(Platform::Linux) << QStringLiteral("../cloudflared");
  QTest::newRow("home sign") << int(Platform::MacOS) << QStringLiteral("~/bin/cloudflared");
  QTest::newRow("windows bare name") << int(Platform::Windows) << QStringLiteral("cloudflared.exe");
  QTest::newRow("windows dot backslash") << int(Platform::Windows) << QStringLiteral(".\\cloudflared.exe");
  QTest::newRow("windows drive relative") << int(Platform::Windows) << QStringLiteral("C:cloudflared.exe");
  QTest::newRow("windows rooted without a drive") << int(Platform::Windows) << QStringLiteral("\\opt\\cloudflared.exe");
}

void CloudflaredLocatorTests::aRelativeConfiguredPath_isRefusedAndNothingElseIsUsedInstead()
{
  QFETCH(int, platform);
  QFETCH(QString, configured);
  const auto system = static_cast<Platform>(platform);

  // the file is there, in the working directory and in the places of the installers and in PATH: none of it is used
  QStringList everything{configured};
  for (const auto &known : cloudflaredKnownLocations(system)) {
    everything << known;
  }
  const auto pathVariable = system == Platform::Windows ? QStringLiteral("C:\\Tools") : QStringLiteral("/Users/me/bin");
  everything
      << (system == Platform::Windows ? QStringLiteral("C:\\Tools/cloudflared.exe")
                                      : QStringLiteral("/Users/me/bin/cloudflared"));

  const auto location = locateCloudflared(configured, pathVariable, system, holding(everything));
  QVERIFY(!location.found());
  QVERIFY(location.path.isEmpty());
  QVERIFY(location.configuredPathRelative);
}

void CloudflaredLocatorTests::theLocationIsOneCanonicalPath_whereverItWasFound()
{
  const auto real = QStringLiteral("/opt/homebrew/Cellar/cloudflared/2026.1.0/bin/cloudflared");
  const PathResolver resolve = [&](const QString &path) {
    // every path that the files below are known by is a link to the same file
    return holding({QStringLiteral("/Users/me/tools/cloudflared"), QStringLiteral("/opt/homebrew/bin/cloudflared"), QStringLiteral("/Users/me/bin/cloudflared")})(
               path
           )
               ? real
               : path;
  };
  const auto files = holding(
      {QStringLiteral("/Users/me/tools/cloudflared"), QStringLiteral("/opt/homebrew/bin/cloudflared"),
       QStringLiteral("/Users/me/bin/cloudflared")}
  );

  const auto configured =
      locateCloudflared(QStringLiteral("/Users/me/tools/cloudflared"), QString(), Platform::MacOS, files, resolve);
  QCOMPARE(configured.source, Source::Configured);
  QCOMPARE(configured.path, real);

  const auto known = locateCloudflared(QString(), QString(), Platform::MacOS, files, resolve);
  QCOMPARE(known.source, Source::KnownLocation);
  QCOMPARE(known.path, real);

  const auto onPath = locateCloudflared(QString(), QStringLiteral("/Users/me/bin"), Platform::Linux, files, resolve);
  QCOMPARE(onPath.source, Source::Path);
  QCOMPARE(onPath.path, real);
}

void CloudflaredLocatorTests::aCandidateThatCannotBeResolved_isNotUsed()
{
  const auto known = QStringLiteral("/opt/homebrew/bin/cloudflared");
  const auto second = QStringLiteral("/usr/local/bin/cloudflared");
  // a dangling link: it is a file for the check and has no place for the resolver
  const PathResolver resolve = [&](const QString &path) { return path == known ? QString() : path; };

  const auto location = locateCloudflared(QString(), QString(), Platform::MacOS, holding({known, second}), resolve);
  QCOMPARE(location.path, second);
  QCOMPARE(location.source, Source::KnownLocation);

  const auto none = locateCloudflared(QString(), QString(), Platform::MacOS, holding({known}), resolve);
  QVERIFY(!none.found());
}

void CloudflaredLocatorTests::locateWithTheSystem_neverTakesAFileOfTheWorkingDirectory()
{
#ifdef Q_OS_WIN
  QSKIP("an executable file is made with a permission bit, which Windows does not have");
#else
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto file = dir.filePath(QStringLiteral("cloudflared"));
  QFile out(file);
  QVERIFY(out.open(QIODevice::WriteOnly));
  out.write("#!/bin/sh\nexit 0\n");
  out.close();
  QVERIFY(out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

  const auto before = QDir::currentPath();
  QVERIFY(QDir::setCurrent(dir.path()));
  const auto location = locateCloudflared(QStringLiteral("cloudflared"));
  QVERIFY(QDir::setCurrent(before));

  QVERIFY(location.configuredPathRelative);
  QVERIFY(!location.found());
#endif
}

void CloudflaredLocatorTests::locateWithTheSystem_resolvesALinkToTheFileItIs()
{
#ifdef Q_OS_WIN
  QSKIP("links need a privilege on Windows");
#else
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const auto real = dir.filePath(QStringLiteral("real-cloudflared"));
  QFile out(real);
  QVERIFY(out.open(QIODevice::WriteOnly));
  out.write("#!/bin/sh\nexit 0\n");
  out.close();
  QVERIFY(out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
  const auto link = dir.filePath(QStringLiteral("link-to-cloudflared"));
  QVERIFY(QFile::link(real, link));

  const auto location = locateCloudflared(link);
  QCOMPARE(location.source, Source::Configured);
  QCOMPARE(location.path, QFileInfo(real).canonicalFilePath());
  QVERIFY(QDir::isAbsolutePath(location.path));
  QVERIFY(!QFileInfo(location.path).isSymLink());
#endif
}

void CloudflaredLocatorTests::isAbsolutePath_knowsTheSyntaxOfEachSystem()
{
  QVERIFY(isAbsolutePath(QStringLiteral("/opt/x"), Platform::MacOS));
  QVERIFY(isAbsolutePath(QStringLiteral("/opt/x"), Platform::Linux));
  QVERIFY(!isAbsolutePath(QStringLiteral("opt/x"), Platform::Linux));
  QVERIFY(!isAbsolutePath(QStringLiteral(""), Platform::Linux));
  QVERIFY(isAbsolutePath(QStringLiteral("C:/Tools/x.exe"), Platform::Windows));
  QVERIFY(isAbsolutePath(QStringLiteral("c:\\Tools\\x.exe"), Platform::Windows));
  QVERIFY(isAbsolutePath(QStringLiteral("\\\\server\\share\\x.exe"), Platform::Windows));
  QVERIFY(!isAbsolutePath(QStringLiteral("C:x.exe"), Platform::Windows));
  QVERIFY(!isAbsolutePath(QStringLiteral("x.exe"), Platform::Windows));
  QVERIFY(!isAbsolutePath(QStringLiteral("/opt/x"), Platform::Windows));
}

QTEST_MAIN(CloudflaredLocatorTests)
