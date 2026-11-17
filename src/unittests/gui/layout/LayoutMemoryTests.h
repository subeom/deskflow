/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

class LayoutMemoryTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void init();

  void off_doesNothingAtAll();
  void start_theFirstTime_remembersTheArrangement();
  void start_withTheSameDisplays_doesNothing();
  void start_afterTheDisplaysChangedWhileClosed_putsTheLayoutBack();
  void aBurstOfEvents_isOneLook();
  void anEndlessBurst_isLookedAtAfterTheMaximumWait();
  void anEventThatChangesNothing_isIgnored();
  void exactMatch_putsTheLayoutBackAndRestarts();
  void similarMatch_putsTheLayoutBack_andCopiesItOnTheFirstEdit();
  void noMatch_keepsTheArrangement_putsComputersAgainstTheServer_andRemembers();
  void noMatch_withoutAnOldSituation_justRemembers();
  void noRestart_whenTheConfigurationDoesNotChange();
  void noRestart_whenTheCoreDoesNotRun();
  void restarts_areTheMinimumGapApart_andNeverLoop();
  void aRestartThatIsNotNeededAnyMore_isNotDone();
  void undo_putsTheArrangementBack_andRestarts();
  void undo_ofAMadeProfile_removesIt();
  void undo_isGoneWhenTheUserSaves_andWhenThereIsNothingToUndo();
  void undo_whenNothingChanged_isNotOffered();
  void aDialogThatIsOpen_defersTheChange_andItIsMadeAfter();
  void displaysThatCannotBeRead_areTriedAgain_thenGivenUp();
  void aLayoutThatCannotBeApplied_leavesTheArrangementAndDoesNotRetry();
  void poll_seesAChangeThatNoEventToldOf();
  void configSaved_isRememberedForTheSituationThatWasActive();
  void configSaved_changesOnlyWhatChanged();
  void saveCurrentAs_namesTheProfileOfTheSituation();
  void stop_forgetsEverything();
  void aStoreOfANewerVersion_switchesItOffWithAWarning();
  void whatIsRemembered_survivesARestartOfTheProgram();
  void theTimesAreTheOnesOfTheHost();
  void theLogLinesHaveTheDocumentedFormat();
  void property_anySequenceOfEvents_keepsTheRules();

private:
  QString path() const
  {
    return m_dir.filePath(QStringLiteral("layout-profiles.json"));
  }
  QTemporaryDir m_dir;
};
