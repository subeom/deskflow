/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class TunnelStateMachineTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aNewMachine_isIdleAndKnowsItsTarget();
  void start_launchesAndWaitsForTheLocalPort();
  void listening_needsAnOpenPortThatStaysOpenForTheProcessToo();
  void probing_startsByTimeWhenTheListenerLineIsNeverSeen();
  void listening_needsTheListenerLineOfCloudflaredToo();
  void listening_withoutTheLine_isAcceptedOnlyAfterALongerWatch();
  void aBindFailureAfterTheTunnelListens_isAPortConflictToo();
  void probing_isRepeatedUntilThePortOpens();
  void aPortThatDoesNotOpenInTime_failsAndKillsTheProcess();
  void aPortThatSomethingElseHolds_isNotTakenForTheTunnel();
  void aLoginThatIsAskedFor_failsKillsAndIsNeverRetried_data();
  void aLoginThatIsAskedFor_failsKillsAndIsNeverRetried();
  void aProblemOfTheConnection_isShownWhileListeningAndEndsWithAConnection();
  void aProcessThatStops_isStartedAgainWithGrowingPauses();
  void thePausesStartOverAfterAStableRun();
  void aProcessThatStopsAtOnceAgainAndAgain_failsWithItsExitCode();
  void aRunThatLastedMoreThanAMomentEndsTheCountOfQuickExits();
  void aProcessThatCannotBeStarted_meansCloudflaredIsMissing();
  void stop_killsARunningProcessAndEndsIdleWhenItIsGone();
  void stop_whileWaitingToRestartIsIdleAtOnce();
  void stop_asksAgainOnceWhenTheProcessDoesNotGo();
  void start_isIgnoredWhileRunningStoppingOrWhileTheLastProcessIsStillGoing();
  void rejectStart_failsWithoutAProcessAndAStartLeavesIt();
  void status_hasTheTextForEveryStateAndFailure();
  void needsUserAction_isTrueWhereRestartingCannotHelp();
};
