/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClipboardSender.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/ClipboardChunk.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/StreamChunker.h"
#include "io/IStream.h"

#include <algorithm>

using namespace deskflow::clipboard;

namespace {
using Clock = ClipboardTransferLog::Clock;
} // namespace

ClipboardSender::ClipboardSender(
    deskflow::IStream *stream, IEventQueue *events, void *chunkTarget, std::string peer, size_t pacedChunkSize
)
    : m_stream(stream),
      m_events(events),
      m_chunkTarget(chunkTarget),
      m_streamTarget(stream->getEventTarget()),
      m_peer(std::move(peer)),
      m_pacedChunkSize(std::max<size_t>(pacedChunkSize, 1))
{
  m_events->addHandler(EventTypes::ClipboardSending, m_chunkTarget, [this](const auto &e) {
    onChunkEvent(e.getDataObject());
  });
  m_events->addHandler(EventTypes::StreamOutputFlushed, m_streamTarget, [this](const auto &) { onFlushed(); });
}

ClipboardSender::~ClipboardSender()
{
  m_events->removeHandler(EventTypes::ClipboardSending, m_chunkTarget);
  m_events->removeHandler(EventTypes::StreamOutputFlushed, m_streamTarget);
}

void ClipboardSender::send(ClipboardID id, uint32_t sequence, const std::string &data, bool paced)
{
  const auto chunkSize = paced ? m_pacedChunkSize : kClipboardChunkSize;
  m_log.begin(
      TransferDirection::Sent, id, m_peer, describeFormats(data), data.size(),
      transferChunkCount(data.size(), chunkSize), paced, Clock::now()
  );

  if (!paced) {
    StreamChunker::sendClipboard(data, data.size(), id, sequence, m_events, m_chunkTarget);
    return;
  }

  removeTransfer(id);
  m_queue.push_back(Transfer{id, sequence, data});
  if (!m_waitingForFlush) {
    writeNextChunk();
  }
}

bool ClipboardSender::cancel(ClipboardID id)
{
  const auto dropped = removeTransfer(id);
  m_log.cancel(TransferDirection::Sent, id);
  return dropped;
}

bool ClipboardSender::isBusy() const
{
  return m_waitingForFlush || !m_queue.empty();
}

std::string ClipboardSender::describeActive() const
{
  return m_log.describeActive(Clock::now());
}

void ClipboardSender::onChunkEvent(void *chunk)
{
  ClipboardChunk::send(m_stream, chunk);
  if (ClipboardChunk::getMark(chunk) == ChunkType::DataEnd) {
    if (const auto line = m_log.finish(TransferDirection::Sent, ClipboardChunk::getId(chunk), Clock::now())) {
      LOG_INFO("%s", line->c_str());
    }
  }
}

void ClipboardSender::onFlushed()
{
  m_waitingForFlush = false;
  if (m_queue.empty()) {
    setBulkSend(false);
  } else {
    writeNextChunk();
  }
}

void ClipboardSender::writeNextChunk()
{
  auto &transfer = m_queue.front();
  setBulkSend(true);

  if (!transfer.started) {
    transfer.started = true;
    ClipboardChunk::write(
        m_stream, transfer.id, transfer.sequence, ChunkType::DataStart, std::to_string(transfer.data.size())
    );
  }

  const auto size = std::min(m_pacedChunkSize, transfer.data.size() - transfer.offset);
  ClipboardChunk::write(
      m_stream, transfer.id, transfer.sequence, ChunkType::DataChunk, transfer.data.substr(transfer.offset, size)
  );
  transfer.offset += size;

  // the end chunk is a few bytes, so it goes with the last data chunk instead of costing another release
  if (transfer.offset >= transfer.data.size()) {
    ClipboardChunk::write(m_stream, transfer.id, transfer.sequence, ChunkType::DataEnd, {});
    if (const auto line = m_log.finish(TransferDirection::Sent, transfer.id, Clock::now())) {
      LOG_INFO("%s", line->c_str());
    }
    m_queue.pop_front();
  }

  m_waitingForFlush = true;
}

bool ClipboardSender::removeTransfer(ClipboardID id)
{
  return std::erase_if(m_queue, [id](const Transfer &transfer) { return transfer.id == id; }) > 0;
}

void ClipboardSender::setBulkSend(bool active)
{
  if (m_bulkSend != active) {
    m_bulkSend = active;
    m_stream->setBulkSend(active);
  }
}
