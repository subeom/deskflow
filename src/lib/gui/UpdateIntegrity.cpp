/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateIntegrity.h"

#include <QCryptographicHash>

namespace deskflow::gui {

UpdateIntegrityResult verifyUpdatePayload(
    const UpdateAsset &asset, const QByteArray &payload, const std::optional<QByteArray> &checksums
)
{
  if (asset.size != payload.size()) {
    return {false, QStringLiteral("update size does not match release metadata")};
  }
  if (!isValidSha256Digest(asset.digest)) {
    return {false, QStringLiteral("release asset has no valid SHA-256 digest")};
  }

  const auto actual =
      QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
  if (actual != asset.digest.mid(7).toLower()) {
    return {false, QStringLiteral("update SHA-256 digest mismatch")};
  }

  if (checksums) {
    const auto parsed = parseChecksumManifest(*checksums, asset.name);
    if (!parsed.checksum || *parsed.checksum != actual) {
      return {
          false,
          parsed.error.isEmpty() ? QStringLiteral("update checksum manifest mismatch") : parsed.error
      };
    }
  }

  return {true, {}};
}

} // namespace deskflow::gui
