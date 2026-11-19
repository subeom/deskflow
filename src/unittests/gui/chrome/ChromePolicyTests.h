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
  void reasonName_isAWordForEveryReason();
  void current_hasTheLookAndThePlatformThatItIsAskedAbout();
};
