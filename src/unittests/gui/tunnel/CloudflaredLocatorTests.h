/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class CloudflaredLocatorTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void knownLocations_areTheOnesOfTheInstallersOfEachSystem();
  void fileName_hasTheExtensionOfWindowsOnly();
  void locate_prefersTheConfiguredPathThenTheKnownPlacesThenPath();
  void locate_aConfiguredPathThatCannotBeRunFallsBackAndSaysSo();
  void locate_searchesTheDirectoriesOfPathInOrderAndNeverARelativeOne_data();
  void locate_searchesTheDirectoriesOfPathInOrderAndNeverARelativeOne();
  void locate_saysNotFoundWhenThereIsNothing();
  void locate_withTheSystemCheckDoesNotRunWhatIsNotAnExecutableFile();
  void installHint_namesTheInstallerOfWindowsAndMac();
};
