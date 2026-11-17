/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The content of the menu of the tray icon: no window, no tray; what the menu is made of for each state of the program
class TrayMenuModelTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void stopped_offersStart_theWindowAndQuit();
  void running_offersRestartAndStop_thenTheFacts();
  void error_ofAClient_offersTestConnectionAndStop();
  void error_ofAServer_offersStopWhileItRuns_andStartWhenItCouldNotStart();
  void noMode_hasNothingToStart_dimmedOrLeftOutByTheOs();
  void minimize_isOnlyOfferedWhileTheWindowIsOnTheScreen();
  void start_isDimmedWhenTheSettingsCannotStartTheCore();
  void computers_areRowsOfTextThatCannotBeChosen();
  void update_isARowThatOpensTheDownload_aboveTheComputers();
  void header_saysTheStateInWordsAndInAChip();
  void toolTip_saysTheStateInWords();
  void externalRows_areUnderTheHeader_andGoWhenTheirTextIsEmpty();
  void rows_areOrderedInGroups_withNoSeparatorBesideAnother_data();
  void rows_areOrderedInGroups_withNoSeparatorBesideAnother();
  void icons_areAllOrNoneInAGroup();
  void longSentences_areCutShortAndKeepTheWholeForTheToolTip();
  void themeIcons_haveANameInTheThemeOfTheDesktop();
  void sharingMenu_hasEveryRow_andDimsTheOnesThatDoNotApply();
};
