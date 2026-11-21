/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateReleaseTests.h"

#include "gui/UpdateRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using deskflow::gui::UpdateEnvironment;
using deskflow::gui::UpdateInstallMode;
using deskflow::gui::parseChecksumManifest;
using deskflow::gui::parseGitHubRelease;
using deskflow::gui::planUpdate;

namespace {

constexpr auto kArmDigest = "sha256:38749e16c5971bfefc10278fad8adf8ad57ae2ec3e2a11275be1774a1dfb2fdf";
constexpr auto kX64Digest = "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr auto kSumsDigest = "sha256:2a5cb53a2bd6b1d471ea757983c59c63fa65687b4fd09a69424d5ccf260763b6";
constexpr auto kVersion = "2.0.6";
constexpr auto kTag = "v2.0.6";

QJsonObject asset(const QString &name, const QString &tag, const QString &digest, qint64 size = 1024)
{
  return {
      {"name", name},
      {"browser_download_url",
       QStringLiteral("https://github.com/subeom/deskflow/releases/download/%1/%2").arg(tag, name)},
      {"size", size},
      {"digest", digest},
  };
}

QJsonObject releaseObject(
    const QJsonArray &assets,
    const QString &tag = QString::fromLatin1(kTag),
    const QString &pageUrl = QStringLiteral("https://github.com/subeom/deskflow/releases/tag/v2.0.6")
)
{
  return {
      {"draft", false},     {"prerelease", false}, {"tag_name", tag},
      {"html_url", pageUrl}, {"assets", assets},
  };
}

QByteArray releaseJson(const QJsonArray &assets, const QString &tag = QString::fromLatin1(kTag))
{
  return QJsonDocument(releaseObject(assets, tag)).toJson(QJsonDocument::Compact);
}

QJsonArray automaticAssets()
{
  return {
      asset(
          QStringLiteral("deskflow-2.0.6-macos-arm64.dmg"), QString::fromLatin1(kTag),
          QString::fromLatin1(kArmDigest),
          30'437'214
      ),
      asset(QStringLiteral("SHA256SUMS.txt"), QString::fromLatin1(kTag), QString::fromLatin1(kSumsDigest), 97)
  };
}

UpdateEnvironment directMac(const QString &architecture)
{
  return {QStringLiteral("macos"), architecture, false, true};
}

} // namespace

void UpdateReleaseTests::latestStableRelease_parsesForkArm64Dmg()
{
  const auto parsed = parseGitHubRelease(releaseJson(automaticAssets()));
  QVERIFY(parsed.release);
  QVERIFY(parsed.error.isEmpty());
  QCOMPARE(parsed.release->tag, QString::fromLatin1(kTag));
  QCOMPARE(parsed.release->version, QString::fromLatin1(kVersion));
  QCOMPARE(
      parsed.release->pageUrl,
      QUrl(QStringLiteral("https://github.com/subeom/deskflow/releases/tag/v2.0.6"))
  );
  QCOMPARE(parsed.release->assets.size(), 2);

  const auto plan = planUpdate(*parsed.release, directMac(QStringLiteral("arm64")));
  QVERIFY(plan);
  QCOMPARE(plan->mode, UpdateInstallMode::AutomaticMac);
  QVERIFY(plan->installer);
  QCOMPARE(plan->installer->name, QStringLiteral("deskflow-2.0.6-macos-arm64.dmg"));
  QVERIFY(plan->checksums);
  QCOMPARE(plan->checksums->name, QStringLiteral("SHA256SUMS.txt"));
}

