/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateIntegrity.h"

#include <QCryptographicHash>
#include <QRegularExpression>

namespace deskflow::gui {

namespace {

std::optional<QString> checksumForAsset(const QByteArray &sums, const QString &assetName)
{
  static const QRegularExpression checksumLine(QStringLiteral(R"(^([0-9a-fA-F]{64})\s+\*?(.+)$)"));
  const auto lines = sums.split('\n');
  for (const auto &line : lines) {
    const auto match = checksumLine.match(QString::fromUtf8(line.trimmed()));
    if (match.hasMatch() && match.captured(2) == assetName) {
      return match.captured(1).toLower();
    }
  }
  return std::nullopt;
}

} // namespace

UpdateIntegrityResult
verifyUpdatePayload(const UpdateAsset &asset, const QByteArray &payload, const std::optional<QByteArray> &sums)
{
  if (asset.size != payload.size()) {
    return {false, QStringLiteral("update size does not match release metadata")};
  }
  if (!isValidSha256Digest(asset.digest)) {
    return {false, QStringLiteral("release asset has no valid SHA-256 digest")};
  }

  const auto expected = asset.digest.mid(7).toLower();
  const auto actual = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
  if (actual != expected) {
    return {false, QStringLiteral("update SHA-256 digest mismatch")};
  }

  if (sums.has_value()) {
    const auto officialChecksum = checksumForAsset(*sums, asset.name);
    if (!officialChecksum || *officialChecksum != actual) {
      return {false, QStringLiteral("update checksum is missing from or mismatches sums.txt")};
    }
  }

  return {true, {}};
}

} // namespace deskflow::gui
