/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QTemporaryDir>

class ServerConfigAdvancedTabTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void systemMode_keepsTheGroupBoxesAndTheCheckBoxesAsTheyWere();
  void deskflowMode_hasACardForEachGroupOnAScrollingBackdrop();
  void deskflowMode_movesEveryControlIntoARow_andHidesTheGroupBoxes();
  void deskflowMode_describesWhatHasAPlainTooltip_andKeepsTheHelpOfShiftF1();
  void deskflowMode_namesTheRowsByTheirControls();
  void deskflowMode_clipboardLimitHasTheLabelAsItsTitle();
  void deskflowMode_experimentalCardHasAChipAndTheTitleWithoutItsPrefix();
  void deskflowMode_switchesDriveTheDialogAsBefore();
  void deskflowMode_everySwitch_followsAClickAndTheKeySpace();
  void deskflowMode_settingsThatAreLoaded_showWithoutMovingTheKnob();
  void deskflowMode_aSettingOfAnotherOs_hasNoRow();
  void deskflowMode_tabKeyWalksTheControlsInTheOrderOfTheScreen();
  void deskflowMode_externalConfig_isBoundBothWaysAndFollowsTheGroupBox();

private:
  QTemporaryDir m_settingsDir;
};
