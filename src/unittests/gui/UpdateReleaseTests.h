/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class UpdateReleaseTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void latestStableRelease_parsesForkArm64Dmg();
  void latestStableRelease_rejectsDraftPrereleaseAndWrongRepository();
  void latestStableRelease_rejectsDuplicateUnsafeOrMismatchedAssets();
  void latestStableRelease_rejectsMissingOrInvalidDigest();
  void updatePlan_selectsExactArchitectureAndNeverFallbacks();
  void updatePlan_keepsWindowsLinuxAndPackageManagedMacManual();
  void updatePlan_serviceUnknownAndUnverifiedCICandidatesStayManual();
  void versionComparison_rejectsPrereleaseAndNeverDowngrades();
  void checksumManifest_acceptsOneExactGithubLine();
  void checksumManifest_rejectsDuplicateMissingPathLikeAndMalformedLines();
};
