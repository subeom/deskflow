/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class ClipboardTransferLogTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void describeFormatsListsEachFormatWithItsSize();
  void describeFormatsHandlesEmptyAndTruncatedData();
  void chunkCountIncludesStartAndEnd();
  void sentSummaryReportsFormatsChunksAndDuration();
  void receivedSummaryCountsChunksAndTakesFormatsFromData();
  void finishWithoutBeginProducesNothing();
  void describeActiveNamesTransfersInFlight();
  void cancelDropsTheTransfer();
  void skipLinesNameTheReason();
  void keepAliveTimeoutLineReportsSilenceAndTransfer();
};
