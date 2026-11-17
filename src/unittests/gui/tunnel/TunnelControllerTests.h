/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class TunnelControllerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void withTheTunnelOff_nothingIsRequiredNothingStartsAndNothingIsWritten();
  void withTheTunnelOn_itIsRequiredInClientModeOnly();
  void acquire_startsTheTunnelWithWhatTheSettingsHold();
  void theGate_isListeningOnlyForTheConfigurationThatRuns();
  void acquire_whileTheSameTunnelRuns_startsNothingNew();
  void acquire_afterTheConfigurationChanged_startsTheTunnelAgain();
  void release_keepsTheTunnelForAWhileAndAnAcquireCancelsTheStop();
  void release_stopsTheTunnelAfterTheLinger();
  void aSwitchToTheServer_stopsTheTunnelAtOnce();
  void aBusyPort_isWrittenAsTheActivePortAndRemovedAtTheEnd();
  void thePortThatWasAsked_isNotWrittenAsAnActivePort();
  void aStaleActivePort_isClearedWhenTheControllerIsMade();
  void aStartThatCannotHappen_isToldAsFailed_andTheTunnelStaysRequired();
  void shutdown_endsTheProcess();
  void theLog_isForwardedWithoutTheToken();
  void theServerConnecting_clearsAProblemOfTheConnection();

private:
  QString m_dir;
};
