/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_6.h"

#include "base/Log.h"
#include "deskflow/ClipboardChunk.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"
#include "server/Server.h"

using deskflow::clipboard::ClipboardTransferLog;
using deskflow::clipboard::describeFormats;
using deskflow::clipboard::formatSkipLine;
using deskflow::clipboard::SkipReason;
using deskflow::clipboard::TransferDirection;
using Clock = ClipboardTransferLog::Clock;

//
// ClientProxy1_6
//

ClientProxy1_6::ClientProxy1_6(const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events)
    : ClientProxy1_5(name, stream, server, events),
      m_events(events),
      m_sender(stream, events, this, name)
{
}

ClientProxy1_6::~ClientProxy1_6() = default;

bool ClientProxy1_6::shouldPaceClipboard() const
{
  return false;
}

bool ClientProxy1_6::sendsClipboardId(ClipboardID) const
{
  return true;
}

bool ClientProxy1_6::mirrorsSelection() const
{
  return false;
}

void ClientProxy1_6::setClipboard(ClipboardID id, const IClipboard *clipboard)
{
  if (!sendsClipboardId(id)) {
    if (!m_loggedSkippedId) {
      m_loggedSkippedId = true;
      LOG_INFO("%s", formatSkipLine(id, getName(), SkipReason::SelectionNotShared).c_str());
    }
    return;
  }

  // ignore if this clipboard is already clean
  if (m_clipboard[id].m_dirty) {
    // this clipboard is now clean
    m_clipboard[id].m_dirty = false;
    Clipboard::copy(&m_clipboard[id].m_clipboard, clipboard);

    std::string data = m_clipboard[id].m_clipboard.marshall();
    LOG_DEBUG("sending clipboard %d to \"%s\"", id, getName().c_str());

    m_sender.send(id, 0, data, shouldPaceClipboard());
  }
}

void ClientProxy1_6::setClipboardDirty(ClipboardID id, bool dirty)
{
  // the Server changes the state when this computer took the clipboard or copied something new, or when another one
  // did: what is on its way to this computer is stale either way, and would overwrite a newer copy
  m_sender.cancel(id);
  ClientProxy1_5::setClipboardDirty(id, dirty);
}

void ClientProxy1_6::grabClipboard(ClipboardID id)
{
  // the grab says the data of a running transfer is stale, so it must not follow the grab
  m_sender.cancel(id);
  ClientProxy1_5::grabClipboard(id);
}

bool ClientProxy1_6::recvClipboard()
{
  // parse message
  ClipboardID id;
  uint32_t seq;

  auto r = ClipboardChunk::assemble(
      getStream(), m_clipboardDataCached, id, seq, m_clipboardChunkState, m_server->getMaximumClipboardSizeBytes()
  );

  if (r == TransferState::Started) {
    size_t size = ClipboardChunk::getExpectedSize(m_clipboardChunkState);
    LOG_DEBUG("receiving clipboard %d size=%zu", id, size);
    m_transferLog.begin(TransferDirection::Received, id, getName(), {}, size, 0, false, Clock::now());
  } else if (r == TransferState::InProgress) {
    m_transferLog.addChunk(TransferDirection::Received, id);
  } else if (r == TransferState::Finished) {
    if (const auto line = m_transferLog.finish(
            TransferDirection::Received, id, Clock::now(), describeFormats(m_clipboardDataCached)
        )) {
      LOG_INFO("%s", line->c_str());
    }
    LOG(
        (CLOG_DEBUG "received client \"%s\" clipboard %d seqnum=%d, size=%zu", getName().c_str(), id, seq,
         m_clipboardDataCached.size())
    );
    // save clipboard, and notify. A Windows or macOS client that sends one clipboard sends it for both ids to the
    // computers that need the selection
    const auto publish = [&](ClipboardID target) {
      m_clipboard[target].m_clipboard.unmarshall(m_clipboardDataCached, 0);
      m_clipboard[target].m_sequenceNumber = seq;
      auto *info = new ClipboardInfo;
      info->m_id = target;
      info->m_sequenceNumber = seq;
      m_events->addEvent(Event(EventTypes::ClipboardChanged, getEventTarget(), info));
    };
    publish(id);
    if (id == kClipboardClipboard && mirrorsSelection()) {
      publish(kClipboardSelection);
    }
    m_clipboardDataCached.clear();
    m_clipboardDataCached.shrink_to_fit();
  } else if (r == TransferState::Error) {
    m_transferLog.cancel(TransferDirection::Received, id);
    return false;
  }

  return true;
}

std::string ClientProxy1_6::describeClipboardActivity() const
{
  return deskflow::clipboard::joinActivity(m_transferLog.describeActive(Clock::now()), m_sender.describeActive());
}
