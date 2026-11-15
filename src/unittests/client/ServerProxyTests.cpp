/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerProxyTests.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/ILogOutputter.h"
#include "client/Client.h"
#include "client/ServerProxy.h"
#include "client/ServerProxy1_9.h"
#include "common/InputCapabilities.h"
#include "common/PlatformInfo.h"
#include "deskflow/AppUtil.h"
#include "deskflow/Clipboard.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"

#include <QTest>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace {

class TestAppUtil : public AppUtil
{
public:
  int run() override
  {
    return 0;
  }

  std::vector<std::string> getKeyboardLayoutList() override
  {
    return {"en"};
  }

  std::string getCurrentLanguageCode() override
  {
    return "en";
  }
};

class FakeStream : public deskflow::IStream
{
public:
  void push(const std::string &bytes)
  {
    m_chunks.push_back(bytes);
  }

  void close() override
  {
    m_chunks.clear();
    m_inputShutdown = true;
  }

  uint32_t read(void *buffer, uint32_t size) override
  {
    if (m_inputShutdown || m_chunks.empty() || size == 0) {
      return 0;
    }

    auto &front = m_chunks.front();
    const size_t bytesToRead = std::min(static_cast<size_t>(size), front.size());
    if (buffer != nullptr) {
      std::memcpy(buffer, front.data(), bytesToRead);
    }

    front.erase(0, bytesToRead);
    if (front.empty()) {
      m_chunks.pop_front();
    }
    return static_cast<uint32_t>(bytesToRead);
  }

  void write(const void *data, uint32_t size) override
  {
    m_output.append(static_cast<const char *>(data), size);
  }

  QByteArray takeOutput()
  {
    auto output = m_output;
    m_output.clear();
    return output;
  }

  void flush() override
  {
  }

  void shutdownInput() override
  {
    close();
  }

  void shutdownOutput() override
  {
  }

  void *getEventTarget() const override
  {
    return const_cast<FakeStream *>(this);
  }

  bool isReady() const override
  {
    return !m_inputShutdown && !m_chunks.empty();
  }

  uint32_t getSize() const override
  {
    size_t total = 0;
    for (const auto &chunk : m_chunks) {
      total += chunk.size();
    }
    return static_cast<uint32_t>(std::min<size_t>(total, UINT32_MAX));
  }

private:
  std::deque<std::string> m_chunks;
  bool m_inputShutdown = false;
  QByteArray m_output;
};

class RecordingEventQueue : public IEventQueue
{
public:
  ~RecordingEventQueue() override
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
    return timer();
  }

  EventQueueTimer *newOneShotTimer(double, void *) override
  {
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
    for (auto it = m_handlers.begin(); it != m_handlers.end();) {
      if (it->first.target == target) {
        it = m_handlers.erase(it);
      } else {
        ++it;
      }
    }
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
  std::map<HandlerKey, EventHandler> m_handlers;
  std::vector<Event> m_addedEvents;
};

class TestServerProxy : public ServerProxy
{
public:
  using ServerProxy::ServerProxy;

  bool parseHandshakeMessageReturnsDisconnect(const uint8_t *code)
  {
    return parseHandshakeMessage(code) == ConnectionResult::Disconnect;
  }

  bool parseMessageForTest(const uint8_t *code)
  {
    return parseMessage(code) == ConnectionResult::Okay;
  }

  ScrollDelta lastWheelDelta() const
  {
    return m_lastWheelDelta;
  }

protected:
  void deliverMouseWheel(int32_t xDelta, int32_t yDelta) override
  {
    m_lastWheelDelta = {xDelta, yDelta};
  }

private:
  ScrollDelta m_lastWheelDelta{0, 0};
};

class TestServerProxy1_9 : public ServerProxy1_9
{
public:
  TestServerProxy1_9(
      Client *client, deskflow::IStream *stream, IEventQueue *events, InputCapabilities localCapabilities
  )
      : ServerProxy1_9(client, stream, events),
        m_localCapabilities(localCapabilities)
  {
  }

  bool suppressesKeyRepeatForTest(KeyID id)
  {
    return shouldSuppressKeyRepeat(id, 0);
  }

  void onOptionsChangedForTest(const OptionsList &options)
  {
    onOptionsChanged(options);
  }

  bool parseMessageForTest(const uint8_t *code)
  {
    return parseMessage(code) == ConnectionResult::Okay;
  }

  bool handleSemanticMediaKeyForTest(KeyID id)
  {
    return handleSemanticMediaKey(id);
  }

  KeyID lastSemanticMediaKey() const
  {
    return m_lastSemanticMediaKey;
  }

