/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class UpdateReleaseTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void latestStableRelease_parsesAndSelectsMacInstaller();
  void latestStableRelease_rejectsDraftOrPrerelease();
  void updatePlan_keepsWindowsAndPackageManagedMacManual();
  void updatePlan_rejectsMissingOrInvalidDigest();
  void versionComparison_neverDowngradesAndOrdersStages();
};
