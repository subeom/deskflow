/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The frame of the new look: the header in the place of the title bar and of the menu bar. Every window here is made in
// a directory of its own for the settings and for the temporary files, and the core is never started. The frame of the
// system is what the tests get from the offscreen platform, so the ones that need the frame of the new look ask for it
// by the variable that the person can use to ask (DESKFLOW_WINDOW_FRAME).
class WindowChromeTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void systemLook_hasNoHeader_andKeepsItsMenuBar();
  void offscreen_keepsTheFrameOfTheSystem_unlessAskedOtherwise();
  void clientSideFrame_putsTheHeaderAboveTheShell_andTheTitleFollowsTheWindow();
  void optOut_keepsTheFrameOfTheSystem_data();
  void optOut_keepsTheFrameOfTheSystem();

  void header_hasTheButtonsOfTheWindowWithTheirNames_andSaysWhatWasAskedFor();
  void header_ofAMac_hasNoButtonsOfItsOwn_andLeavesRoomForTheOnesOfTheSystem();
  void header_saysWhereItTakesTheMouse_andWhereTheWindowIsDragged();
  void header_followsTheLanguage();

  void headerMenu_isTheTableOfTheOldMenus_inItsOrder_withALineBetweenGroups();
  void headerMenu_hasAnActionOfTheWindowForEveryEntryOfTheTable();
  void everyKeyOfTheOldMenus_isKeptByTheWindowWithoutAMenuBar();
  void theKeyOfTheSidebar_worksWhenTheMenuBarIsHidden();
  void closeOfTheHeader_goesToTheTrayOrEndsTheProgram_asTheSettingsSay();

  void headerAndShell_keepTheMinimumWidth_withWideFonts_data();
  void headerAndShell_keepTheMinimumWidth_withWideFonts();
  void theButtonsOfTheHeader_areNamedForTheScreenReader_andAreButtons();

private:
  QTemporaryDir m_dir;
};
