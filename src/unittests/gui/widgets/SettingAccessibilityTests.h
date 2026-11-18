/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class SettingAccessibilityTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void switchRole_isASwitchFromQt611_andACheckBoxBefore();
  void toggleSwitch_isASwitchWithItsStateAndItsActions();
  void toggleSwitch_withATextIsNamedByIt();
  void toggleSwitch_whenDisabled_saysSoAndIgnoresTheAction();
  void toggleSwitch_withoutTheStyleLayer_isACheckBoxForAScreenReader();
  void row_isAGroupNamedByItsTitle_andNamesItsControls();
  void row_leavesTheNameOfAControlThatHasOne();
  void card_isAGroupNamedByItsHeader_andTheHeaderIsAHeading();
  void chip_isTextThatNamesItsKind();
  void hero_isAGroupNamedByItsHeadlineAndDescribedByItsDetail();
  void card_tree_hasTheRolesAndNamesOfWhatIsOnTheScreen();
};