void UpdateReleaseTests::latestStableRelease_rejectsDraftPrereleaseAndWrongRepository()
{
  for (const auto field : {QStringLiteral("draft"), QStringLiteral("prerelease")}) {
    auto object = releaseObject(automaticAssets());
    object.insert(field, true);
    QVERIFY(!parseGitHubRelease(QJsonDocument(object).toJson(QJsonDocument::Compact)).release);
  }

  auto wrongPage = releaseObject(
      automaticAssets(), QString::fromLatin1(kTag),
      QStringLiteral("https://github.com/deskflow/deskflow/releases/tag/v2.0.6")
  );
  QVERIFY(!parseGitHubRelease(QJsonDocument(wrongPage).toJson(QJsonDocument::Compact)).release);

  auto wrongDownload = automaticAssets();
  auto first = wrongDownload.takeAt(0).toObject();
  first.insert(
      QStringLiteral("browser_download_url"),
      QStringLiteral("https://github.com/deskflow/deskflow/releases/download/v2.0.6/"
                     "deskflow-2.0.6-macos-arm64.dmg")
  );
  wrongDownload.prepend(first);
  QVERIFY(!parseGitHubRelease(releaseJson(wrongDownload)).release);
}

void UpdateReleaseTests::latestStableRelease_rejectsDuplicateUnsafeOrMismatchedAssets()
{
  auto duplicate = automaticAssets();
  duplicate.append(duplicate.at(0));
  QVERIFY(!parseGitHubRelease(releaseJson(duplicate)).release);

  auto unsafe = automaticAssets();
  auto first = unsafe.at(0).toObject();
  first.insert(QStringLiteral("name"), QStringLiteral("../Deskflow.dmg"));
  unsafe.replace(0, first);
  QVERIFY(!parseGitHubRelease(releaseJson(unsafe)).release);

  auto mismatched = automaticAssets();
  first = mismatched.at(0).toObject();
  first.insert(
      QStringLiteral("browser_download_url"),
      QStringLiteral("https://github.com/subeom/deskflow/releases/download/v2.0.6/not-the-asset.dmg")
  );
  mismatched.replace(0, first);
  QVERIFY(!parseGitHubRelease(releaseJson(mismatched)).release);
}

void UpdateReleaseTests::latestStableRelease_rejectsMissingOrInvalidDigest()
{
  auto missing = automaticAssets();
  auto first = missing.at(0).toObject();
  first.remove(QStringLiteral("digest"));
  missing.replace(0, first);
  QVERIFY(!parseGitHubRelease(releaseJson(missing)).release);

  auto malformed = automaticAssets();
  first = malformed.at(0).toObject();
  first.insert(QStringLiteral("digest"), QStringLiteral("sha256:not-a-hash"));
  malformed.replace(0, first);
  QVERIFY(!parseGitHubRelease(releaseJson(malformed)).release);

  auto zeroSize = automaticAssets();
  first = zeroSize.at(0).toObject();
  first.insert(QStringLiteral("size"), 0);
  zeroSize.replace(0, first);
  QVERIFY(!parseGitHubRelease(releaseJson(zeroSize)).release);
}

void UpdateReleaseTests::updatePlan_selectsExactArchitectureAndNeverFallbacks()
{
  auto assets = automaticAssets();
  assets.prepend(
      asset(
          QStringLiteral("deskflow-2.0.6-macos-x86_64.dmg"), QString::fromLatin1(kTag),
          QString::fromLatin1(kX64Digest)
      )
  );
  const auto parsed = parseGitHubRelease(releaseJson(assets));
  QVERIFY(parsed.release);

  const auto arm = planUpdate(*parsed.release, directMac(QStringLiteral("aarch64")));
  QVERIFY(arm);
  QVERIFY(arm->installer);
  QCOMPARE(arm->installer->name, QStringLiteral("deskflow-2.0.6-macos-arm64.dmg"));

  const auto x64 = planUpdate(*parsed.release, directMac(QStringLiteral("amd64")));
  QVERIFY(x64);
  QVERIFY(x64->installer);
  QCOMPARE(x64->installer->name, QStringLiteral("deskflow-2.0.6-macos-x86_64.dmg"));

  const auto unknown = planUpdate(*parsed.release, directMac(QStringLiteral("riscv64")));
  QVERIFY(unknown);
  QCOMPARE(unknown->mode, UpdateInstallMode::Manual);
  QVERIFY(!unknown->installer);

  const auto armOnly = parseGitHubRelease(releaseJson(automaticAssets()));
  QVERIFY(armOnly.release);
  const auto missingArchitecture =
      planUpdate(*armOnly.release, directMac(QStringLiteral("x86_64")));
  QVERIFY(missingArchitecture);
  QCOMPARE(missingArchitecture->mode, UpdateInstallMode::Manual);
  QVERIFY(!missingArchitecture->installer);
}

