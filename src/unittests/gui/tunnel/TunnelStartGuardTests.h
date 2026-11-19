/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class TunnelStartGuardTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aStartOfAServerOrWithoutATunnel_isNotTouched();
  void aTunnelThatListensAlready_letsTheStartThrough_andIsToldItIsNeeded();
  void aTunnelThatDoesNotListen_postponesTheStartAndStartsTheCoreWhenItListens();
  void theCoreIsStartedOnceAndTheStartThatItMakesPassesTheGuard();
  void aTunnelThatFailsWhileTheCoreWaits_reportsItAsWaiting();
  void aTunnelThatFailsAtOnce_isHandledAndTheStartStillEndsPostponed();
  void aTunnelThatFailsAfterTheCoreStarted_reportsItAsNotWaiting();
  void aStopWhileWaiting_cancelsTheStartAndReleasesTheTunnel();
  void aListeningThatCameAfterAStop_doesNotStartTheCore();
  void aTunnelThatListensAfterTheConfigurationBecameUnusable_doesNotStartTheCore_data();
  void aTunnelThatListensAfterTheConfigurationBecameUnusable_doesNotStartTheCore();
  void theStartAfterAnUnusableResume_asksTheTunnelAgain();
  void everyStopReleasesTheTunnel();
};
