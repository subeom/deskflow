/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class UpdateCacheTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void stage_publishesAtomicallyAndReloadReverifies();
  void stage_rejectsCorruptOrUnexpectedPayload();
  void find_rejectsWrongPlanManifestDigestSizeVersionAndRepository();
  void load_rejectsSymlinkRootStageManifestInstallerAndChecksum();
  void load_rejectsEscapedPathsHardlinksAndUnsafeModes();
  void discard_removesOnlyAValidatedStageChild();
  void state_roundTripsEtagSnapshotAndRateLimitWithoutRawBody();
  void cleanup_removesExpiredInvalidIncomingAndOlderStages();
};
