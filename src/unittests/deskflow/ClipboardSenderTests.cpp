/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClipboardSenderTests.h"

#include "ClipboardTestSupport.h"
#include "base/Event.h"
#include "deskflow/ClipboardChunk.h"
#include "deskflow/ClipboardSender.h"
#include "deskflow/ProtocolUtil.h"
#include "deskflow/StreamChunker.h"

#include <cstring>
#include <memory>

using namespace clipboardtest;

namespace {

constexpr size_t kTestChunk = 1000;

void flush(TestEventQueue &events, TestStream &stream)
{
  events.dispatchEvent(Event(EventTypes::StreamOutputFlushed, stream.getEventTarget()));
}

uint8_t markOf(const std::string &packet)
{
  // DCLP + id(1) + sequence(4) + mark(1) + string length(4) + data
  return static_cast<uint8_t>(packet.at(4 + 1 + 4));
}

bool isClipboardPacket(const std::string &packet)
{
  return packet.size() >= 4 && std::memcmp(packet.data(), kMsgDClipboard, 4) == 0;
}

bool isDataChunk(const std::string &packet)
{
  return isClipboardPacket(packet) && markOf(packet) == ChunkType::DataChunk;
}

void writeInput(TestStream &stream)
{
  ProtocolUtil::writef(&stream, kMsgDMouseMove, 10, 20);
}

// runs the flush events until the sender has nothing left to write
void drain(ClipboardSender &sender, TestEventQueue &events, TestStream &stream)
{
  for (size_t guard = 0; sender.isBusy() && guard < 100000; ++guard) {
    flush(events, stream);
  }
}

std::vector<std::string> legacyPackets(const std::string &data, ClipboardID id, uint32_t sequence)
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  StreamChunker::sendClipboard(data, data.size(), id, sequence, &events, &target);
  for (const auto &event : events.addedEvents()) {
    ClipboardChunk::send(&stream, event.getDataObject());
  }
  return stream.packets();
}

} // namespace

void ClipboardSenderTests::initTestCase()
{
  m_log.setFilter(LogLevel::Level::Debug);
}

void ClipboardSenderTests::unpaced_enqueuesTheLegacyChunkEventsUnchanged()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  const auto data = marshalledText(kClipboardChunkSize + 1000);

  {
    ClipboardSender sender(&stream, &events, &target, "peer");
    sender.send(0, 7, data, false);

    // nothing is written until the events are handled; then the bytes are those of the unchanged StreamChunker path
    QCOMPARE(stream.packets().size(), size_t{0});
    QCOMPARE(events.addedEvents().size(), size_t{4});
    for (const auto &event : events.addedEvents()) {
      QVERIFY(events.dispatchEvent(Event(event.getType(), event.getTarget(), event.getDataObject())));
    }
  }

  QCOMPARE(stream.packets(), legacyPackets(data, 0, 7));
}

void ClipboardSenderTests::unpaced_neverTouchesTheSocketOrWaitsForFlush()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer");

  sender.send(0, 0, marshalledText(100), false);
  sender.send(1, 0, marshalledText(100), false);

  QVERIFY(stream.bulkSendCalls().empty());
  QVERIFY(!sender.isBusy());
  flush(events, stream);
  QCOMPARE(stream.packets().size(), size_t{0});
}

void ClipboardSenderTests::paced_writesOneDataChunkPerFlush()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);
  const auto data = marshalledText(3000 - 12); // 3000 bytes marshalled: three full chunks
  QCOMPARE(data.size(), size_t{3000});

  sender.send(0, 0, data, true);
  // the start chunk goes out with the first data chunk, and nothing before the stream has drained
  QCOMPARE(stream.packets().size(), size_t{2});
  QCOMPARE(markOf(stream.packets()[0]), static_cast<uint8_t>(ChunkType::DataStart));
  QCOMPARE(markOf(stream.packets()[1]), static_cast<uint8_t>(ChunkType::DataChunk));

  flush(events, stream);
  QCOMPARE(stream.packets().size(), size_t{3});
  flush(events, stream);
  // the last data chunk completes the transfer, so the end chunk follows it in the same release
  QCOMPARE(stream.packets().size(), size_t{5});
  QCOMPARE(markOf(stream.packets()[4]), static_cast<uint8_t>(ChunkType::DataEnd));

  flush(events, stream);
  QCOMPARE(stream.packets().size(), size_t{5});
  QVERIFY(!sender.isBusy());
}

