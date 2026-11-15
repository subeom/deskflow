/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClipboardTransferLogTests.h"

#include "deskflow/Clipboard.h"
#include "deskflow/ClipboardTransferLog.h"

#include <chrono>
#include <string>

using namespace deskflow::clipboard;
using namespace std::chrono_literals;

namespace {

using Clock = ClipboardTransferLog::Clock;

std::string marshalled(const std::string &text, const std::string &html = {})
{
  Clipboard clipboard;
  clipboard.open(0);
  clipboard.empty();
  if (!text.empty()) {
    clipboard.add(IClipboard::Format::Text, text);
  }
  if (!html.empty()) {
    clipboard.add(IClipboard::Format::HTML, html);
  }
  clipboard.close();
  return clipboard.marshall();
}

} // namespace

void ClipboardTransferLogTests::describeFormatsListsEachFormatWithItsSize()
{
  QCOMPARE(QString::fromStdString(describeFormats(marshalled("hello"))), QStringLiteral("Text 5 B"));
  QCOMPARE(
      QString::fromStdString(describeFormats(marshalled("hello", "<b>hi</b>"))), QStringLiteral("Text 5 B, HTML 9 B")
  );
}

void ClipboardTransferLogTests::describeFormatsHandlesEmptyAndTruncatedData()
{
  QCOMPARE(QString::fromStdString(describeFormats(marshalled(""))), QStringLiteral("no data"));
  QCOMPARE(QString::fromStdString(describeFormats("")), QStringLiteral("unreadable"));

  auto data = marshalled("hello");
  data.resize(data.size() - 3);
  QCOMPARE(QString::fromStdString(describeFormats(data)), QStringLiteral("unreadable"));
}

void ClipboardTransferLogTests::chunkCountIncludesStartAndEnd()
{
  QCOMPARE(transferChunkCount(0, 512), size_t{2});
  QCOMPARE(transferChunkCount(1, 512), size_t{3});
  QCOMPARE(transferChunkCount(512, 512), size_t{3});
  QCOMPARE(transferChunkCount(513, 512), size_t{4});
  QCOMPARE(transferChunkCount(3 * 1024 * 1024, 512 * 1024), size_t{8});
}

void ClipboardTransferLogTests::sentSummaryReportsFormatsChunksAndDuration()
{
  ClipboardTransferLog log;
  const auto start = Clock::now();
  const auto data = marshalled("hello");
  log.begin(TransferDirection::Sent, 0, "mini", describeFormats(data), data.size(), 3, false, start);

  const auto line = log.finish(TransferDirection::Sent, 0, start + 12ms);
  QVERIFY(line.has_value());
  QCOMPARE(
      QString::fromStdString(*line),
      QStringLiteral("clipboard 0 sent to \"mini\": Text 5 B; 17 B in 3 chunks, 12 ms, unpaced")
  );

  // a second finish has nothing to report
  QVERIFY(!log.finish(TransferDirection::Sent, 0, start + 20ms).has_value());
}

void ClipboardTransferLogTests::receivedSummaryCountsChunksAndTakesFormatsFromData()
{
  ClipboardTransferLog log;
  const auto start = Clock::now();
  const auto data = marshalled("hello", "<b>hi</b>");
  log.begin(TransferDirection::Received, 1, "server", {}, data.size(), 0, false, start);
  log.addChunk(TransferDirection::Received, 1);
  log.addChunk(TransferDirection::Received, 1);

  const auto line = log.finish(TransferDirection::Received, 1, start + 250ms, describeFormats(data));
  QVERIFY(line.has_value());
  QVERIFY(QString::fromStdString(*line).startsWith(QStringLiteral("clipboard 1 received from \"server\": ")));
  QVERIFY(QString::fromStdString(*line).contains(QStringLiteral("Text 5 B, HTML 9 B")));
  QVERIFY(QString::fromStdString(*line).endsWith(QStringLiteral("in 2 chunks, 250 ms")));
}