void UpdateReleaseTests::updatePlan_keepsWindowsLinuxAndPackageManagedMacManual()
{
  const auto parsed = parseGitHubRelease(releaseJson(automaticAssets()));
  QVERIFY(parsed.release);

  const QList<UpdateEnvironment> manual{
      {QStringLiteral("windows"), QStringLiteral("x86_64"), false, false},
      {QStringLiteral("windows"), QStringLiteral("arm64"), false, false},
      {QStringLiteral("linux"), QStringLiteral("x86_64"), true, false},
      {QStringLiteral("macos"), QStringLiteral("arm64"), true, false},
  };
  for (const auto &environment : manual) {
    const auto plan = planUpdate(*parsed.release, environment);
    QVERIFY2(plan, qPrintable(environment.platform));
    QCOMPARE(plan->mode, UpdateInstallMode::Manual);
    QVERIFY(!plan->installer);
    QCOMPARE(plan->releaseUrl, parsed.release->pageUrl);
  }

  QVERIFY(!planUpdate(*parsed.release, {QStringLiteral("freebsd"), QStringLiteral("x86_64"), false, false}));
}

void UpdateReleaseTests::updatePlan_serviceUnknownAndUnverifiedCICandidatesStayManual()
{
  const auto parsed = parseGitHubRelease(releaseJson(automaticAssets()));
  QVERIFY(parsed.release);

  auto service = directMac(QStringLiteral("arm64"));
  service.serviceMode = true;
  const auto servicePlan = planUpdate(*parsed.release, service);
  QVERIFY(servicePlan);
  QCOMPARE(servicePlan->mode, UpdateInstallMode::Manual);
  QVERIFY(!servicePlan->installer);

  auto unknownCpu = directMac(QStringLiteral("riscv64"));
  const auto unknownCpuPlan = planUpdate(*parsed.release, unknownCpu);
  QVERIFY(unknownCpuPlan);
  QCOMPARE(unknownCpuPlan->mode, UpdateInstallMode::Manual);
  QVERIFY(!unknownCpuPlan->installer);

  auto unsupported = directMac(QStringLiteral("arm64"));
  unsupported.supported = false;
  QVERIFY(!planUpdate(*parsed.release, unsupported));

  auto ciOnly = automaticAssets();
  auto checksum = ciOnly.takeAt(1).toObject();
  checksum.insert(QStringLiteral("name"), QStringLiteral("sums.txt"));
  checksum.insert(
      QStringLiteral("browser_download_url"),
      QStringLiteral("https://github.com/subeom/deskflow/releases/download/v2.0.6/sums.txt")
  );
  ciOnly.append(checksum);
  const auto ciRelease = parseGitHubRelease(releaseJson(ciOnly));
  QVERIFY(ciRelease.release);
  const auto ciPlan = planUpdate(*ciRelease.release, directMac(QStringLiteral("arm64")));
  QVERIFY(ciPlan);
  QCOMPARE(ciPlan->mode, UpdateInstallMode::Manual);
  QVERIFY(!ciPlan->installer);
  QVERIFY(!ciPlan->checksums);
}

