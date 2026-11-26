/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Event.h"
#include "deskflow/PacketStreamFilter.h"

//! The packet filter that the proxies of a link have in the program, with the way into it that a test needs
/*!
The streams of the tests are plain buffers; the program reads and writes packets (a length and a payload), and where a
message ends is the end of its packet. A test that has to be as the program is puts one of these around its buffer, and
calls received() after it has put the bytes of the peer into the buffer.
*/
class TestPacketStream : public PacketStreamFilter
{
public:
  using PacketStreamFilter::PacketStreamFilter;

  //! The bytes that the socket received are read into the packets
  void received()
  {
    filterEvent(Event(EventTypes::StreamInputReady, getStream()->getEventTarget()));
  }
};
