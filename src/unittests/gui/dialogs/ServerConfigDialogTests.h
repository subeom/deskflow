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
  void arrangement_screenshots_areMadeOfTheDialogs();

private:
  QTemporaryDir m_settingsDir;
};