void ClipboardSenderTests::paced_defaultChunkIs32KiB()
{
  QCOMPARE(kPacedClipboardChunkSize, size_t{32 * 1024});

  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer");
  const auto data = marshalledText(3 * 1024 * 1024 - 12);

  sender.send(0, 0, data, true);
  drain(sender, events, stream);

  size_t dataChunks = 0;
  for (const auto &packet : stream.packets()) {
    dataChunks += isDataChunk(packet) ? 1 : 0;
  }
  QCOMPARE(dataChunks, size_t{96});
}

void ClipboardSenderTests::paced_streamIsReassembledByTheRealReceiver()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);
  const auto data = marshalledText(5555);

  sender.send(0, 3, data, true);
  drain(sender, events, stream);

  ChunkReassembly receiver;
  receiver.feedAll(stream.packets());
  QCOMPARE(receiver.errors(), size_t{0});
  QVERIFY(receiver.finished(0));
  QCOMPARE(receiver.finishedData(0), data);
}

void ClipboardSenderTests::paced_inputWrittenBetweenFlushesIsNeverBehindMoreThanOneChunk()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);

  sender.send(0, 0, marshalledText(8000), true);
  while (sender.isBusy()) {
    writeInput(stream);
    flush(events, stream);
  }

  // no two data chunks are ever adjacent: an input packet was written between any two of them
  const auto &packets = stream.packets();
  size_t dataChunks = 0;
  for (size_t i = 0; i < packets.size(); ++i) {
    if (isDataChunk(packets[i])) {
      ++dataChunks;
      if (i + 1 < packets.size()) {
        QVERIFY2(!isDataChunk(packets[i + 1]), "two data chunks without input between them");
      }
    }
  }
  QVERIFY(dataChunks >= 8);
}

void ClipboardSenderTests::paced_sendsTheIdsOneAfterTheOther()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);
  const auto data = marshalledText(2500);

  sender.send(0, 0, data, true);
  sender.send(1, 0, data, true);
  drain(sender, events, stream);

  ChunkReassembly receiver;
  receiver.feedAll(stream.packets());
  QCOMPARE(receiver.errors(), size_t{0});
  QVERIFY(receiver.finished(0));
  QVERIFY(receiver.finished(1));

  // the second transfer only starts once the first has ended
  size_t endOfFirst = 0;
  size_t startOfSecond = 0;
  for (size_t i = 0; i < stream.packets().size(); ++i) {
    const auto &packet = stream.packets()[i];
    const auto id = static_cast<uint8_t>(packet.at(4));
    if (markOf(packet) == ChunkType::DataEnd && id == 0) {
      endOfFirst = i;
    } else if (markOf(packet) == ChunkType::DataStart && id == 1) {
      startOfSecond = i;
    }
  }
  QVERIFY(startOfSecond > endOfFirst);
}

void ClipboardSenderTests::paced_newTransferSupersedesTheRunningTransferOfTheSameId()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);
  const auto oldData = marshalledText(6000, 'a');
  const auto newData = marshalledText(3000, 'b');

  sender.send(0, 0, oldData, true);
  flush(events, stream);
  flush(events, stream);
  sender.send(0, 1, newData, true);
  drain(sender, events, stream);

  size_t ends = 0;
  size_t starts = 0;
  for (const auto &packet : stream.packets()) {
    if (isClipboardPacket(packet)) {
      ends += markOf(packet) == ChunkType::DataEnd ? 1 : 0;
      starts += markOf(packet) == ChunkType::DataStart ? 1 : 0;
    }
  }
  // the old transfer was abandoned without an end chunk
  QCOMPARE(starts, size_t{2});
  QCOMPARE(ends, size_t{1});

  ChunkReassembly receiver;
  receiver.feedAll(stream.packets());
  QCOMPARE(receiver.errors(), size_t{0});
  QVERIFY(receiver.finished(0));
  QCOMPARE(receiver.finishedData(0), newData);
}

