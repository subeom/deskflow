/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The main window of the new look and of the system. Every window here is made in a directory of its own for the
// settings and for the temporary files (the single-instance check of the window opens a local socket there): none of
// them reaches the settings or the socket of a program that is running. The core is never started.
class MainShellTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void systemLook_hasNoShell_andKeepsItsFixedSizeAndItsStatusBar();
  void deskflowLook_hasTheShell_hidesTheStatusBar_andCanBeResizedFrom720();
  void deskflowLook_theModeIsOneChoiceInTheControlAndInTheRadioButtons();
  void deskflowLook_theHeroSaysTheStateOfTheCoreAndTheStatusBar();
  void deskflowLook_theClientConnectionKeepsItsControlsWorking_andHasPlacesForMore();
  void deskflowLook_theComputersAreThoseOfTheConfig_andTheConnectedOnesSayConnected();
  void deskflowLook_theSidebarIsRemembered_andANarrowWindowDoesNotChangeIt();
  void deskflowLook_theLogIsFoldedUntilTheUserOrAnErrorUnfoldsIt();
  void deskflowLook_theNoticeOfTheLayoutMemoryStaysAboveThePage();
  void deskflowLook_theWindowHasTheNamesAndTheRolesOfTheShell();
  void deskflowLook_aChangeOfLanguageKeepsTheShell();
  void deskflowLook_theSettingsArePagesOfTheSidebar_andPreferencesOpensTheFirst();
  void deskflowLook_aChangeThatTheCoreReadsAsksForARestart_andOnlyWhileItRuns();
  void deskflowLook_theViewMenuFoldsTheSidebar_withAShortcutThatNothingElseHas();
  void deskflowLook_fitsAtEveryWidth_withLargerTextAndLongKoreanStrings_data();
  void deskflowLook_fitsAtEveryWidth_withLargerTextAndLongKoreanStrings();

private:
  QTemporaryDir m_dir;
};
