/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CloudflaredLocatorTests.h"

#include "gui/tunnel/CloudflaredLocator.h"

#include <QFile>
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

QTEST_MAIN(CloudflaredLocatorTests)
