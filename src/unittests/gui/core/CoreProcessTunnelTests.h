/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

// What the process of the core does with the gate of the Cloudflare Tunnel. Only the starts that wait for the tunnel
// are made here: a start that goes on would run the real core.
class CoreProcessTunnelTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void withoutAGate_noTunnelIsRequired();
  void theTunnelIsRequiredByAClientOnly();
  void aStartThatWaitsForTheTunnel_isPostponedAndShowsAsStarting();
  void aTunnelThatFailsWhileTheStartWaits_endsTheStart();
  void aTunnelThatFailsAtOnce_endsTheStart();
  void aTunnelThatListensAfterTheSettingsBecameUnusable_endsTheStartInsteadOfStartingTheCore();
  void aStopWhileTheStartWaits_cancelsItAndLetsTheTunnelGo();
  void aTunnelThatListensAfterAStop_doesNotStartTheCore();
  void aCleanupOfACoreThatIsNotRunning_leavesTheTunnelAlone();
  void aNewGate_replacesTheOldOne();

  // the quit in service mode, with a stand-in of the client of the service
  void theQuitInServiceMode_stopsTheServiceCoreAndNoStartFollows();
  void theQuitWhileTheServiceIsNotConnected_connectsAndSendsTheStop();
  void theQuitThatCannotReachTheService_isBoundedAndWarnsWhatRemains();
  void aStartThatWaitsForTheService_isCancelledByTheQuitAndANewConnectionDoesNotStartTheCore();
  void theQuitInServiceModeWithTheTunnelOff_isWhatItWas();
  void aModeSwitchToTheServerWhileTheStartWaits_resetsTheState();
  void aModeSwitchToTheServerWhileTheTunnelComesUp_leavesNothingBehind();

  void cleanup();

private:
  QString m_dir;
};
