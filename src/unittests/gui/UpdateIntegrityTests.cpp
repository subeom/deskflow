/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateIntegrityTests.h"

#include "gui/UpdateIntegrity.h"
#include "gui/UpdateRelease.h"

#include <QCryptographicHash>
#include <QTest>

#include <optional>

using namespace deskflow::gui;

namespace {

UpdateAsset assetFor(const QByteArray &data)
{
  const auto hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
  return {
      QStringLiteral("deskflow-1.28.0-macos-arm64.dmg"),
      QStringLiteral("https://github.com/deskflow/deskflow/releases/download/v1.28.0/deskflow.dmg"),
      QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash)), data.size()
  };
}

QByteArray sumsFor(const UpdateAsset &asset)
{
  return asset.digest.mid(7).toUtf8() + QByteArrayLiteral("  ") + asset.name.toUtf8() + QByteArrayLiteral("\n");
}

} // namespace

void UpdateIntegrityTests::payload_acceptsMatchingDigestSizeAndChecksum()
{
  const QByteArray payload("verified installer");
  const auto asset = assetFor(payload);
  const auto result = verifyUpdatePayload(asset, payload, sumsFor(asset));
  QVERIFY(result.ok);
  QVERIFY(result.error.isEmpty());
}

void UpdateIntegrityTests::payload_rejectsDigestSizeAndChecksumMismatches()
{
  const QByteArray payload("verified installer");
  auto asset = assetFor(payload);

  asset.size++;
  QVERIFY(!verifyUpdatePayload(asset, payload, sumsFor(asset)).ok);
  asset.size--;

  const auto digest = asset.digest;
  asset.digest = QStringLiteral("sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  QVERIFY(!verifyUpdatePayload(asset, payload, sumsFor(asset)).ok);
  asset.digest = digest;

  const QByteArray wrongSums = QByteArray("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb  ") +
                               asset.name.toUtf8() + QByteArray("\n");
  QVERIFY(!verifyUpdatePayload(asset, payload, wrongSums).ok);
}

void UpdateIntegrityTests::checksum_requiresExactAssetName()
{
  const QByteArray payload("verified installer");
  const auto asset = assetFor(payload);
  const auto otherName = sumsFor(asset).replace(asset.name.toUtf8(), "other.dmg");
  QVERIFY(!verifyUpdatePayload(asset, payload, otherName).ok);
  QVERIFY(verifyUpdatePayload(asset, payload, std::nullopt).ok);
}

QTEST_MAIN(UpdateIntegrityTests)