  ScrollDelta lastWheelDelta() const
  {
    return m_lastWheelDelta;
  }

protected:
  InputCapabilities localInputCapabilities() const override
  {
    return m_localCapabilities;
  }

  void deliverMouseWheel(int32_t xDelta, int32_t yDelta) override
  {
    m_lastWheelDelta = {xDelta, yDelta};
  }

  bool synthesizeSemanticMediaKey(KeyID id) override
  {
    m_lastSemanticMediaKey = id;
    return true;
  }

private:
  InputCapabilities m_localCapabilities;
  ScrollDelta m_lastWheelDelta{0, 0};
  KeyID m_lastSemanticMediaKey = kKeyNone;
};

class CapturingOutputter : public ILogOutputter
{
public:
  void open(const QString &) override
  {
  }

  void close() override
  {
  }

  bool write(LogLevel::Level level, const QString &message) override
  {
    if (level == LogLevel::Level::Info) {
      m_infoLines.append(message);
    }
    return false;
  }

  bool hasInfoLineContaining(const QString &text) const
  {
    return std::ranges::any_of(m_infoLines, [&](const QString &line) { return line.contains(text); });
  }

private:
  QStringList m_infoLines;
};

Client *undereferenceableClient()
{
  // These paths must queue cleanup without calling through to Client.
  return reinterpret_cast<Client *>(0x1);
}

TestAppUtil &testAppUtil()
{
  static TestAppUtil util;
  return util;
}

const Client::DisconnectRequest *disconnectRequest(const RecordingEventQueue &events)
{
  if (events.addedEvents().size() != 1) {
    return nullptr;
  }
  return static_cast<const Client::DisconnectRequest *>(events.addedEvents().front().getDataObject());
}

} // namespace

void ServerProxyTests::initTestCase()
{
  (void)testAppUtil();
  m_log.setFilter(LogLevel::Level::Debug);
}

void ServerProxyTests::handleKeepAliveAlarm_timeout_queuesDisconnectRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  ServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, events.timer())));

  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto &event = events.addedEvents().front();
  QVERIFY(event.getType() == EventTypes::ClientDisconnectRequested);
  QCOMPARE(event.getTarget(), stream.getEventTarget());

  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Disconnect);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("server is not responding"));
}

void ServerProxyTests::handleKeepAliveAlarm_timeout_logsSilenceAndClipboardTransfer()
{
  CapturingOutputter outputter;
  m_log.insert(&outputter, true);

  RecordingEventQueue events;
  FakeStream stream;
  ServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, events.timer())));
  QVERIFY(outputter.hasInfoLineContaining(QStringLiteral("server is dead: no keepalive for ")));
  QVERIFY(outputter.hasInfoLineContaining(QStringLiteral("clipboard transfer in flight: none")));

  // a clipboard handed to the stream and not yet written is reported as being sent
  Clipboard clipboard;
  clipboard.open(0);
  clipboard.empty();
  clipboard.add(IClipboard::Format::Text, "hello");
  clipboard.close();
  proxy.onClipboardChanged(0, &clipboard);

  QVERIFY(events.dispatchEvent(Event(EventTypes::Timer, events.timer())));
  QVERIFY(outputter.hasInfoLineContaining(QStringLiteral("clipboard transfer in flight: sending clipboard 0 (17 B")));

  m_log.remove(&outputter);
}

void ServerProxyTests::handleData_incompleteMessage_queuesDisconnectRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  stream.push("DF");
  ServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto &event = events.addedEvents().front();
  QVERIFY(event.getType() == EventTypes::ClientDisconnectRequested);
  QCOMPARE(event.getTarget(), stream.getEventTarget());

  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Disconnect);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("incomplete message from server"));
}

void ServerProxyTests::parseHandshakeMessage_protocolError_queuesRefusalRequest()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  QVERIFY(proxy.parseHandshakeMessageReturnsDisconnect(reinterpret_cast<const uint8_t *>(kMsgEBad)));
  QCOMPARE(events.addedEvents().size(), static_cast<size_t>(1));
  const auto *request = disconnectRequest(events);
  QVERIFY(request != nullptr);
  QVERIFY(request->kind() == Client::DisconnectRequest::Kind::Refuse);
  QVERIFY(request->refusalReason() == deskflow::core::ConnectionRefusal::ProtocolError);
  QCOMPARE(QString::fromUtf8(request->message()), QStringLiteral("server reported a protocol error"));
}