void UpdateReleaseTests::versionComparison_rejectsPrereleaseAndNeverDowngrades()
{
  QVERIFY(deskflow::gui::isNewerUpdateVersion(QStringLiteral("2.0.5"), QStringLiteral("2.0.6")));
  QVERIFY(deskflow::gui::isNewerUpdateVersion(QStringLiteral("2.0.5.2"), QStringLiteral("2.0.6")));
  QVERIFY(deskflow::gui::isNewerUpdateVersion(QStringLiteral("2.9.9"), QStringLiteral("3.0.0")));
  QVERIFY(!deskflow::gui::isNewerUpdateVersion(QStringLiteral("2.0.6"), QStringLiteral("2.0.6")));
  QVERIFY(!deskflow::gui::isNewerUpdateVersion(QStringLiteral("2.0.5.2"), QStringLiteral("2.0.5")));
  QVERIFY(!deskflow::gui::isNewerUpdateVersion(QStringLiteral("2.1.0"), QStringLiteral("2.0.99")));
  QVERIFY(!deskflow::gui::isNewerUpdateVersion(QStringLiteral("2.0.5"), QStringLiteral("2.0.6-rc1")));
  QVERIFY(!deskflow::gui::isNewerUpdateVersion(QStringLiteral("v2.0.5"), QStringLiteral("2.0.6")));
}

void UpdateReleaseTests::checksumManifest_acceptsOneExactGithubLine()
{
  const auto hash = QString::fromLatin1(kArmDigest).mid(7);
  const auto payload =
      QStringLiteral(
          "%1  deskflow-2.0.6-macos-arm64.dmg\n"
          "%2 *deskflow-2.0.6-macos-x86_64.dmg\n"
      )
          .arg(hash.toUpper(), QString::fromLatin1(kX64Digest).mid(7).toUpper())
          .toUtf8();
  const auto result = parseChecksumManifest(payload, QStringLiteral("deskflow-2.0.6-macos-arm64.dmg"));
  QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
  QVERIFY(result.checksum);
  QCOMPARE(*result.checksum, hash.toLower());

  auto crlf = payload;
  crlf.replace(QByteArrayLiteral("\n"), QByteArrayLiteral("\r\n"));
  const auto crlfResult =
      parseChecksumManifest(crlf, QStringLiteral("deskflow-2.0.6-macos-arm64.dmg"));
  QVERIFY2(crlfResult.error.isEmpty(), qPrintable(crlfResult.error));
  QVERIFY(crlfResult.checksum);
  QCOMPARE(*crlfResult.checksum, hash.toLower());
}

void UpdateReleaseTests::checksumManifest_rejectsDuplicateMissingPathLikeAndMalformedLines()
{
  const auto line =
      QStringLiteral("%1  deskflow-2.0.6-macos-arm64.dmg\n").arg(QString::fromLatin1(kArmDigest).mid(7));
  const QList<QByteArray> invalid{
      {},
      line.toUtf8() + line.toUtf8(),
      QStringLiteral("%1  ../deskflow-2.0.6-macos-arm64.dmg\n").arg(QString::fromLatin1(kArmDigest).mid(7)).toUtf8(),
      QStringLiteral("%1 deskflow-2.0.6-macos-arm64.dmg\n").arg(QString::fromLatin1(kArmDigest).mid(7)).toUtf8(),
      QStringLiteral("%1  subdir/deskflow-2.0.6-macos-arm64.dmg\n")
          .arg(QString::fromLatin1(kArmDigest).mid(7))
          .toUtf8(),
      line.toUtf8().chopped(1) + QByteArrayLiteral("\r"),
      QByteArrayLiteral("not-a-checksum\n"),
  };
  for (const auto &payload : invalid) {
    const auto result = parseChecksumManifest(payload, QStringLiteral("deskflow-2.0.6-macos-arm64.dmg"));
    QVERIFY2(!result.error.isEmpty(), payload.constData());
  }

  QByteArray tooManyLines;
  for (int i = 0; i < 257; ++i)
    tooManyLines += QStringLiteral("%1  release-%2.dmg\n").arg(QString::fromLatin1(kArmDigest).mid(7)).arg(i).toUtf8();
  QVERIFY(!parseChecksumManifest(tooManyLines, QStringLiteral("release-256.dmg")).error.isEmpty());
}

QTEST_MAIN(UpdateReleaseTests)
