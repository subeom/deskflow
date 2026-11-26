/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The settings pages of the main window: the controls are tied to the settings, a change is written at once, and what
// is written is what the dialog of the settings writes for the same change. All with a settings file of the test.
class SettingsPagesTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void effects_everyBoundSettingSaysWhatItNeeds();
  void pages_updateCard_keepsStartupChoiceAndOffersOneManualCheck();
  void pages_theWindowFrameIsChosenWithOneSwitch_whichSaysTheFrameOfTheNewLook();
  void pages_theAppearanceCard_isTheWayBackToTheLookOfTheSystem_andTheWayForward();
  void effects_theSettingsThatTheCoreReadsNeedARestart();
  void pages_inputIndicatorDefaultsAndDependencies();

  void pages_areInTheOrderOfTheSidebar_withTitlesAndCards();
  void pages_changeIsWrittenAtOnce_andNothingIsWrittenForNoChange();
  void pages_writeWhatTheDialogWritesForTheSameChanges();
  void pages_writeWhatTheServerDialogWritesForTheSameChanges();
  void pages_theServerSettingsFollowTheirRules();
  void pages_takeASettingThatChangedElsewhere();
  void pages_aValueThatIsNotGoodIsNotWritten_andSaysWhy();
  void pages_whatDependsOnAnotherControlFollowsIt();
  void pages_haveTheRolesAndTheNamesOfTheirParts();
  void pages_theTreeOfAScreenReaderHasHeadingsGroupsAndNamedControls();
  void pages_theBackgroundServiceIsOnlyOnWindows();
  void switches_everySwitchOfThePages_followsAClickAndTheKeySpace();

  void banner_asksWhenAskedAndTellsItsAlert();

private:
  QTemporaryDir m_dir;
};
