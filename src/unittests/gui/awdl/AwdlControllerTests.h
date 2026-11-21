/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The controller of the option that turns off the wireless link of AirDrop while sharing, with a system that is a fake:
// what it asks of the system and when, from the start of the core to its stop. No program is run and no dialog is
// shown.
class AwdlControllerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void optionOff_runsNothingAtAll();
  void notAMac_runsNothingAtAll_notEvenTheButtons();
  void theCoreStarts_withTheLinkUp_asksOnce_andReadsAgainToCheck();
  void aNo_isLoggedOnce_leavesTheHint_andIsNeverAskedAgainByItself();
  void aFailure_isLoggedAsAFailure_andLeavesTheHint();
  void theLinkComesBackWhileTheCoreRuns_isAHint_andTheButtonAsks();
  void theCoreStops_restoresNothing_andStopsReading();
  void aNewRunOfTheCore_asksAgain_once();
  void theOptionChangedWhileTheCoreRuns_isFollowedAtOnce();
  void turnBackOn_isNotTakenBackInThisRun();
  void turnOffNow_isTheButton_whateverTheOptionAndTheCore();
  void onlyOneDialogAtATime();
  void aStateThatIsNotKnown_neverAsks();
  void theSentenceOfTheDialog_isTheOneThatIsGiven();
  void signals_sayOnlyWhatChanged();
  void theLinkIsNeverTurnedOnByItself_inAnyRun();

  void theLinkBeingUpIsNoProblem_withoutTheSpikesThereIsNoHint();
  void theSpikes_withTheLinkUp_areTheHint_andTheyGoWithTheLink();
  void aWire_isSaid_andNothingIsSent();
  void aGatewayThatIsNotLocal_orAConnectionThatIsNotWiFi_isNeverMeasured();
  void aBurst_goesToTheRouteThatWasFound_andToNothingElse();
  void theBursts_comeOnceAMinute_whileTheCoreRunsAndTheLinkIsUp();
  void withTheOptionOff_nothingIsLookedUpOrSent_andTheStoppedCoreSendsNothing();
  void measureNow_isTheButton_onWiFiOnly_whateverTheOptionAndTheCore();
  void refresh_looksAtTheRouteAndSendsNothing();
  void summary_saysWhatTheStatusLineSays();
  void aBurstThatComesAfterTheStop_saysNothing();
};