void ClipboardSenderTests::paced_newTransferSupersedesAQueuedTransferOfTheSameId()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);
  const auto first = marshalledText(3000, 'a');
  const auto oldSelection = marshalledText(3000, 'b');
  const auto newSelection = marshalledText(2000, 'c');

  sender.send(0, 0, first, true);
  sender.send(1, 0, oldSelection, true);
  sender.send(1, 1, newSelection, true);
  drain(sender, events, stream);

  ChunkReassembly receiver;
  receiver.feedAll(stream.packets());
  QCOMPARE(receiver.errors(), size_t{0});
  QCOMPARE(receiver.finishedData(0), first);
  QCOMPARE(receiver.finishedData(1), newSelection);

  // the superseded selection never started
  size_t selectionStarts = 0;
  for (const auto &packet : stream.packets()) {
    if (isClipboardPacket(packet) && markOf(packet) == ChunkType::DataStart && packet.at(4) == 1) {
      ++selectionStarts;
    }
  }
  QCOMPARE(selectionStarts, size_t{1});
}

void ClipboardSenderTests::paced_abortedTransferLeavesTheReceiverAbleToTakeTheNextOne()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);

  // abort a transfer of id 0 in favour of one for id 1: the receiver starts over at the new start chunk
  sender.send(0, 0, marshalledText(6000, 'a'), true);
  flush(events, stream);
  sender.cancel(0);
  const auto data = marshalledText(2500, 'z');
  sender.send(1, 0, data, true);
  drain(sender, events, stream);

  ChunkReassembly receiver;
  receiver.feedAll(stream.packets());
  QCOMPARE(receiver.errors(), size_t{0});
  QVERIFY(!receiver.finished(0));
  QCOMPARE(receiver.finishedData(1), data);
}

void ClipboardSenderTests::cancel_dropsRunningAndQueuedTransfers()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);

  sender.send(0, 0, marshalledText(6000), true);
  sender.send(1, 0, marshalledText(6000), true);
  sender.cancel(0);
  sender.cancel(1);
  const auto written = stream.packets().size();

  flush(events, stream);
  flush(events, stream);
  QCOMPARE(stream.packets().size(), written);
  QVERIFY(!sender.isBusy());
}

void ClipboardSenderTests::paced_bulkSendHintIsOnlyActiveWhileTransferring()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);
  QVERIFY(stream.bulkSendCalls().empty());

  sender.send(0, 0, marshalledText(2500), true);
  QCOMPARE(stream.bulkSendCalls(), std::vector<bool>({true}));

  flush(events, stream);
  flush(events, stream);
  // the end chunk is written; the hint stays on until the stream has drained
  QCOMPARE(stream.bulkSendCalls(), std::vector<bool>({true}));

  flush(events, stream);
  QCOMPARE(stream.bulkSendCalls(), std::vector<bool>({true, false}));
}

void ClipboardSenderTests::destroyedSender_ignoresLaterEvents()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  {
    ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);
    sender.send(0, 0, marshalledText(6000), true);
    QVERIFY(events.hasHandler(EventTypes::StreamOutputFlushed, stream.getEventTarget()));
  }

  const auto written = stream.packets().size();
  QVERIFY(!events.hasHandler(EventTypes::StreamOutputFlushed, stream.getEventTarget()));
  QVERIFY(!events.hasHandler(EventTypes::ClipboardSending, &target));
  flush(events, stream);
  QCOMPARE(stream.packets().size(), written);
}

void ClipboardSenderTests::paced_emptyChunkBoundaryStillFinishes()
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  // a payload that is an exact multiple of the chunk size must not end with an empty data chunk
  ClipboardSender sender(&stream, &events, &target, "peer", kTestChunk);
  const auto data = marshalledText(2000 - 12);
  QCOMPARE(data.size(), size_t{2000});

  sender.send(0, 0, data, true);
  drain(sender, events, stream);

  size_t dataChunks = 0;
  for (const auto &packet : stream.packets()) {
    dataChunks += isDataChunk(packet) ? 1 : 0;
  }
  QCOMPARE(dataChunks, size_t{2});

  ChunkReassembly receiver;
  receiver.feedAll(stream.packets());
  QCOMPARE(receiver.finishedData(0), data);
}

QTEST_MAIN(ClipboardSenderTests)
