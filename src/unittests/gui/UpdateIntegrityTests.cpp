/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateIntegrityTests.h"

#include "gui/UpdateIntegrity.h"

#include <QCryptographicHash>

namespace {

QString digest(const QByteArray &data)
{
  return QStringLiteral("sha256:%1").arg(
      QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex())
  );
}

deskflow::gui::UpdateAsset asset(const QString &name, const QByteArray &payload)
{
  return {
      name,
      QUrl(QStringLiteral("https://github.com/subeom/deskflow/releases/download/v2.0.6/%1").arg(name)),
      digest(payload),
      payload.size()
  };
}

QByteArray checksums(const QString &name, const QByteArray &payload)
{
  return QStringLiteral("%1  %2\n").arg(digest(payload).mid(7), name).toUtf8();
}

} // namespace

void UpdateIntegrityTests::payload_acceptsExactSizeDigestAndChecksum()
{
  const QByteArray payload{"verified installer bytes"};
  const auto installer = asset(QStringLiteral("deskflow-2.0.6-macos-arm64.dmg"), payload);
  const auto sums = checksums(installer.name, payload);
  const auto result = deskflow::gui::verifyUpdatePayload(installer, payload, sums);
  QVERIFY(result.ok);
  QVERIFY(result.error.isEmpty());
}

void UpdateIntegrityTests::payload_rejectsOneByteSizeDigestAndChecksumMutations()
{
  const QByteArray payload{"verified installer bytes"};
  auto installer = asset(QStringLiteral("deskflow-2.0.6-macos-arm64.dmg"), payload);
  const auto sums = checksums(installer.name, payload);

  auto wrongSize = installer;
  wrongSize.size++;
  QVERIFY(!deskflow::gui::verifyUpdatePayload(wrongSize, payload, sums).ok);

  auto wrongDigest = installer;
  wrongDigest.digest =
      QStringLiteral("sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  QVERIFY(!deskflow::gui::verifyUpdatePayload(wrongDigest, payload, sums).ok);

  auto changed = payload;
  changed[0] = 'x';
  QVERIFY(!deskflow::gui::verifyUpdatePayload(installer, changed, sums).ok);

  QByteArray changedSums = sums;
  changedSums[0] = changedSums.at(0) == 'a' ? 'b' : 'a';
  QVERIFY(!deskflow::gui::verifyUpdatePayload(installer, payload, changedSums).ok);
}

void UpdateIntegrityTests::checksumAsset_matchesParsedManifestExactly()
{
  const QByteArray payload{"installer"};
  const auto installer = asset(QStringLiteral("deskflow-2.0.6-macos-arm64.dmg"), payload);
  const auto other = checksums(QStringLiteral("deskflow-2.0.6-macos-x86_64.dmg"), payload);
  QVERIFY(!deskflow::gui::verifyUpdatePayload(installer, payload, other).ok);
}

QTEST_MAIN(UpdateIntegrityTests)
