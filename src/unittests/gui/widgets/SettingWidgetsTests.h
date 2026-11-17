/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class SettingWidgetsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void inlineHint_isSmallerSecondaryTextThatWrapsAndFollowsTheLook();

  void statusChip_hasAnIconAndAWordForEveryKindButNeutral();
  void statusChip_textKeepsItsContrastOnItsFill_data();
  void statusChip_textKeepsItsContrastOnItsFill();
  void statusChip_isAsWideAsItsIconAndText();
  void statusChip_isPaintedWithTheFillOfItsKind();

  void sectionHeader_showsATitleAndOwnsTheWidgetAtItsEnd();

  void row_takesTheTextOfASwitchAsItsTitle();
  void row_showsTheMnemonicOfItsTitleAndReadsItWithoutIt();
  void row_leavesItsControlsAsTheyWere();
  void row_isShownWhileOneOfItsControlsIsShown();
  void row_dimsItsTextWhenNoneOfItsControlsIsEnabled();
  void row_followsTheRowThatIsDisabled();
  void row_aClickOnItsTextTogglesItsSwitch();
  void row_aClickOnItsTextDoesNothingForOtherControls();
  void row_isAtLeast44PixelsHigh_andGrowsForAHintThatWraps();
  void row_withExpandingControls_givesThemTheRoom();
  void row_mnemonicOfTheTitleFocusesTheControl();

  void card_hasNoHairlineAboveItsFirstRowThatIsShown();
  void card_isHiddenWhenNoneOfItsRowsIsShown();
  void card_hasAHeaderWithAWidgetAtItsEnd();
  void card_isTheDensePanelWithRowsToItsEdge();
  void card_paintsTheHairlineBetweenRowsInTheTokenColour();

  void hero_showsTheStateTheDetailAndTheChips();
  void hero_badgeHasTheColourOfItsKind();
  void hero_announcesAHeadlineThatChanges();
  void hero_putsItsActionsAtTheEndAndUnderTheTextWhenItIsNarrow();
  void hero_holdsAnAccessoryUnderTheChips_evenWhenChipsComeAfterIt();
  void card_clearRowsDeletesTheRowsAndLeavesTheCardUsable();
  void card_hiddenByItsRowsIsShownAgainWhenTheRowsAreCleared();

  void widgets_followTheLookWhenItChanges();
};
