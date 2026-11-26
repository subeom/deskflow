/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ClipboardChunk.h"
#include "deskflow/ClipboardSender.h"
#include "deskflow/ClipboardTransferLog.h"
#include "server/ClientProxy1_5.h"

#include <array>
#include <optional>
#include <string>
#include <utility>

class Server;
class IEventQueue;

//! Proxy for client implementing protocol version 1.6
class ClientProxy1_6 : public ClientProxy1_5
{
public:
  ClientProxy1_6(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_6() override;

  void setClipboard(ClipboardID id, const IClipboard *clipboard) override;
  void grabClipboard(ClipboardID id) override;
  void setClipboardDirty(ClipboardID id, bool dirty) override;
  bool recvClipboard() override;

protected:
  std::string describeClipboardActivity() const override;

  //! Whether clipboards are sent to this client as a paced stream, which needs a negotiated protocol that has one
  virtual bool shouldPaceClipboard() const;

  //! Whether the data of clipboard \p id is sent to this client; the selection is not to a client that ignores the id
  virtual bool sendsClipboardId(ClipboardID id) const;

  //! Whether this client sends one clipboard for a Windows or macOS computer, which is also its selection
  virtual bool mirrorsSelection() const;

  //! Whether this client keeps its clipboard when it is told that another computer took it
  /*!
  A macOS computer does (it does nothing then), so what it was sent last is still on it unless it copied something
  itself since, and sending the same bytes again only rewrites its pasteboard.  Windows empties its clipboard then,
  and nothing is known of other platforms: they are sent the data again.
  */
  virtual bool keepsClipboardWhenGrabbed() const;

  void notifyClipboardGrabbed(ClipboardID id, uint32_t sequence) override;

private:
  IEventQueue *m_events;
  deskflow::clipboard::ClipboardTransferLog m_transferLog;
  bool m_loggedSkippedId = false;
  // size and hash of what this client was last sent and has not changed since, per clipboard
  std::array<std::optional<std::pair<size_t, size_t>>, kClipboardEnd> m_sent;
  ClipboardSender m_sender;
  std::string m_clipboardDataCached;
  ClipboardChunkAssemblyState m_clipboardChunkState;
};
