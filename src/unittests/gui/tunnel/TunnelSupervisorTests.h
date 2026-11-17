/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class TunnelSupervisorTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aStartWithoutTls_isRefusedBeforeAnythingIsRead();
  void aHostnameThatIsNoHostname_isRefused();
  void aMissingCloudflared_isReported();
  void aRelativePathOfCloudflared_isRefusedBeforeTheTokenIsRead();
  void missingOrIncompleteCredentials_areReported();
  void aBusyPortWithNoFreeOne_isReported();
  void aBusyPreferredPort_givesWayToAFreePort_andTellsIt();

  void theTokenIsInTheEnvironmentOfTheProcessOnly();
  void theTunnelListens_whenThePortIsOpenAndTheProcessStays();
  void theLoggedListener_startsTheProbeAtOnce();
  void aPortThatNeverOpens_timesOutAndEndsTheProcess();
  void aSecondStart_whileRunning_startsNothing();

  void everyLineThatIsLogged_isFreeOfTheToken();
  void aLoginThatIsAskedFor_endsTheProcessAndIsNotRestarted();
  void aProcessThatStaysAfterBeingAsked_isEndedHard();
  void aProcessThatStopsOnItsOwn_isStartedAgainWithTheSameTokenAndPort();
  void aProcessThatKeepsStoppingAtOnce_failsAndForgetsTheToken();
  void aProcessThatCannotBeStarted_isReported();
  void aProblemOfTheConnection_isShownUntilTheServerConnects();
  void everyRefusedConnection_isCountedWithItsReason();

  void stop_endsTheProcessAndGoesIdle();
  void stop_inBackoff_needsNoKill();
  void aStartWhileStopping_isDoneWhenTheProcessIsGone();
  void stop_forgetsAStartThatWaits();
  void stopAndWait_waitsForTheProcess();
  void stopAndWait_dropsAProcessThatNeverReports();
  void theSupervisorGoing_endsTheProcessAndRemovesTheRecord();

  void aSleep_doesNotTimeOutAStart();
  void aSleep_restartsAProcessWhoseListenerIsGone();
  void aSleep_leavesAProcessWhoseListenerIsThere();

  void theRunningProcess_isRecordedAndTheRecordRemovedAtItsEnd();
  void aLeftoverProcessOfAnEarlierRun_isEndedBeforeTheStart();
  void aLeftoverRecordOfAnotherProcess_isLeftAlone();
};