void ServerProxyTests::legacyScroll_preservesDmwmValues()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy proxy(undereferenceableClient(), &stream, &events);

  FakeStream wheelPacket;
  ProtocolUtil::writef(&wheelPacket, kMsgDMouseWheel, static_cast<int16_t>(120), static_cast<int16_t>(-120));
  const auto wheelBytes = wheelPacket.takeOutput();
  stream.push(std::string(wheelBytes.constData() + 4, static_cast<size_t>(wheelBytes.size() - 4)));
  QVERIFY(proxy.parseMessageForTest(reinterpret_cast<const uint8_t *>(kMsgDMouseWheel)));
  QCOMPARE(proxy.lastWheelDelta().x, 120);
  QCOMPARE(proxy.lastWheelDelta().y, -120);
}

void ServerProxyTests::inputCapabilities_v19_readsSourceAndWritesDestinationCapabilities()
{
  RecordingEventQueue events;
  FakeStream stream;
  const InputCapabilities sourceCapabilities{InputPlatform::MacOS, 0x01020304};

  ProtocolUtil::writef(
      &stream, kMsgDInputCapabilities, static_cast<uint8_t>(sourceCapabilities.platform),
      static_cast<uint32_t>(sourceCapabilities.capabilities)
  );
  const auto packet = stream.takeOutput();
  stream.push(std::string(packet.constData(), static_cast<size_t>(packet.size())));

  TestServerProxy1_9 proxy(undereferenceableClient(), &stream, &events, deskflow::platform::currentInputCapabilities());
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  QCOMPARE(
      static_cast<uint8_t>(proxy.sourceInputCapabilities().platform), static_cast<uint8_t>(sourceCapabilities.platform)
  );
  QCOMPARE(proxy.sourceInputCapabilities().capabilities, sourceCapabilities.capabilities);

  FakeStream expected;
  ProtocolUtil::writef(
      &expected, kMsgCInputCapabilities, static_cast<uint8_t>(deskflow::platform::currentInputPlatform()),
      deskflow::platform::currentInputCapabilities().capabilities
  );
  QCOMPARE(stream.takeOutput(), expected.takeOutput());
}

void ServerProxyTests::f18_option_isIgnoredByTheDestination()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy1_9 proxy(
      undereferenceableClient(), &stream, &events, InputCapabilities{InputPlatform::Windows, kInputCapabilityHangul}
  );

  FakeStream sourcePacket;
  ProtocolUtil::writef(
      &sourcePacket, kMsgDInputCapabilities, static_cast<uint8_t>(InputPlatform::MacOS), kInputCapabilityProtocolV19
  );
  const auto sourceBytes = sourcePacket.takeOutput();
  stream.push(std::string(sourceBytes.constData(), static_cast<size_t>(sourceBytes.size())));
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  // F18 is mapped to Hangul by the source before it writes the key, so the destination never maps it again and
  // never suppresses a repeat on account of it; Command is the plain Windows key here too
  proxy.onOptionsChangedForTest({kOptionCrossOSF18Key, static_cast<OptionValue>(F18Action::Hangul)});
  for (const auto key : {kKeyF18, kKeyHangul, kKeySuper_L, kKeySuper_R}) {
    QVERIFY(!proxy.suppressesKeyRepeatForTest(key));
  }
}

void ServerProxyTests::crossOSScroll_v19TransformsDmwmBeforeDelivery()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy1_9 proxy(
      undereferenceableClient(), &stream, &events,
      InputCapabilities{InputPlatform::Windows, kInputCapabilityCrossOSScroll}
  );

  FakeStream sourcePacket;
  ProtocolUtil::writef(
      &sourcePacket, kMsgDInputCapabilities, static_cast<uint8_t>(InputPlatform::MacOS), kInputCapabilityProtocolV19
  );
  const auto sourceBytes = sourcePacket.takeOutput();
  stream.push(std::string(sourceBytes.constData(), static_cast<size_t>(sourceBytes.size())));
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  proxy.onOptionsChangedForTest({kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 0});
  FakeStream wheelPacket;
  ProtocolUtil::writef(&wheelPacket, kMsgDMouseWheel, static_cast<int16_t>(120), static_cast<int16_t>(-120));
  const auto wheelBytes = wheelPacket.takeOutput();
  stream.push(std::string(wheelBytes.constData() + 4, static_cast<size_t>(wheelBytes.size() - 4)));
  QVERIFY(proxy.parseMessageForTest(reinterpret_cast<const uint8_t *>(kMsgDMouseWheel)));
  QCOMPARE(proxy.lastWheelDelta().x, -120);
  QCOMPARE(proxy.lastWheelDelta().y, -120);

  proxy.onOptionsChangedForTest({});
  stream.push(std::string(wheelBytes.constData() + 4, static_cast<size_t>(wheelBytes.size() - 4)));
  QVERIFY(proxy.parseMessageForTest(reinterpret_cast<const uint8_t *>(kMsgDMouseWheel)));
  QCOMPARE(proxy.lastWheelDelta().x, 120);
  QCOMPARE(proxy.lastWheelDelta().y, -120);
}

