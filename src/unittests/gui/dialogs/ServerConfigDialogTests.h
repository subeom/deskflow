/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QTemporaryDir>

class ServerConfigDialogTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();
  void experimentalMultiMonitor_isOffByDefaultAndSaysItIsExperimental();
  void experimentalMultiMonitor_isLoadedSavedAndRestoredToOff();
  void experimentalMultiMonitor_makesTheDialogModified();
  void experimentalMultiMonitor_isToldToTheComputerDialogsThatItOpens();
  void clipboardTextOnly_isOffByDefault();
  void clipboardTextOnly_isOnlyAvailableWhileClipboardsAreShared();
  void clipboardTextOnly_replacesTheSizeLimit();
  void clipboardTextOnly_isLoadedFromTheSettings();
  void clipboardTextOnly_isSavedToTheSettings();
  void clipboardTextOnly_isTurnedOffByRestoringDefaults();
  void clipboardTextOnly_makesTheDialogModified();
  void clipboardTextOnly_explainsWhatIsNotShared();
  void arrangement_isNotThereWhileTheExperimentIsOff();
  void arrangement_isOfferedWhenTheExperimentIsOn_andTheClassicGridStays();
  void arrangement_isUsedOnRequest_savedWithThePlaces_andLeftAgain();
  void arrangement_withComputersOnTopOfEachOther_cannotBeSaved();
  void arrangement_aComputerIsRemovedFromTheCanvas_theServerIsNot();
  void arrangement_aComputerThatReportsAnotherScreen_isWarnedOf();
  void arrangement_aSizeThatIsAssumed_isSaidInTheStatusLine();
  void arrangement_inCanvasMode_theGridOnlyWidgetsGiveWayToButtons_andAComputerIsAdded();
  void arrangement_saveKeepsWhatComputersReportedWhileTheDialogWasOpen();
  void arrangement_firstUse_adoptsTheSizesThatComputersReported();
  void arrangement_withAnExternalConfig_canBeSavedWhateverTheArrangementIs();
  void arrangement_monitorsAreDrawnInTheEditor_andKeptOnOk_notOnCancel();
  void arrangement_doubleClick_drawsTheMonitors_butTheServerHasItsSettings();
  void arrangement_aComputerScaledOnTheCanvas_hasItsZoomSaved_andTheHintSaysHow();
  void arrangement_aComputerResizedOnTheCanvas_hasItsSizeSaidAndSaved();
  void arrangement_screenshots_areMadeOfTheDialogs();
  void rememberArrangements_isOffByDefault_andIsLoadedSavedAndRestoredToOff();
  void rememberArrangements_makesTheDialogModified_andCancelDoesNotSaveIt();

  // phase 5: the toolbar of the arrangement in the new look; with the system look it is what it was
  void newLook_theToolbarOfTheArrangementIsGroupedAndStillHasEveryHook();
  void systemLook_theToolbarOfTheArrangementIsWhatItWas();

  // the grid of the classic arrangement builds the option of its cells from a widget that it deletes
  void computerGrid_neverHandsAStyleAnObjectThatIsGone();

private:
  QTemporaryDir m_settingsDir;
};
