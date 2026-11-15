/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateReleaseTests.h"

#include "gui/UpdateRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

namespace {

QByteArray releaseJson(bool draft, bool prerelease, const QByteArray &digest = QByteArray("use-valid-digest"))
{
  const auto installerDigest =
      digest == QByteArray("use-valid-digest")
          ? QByteArray("sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
          : digest;
  const QJsonArray assets = {
      QJsonObject{
          {"name", "deskflow-1.28.0-macos-arm64.dmg"},
          {"browser_download_url", "https://github.com/deskflow/deskflow/releases/download/v1.28.0/"
                                   "deskflow-1.28.0-macos-arm64.dmg"},
          {"size", 120},
          {"digest", QString::fromLatin1(installerDigest)},
      },
      QJsonObject{
          {"name", "deskflow-1.28.0-macos-x86_64.dmg"},
          {"browser_download_url", "https://github.com/deskflow/deskflow/releases/download/v1.28.0/"
                                   "deskflow-1.28.0-macos-x86_64.dmg"},
          {"size", 121},
          {"digest", "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
      },
      QJsonObject{
          {"name", "deskflow-1.28.0-win-x64.msi"},
          {"browser_download_url", "https://github.com/deskflow/deskflow/releases/download/v1.28.0/"
                                   "deskflow-1.28.0-win-x64.msi"},
          {"size", 122},
          {"digest", "sha256:cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"},
      },
      QJsonObject{
          {"name", "sums.txt"},
          {"browser_download_url", "https://github.com/deskflow/deskflow/releases/download/v1.28.0/sums.txt"},
          {"size", 123},
          {"digest", "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"},
      },
  };
  const QJsonObject release{
      {"tag_name", "v1.28.0"},    {"draft", draft},
      {"prerelease", prerelease}, {"html_url", "https://github.com/deskflow/deskflow/releases/tag/v1.28.0"},
      {"assets", assets},
  };
  return QJsonDocument(release).toJson(QJsonDocument::Compact);
}

QByteArray releaseJsonWithAssetSize(const QString &name, qint64 size)
{
  auto document = QJsonDocument::fromJson(releaseJson(false, false));
  auto root = document.object();
  auto assets = root.value(QStringLiteral("assets")).toArray();
  for (auto index = 0; index < assets.size(); ++index) {
    auto asset = assets.at(index).toObject();
    if (asset.value(QStringLiteral("name")).toString() == name) {
      asset.insert(QStringLiteral("size"), size);
      assets.replace(index, asset);
    }
  }
  root.insert(QStringLiteral("assets"), assets);
  document.setObject(root);
  return document.toJson(QJsonDocument::Compact);
}

} // namespace

void UpdateReleaseTests::latestStableRelease_parsesAndSelectsMacInstaller()
{
  const auto release = deskflow::gui::parseGitHubRelease(releaseJson(false, false));
  QVERIFY(release.release.has_value());
  QCOMPARE(release.release->version, QStringLiteral("1.28.0"));

  const auto plan =
      deskflow::gui::planUpdate(*release.release, QStringLiteral("macos"), QStringLiteral("arm64"), false);
  QVERIFY(plan.has_value());
  QCOMPARE(plan->mode, deskflow::gui::UpdateInstallMode::Automatic);
  QVERIFY(plan->installer.has_value());
  QCOMPARE(plan->installer->name, QStringLiteral("deskflow-1.28.0-macos-arm64.dmg"));
  QVERIFY(plan->checksums.has_value());
  QCOMPARE(plan->checksums->name, QStringLiteral("sums.txt"));
}

void UpdateReleaseTests::latestStableRelease_rejectsDraftOrPrerelease()
{
  QVERIFY(!deskflow::gui::parseGitHubRelease(releaseJson(true, false)).release.has_value());
  QVERIFY(!deskflow::gui::parseGitHubRelease(releaseJson(false, true)).release.has_value());
}

void UpdateReleaseTests::latestStableRelease_rejectsMismatchedOfficialUrls()
{
  const auto releasePageSuffix = releaseJson(false, false)
                                     .replace(
                                         "https://github.com/deskflow/deskflow/releases/tag/v1.28.0",
                                         "https://github.com/deskflow/deskflow/releases/tag/v1.28.0?download=1"
                                     );
  QVERIFY(!deskflow::gui::parseGitHubRelease(releasePageSuffix).release.has_value());

  const auto assetNameMismatch =
      releaseJson(false, false)
          .replace("deskflow-1.28.0-macos-arm64.dmg\",", "deskflow-1.28.0-macos-arm64.dmg?unexpected=true\",");
  QVERIFY(!deskflow::gui::parseGitHubRelease(assetNameMismatch).release.has_value());
}

void UpdateReleaseTests::updatePlan_keepsWindowsAndPackageManagedMacManual()
{
  const auto release = deskflow::gui::parseGitHubRelease(releaseJson(false, false));
  QVERIFY(release.release.has_value());

  const auto windows =
      deskflow::gui::planUpdate(*release.release, QStringLiteral("windows"), QStringLiteral("x86_64"), false);
  QVERIFY(windows.has_value());
  QCOMPARE(windows->mode, deskflow::gui::UpdateInstallMode::Manual);
  QVERIFY(windows->installer.has_value());
  QCOMPARE(windows->installer->name, QStringLiteral("deskflow-1.28.0-win-x64.msi"));

  const auto homebrew =
      deskflow::gui::planUpdate(*release.release, QStringLiteral("macos"), QStringLiteral("arm64"), true);
  QVERIFY(homebrew.has_value());
  QCOMPARE(homebrew->mode, deskflow::gui::UpdateInstallMode::Manual);
}

void UpdateReleaseTests::updatePlan_rejectsMissingOrInvalidDigest()
{
  for (const auto &digest : {QByteArray(), QByteArray("sha256:short"), QByteArray("sha512:deadbeef")}) {
    const auto release = deskflow::gui::parseGitHubRelease(releaseJson(false, false, digest));
    QVERIFY(release.release.has_value());
    const auto plan =
        deskflow::gui::planUpdate(*release.release, QStringLiteral("macos"), QStringLiteral("arm64"), false);
    QVERIFY(!plan.has_value());
  }
}

void UpdateReleaseTests::updatePlan_rejectsOversizedAutomaticDownloads()
{
  const auto installer = QStringLiteral("deskflow-1.28.0-macos-arm64.dmg");
  const auto release = deskflow::gui::parseGitHubRelease(releaseJsonWithAssetSize(installer, 1024LL * 1024 * 1024));
  QVERIFY(release.release.has_value());
  QVERIFY(
      !deskflow::gui::planUpdate(*release.release, QStringLiteral("macos"), QStringLiteral("arm64"), false).has_value()
  );

  const auto checksums =
      deskflow::gui::parseGitHubRelease(releaseJsonWithAssetSize(QStringLiteral("sums.txt"), 4 * 1024 * 1024));
  QVERIFY(checksums.release.has_value());
  QVERIFY(!deskflow::gui::planUpdate(*checksums.release, QStringLiteral("macos"), QStringLiteral("arm64"), false)
               .has_value());
}

void UpdateReleaseTests::versionComparison_neverDowngradesAndOrdersStages()
{
  QVERIFY(deskflow::gui::isNewerUpdateVersion(QStringLiteral("1.27.0.99"), QStringLiteral("1.27.0")) == false);
  QVERIFY(deskflow::gui::isNewerUpdateVersion(QStringLiteral("1.26.9"), QStringLiteral("1.27.0")));
  QVERIFY(deskflow::gui::isNewerUpdateVersion(QStringLiteral("1.27.0-beta"), QStringLiteral("1.27.0")));
  QVERIFY(deskflow::gui::isNewerUpdateVersion(QStringLiteral("1.27.0-rc2"), QStringLiteral("1.27.0-rc10")));
  QVERIFY(!deskflow::gui::isNewerUpdateVersion(QStringLiteral("1.28.0"), QStringLiteral("1.27.0")));
}

QTEST_MAIN(UpdateReleaseTests)