void ServerProxyTests::crossOSScroll_mouseWheelOnlyScopeFailsClosedForPlainDmwm()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy1_9 proxy(
      undereferenceableClient(), &stream, &events,
      InputCapabilities{InputPlatform::Windows, kInputCapabilityCrossOSScroll}
  );

  FakeStream sourcePacket;
  ProtocolUtil::writef(
      &sourcePacket, kMsgDInputCapabilities, static_cast<uint8_t>(InputPlatform::MacOS), kInputCapabilityProtocolV19
  );
  const auto sourceBytes = sourcePacket.takeOutput();
  stream.push(std::string(sourceBytes.constData(), static_cast<size_t>(sourceBytes.size())));
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  FakeStream wheelPacket;
  ProtocolUtil::writef(&wheelPacket, kMsgDMouseWheel, static_cast<int16_t>(120), static_cast<int16_t>(-120));
  const auto wheelBytes = wheelPacket.takeOutput();
  const auto deliverWheel = [&]() {
    stream.push(std::string(wheelBytes.constData() + 4, static_cast<size_t>(wheelBytes.size() - 4)));
    return proxy.parseMessageForTest(reinterpret_cast<const uint8_t *>(kMsgDMouseWheel));
  };

  // A plain DMWM does not say which device produced it.  Under the mouse-wheel-only scope the source already
  // inverted the events it recognized as a mouse wheel, so the destination must not invert anything again even
  // if a server (wrongly) also sent the invert flags.
  const auto wheelOnly = static_cast<OptionValue>(ScrollInvertScope::MouseWheelOnly);
  proxy.onOptionsChangedForTest(
      {kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 1, kOptionCrossOSInvertScope, wheelOnly}
  );
  QVERIFY(deliverWheel());
  QCOMPARE(proxy.lastWheelDelta().x, 120);
  QCOMPARE(proxy.lastWheelDelta().y, -120);

  // the default scope inverts every packet as before
  proxy.onOptionsChangedForTest({kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 1});
  QVERIFY(deliverWheel());
  QCOMPARE(proxy.lastWheelDelta().x, -120);
  QCOMPARE(proxy.lastWheelDelta().y, 120);

  // and a later options update without the scope drops it again
  proxy.onOptionsChangedForTest(
      {kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 1, kOptionCrossOSInvertScope, wheelOnly}
  );
  proxy.onOptionsChangedForTest({kOptionCrossOSInvertScrollX, 1});
  QVERIFY(deliverWheel());
  QCOMPARE(proxy.lastWheelDelta().x, -120);
  QCOMPARE(proxy.lastWheelDelta().y, -120);
}

void ServerProxyTests::semanticMedia_v19HandlesOnlyEligibleKeys()
{
  RecordingEventQueue events;
  FakeStream stream;
  TestServerProxy1_9 proxy(
      undereferenceableClient(), &stream, &events,
      InputCapabilities{InputPlatform::Windows, kInputCapabilitySemanticMedia}
  );

  FakeStream sourcePacket;
  ProtocolUtil::writef(
      &sourcePacket, kMsgDInputCapabilities, static_cast<uint8_t>(InputPlatform::MacOS), kInputCapabilityProtocolV19
  );
  const auto sourceBytes = sourcePacket.takeOutput();
  stream.push(std::string(sourceBytes.constData(), static_cast<size_t>(sourceBytes.size())));
  QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));

  proxy.onOptionsChangedForTest({kOptionCrossOSSemanticMedia, 1});
  FakeStream keyPacket;
  const std::string language = "en";
  ProtocolUtil::writef(&keyPacket, kMsgDKeyDown, kKeyAudioPlay, 0, 0, &language);
  const auto keyBytes = keyPacket.takeOutput();
  stream.push(std::string(keyBytes.constData() + 4, static_cast<size_t>(keyBytes.size() - 4)));
  QVERIFY(proxy.parseMessageForTest(reinterpret_cast<const uint8_t *>(kMsgDKeyDown)));
  QCOMPARE(proxy.lastSemanticMediaKey(), kKeyAudioPlay);
  QVERIFY(!proxy.handleSemanticMediaKeyForTest(kKeyBrightnessUp));
  QVERIFY(!proxy.handleSemanticMediaKeyForTest(kKeyF1));

  proxy.onOptionsChangedForTest({});
  QVERIFY(!proxy.handleSemanticMediaKeyForTest(kKeyAudioPlay));
}

QTEST_MAIN(ServerProxyTests)
