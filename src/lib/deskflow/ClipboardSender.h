/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ClipboardTransferLog.h"
#include "deskflow/ClipboardTypes.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

class IEventQueue;

namespace deskflow {
class IStream;
}

//! Size of the data chunks of a paced clipboard transfer
inline constexpr size_t kPacedClipboardChunkSize = 32 * 1024;

//! Sends the marshalled clipboards of one connection
/*!
A clipboard is sent in one of two ways.

An unpaced transfer is the original behaviour: StreamChunker queues the whole transfer as events at once, in chunks
of \c kClipboardChunkSize, and the stream gets all of it before anything that is written later.

A paced transfer is a low priority channel inside the same stream. The data is cut into small chunks and only one of
them is written each time the stream reports that its output has drained (StreamOutputFlushed), so mouse, key and
keepalive messages written in between are never queued behind more than one chunk. The messages are the same ones
(start, data, end) that every receiver reassembles whatever the chunk size, so only the chunk boundaries and their
timing differ.

Paced transfers are sent one after the other. A transfer for a clipboard id replaces any transfer of that id that is
running or waiting. The old one is abandoned without an end chunk; the receiver drops its partial data when the next
start chunk arrives. Whether to pace a link is the caller's decision: it must have negotiated a protocol that
implements the paced channel.
*/
class ClipboardSender
{
public:
  /*!
  \p stream is written to and reports when it has drained; \p chunkTarget is the event target for the chunk events of
  unpaced transfers; \p peer names the other side in the log.
  */
  ClipboardSender(
      deskflow::IStream *stream, IEventQueue *events, void *chunkTarget, std::string peer,
      size_t pacedChunkSize = kPacedClipboardChunkSize
  );
  ClipboardSender(ClipboardSender const &) = delete;
  ClipboardSender(ClipboardSender &&) = delete;
  ~ClipboardSender();

  ClipboardSender &operator=(ClipboardSender const &) = delete;
  ClipboardSender &operator=(ClipboardSender &&) = delete;

  void send(ClipboardID id, uint32_t sequence, const std::string &data, bool paced);

  //! Drops the paced transfer of \p id, whether it is running or waiting; true if there was one
  bool cancel(ClipboardID id);

  //! True while a paced transfer is running, waiting, or its last chunk has not drained yet
  bool isBusy() const;

  //! The transfers in flight, for the log
  std::string describeActive() const;

private:
  struct Transfer
  {
    ClipboardID id;
    uint32_t sequence;
    std::string data;
    size_t offset = 0;
    bool started = false;
  };

  void onChunkEvent(void *chunk);
  void onFlushed();
  void writeNextChunk();
  bool removeTransfer(ClipboardID id);
  void setBulkSend(bool active);

  deskflow::IStream *m_stream;
  IEventQueue *m_events;
  void *m_chunkTarget;
  void *m_streamTarget;
  std::string m_peer;
  size_t m_pacedChunkSize;
  std::deque<Transfer> m_queue;
  bool m_waitingForFlush = false;
  bool m_bulkSend = false;
  deskflow::clipboard::ClipboardTransferLog m_log;
};
