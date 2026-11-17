/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class TunnelTestRunnerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void aTunnelThatWorks_isStartedForTheTestPassesAndIsLetGo();
  void aTunnelThatRunsForTheClient_isUsedAndLeftRunning();
  void aTunnelThatIsStartingForTheClient_isWaitedForAndNotLetGoByTheTest();
  void everyPrerequisiteIsReported_andNothingIsStartedWhenOneIsMissing();
  void theSecretIsNotReadToTestThatATokenIsStored();
  void aTunnelThatDoesNotStart_failsAtTheProcessAndSkipsTheRest();
  void aLocalPortThatDoesNotOpen_failsAtThePortAfterTheProcessStarted();
  void aConnectionThatCloudflaredRefusesWithAReason_namesTheReason();
  void aConnectionThatIsClosedWithoutAReason_isAConnectFailure();
  void aConnectionThatTheLocalPortRefuses_isAFailure();
  void anOldProblemOfTheConnection_isNotTakenForANewOne();
  void aTestThatIsCancelled_tellsNothingMoreAndLetsTheTunnelGo();
  void theSummaryLine_isEnglishAndNamesNoToken();
  void failureKeys_areTheSameInEveryLanguage();

  void theSystemHold_tellsHeldClosedAndRefused();

private:
  QString m_dir;
};
