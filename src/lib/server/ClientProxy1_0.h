/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/Clipboard.h"
#include "deskflow/ProtocolTypes.h"
#include "server/ClientProxy.h"

#include <chrono>
#include <string>

class Event;
class EventQueueTimer;
class IEventQueue;

//! Proxy for client implementing protocol version 1.0
class ClientProxy1_0 : public ClientProxy
{
public:
  ClientProxy1_0(const std::string &name, deskflow::IStream *adoptedStream, IEventQueue *events);
  ClientProxy1_0(ClientProxy1_0 const &) = delete;
  ClientProxy1_0(ClientProxy1_0 &&) = delete;
  ~ClientProxy1_0() override;

  ClientProxy1_0 &operator=(ClientProxy1_0 const &) = delete;
  ClientProxy1_0 &operator=(ClientProxy1_0 &&) = delete;

  // IComputer
  bool getClipboard(ClipboardID id, IClipboard *) const override;
  void getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const override;
  void getCursorPos(int32_t &x, int32_t &y) const override;

  // IClient overrides
  void enter(int32_t xAbs, int32_t yAbs, uint32_t seqNum, KeyModifierMask mask, bool forScreensaver) override;
  bool leave() override;
  void setClipboard(ClipboardID, const IClipboard *) override;
  void grabClipboard(ClipboardID) override;
  void setClipboardDirty(ClipboardID, bool) override;
  void keyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override;
  void keyRepeat(KeyID, KeyModifierMask, int32_t count, KeyButton, const std::string &) override;
  void keyUp(KeyID, KeyModifierMask, KeyButton) override;
  void mouseDown(ButtonID) override;
  void mouseUp(ButtonID) override;
  void mouseMove(int32_t xAbs, int32_t yAbs) override;
  void mouseRelativeMove(int32_t xRel, int32_t yRel) override;
  void mouseWheel(int32_t xDelta, int32_t yDelta) override;
  void screensaver(bool activate) override;
  void resetOptions() override;
  void setOptions(const OptionsList &options) override;
  void sendDragInfo(uint32_t fileCount, const char *info, size_t size) override;
  void fileChunkSending(uint8_t mark, char *data, size_t dataSize) override;
  std::string getSecureInputApp() const override;
  void secureInputNotification(const std::string &app) const override;

protected:
  virtual bool parseHandshakeMessage(const uint8_t *code);
  virtual bool parseMessage(const uint8_t *code);
  virtual bool isHandshakeCompleteReady() const;
  void maybeCompleteHandshake();

  virtual void resetHeartbeatRate();
  virtual void setHeartbeatRate(double rate, double alarm);
  virtual void resetHeartbeatTimer();
  virtual void addHeartbeatTimer();
  virtual void removeHeartbeatTimer();
  virtual bool recvClipboard();

  //! The clipboard transfers in flight, for the log when the client stops responding
  virtual std::string describeClipboardActivity() const;

  //! The name of the client, as it is written in the lines of the log that a program reads
  std::string loggableName() const;

  //! Tells the Server that the client took clipboard \p id
  virtual void notifyClipboardGrabbed(ClipboardID id, uint32_t sequence);

  //! How many bytes of the message that is being parsed come after its code
  /*!
  The stream of a client is made of packets, and the size of a packet is the one thing that says where a message ends:
  a message that is shorter or longer than its format says must not take the bytes of the next one. It is the size of
  the packet that holds the message, less its code, as it was before the code was read; a stream that is not made of
  packets says its whole buffer.
  */
  uint32_t bytesAfterCode() const
  {
    return m_packetBytes > 4 ? m_packetBytes - 4 : 0;
  }

private:
  void disconnect();
  void removeHandlers();

  void handleData();
  void handleDisconnect();
  void handleWriteError();
  void handleFlatline();

  bool recvInfo();
  bool recvGrabClipboard();

protected:
  struct ClientClipboard
  {
  public:
    ClientClipboard() = default;

  public:
    Clipboard m_clipboard;
    uint32_t m_sequenceNumber = 0;
    bool m_dirty = true;
  };

  ClientClipboard m_clipboard[kClipboardEnd];

private:
  using MessageParser = bool (ClientProxy1_0::*)(const uint8_t *);

  ClientInfo m_info;
  double m_heartbeatAlarm;
  EventQueueTimer *m_heartbeatTimer = nullptr;
  MessageParser m_parser = &ClientProxy1_0::parseHandshakeMessage;
  IEventQueue *m_events;
  bool m_handshakeInfoReceived = false;
  bool m_handshakeComplete = false;
  uint32_t m_packetBytes = 0;
  std::chrono::steady_clock::time_point m_lastData = std::chrono::steady_clock::now();
};
