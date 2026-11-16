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
#include "common/ClipboardCapabilities.h"
#include "common/InputCapabilities.h"
#include "common/MonitorCapabilities.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "deskflow/AppUtil.h"
#include "deskflow/Clipboard.h"
#include "deskflow/MonitorReport.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"
#include "server/ClientProxy1_9.h"

#include <QTemporaryDir>
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

  void
  setLocal(const deskflow::server::MonitorRect &shape, const std::vector<deskflow::monitors::MonitorInfo> &monitors)
  {
    m_shape = shape;
    m_monitors = monitors;
  }

  void tellMonitorsChanged()
  {
    onMonitorsChanged();
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

  LocalMonitors localMonitors() const override
  {
    return {m_shape, m_monitors};
  }

  deskflow::server::MonitorRect localShape() const override
  {
    return m_shape;
  }

  ClientInfo currentInfo() const override
  {
    ClientInfo info;
    info.m_x = m_shape.x;
    info.m_y = m_shape.y;
    info.m_w = m_shape.w;
    info.m_h = m_shape.h;
    info.m_mx = m_shape.x + 1;
    info.m_my = m_shape.y + 1;
    return info;
  }

private:
  deskflow::server::MonitorRect m_shape{0, 0, 0, 0};
  std::vector<deskflow::monitors::MonitorInfo> m_monitors;
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

  int infoLineCount(const QString &text) const
  {
    return static_cast<int>(std::ranges::count_if(m_infoLines, [&](const QString &line) {
      return line.contains(text);
    }));
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
  // the proxy of the server reads the experiment from the settings, which are not the ones of the user
  static QTemporaryDir settingsDir;
  QVERIFY(settingsDir.isValid());
  Settings::setSettingsFile(settingsDir.filePath(QStringLiteral("Deskflow.conf")));
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
      deskflow::platform::currentInputCapabilities().capabilities | kClipboardCapabilityPaced |
          kClipboardCapabilitySingleId
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

namespace {

using deskflow::monitors::MonitorInfo;
using deskflow::server::MonitorRect;

// 2560x1440 (primary, 150 %) and 1920x1080 side by side, top aligned: the shape is 0,0 4480x1440
const std::vector<MonitorInfo> kTwoMonitors{{{0, 0, 2560, 1440}, 150, true}, {{2560, 0, 1920, 1080}, 100, false}};
const MonitorRect kTwoMonitorsShape{0, 0, 4480, 1440};

std::string stdString(const QByteArray &bytes)
{
  return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QByteArray messageBytes(const char *format, auto... args)
{
  FakeStream packet;
  ProtocolUtil::writef(&packet, format, args...);
  return packet.takeOutput();
}

QByteArray reportBytes(uint16_t generation, const std::vector<deskflow::monitors::MonitorEntry> &monitors)
{
  const auto message = deskflow::monitors::encodeReport({generation, monitors});
  return QByteArray(reinterpret_cast<const char *>(message.data()), static_cast<qsizetype>(message.size()));
}

QByteArray infoBytes(const MonitorRect &shape)
{
  return messageBytes(
      kMsgDInfo, static_cast<int16_t>(shape.x), static_cast<int16_t>(shape.y), static_cast<int16_t>(shape.w),
      static_cast<int16_t>(shape.h), static_cast<int16_t>(0), static_cast<int16_t>(shape.x + 1),
      static_cast<int16_t>(shape.y + 1)
  );
}

const std::vector<deskflow::monitors::MonitorEntry> kTwoMonitorsOnTheWire{
    {0, 0, 2560, 1440, 150, true}, {2560, 0, 1920, 1080, 100, false}
};

// the capability mask that the destination wrote before this feature
InputCapabilityMask legacyDestinationMask()
{
  return deskflow::platform::currentInputCapabilities().capabilities | kClipboardCapabilityPaced |
         kClipboardCapabilitySingleId;
}

QByteArray capabilitiesReply(InputCapabilityMask mask)
{
  return messageBytes(kMsgCInputCapabilities, static_cast<uint8_t>(deskflow::platform::currentInputPlatform()), mask);
}

// a destination (client side) proxy that the server has said hello to
struct DestinationLink
{
  RecordingEventQueue events;
  FakeStream stream;
  std::unique_ptr<TestServerProxy1_9> proxy;

  DestinationLink(const MonitorRect &shape, const std::vector<MonitorInfo> &monitors)
  {
    proxy = std::make_unique<TestServerProxy1_9>(
        undereferenceableClient(), &stream, &events, deskflow::platform::currentInputCapabilities()
    );
    proxy->setLocal(shape, monitors);
    // the server asks for the shape of the computer first, as it always does, and gets it
    receive(messageBytes(kMsgQInfo));
    (void)written();
  }

  void receive(const QByteArray &bytes)
  {
    stream.push(stdString(bytes));
    QVERIFY(events.dispatchEvent(Event(EventTypes::StreamInputReady, stream.getEventTarget())));
  }

  void sourceSays(InputCapabilityMask mask)
  {
    receive(
        messageBytes(kMsgDInputCapabilities, static_cast<uint8_t>(InputPlatform::MacOS), static_cast<uint32_t>(mask))
    );
  }

  QByteArray written()
  {
    return stream.takeOutput();
  }
};

} // namespace

// a server that does not set the bit (every server before this one, and this one with the experiment off) is answered
// byte for byte as it always was, and is never sent a report
void ServerProxyTests::monitorReport_serverWithoutTheBit_clientAnswersAsBeforeAndSaysNothing()
{
  DestinationLink link(kTwoMonitorsShape, kTwoMonitors);

  link.sourceSays(kInputCapabilityProtocolV19 | kClipboardCapabilityPaced | kClipboardCapabilitySingleId);
  QCOMPARE(link.written(), capabilitiesReply(legacyDestinationMask()));

  link.proxy->tellMonitorsChanged();
  QCOMPARE(link.written(), QByteArray());
  link.proxy->onInfoChanged();
  QCOMPARE(link.written(), infoBytes(kTwoMonitorsShape));
}

void ServerProxyTests::monitorReport_serverWithTheBit_clientAcknowledgesAndReports()
{
  DestinationLink link(kTwoMonitorsShape, kTwoMonitors);

  link.sourceSays(kInputCapabilityProtocolV19 | kClipboardCapabilityPaced | kLayoutCapabilityMonitors);

  // the acknowledgement, then the report, in that order, in this one reply
  QCOMPARE(
      link.written(),
      capabilitiesReply(legacyDestinationMask() | kLayoutCapabilityMonitors) + reportBytes(1, kTwoMonitorsOnTheWire)
  );
}

// a computer that has no monitors to list (Linux, a machine without a display) does not acknowledge, whatever is asked
void ServerProxyTests::monitorReport_clientThatCannotListItsMonitors_doesNotAcknowledge()
{
  DestinationLink link(kTwoMonitorsShape, {});

  link.sourceSays(kInputCapabilityProtocolV19 | kLayoutCapabilityMonitors);
  QCOMPARE(link.written(), capabilitiesReply(legacyDestinationMask()));
  link.proxy->tellMonitorsChanged();
  QCOMPARE(link.written(), QByteArray());
}

void ServerProxyTests::monitorReport_afterEveryDinf_butOnlyWhenTheMonitorsChanged()
{
  DestinationLink link(kTwoMonitorsShape, kTwoMonitors);
  link.sourceSays(kLayoutCapabilityMonitors);
  (void)link.written();

  // the server asks again: the shape is sent and the monitors are the same, so there is nothing more to say
  link.proxy->onInfoChanged();
  QCOMPARE(link.written(), infoBytes(kTwoMonitorsShape));

  // the monitors swap places: the same shape, another list: DINF and then the report
  link.proxy->setLocal(kTwoMonitorsShape, {{{0, 0, 1920, 1080}, 100, false}, {{1920, 0, 2560, 1440}, 150, true}});
  link.proxy->onInfoChanged();
  QCOMPARE(
      link.written(),
      infoBytes(kTwoMonitorsShape) + reportBytes(2, {{0, 0, 1920, 1080, 100, false}, {1920, 0, 2560, 1440, 150, true}})
  );
}

void ServerProxyTests::monitorReport_monitorsChangedButNotTheShape_sendsJustTheReport()
{
  DestinationLink link(kTwoMonitorsShape, kTwoMonitors);
  link.sourceSays(kLayoutCapabilityMonitors);
  (void)link.written();

  // nothing changed: nothing is sent
  link.proxy->tellMonitorsChanged();
  QCOMPARE(link.written(), QByteArray());

  // a display scale changed: no DINF, one report
  link.proxy->setLocal(kTwoMonitorsShape, {{{0, 0, 2560, 1440}, 200, true}, {{2560, 0, 1920, 1080}, 100, false}});
  link.proxy->tellMonitorsChanged();
  QCOMPARE(link.written(), reportBytes(2, {{0, 0, 2560, 1440, 200, true}, {2560, 0, 1920, 1080, 100, false}}));
}

// the shape and the monitors change together, and the event of the monitors is dispatched before the one of the shape
// (Windows can do that): the report is of a shape that the server does not have yet, so it waits for the DINF
void ServerProxyTests::monitorReport_monitorsEventBeforeTheShapeEvent_waitsForTheDinf()
{
  DestinationLink link(kTwoMonitorsShape, kTwoMonitors);
  link.sourceSays(kLayoutCapabilityMonitors);
  (void)link.written();

  const MonitorRect threeShape{0, 0, 6400, 1440};
  link.proxy->setLocal(
      threeShape,
      {{{0, 0, 2560, 1440}, 150, true}, {{2560, 0, 1920, 1080}, 100, false}, {{4480, 0, 1920, 1440}, 100, false}}
  );
  link.proxy->tellMonitorsChanged();
  // nothing: a report of 0,0 6400x1440 would be thrown away by a server that has the DINF of 0,0 4480x1440
  QCOMPARE(link.written(), QByteArray());

  // the DINF of the new shape, and the report right behind it
  link.proxy->onInfoChanged();
  QCOMPARE(
      link.written(),
      infoBytes(threeShape) +
          reportBytes(
              2, {{0, 0, 2560, 1440, 150, true}, {2560, 0, 1920, 1080, 100, false}, {4480, 0, 1920, 1440, 100, false}}
          )
  );
}

void ServerProxyTests::monitorReport_monitorsThatDoNotTileTheShape_areNotReportedAndForgotten()
{
  CapturingOutputter outputter;
  m_log.insert(&outputter, true);

  // the platform tells monitors that do not make the shape that it reports: no report, and one line that says why
  DestinationLink link(kTwoMonitorsShape, {{{0, 0, 2560, 1440}, 100, true}});
  link.sourceSays(kLayoutCapabilityMonitors);
  QCOMPARE(link.written(), capabilitiesReply(legacyDestinationMask() | kLayoutCapabilityMonitors));
  link.proxy->tellMonitorsChanged();
  QCOMPARE(link.written(), QByteArray());
  QVERIFY(outputter.hasInfoLineContaining(QStringLiteral("monitors are not reported")));
  QCOMPARE(outputter.infoLineCount(QStringLiteral("monitors are not reported")), 1);

  // they tile it now: reported
  link.proxy->setLocal(kTwoMonitorsShape, kTwoMonitors);
  link.proxy->tellMonitorsChanged();
  QCOMPARE(link.written(), reportBytes(1, kTwoMonitorsOnTheWire));

  // and not any more: the server is told that there are none to use, once
  link.proxy->setLocal(kTwoMonitorsShape, {{{0, 0, 1920, 1080}, 100, true}});
  link.proxy->tellMonitorsChanged();
  QCOMPARE(link.written(), reportBytes(2, {}));
  link.proxy->tellMonitorsChanged();
  QCOMPARE(link.written(), QByteArray());

  m_log.remove(&outputter);
}

void ServerProxyTests::monitorReport_matrix_data()
{
  QTest::addColumn<bool>("experiment");
  QTest::addColumn<bool>("clientCanReport");
  QTest::addColumn<bool>("reportReachesTheServer");

  QTest::newRow("new server, new client") << true << true << true;
  QTest::newRow("new server, a client that cannot list its monitors") << true << false << false;
  QTest::newRow("experiment off (or a server before this one), new client") << false << true << false;
  QTest::newRow("experiment off, a client that cannot list its monitors") << false << false << false;
}

// the two real proxies, one against the other, byte by byte: the report is on the wire exactly when both ends have the
// bit, and a link that has not negotiated it carries nothing of it
void ServerProxyTests::monitorReport_matrix()
{
  QFETCH(bool, experiment);
  QFETCH(bool, clientCanReport);
  QFETCH(bool, reportReachesTheServer);

  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, experiment);

  // the proxy of the server owns its stream
  RecordingEventQueue serverEvents;
  auto *serverStreamOwned = new FakeStream;
  FakeStream &serverStream = *serverStreamOwned;
  ClientProxy1_9 serverSide("client", serverStreamOwned, reinterpret_cast<Server *>(0x1), &serverEvents);

  RecordingEventQueue clientEvents;
  FakeStream clientStream;
  TestServerProxy1_9 clientSide(
      undereferenceableClient(), &clientStream, &clientEvents, deskflow::platform::currentInputCapabilities()
  );
  clientSide.setLocal(kTwoMonitorsShape, clientCanReport ? kTwoMonitors : std::vector<MonitorInfo>{});

  QByteArray toServer;
  for (int round = 0; round < 4; ++round) {
    const auto toClient = serverStream.takeOutput();
    if (!toClient.isEmpty()) {
      clientStream.push(stdString(toClient));
      clientEvents.dispatchEvent(Event(EventTypes::StreamInputReady, clientStream.getEventTarget()));
    }
    const auto answer = clientStream.takeOutput();
    toServer += answer;
    if (!answer.isEmpty()) {
      serverStream.push(stdString(answer));
      serverEvents.dispatchEvent(Event(EventTypes::StreamInputReady, serverStream.getEventTarget()));
    }
  }

  QCOMPARE(toServer.contains("DMON"), reportReachesTheServer);
  QCOMPARE(serverSide.reportedMonitors() != nullptr, reportReachesTheServer);
  if (reportReachesTheServer) {
    QCOMPARE(serverSide.reportedMonitors()->monitors.size(), static_cast<size_t>(2));
    // the proxy of the server said that the monitors were reported
    QVERIFY(std::ranges::any_of(serverEvents.addedEvents(), [](const Event &event) {
      return event.getType() == EventTypes::ClientProxyMonitorsReported;
    }));
  }
  // the capability mask in the CIPC that went to the server: the bit is there only when it is reported
  const auto cipc = toServer.indexOf("CIPC");
  QVERIFY(cipc >= 0);
  // the code, the platform byte, then the mask in four bytes: bit 12 is bit 4 of the third of them
  const auto thirdByteOfMask = static_cast<uint8_t>(toServer[cipc + 4 + 1 + 2]);
  QCOMPARE((thirdByteOfMask & 0x10) != 0, reportReachesTheServer);

  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
}

QTEST_MAIN(ServerProxyTests)
