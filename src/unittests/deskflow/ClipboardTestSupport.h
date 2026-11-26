/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "deskflow/Clipboard.h"
#include "deskflow/ClipboardChunk.h"
#include "deskflow/ProtocolTypes.h"
#include "io/IStream.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace clipboardtest {

//! A stream that records every write() as one packet and serves reads from a buffer
class TestStream : public deskflow::IStream
{
public:
  void close() override
  {
    m_input.clear();
  }

  uint32_t read(void *buffer, uint32_t n) override
  {
    const auto take = std::min<size_t>(n, m_input.size());
    if (buffer != nullptr && take > 0) {
      std::memcpy(buffer, m_input.data(), take);
    }
    m_input.erase(0, take);
    return static_cast<uint32_t>(take);
  }

  void write(const void *buffer, uint32_t n) override
  {
    m_packets.emplace_back(static_cast<const char *>(buffer), n);
  }

  void flush() override
  {
  }

  void shutdownInput() override
  {
  }

  void shutdownOutput() override
  {
  }

  void *getEventTarget() const override
  {
    return const_cast<TestStream *>(this);
  }

  bool isReady() const override
  {
    return !m_input.empty();
  }

  uint32_t getSize() const override
  {
    return static_cast<uint32_t>(m_input.size());
  }

  void setBulkSend(bool active) override
  {
    m_bulkSendCalls.push_back(active);
  }

  void feed(const std::string &bytes)
  {
    m_input += bytes;
  }

  const std::vector<std::string> &packets() const
  {
    return m_packets;
  }

  std::vector<std::string> takePackets()
  {
    return std::exchange(m_packets, {});
  }

  const std::vector<bool> &bulkSendCalls() const
  {
    return m_bulkSendCalls;
  }

private:
  std::string m_input;
  std::vector<std::string> m_packets;
  std::vector<bool> m_bulkSendCalls;
};

//! An event queue that records added events, keeps handlers and dispatches events on request
class TestEventQueue : public IEventQueue
{
public:
  ~TestEventQueue() override
  {
    for (const auto &event : m_addedEvents) {
      Event::deleteData(event);
    }
  }

  int loop() override
  {
    return 0;
  }

  void adoptBuffer(IEventQueueBuffer *) override
  {
  }

  bool getEvent(Event &, double = -1.0) override
  {
    return false;
  }

  bool dispatchEvent(const Event &event) override
  {
    const auto handler = m_handlers.find(HandlerKey{event.getType(), event.getTarget()});
    if (handler == m_handlers.end()) {
      return false;
    }
    const auto callback = handler->second;
    callback(event);
    return true;
  }

  void addEvent(Event &&event) override
  {
    m_addedEvents.emplace_back(std::move(event));
  }

  EventQueueTimer *newTimer(double, void *) override
  {
    ++m_timersCreated;
    return timer();
  }

  EventQueueTimer *newOneShotTimer(double, void *) override
  {
    ++m_timersCreated;
    return timer();
  }

  void deleteTimer(EventQueueTimer *) override
  {
  }

  void addHandler(EventTypes type, void *target, const EventHandler &handler) override
  {
    m_handlers[HandlerKey{type, target}] = handler;
  }

  void removeHandler(EventTypes type, void *target) override
  {
    m_handlers.erase(HandlerKey{type, target});
  }

  void removeHandlers(void *target) override
  {
    std::erase_if(m_handlers, [&](const auto &entry) { return entry.first.target == target; });
  }

  void waitForReady() const override
  {
  }

  void *getSystemTarget() override
  {
    return this;
  }

  EventQueueTimer *timer()
  {
    return reinterpret_cast<EventQueueTimer *>(&m_timerStorage);
  }

  const std::vector<Event> &addedEvents() const
  {
    return m_addedEvents;
  }

  size_t timersCreated() const
  {
    return m_timersCreated;
  }

  bool hasHandler(EventTypes type, void *target) const
  {
    return m_handlers.contains(HandlerKey{type, target});
  }

private:
  struct HandlerKey
  {
    EventTypes type;
    void *target;

    bool operator<(const HandlerKey &other) const
    {
      if (type != other.type) {
        return static_cast<uint32_t>(type) < static_cast<uint32_t>(other.type);
      }
      return std::less<void *>{}(target, other.target);
    }
  };

  int m_timerStorage = 0;
  size_t m_timersCreated = 0;
  std::map<HandlerKey, EventHandler> m_handlers;
  std::vector<Event> m_addedEvents;
};

//! A marshalled clipboard with one text format of \p textBytes bytes
inline std::string marshalledText(size_t textBytes, char fill = 'a')
{
  Clipboard clipboard;
  clipboard.open(0);
  clipboard.empty();
  clipboard.add(IClipboard::Format::Text, std::string(textBytes, fill));
  clipboard.close();
  return clipboard.marshall();
}

//! Feeds the packets of a clipboard stream to the real chunk reassembly, as the receiving proxies do
class ChunkReassembly
{
public:
  struct Result
  {
    TransferState state;
    ClipboardID id;
  };

  //! Returns false when the packet is not a clipboard chunk
  bool feed(const std::string &packet, Result *result = nullptr)
  {
    if (packet.size() < 4 || std::memcmp(packet.data(), kMsgDClipboard, 4) != 0) {
      return false;
    }
    TestStream stream;
    stream.feed(packet.substr(4));
    ClipboardID id = 0;
    uint32_t sequence = 0;
    const auto state = ClipboardChunk::assemble(&stream, m_data, id, sequence, m_state, m_maxSize);
    if (state == TransferState::Finished) {
      m_finished[id] = m_data;
      m_data.clear();
    }
    if (state == TransferState::Error) {
      ++m_errors;
    }
    if (result != nullptr) {
      *result = {state, id};
    }
    return true;
  }

  void feedAll(const std::vector<std::string> &packets)
  {
    for (const auto &packet : packets) {
      feed(packet);
    }
  }

  bool finished(ClipboardID id) const
  {
    return m_finished.contains(id);
  }

  const std::string &finishedData(ClipboardID id) const
  {
    return m_finished.at(id);
  }

  size_t errors() const
  {
    return m_errors;
  }

private:
  std::string m_data;
  ClipboardChunkAssemblyState m_state;
  std::map<ClipboardID, std::string> m_finished;
  size_t m_maxSize = 64 * 1024 * 1024;
  size_t m_errors = 0;
};

} // namespace clipboardtest
