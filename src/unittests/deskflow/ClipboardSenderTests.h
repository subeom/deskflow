/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QTest>

class ClipboardSenderTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void unpaced_enqueuesTheLegacyChunkEventsUnchanged();
  void unpaced_neverTouchesTheSocketOrWaitsForFlush();
  void paced_writesOneDataChunkPerFlush();
  void paced_defaultChunkIs32KiB();
  void paced_streamIsReassembledByTheRealReceiver();
  void paced_inputWrittenBetweenFlushesIsNeverBehindMoreThanOneChunk();
  void paced_sendsTheIdsOneAfterTheOther();
  void paced_newTransferSupersedesTheRunningTransferOfTheSameId();
  void paced_newTransferSupersedesAQueuedTransferOfTheSameId();
  void paced_abortedTransferLeavesTheReceiverAbleToTakeTheNextOne();
  void cancel_dropsRunningAndQueuedTransfers();
  void paced_bulkSendHintIsOnlyActiveWhileTransferring();
  void destroyedSender_ignoresLaterEvents();
  void paced_emptyChunkBoundaryStillFinishes();

private:
  Log m_log;
};
