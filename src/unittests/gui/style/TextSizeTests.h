/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The service that gives the new look its size of text: it changes the font of the application and the scale of the
// controls, follows the setting, and does nothing with the look of the system. The system and the OS are made up here.
class TextSizeTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void withoutTheService_theControlsAreAsTheyWereDrawn();
  void windows_raisesASmallSystemFont_andTheControlsStayAsDrawn();
  void mac_keepsTheFontOfTheSystemAsItIs_andTellsNoWidget();
  void thePercentIsWritten_theFontAndTheControlsFollow_andTheWidgetsAreTold();
  void aChangeOfTheSettingFromElsewhere_isFollowed();
  void aStoredPercentThatIsNoStep_isTheDefault();
  void steps_goOneAtATime_andStopAtTheEnds_andResetGoesBackToTheDefault();
  void refresh_readsTheSystemAgain();
  void theTextSizeOfTheOs_isMultipliedIntoTheAutomaticSize();
  void aFontInPoints_keepsItsPointsAndItsFamily();
  void aWidgetThatSetsItsOwnFont_isToldOfTheNewSize_throughTheLookFollower();
  void onTextSizeChanged_endsWithItsContext();
  void fontRatio_isHowManyTimesTheFontOfTheSystemTheTextIs();
  void describe_saysTheSizeAndWhereItComesFrom();
  void fontPixels_isTheAskedSize_whereTheFontDatabaseCannotAnswer();
  void uninstall_putsTheFontOfTheSystemBack();
  void applyAppearance_hasTheServiceForTheNewLook_andNotForTheSystem();

private:
  QTemporaryDir m_dir;
};
