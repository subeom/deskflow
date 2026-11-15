/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateCacheTests.h"

#include "gui/UpdateCache.h"
#include "gui/UpdateRelease.h"

#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QTest>

using namespace deskflow::gui;

namespace {

UpdatePlan planFor(const QByteArray &payload)
{
  const auto hash = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
  UpdatePlan plan;
  plan.releaseVersion = QStringLiteral("1.28.0");
  plan.releaseUrl = QStringLiteral("https://github.com/deskflow/deskflow/releases/tag/v1.28.0");
  plan.installer = UpdateAsset{
      QStringLiteral("deskflow-1.28.0-macos-arm64.dmg"),
      QStringLiteral("https://github.com/deskflow/deskflow/releases/download/v1.28.0/deskflow.dmg"),
      QStringLiteral("sha256:%1").arg(hash), payload.size()
  };
  plan.mode = UpdateInstallMode::Automatic;
  return plan;
}

QByteArray sumsFor(const UpdatePlan &plan)
{
  return plan.installer->digest.mid(7).toUtf8() + QByteArrayLiteral("  ") + plan.installer->name.toUtf8() +
         QByteArrayLiteral("\n");
}

} // namespace

void UpdateCacheTests::stage_publishesVerifiedArtifactAndManifest()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QByteArray payload("verified installer");
  const auto plan = planFor(payload);
  UpdateCache cache(directory.path());

  const auto result = cache.stage(plan, payload, sumsFor(plan));
  QVERIFY(result.ok);
  QVERIFY(QFileInfo::exists(result.staged.installerPath));
  QVERIFY(QFileInfo::exists(result.staged.manifestPath));
  QCOMPARE(result.staged.version, QStringLiteral("1.28.0"));
  QCOMPARE(result.staged.installerName, plan.installer->name);
  QVERIFY(cache.find(plan).has_value());
}

void UpdateCacheTests::stage_rejectsInvalidPayloadWithoutPublishing()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QByteArray payload("verified installer");
  auto plan = planFor(payload);
  UpdateCache cache(directory.path());

  plan.installer->size++;
  const auto result = cache.stage(plan, payload, sumsFor(plan));
  QVERIFY(!result.ok);
  QVERIFY(!cache.find(plan).has_value());
}

void UpdateCacheTests::stage_deduplicatesExistingVerifiedArtifact()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QByteArray payload("verified installer");
  const auto plan = planFor(payload);
  UpdateCache cache(directory.path());

  const auto first = cache.stage(plan, payload, sumsFor(plan));
  QVERIFY(first.ok);
  const auto firstWrite = QFileInfo(first.staged.installerPath).lastModified();
  const auto second = cache.stage(plan, payload, sumsFor(plan));
  QVERIFY(second.ok);
  QCOMPARE(second.staged.installerPath, first.staged.installerPath);
  QCOMPARE(QFileInfo(second.staged.installerPath).lastModified(), firstWrite);
}

QTEST_MAIN(UpdateCacheTests)
