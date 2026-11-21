/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class UpdateInstallerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void launch_requiresExplicitConfirmationAndHelperReady();
  void launch_refusesPackageManagedSymlinkAndWrongExecutable();
  void launch_bindsManifestReceiptPidAndProcessStartIdentity();
  void apply_waitsForParentExitAndShutdownReceipt();
  void apply_reverifiesCopiesAndAtomicallySwaps();
  void apply_removesQuarantineOnlyAfterCandidateVerification();
  void apply_rollsBackForRenameLaunchAndHandshakeFailure();
  void apply_rollsBackWhenSuccessReceiptCannotBePublished();
  void apply_keepsOldBackupWhenRollbackRelaunchFails();
  void apply_rejectsMismatchedPidAndReceipt();
};
