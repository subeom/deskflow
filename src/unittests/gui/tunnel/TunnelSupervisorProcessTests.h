/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

// The supervisor with the real process, ports, clock and inspector, and the stand-in of cloudflared (never the real
// one)
class TunnelSupervisorProcessTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aTunnelThatWorks_listensCarriesAConnectionAndStops();
  void theTokenReachesTheProcessThroughTheEnvironmentOnly();
  void aProcessThatWritesTheToken_isNeverLoggedWithIt();
  void aTokenInTheLastLineWithoutAnEnd_isMaskedLikeEveryLine();
  void aTokenInALongLine_neverReachesTheLog();
  void aTokenCutBetweenTwoWrites_isMasked();
  void aLoginThatIsAskedFor_failsAndLeavesNoProcess();
  void aProcessThatStopsAtOnce_failsAfterItsRestarts();
  void aProcessThatNeverListens_failsWithATimeOut();
  void aListenerThatIsNotOpened_failsWithATimeOut();
  void aPortThatIsBusy_givesWayToAFreeOne();
  void aPortTakenAfterTheCheck_isNotTheTunnel_andTheStartGoesOnOnAFreePort();
  void anEdgeThatRefuses_isShownWhileTheTunnelListens();
  void aProcessThatDiesWhileListening_isStartedAgain();
  void aLeftoverProcessOfACrashedRun_isEndedAtTheNextStart();
  void stopAndWait_leavesNoProcessBehind();
};
