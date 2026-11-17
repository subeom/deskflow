/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class OrphanRecordTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aRecord_isWrittenAndReadBack();
  void aFileThatIsNotARecord_isNotARecord();
  void aRecordWithoutAProcess_isNotWritten();

  void withoutARecord_nothingIsDone();
  void aRecordOfAProcessThatIsGone_isRemovedAndNothingIsEnded();
  void aProcessWithAnotherStartTime_isLeftAlone();
  void aProcessWithAnotherProgram_isLeftAlone();
  void aProcessThatIsNotCloudflared_isLeftAloneEvenWhenTheRecordSaysSo();
  void aStartTimeWithinTheTolerance_isTheSameProcess();
  void theSameProcess_isAskedToGoFirstAndEndedOnlyWhenItStays();
  void aProcessThatCannotBeEnded_isReported();
  void theRecordIsRemovedInEveryCase();

  void theInspector_knowsTheProcessOfThisProgram();
  void theInspector_doesNotKnowAProcessThatIsGone();
  void theInspector_refusesToEndThisProgramAndTheSystemProcesses();
  void aRealLeftoverProcess_isEnded();
  void aRealProcessWithAnotherStartTime_isNotTouched();
};
