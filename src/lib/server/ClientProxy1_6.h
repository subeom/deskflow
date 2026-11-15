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

#include <string>

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
  bool recvClipboard() override;

protected:
  std::string describeClipboardActivity() const override;

  //! Whether clipboards are sent to this client as a paced stream, which needs a negotiated protocol that has one
  virtual bool shouldPaceClipboard() const;

private:
  IEventQueue *m_events;
  deskflow::clipboard::ClipboardTransferLog m_transferLog;
  ClipboardSender m_sender;
  std::string m_clipboardDataCached;
  ClipboardChunkAssemblyState m_clipboardChunkState;
};
