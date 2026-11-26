/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class UpdateIntegrityTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void payload_acceptsExactSizeDigestAndChecksum();
  void payload_rejectsOneByteSizeDigestAndChecksumMutations();
  void checksumAsset_matchesParsedManifestExactly();
};
