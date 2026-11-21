/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The card of the option "turn off the wireless link of AirDrop and Handoff while sharing" on the Network page, and the
// banner that says that the mouse can stutter: what they say for each state of the link, what their buttons ask of the
// controller, that a computer that is not a Mac has neither, and that every text of them is Korean in Korean. The
// system is a fake: no program is run, no dialog is shown, and awdl0 is not touched.
class AwdlPageTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void card_isOnTheNetworkPageOfAMac_andIsNotMadeWhereThereIsNoAwdl();
  void switch_isOffByDefault_writesTheSetting_andIsLive();
  void switch_saysThatItIsNeededOnlyWhenTheMouseStutters();
  void stateText_saysEachStateOfTheLink_data();
  void stateText_saysEachStateOfTheLink();
  void stateText_saysTheNumbersOfTheLastBurst();
  void card_followsTheController();
  void card_buttonsAskTheController();
  void card_afterTheLinkWasTurnedOff_saysTheNumbersOfTheMeasurementThatFollows();
  void card_withoutAController_hasItsButtonsOff();
  void card_readsTheStateWhenThePageIsShown();
  void banner_isHiddenUntilTheHint_saysTheNumbers_andItsButtonAsks();
  void banner_whileTheDialogIsOpen_hasItsButtonOff();
  // the permission to change the link with no password
  void permissionRow_hasItsTextsAndButtons_andTheTextsSaySheDoesNotAskTwice();
  void permissionRow_followsTheController_data();
  void permissionRow_followsTheController();
  void allowButton_asksTheQuestionFirst_andTheDialogOpensOnlyOnYes();
  void theQuestion_saysWhatIsAdded_whatItAllows_whoElseCanDoIt_andHowToRemoveIt();
  void allowButton_withNoAnswerOffscreen_isNoAndOpensNothing();
  void removeButton_opensTheDialogAtOnce();
  void permissionRow_saysWhatCameOfTheLastThing();
  void permissionRow_isAbsentWhereThereIsNoAwdl();
  void korean_everyTextOfTheOptionIsTranslated();

private:
  QTemporaryDir m_dir;
};
