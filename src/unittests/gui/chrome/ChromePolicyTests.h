/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class ChromePolicyTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void decideFrame_followsTheRulesInTheOrderOfTheirSafety_data();
  void decideFrame_followsTheRulesInTheOrderOfTheirSafety();
  void decideFrame_readsTheVariableWithoutCareForCaseOrSpaces();
  void linux_hasTheFrameOfTheSystemInTheFirstRelease();
  void prefersDeskflowFrame_isWhatThePersonChoseOrWhatThePlatformHasByDefault_data();
  void prefersDeskflowFrame_isWhatThePersonChoseOrWhatThePlatformHasByDefault();
  void frameSwitch_whatThePersonChoseIsKeptAtTheNextStartAndChoosesTheFrame_data();
  void frameSwitch_whatThePersonChoseIsKeptAtTheNextStartAndChoosesTheFrame();
  void reasonName_isAWordForEveryReason();
  void current_hasTheLookAndThePlatformThatItIsAskedAbout();

  void headerMoveMethod_isTheProgramsOnAMac_andTheSystemsElsewhere_data();
  void headerMoveMethod_isTheProgramsOnAMac_andTheSystemsElsewhere();
  void movedWindowPosition_followsTheMouse_andKeepsTheHeaderBelowTheTopOfTheScreen();
};
