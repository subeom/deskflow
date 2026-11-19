/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class ShellWidgetsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void rail_hasTwoWidths_andTheIconsStayWhereTheyAreWhenItFolds();
  void rail_foldsAtOnceWhenItIsNotShownOrMotionIsReduced_andMovesOtherwise();
  void rail_theFirstPageIsShown_aPressSelectsAPage_andAnActionDoesNot();
  void rail_theLabelIsTheNameAndTheTooltipOfTheFoldedRail();
  void rail_isOneStopForTheTabKey_andTheArrowKeysMoveInIt();
  void rail_aHiddenEntryTakesNoRoomAndNoFocus_aStatusLineHasNoFocus();
  void rail_paintsThePageThatIsShownInTheAccent_andTheIconsWhenFolded();
  void rail_theNameOfTheListFollowsTheLanguage();
  void rail_tellsAScreenReaderTheListTheEntriesAndTheStateOfTheButton();

  void segmented_choosesOne_andNeverUnchoosesIt();
  void segmented_isOneStopAndTheArrowKeysChoose();
  void segmented_tellsAScreenReaderTheChoices();

  void shell_unfoldedFoldedAndTheContentFollows();
  void shell_aNarrowWindowFoldsTheRailWithoutChangingTheChoice();
  void shell_theOverlayUnfoldsOverTheContent_andFoldsAgain();
  void shell_asksForTheRoomOfTheRailThatIsFolded();
  void shell_theOverlayIsAsOpaqueAsAPanelOfText();
  void rail_thatIsSolidHidesWhatIsBehindIt();
};
