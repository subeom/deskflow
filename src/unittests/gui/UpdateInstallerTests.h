/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class UpdateInstallerTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void launch_requiresExplicitConfirmation();
  void launch_refusesPackageManagedHomebrewApp();
  void launch_startsDetachedHelperWithManifestAndParentPid();
  void apply_stagesVerifiesAndAtomicallyReplaces();
  void apply_rollsBackAndRelaunchesOldAppWhenNewLaunchFails();
  void apply_discardsStageWhenReverificationFails();
};