void ClipboardTransferLogTests::finishWithoutBeginProducesNothing()
{
  ClipboardTransferLog log;
  QVERIFY(!log.finish(TransferDirection::Sent, 0, Clock::now()).has_value());
  QVERIFY(!log.finish(TransferDirection::Received, 1, Clock::now()).has_value());
}

void ClipboardTransferLogTests::describeActiveNamesTransfersInFlight()
{
  ClipboardTransferLog log;
  const auto start = Clock::now();
  QCOMPARE(QString::fromStdString(log.describeActive(start)), QStringLiteral("none"));

  log.begin(TransferDirection::Received, 0, "server", {}, 2048, 0, false, start);
  log.addChunk(TransferDirection::Received, 0);
  QVERIFY(log.isActive(TransferDirection::Received));
  QVERIFY(!log.isActive(TransferDirection::Sent));
  QCOMPARE(
      QString::fromStdString(log.describeActive(start + 8s)),
      QStringLiteral("receiving clipboard 0 (2048 B, 1 chunks so far, 8000 ms ago)")
  );

  log.begin(TransferDirection::Sent, 1, "server", "Text 1 B", 13, 3, true, start + 1s);
  QCOMPARE(
      QString::fromStdString(log.describeActive(start + 9s)),
      QStringLiteral(
          "receiving clipboard 0 (2048 B, 1 chunks so far, 9000 ms ago), sending clipboard 1 (13 B, 8000 ms "
          "ago)"
      )
  );
}

void ClipboardTransferLogTests::cancelDropsTheTransfer()
{
  ClipboardTransferLog log;
  const auto start = Clock::now();
  log.begin(TransferDirection::Sent, 0, "mini", "Text 1 B", 13, 3, true, start);
  QVERIFY(log.isActive(TransferDirection::Sent));
  log.cancel(TransferDirection::Sent, 0);
  QVERIFY(!log.isActive(TransferDirection::Sent));
  QVERIFY(!log.finish(TransferDirection::Sent, 0, start + 5ms).has_value());
}

void ClipboardTransferLogTests::skipLinesNameTheReason()
{
  QCOMPARE(
      QString::fromStdString(formatSkipLine(0, "mini", SkipReason::OverSize, "3.5 MiB exceeds the 3 MiB limit")),
      QStringLiteral("clipboard 0 skipped for \"mini\": over size (3.5 MiB exceeds the 3 MiB limit)")
  );
  QCOMPARE(
      QString::fromStdString(formatSkipLine(1, "mini", SkipReason::IdenticalContent)),
      QStringLiteral("clipboard 1 skipped for \"mini\": identical content")
  );
  QCOMPARE(
      QString::fromStdString(formatSkipLine(0, "mini", SkipReason::DisabledForComputer)),
      QStringLiteral("clipboard 0 skipped for \"mini\": disabled for this computer")
  );
  QCOMPARE(
      QString::fromStdString(formatSkipLine(0, "mini", SkipReason::SharingDisabled)),
      QStringLiteral("clipboard 0 skipped for \"mini\": clipboard sharing is disabled")
  );
}

void ClipboardTransferLogTests::keepAliveTimeoutLineReportsSilenceAndTransfer()
{
  QCOMPARE(
      QString::fromStdString(formatKeepAliveTimeout("server", 9012ms, 8900ms, "none")),
      QStringLiteral(
          "server is dead: no keepalive for 9012 ms, last data 8900 ms ago, clipboard transfer in flight: "
          "none"
      )
  );
}

void ClipboardTransferLogTests::joinActivityLeavesOutIdleDirections()
{
  QCOMPARE(QString::fromStdString(joinActivity("none", "none")), QStringLiteral("none"));
  QCOMPARE(
      QString::fromStdString(joinActivity("receiving clipboard 0", "none")), QStringLiteral("receiving clipboard 0")
  );
  QCOMPARE(QString::fromStdString(joinActivity("none", "sending clipboard 1")), QStringLiteral("sending clipboard 1"));
  QCOMPARE(
      QString::fromStdString(joinActivity("receiving clipboard 0", "sending clipboard 1")),
      QStringLiteral("receiving clipboard 0, sending clipboard 1")
  );
}

QTEST_MAIN(ClipboardTransferLogTests)
