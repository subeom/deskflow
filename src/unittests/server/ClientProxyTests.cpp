/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientProxyTests.h"

#include "../deskflow/MockEventQueue.h"
#include "common/ClipboardCapabilities.h"
#include "common/InputCapabilities.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "deskflow/AppUtil.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"
#include "server/ClientProxy1_0.h"
#include "server/ClientProxy1_1.h"
#include "server/ClientProxy1_3.h"
#include "server/ClientProxy1_6.h"
#include "server/ClientProxy1_7.h"
#include "server/ClientProxy1_8.h"
#include "server/ClientProxy1_9.h"

#include <array>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <QByteArray>
#include <QTemporaryDir>
#include <QTest>

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

class CapturingStream : public deskflow::IStream
{
public:
  void pushInput(const QByteArray &bytes)
  {
    m_input.push_back(bytes);
  }

  QByteArray take()
  {
    auto bytes = m_buffer;
    m_buffer.clear();
    return bytes;
  }

  void write(const void *buffer, uint32_t n) override
  {
    m_buffer.append(static_cast<const char *>(buffer), n);
  }

  void close() override
  {
    m_input.clear();
  }

  uint32_t read(void *buffer, uint32_t size) override
  {
    if (m_input.empty() || size == 0) {
      return 0;
    }

    auto &front = m_input.front();
    const auto bytesToRead = std::min(static_cast<qsizetype>(size), front.size());
    if (buffer != nullptr) {
      std::memcpy(buffer, front.constData(), static_cast<size_t>(bytesToRead));
    }
    front.remove(0, bytesToRead);
    if (front.isEmpty()) {
      m_input.pop_front();
    }
    return static_cast<uint32_t>(bytesToRead);
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
    return const_cast<CapturingStream *>(this);
  }

  bool isReady() const override
  {
    return !m_input.empty();
  }

  uint32_t getSize() const override
  {
    qsizetype size = 0;
    for (const auto &chunk : m_input) {
      size += chunk.size();
    }
    return static_cast<uint32_t>(size);
  }

private:
  QByteArray m_buffer;
  std::deque<QByteArray> m_input;
};

std::unique_ptr<ClientProxy> makeProxy(int minor, deskflow::IStream *stream, IEventQueue *events)
{
  // the 1.4 and later constructors assert the server pointer is non-null but only store it
  auto *server = reinterpret_cast<Server *>(0x1);

  std::unique_ptr<ClientProxy> proxy;
  switch (minor) {
  case 0:
    proxy = std::make_unique<ClientProxy1_0>("client", stream, events);
    break;
  case 1:
    proxy = std::make_unique<ClientProxy1_1>("client", stream, events);
    break;
  case 3:
    proxy = std::make_unique<ClientProxy1_3>("client", stream, events);
    break;
  case 6:
    proxy = std::make_unique<ClientProxy1_6>("client", stream, server, events);
    break;
  case 7:
    proxy = std::make_unique<ClientProxy1_7>("client", stream, server, events);
    break;
  case 8:
    proxy = std::make_unique<ClientProxy1_8>("client", stream, server, events);
    break;
  case 9:
    proxy = std::make_unique<ClientProxy1_9>("client", stream, server, events);
    break;
  default:
    break;
  }
  return proxy;
}

struct ProxyUnderTest
{
  MockEventQueue events;
  CapturingStream *stream = new CapturingStream;
  std::unique_ptr<ClientProxy> proxy;

  explicit ProxyUnderTest(int minor) : proxy(makeProxy(minor, stream, &events))
  {
    // drop the query-info and layout-sync messages the constructors send
    stream->take();
  }
};

const KeyID kKey = 0x61;
const KeyModifierMask kMask = 0;
const KeyButton kButton = 0x1e;
const int32_t kCount = 3;
const std::string kLang = "en";

// KeyButtons are macOS virtual key codes plus one: Left Command, Right Command and F18
const KeyButton kLeftCommandButton = 0x38;
const KeyButton kRightCommandButton = 0x37;
const KeyButton kF18Button = 0x50;

class TestClientProxy1_9 : public ClientProxy1_9
{
public:
  TestClientProxy1_9(
      const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events,
      InputPlatform sourcePlatform = InputPlatform::MacOS
  )
      : ClientProxy1_9(name, stream, server, events),
        m_sourcePlatform(sourcePlatform)
  {
  }

  bool parseHandshakeMessageForTest(const uint8_t *code)
  {
    return parseHandshakeMessage(code);
  }

  bool parseMessageForTest(const uint8_t *code)
  {
    return parseMessage(code);
  }

protected:
  InputCapabilities localInputCapabilities() const override
  {
    return {m_sourcePlatform, kInputCapabilityProtocolV19};
  }

private:
  InputPlatform m_sourcePlatform;
};

class RecordingEventQueue : public MockEventQueue
{
public:
  void addEvent(Event &&event) override
  {
    if (event.getType() == EventTypes::ClientProxyReady) {
      ++m_readyEvents;
    }
  }

  int readyEvents() const
  {
    return m_readyEvents;
  }

private:
  int m_readyEvents = 0;
};

// A protocol 1.9 proxy on a source of the given platform (macOS unless said otherwise) whose destination has
// already reported \p destination
struct V19Link
{
  RecordingEventQueue events;
  CapturingStream *stream = new CapturingStream;
  std::unique_ptr<TestClientProxy1_9> proxy;
  bool destinationParsed = false;

  explicit V19Link(InputCapabilities destination, InputPlatform source = InputPlatform::MacOS)
  {
    auto *server = reinterpret_cast<Server *>(0x1);
    proxy = std::make_unique<TestClientProxy1_9>("client", stream, server, &events, source);
    (void)stream->take();

    CapturingStream packet;
    ProtocolUtil::writef(
        &packet, kMsgCInputCapabilities, static_cast<uint8_t>(destination.platform), destination.capabilities
    );
    stream->pushInput(packet.take());
    stream->read(nullptr, 4);
    destinationParsed = proxy->parseHandshakeMessageForTest(reinterpret_cast<const uint8_t *>(kMsgCInputCapabilities));
  }

  // what the proxy wrote since the last call
  QByteArray written() const
  {
    return stream->take();
  }
};

const InputCapabilities kWindowsWithScroll{
    InputPlatform::Windows, kInputCapabilityProtocolV19 | kInputCapabilityCrossOSScroll
};

const InputCapabilities kWindowsWithHangul{
    InputPlatform::Windows, kInputCapabilityProtocolV19 | kInputCapabilityHangul
};

constexpr OptionValue kHangulAction = static_cast<OptionValue>(F18Action::Hangul);

enum class KeyEvent
{
  Down,
  Repeat,
  Up
};

// The message a server wrote for a key event before this feature existed, per negotiated protocol minor.  These
// formats are frozen: shipped clients parse them byte for byte, and they are spelled out here independently of the
// proxies so that a change in a proxy cannot move its own expectation.
QByteArray
originalKeyBytes(int minor, KeyEvent event, KeyID id, KeyModifierMask mask, KeyButton button, int32_t count = kCount)
{
  CapturingStream expected;
  if (minor == 0) {
    switch (event) {
    case KeyEvent::Down:
      ProtocolUtil::writef(&expected, kMsgDKeyDown1_0, id, mask);
      break;
    case KeyEvent::Repeat:
      ProtocolUtil::writef(&expected, kMsgDKeyRepeat1_0, id, mask, count);
      break;
    case KeyEvent::Up:
      ProtocolUtil::writef(&expected, kMsgDKeyUp1_0, id, mask);
      break;
    }
  } else if (minor < 8) {
    switch (event) {
    case KeyEvent::Down:
      ProtocolUtil::writef(&expected, kMsgDKeyDown1_1, id, mask, button);
      break;
    case KeyEvent::Repeat:
      ProtocolUtil::writef(&expected, kMsgDKeyRepeat1_1, id, mask, count, button);
      break;
    case KeyEvent::Up:
      ProtocolUtil::writef(&expected, kMsgDKeyUp, id, mask, button);
      break;
    }
  } else {
    switch (event) {
    case KeyEvent::Down:
      ProtocolUtil::writef(&expected, kMsgDKeyDown, id, mask, button, &kLang);
      break;
    case KeyEvent::Repeat:
      ProtocolUtil::writef(&expected, kMsgDKeyRepeat, id, mask, count, button, &kLang);
      break;
    case KeyEvent::Up:
      ProtocolUtil::writef(&expected, kMsgDKeyUp, id, mask, button);
      break;
    }
  }
  return expected.take();
}

// Feeds one key event to \p proxy
void sendKeyEvent(ClientProxy &proxy, KeyEvent event, KeyID id, KeyModifierMask mask, KeyButton button)
{
  switch (event) {
  case KeyEvent::Down:
    proxy.keyDown(id, mask, button, kLang);
    break;
  case KeyEvent::Repeat:
    proxy.keyRepeat(id, mask, kCount, button, kLang);
    break;
  case KeyEvent::Up:
    proxy.keyUp(id, mask, button);
    break;
  }
}

// What a macOS source hands to the proxy for one physical key, exactly as the server produces it
// (OSXKeyState::mapKeyFromEvent -> KeyState::sendKeyEvent -> Server::onKeyDown/onKeyRepeat/onKeyUp): a press and every
// auto-repeat carry the key's own id, an auto-repeat has a count of 1, and the release of an ordinary key carries
// kKeyNone ("the id isn't used, we just need the same button"), so only its KeyButton says which key went up.
// Modifier keys are reported by OSXKeyState::handleModifierKey instead and release with their real id.
class MacSource
{
public:
  explicit MacSource(ClientProxy &proxy) : m_proxy(proxy)
  {
  }

  void press(KeyID id, KeyButton button, KeyModifierMask mask = 0)
  {
    m_proxy.keyDown(id, mask, button, kLang);
  }

  void autoRepeat(KeyID id, KeyButton button, KeyModifierMask mask = 0)
  {
    m_proxy.keyRepeat(id, mask, 1, button, kLang);
  }

  void release(KeyButton button, KeyModifierMask mask = 0)
  {
    m_proxy.keyUp(kKeyNone, mask, button);
  }

  void releaseModifier(KeyID id, KeyButton button, KeyModifierMask mask = 0)
  {
    m_proxy.keyUp(id, mask, button);
  }

private:
  ClientProxy &m_proxy;
};

// The id a macOS source puts on \p event of the ordinary key \p id
KeyID macKeyId(KeyEvent event, KeyID id)
{
  return event == KeyEvent::Up ? kKeyNone : id;
}

QByteArray hangulDownBytes(KeyModifierMask mask = 0)
{
  return originalKeyBytes(9, KeyEvent::Down, kKeyHangul, mask, kF18Button);
}

QByteArray hangulUpBytes(KeyModifierMask mask = 0)
{
  return originalKeyBytes(9, KeyEvent::Up, kKeyHangul, mask, kF18Button);
}

// Turns the F18 option on and drops the option message it caused
void enableF18Hangul(V19Link &link)
{
  link.proxy->setOptions({kOptionCrossOSF18Key, kHangulAction});
  (void)link.written();
}

QByteArray wheelBytes(int32_t xDelta, int32_t yDelta)
{
  CapturingStream expected;
  ProtocolUtil::writef(&expected, kMsgDMouseWheel, xDelta, yDelta);
  return expected.take();
}

QByteArray optionsBytes(const OptionsList &options)
{
  CapturingStream expected;
  ProtocolUtil::writef(&expected, kMsgDSetOptions, &options);
  return expected.take();
}

constexpr OptionValue kMouseWheelOnly = static_cast<OptionValue>(ScrollInvertScope::MouseWheelOnly);

} // namespace

void ClientProxyTests::initTestCase()
{
  // the 1.8 constructor reads the keyboard layouts through AppUtil::instance()
  static TestAppUtil appUtil;
}

// These formats are frozen because shipped third-party clients parse them byte
// for byte: Synergy 1.4 through 1.14.1 negotiate 1.4 through 1.7, Barrier and
// Input Leap negotiate 1.6, and Synergy 1.14.2 onwards and Deskflow negotiate 1.8.
void ClientProxyTests::keyDown_data()
{
  QTest::addColumn<int>("minor");
  QTest::addColumn<QByteArray>("expected");

  QTest::newRow("1.0") << 0 << "DKDN" + QByteArray::fromHex("0061 0000");
  QTest::newRow("1.1") << 1 << "DKDN" + QByteArray::fromHex("0061 0000 001e");
  QTest::newRow("1.6") << 6 << "DKDN" + QByteArray::fromHex("0061 0000 001e");
  QTest::newRow("1.7") << 7 << "DKDN" + QByteArray::fromHex("0061 0000 001e");
  QTest::newRow("1.8") << 8 << "DKDL" + QByteArray::fromHex("0061 0000 001e 00000002") + "en";
  QTest::newRow("1.9") << 9 << "DKDL" + QByteArray::fromHex("0061 0000 001e 00000002") + "en";
}

void ClientProxyTests::keyDown()
{
  QFETCH(int, minor);
  QFETCH(QByteArray, expected);

  ProxyUnderTest test(minor);
  test.proxy->keyDown(kKey, kMask, kButton, kLang);
  QCOMPARE(test.stream->take(), expected);
}

void ClientProxyTests::keyRepeat_data()
{
  QTest::addColumn<int>("minor");
  QTest::addColumn<QByteArray>("expected");

  QTest::newRow("1.0") << 0 << "DKRP" + QByteArray::fromHex("0061 0000 0003");
  QTest::newRow("1.1") << 1 << "DKRP" + QByteArray::fromHex("0061 0000 0003 001e");
  QTest::newRow("1.6") << 6 << "DKRP" + QByteArray::fromHex("0061 0000 0003 001e");
  QTest::newRow("1.7") << 7 << "DKRP" + QByteArray::fromHex("0061 0000 0003 001e");
  QTest::newRow("1.8") << 8 << "DKRP" + QByteArray::fromHex("0061 0000 0003 001e 00000002") + "en";
  QTest::newRow("1.9") << 9 << "DKRP" + QByteArray::fromHex("0061 0000 0003 001e 00000002") + "en";
}

void ClientProxyTests::keyRepeat()
{
  QFETCH(int, minor);
  QFETCH(QByteArray, expected);

  ProxyUnderTest test(minor);
  test.proxy->keyRepeat(kKey, kMask, kCount, kButton, kLang);
  QCOMPARE(test.stream->take(), expected);
}

void ClientProxyTests::keyUp_data()
{
  QTest::addColumn<int>("minor");
  QTest::addColumn<QByteArray>("expected");

  QTest::newRow("1.0") << 0 << "DKUP" + QByteArray::fromHex("0061 0000");
  QTest::newRow("1.1") << 1 << "DKUP" + QByteArray::fromHex("0061 0000 001e");
  QTest::newRow("1.6") << 6 << "DKUP" + QByteArray::fromHex("0061 0000 001e");
  QTest::newRow("1.7") << 7 << "DKUP" + QByteArray::fromHex("0061 0000 001e");
  QTest::newRow("1.8") << 8 << "DKUP" + QByteArray::fromHex("0061 0000 001e");
  QTest::newRow("1.9") << 9 << "DKUP" + QByteArray::fromHex("0061 0000 001e");
}

void ClientProxyTests::keyUp()
{
  QFETCH(int, minor);
  QFETCH(QByteArray, expected);

  ProxyUnderTest test(minor);
  test.proxy->keyUp(kKey, kMask, kButton);
  QCOMPARE(test.stream->take(), expected);
}

void ClientProxyTests::inputCapabilities_v19_sendsSourcePlatformAndCapabilities()
{
  MockEventQueue events;
  auto *stream = new CapturingStream;
  auto *server = reinterpret_cast<Server *>(0x1);
  ClientProxy1_9 proxy("client", stream, server, &events);

  CapturingStream expected;
  ProtocolUtil::writef(
      &expected, kMsgDInputCapabilities, static_cast<uint8_t>(deskflow::platform::currentInputPlatform()),
      deskflow::platform::currentInputCapabilities().capabilities | kClipboardCapabilityPaced |
          kClipboardCapabilitySingleId
  );
  const auto actual = stream->take();
  const auto expectedBytes = expected.take();
  QVERIFY(actual.endsWith(expectedBytes));
}

// The experiment that maps the pointer around the holes of a desktop is the Server's alone: a computer is not told of
// it, so the first bytes that it gets are the same, with it on and with it off, for every protocol that has a handshake
void ClientProxyTests::handshake_monitorExperiment_changesNoByteOnTheWire()
{
  QTemporaryDir settingsDir;
  QVERIFY(settingsDir.isValid());
  Settings::setSettingsFile(settingsDir.filePath(QStringLiteral("Deskflow.conf")));

  const auto handshake = [](bool version19) {
    MockEventQueue events;
    auto *stream = new CapturingStream;
    auto *server = reinterpret_cast<Server *>(0x1);
    if (version19) {
      ClientProxy1_9 proxy("client", stream, server, &events);
      return stream->take();
    }
    ClientProxy1_8 proxy("client", stream, server, &events);
    return stream->take();
  };

  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, false);
  const auto off19 = handshake(true);
  const auto off18 = handshake(false);

  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(
      Settings::Computer::MonitorLayout.arg(QStringLiteral("client")), QStringLiteral("0,0,2560,1440;2560,0,1920,1080")
  );
  const auto on19 = handshake(true);
  const auto on18 = handshake(false);

  QVERIFY(!off19.isEmpty());
  QCOMPARE(on19, off19);
  QCOMPARE(on18, off18);
  // there is no message for monitors: a computer that was sent one would drop what is buffered after it
  QVERIFY(!on19.contains("DMON"));
  QVERIFY(!on19.contains("CMON"));
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  Settings::setValue(Settings::Computer::MonitorLayout.arg(QStringLiteral("client")));
}

void ClientProxyTests::inputCapabilities_v18_doesNotSendCapabilityMessage()
{
  MockEventQueue events;
  auto *stream = new CapturingStream;
  auto *server = reinterpret_cast<Server *>(0x1);
  ClientProxy1_8 proxy("client", stream, server, &events);

  QVERIFY(!stream->take().contains(QByteArray::fromRawData(kMsgDInputCapabilities, 4)));
}

void ClientProxyTests::inputCapabilities_v19_parsesDestinationCapabilities()
{
  MockEventQueue events;
  auto *stream = new CapturingStream;
  auto *server = reinterpret_cast<Server *>(0x1);
  TestClientProxy1_9 proxy("client", stream, server, &events);
  (void)stream->take();

  constexpr InputCapabilities expected{InputPlatform::Windows, 0xaabbccdd};
  CapturingStream packet;
  ProtocolUtil::writef(
      &packet, kMsgCInputCapabilities, static_cast<uint8_t>(expected.platform),
      static_cast<uint32_t>(expected.capabilities)
  );
  stream->pushInput(packet.take());

  stream->read(nullptr, 4);
  QVERIFY(proxy.parseHandshakeMessageForTest(reinterpret_cast<const uint8_t *>(kMsgCInputCapabilities)));
  QCOMPARE(
      static_cast<uint8_t>(proxy.destinationInputCapabilities().platform), static_cast<uint8_t>(expected.platform)
  );
  QCOMPARE(proxy.destinationInputCapabilities().capabilities, expected.capabilities);
}

void ClientProxyTests::inputCapabilities_v19_waitsForCapabilitiesBeforeReady()
{
  RecordingEventQueue events;
  auto *stream = new CapturingStream;
  auto *server = reinterpret_cast<Server *>(0x1);
  TestClientProxy1_9 proxy("client", stream, server, &events);
  (void)stream->take();

  CapturingStream infoPacket;
  ProtocolUtil::writef(
      &infoPacket, kMsgDInfo, static_cast<int16_t>(0), static_cast<int16_t>(0), static_cast<int16_t>(100),
      static_cast<int16_t>(100), static_cast<int16_t>(0), static_cast<int16_t>(50), static_cast<int16_t>(50)
  );
  stream->pushInput(infoPacket.take());
  uint8_t infoCode[4];
  QCOMPARE(stream->read(infoCode, 4), static_cast<uint32_t>(4));
  QVERIFY(proxy.parseHandshakeMessageForTest(infoCode));
  QCOMPARE(events.readyEvents(), 0);

  const InputCapabilities capabilities{InputPlatform::Windows, kInputCapabilityProtocolV19};
  CapturingStream capabilitiesPacket;
  ProtocolUtil::writef(
      &capabilitiesPacket, kMsgCInputCapabilities, static_cast<uint8_t>(capabilities.platform),
      capabilities.capabilities
  );
  stream->pushInput(capabilitiesPacket.take());
  uint8_t capabilityCode[4];
  QCOMPARE(stream->read(capabilityCode, 4), static_cast<uint32_t>(4));
  QVERIFY(proxy.parseMessageForTest(capabilityCode));
  QCOMPARE(events.readyEvents(), 1);
}

void ClientProxyTests::mouseWheel_data()
{
  QTest::addColumn<int>("minor");
  QTest::addColumn<QByteArray>("expected");

  const auto expected = "DMWM" + QByteArray::fromHex("0078 ff88");
  QTest::newRow("1.3") << 3 << expected;
  QTest::newRow("1.8") << 8 << expected;
  QTest::newRow("1.9") << 9 << expected;
}

void ClientProxyTests::mouseWheel()
{
  QFETCH(int, minor);
  QFETCH(QByteArray, expected);

  ProxyUnderTest test(minor);
  test.proxy->mouseWheel(120, -120);
  QCOMPARE(test.stream->take(), expected);
}

void ClientProxyTests::scrollSource_legacyProxiesIgnoreSource_data()
{
  QTest::addColumn<int>("minor");
  QTest::addColumn<QByteArray>("expected");

  // 1.0 only carries the vertical delta; every later proxy carries both
  QTest::newRow("1.0") << 0 << "DMWM" + QByteArray::fromHex("ff88");
  const auto expected = "DMWM" + QByteArray::fromHex("0078 ff88");
  QTest::newRow("1.3") << 3 << expected;
  QTest::newRow("1.8") << 8 << expected;
  // a 1.9 proxy that has not seen the destination's capabilities cannot map anything
  QTest::newRow("1.9 without destination capabilities") << 9 << expected;
}

void ClientProxyTests::scrollSource_legacyProxiesIgnoreSource()
{
  QFETCH(int, minor);
  QFETCH(QByteArray, expected);

  for (const auto source : {ScrollSource::Unknown, ScrollSource::Mouse, ScrollSource::Trackpad}) {
    ProxyUnderTest test(minor);
    test.proxy->mouseWheel(120, -120, source);
    QCOMPARE(test.stream->take(), expected);
  }
}

void ClientProxyTests::scrollScope_all_forwardsOptionsAndKeepsLegacyWheelBytes()
{
  V19Link link(kWindowsWithScroll);
  QVERIFY(link.destinationParsed);

  // the destination keeps inverting in this scope, so the options it needs are forwarded untouched
  const OptionsList options{kOptionClipboardSharing, 1, kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 1};
  link.proxy->setOptions(options);
  QCOMPARE(link.written(), optionsBytes(options));

  const auto legacy = "DMWM" + QByteArray::fromHex("0078 ff88");
  QCOMPARE(wheelBytes(120, -120), legacy);
  for (const auto source : {ScrollSource::Unknown, ScrollSource::Mouse, ScrollSource::Trackpad}) {
    link.proxy->mouseWheel(120, -120, source);
    QCOMPARE(link.written(), legacy);
  }
  link.proxy->mouseWheel(120, -120);
  QCOMPARE(link.written(), legacy);
}

void ClientProxyTests::scrollScope_mouseWheelOnly_invertsOnlyMouseEvents()
{
  V19Link link(kWindowsWithScroll);
  QVERIFY(link.destinationParsed);

  link.proxy->setOptions(
      {kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 1, kOptionCrossOSInvertScope, kMouseWheelOnly}
  );
  (void)link.written();

  link.proxy->mouseWheel(120, -120, ScrollSource::Mouse);
  QCOMPARE(link.written(), wheelBytes(-120, 120));

  link.proxy->mouseWheel(120, -120, ScrollSource::Trackpad);
  QCOMPARE(link.written(), wheelBytes(120, -120));

  link.proxy->mouseWheel(120, -120, ScrollSource::Unknown);
  QCOMPARE(link.written(), wheelBytes(120, -120));

  // a caller that does not know the device (the two argument overload) is never inverted
  link.proxy->mouseWheel(120, -120);
  QCOMPARE(link.written(), wheelBytes(120, -120));
}

void ClientProxyTests::scrollScope_mouseWheelOnly_invertsOnlyConfiguredAxes()
{
  V19Link link(kWindowsWithScroll);
  QVERIFY(link.destinationParsed);

  link.proxy->resetOptions();
  link.proxy->setOptions({kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScope, kMouseWheelOnly});
  (void)link.written();
  link.proxy->mouseWheel(120, -120, ScrollSource::Mouse);
  QCOMPARE(link.written(), wheelBytes(-120, -120));

  link.proxy->resetOptions();
  link.proxy->setOptions({kOptionCrossOSInvertScrollY, 1, kOptionCrossOSInvertScope, kMouseWheelOnly});
  (void)link.written();
  link.proxy->mouseWheel(120, -120, ScrollSource::Mouse);
  QCOMPARE(link.written(), wheelBytes(120, 120));

  link.proxy->resetOptions();
  link.proxy->setOptions({kOptionCrossOSInvertScope, kMouseWheelOnly});
  (void)link.written();
  link.proxy->mouseWheel(120, -120, ScrollSource::Mouse);
  QCOMPARE(link.written(), wheelBytes(120, -120));
}

void ClientProxyTests::scrollScope_mouseWheelOnly_withholdsInvertOptionsFromDestination()
{
  V19Link link(kWindowsWithScroll);
  QVERIFY(link.destinationParsed);

  // A destination that predates the scope would invert every packet if it were told to, trackpad included,
  // so the source keeps the invert flags to itself.  Everything else is forwarded in its original order.
  link.proxy->setOptions({
      kOptionClipboardSharing,
      1,
      kOptionCrossOSInvertScrollX,
      1,
      kOptionCrossOSInvertScope,
      kMouseWheelOnly,
      kOptionCrossOSInvertScrollY,
      1,
      kOptionCrossOSSemanticMedia,
      1,
  });
  const OptionsList forwarded{kOptionClipboardSharing,     1, kOptionCrossOSInvertScope, kMouseWheelOnly,
                              kOptionCrossOSSemanticMedia, 1};
  QCOMPARE(link.written(), optionsBytes(forwarded));
}

void ClientProxyTests::scrollScope_resetOptions_restoresDestinationInversion()
{
  V19Link link(kWindowsWithScroll);
  QVERIFY(link.destinationParsed);

  link.proxy->setOptions(
      {kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 1, kOptionCrossOSInvertScope, kMouseWheelOnly}
  );
  link.proxy->resetOptions();
  (void)link.written();

  // back to the default scope: the flags reach the destination again and the source leaves the wheel alone
  const OptionsList options{kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 1};
  link.proxy->setOptions(options);
  QCOMPARE(link.written(), optionsBytes(options));
  link.proxy->mouseWheel(120, -120, ScrollSource::Mouse);
  QCOMPARE(link.written(), wheelBytes(120, -120));
}

void ClientProxyTests::scrollScope_ineligibleLinks_keepLegacyBytes_data()
{
  QTest::addColumn<int>("platform");
  QTest::addColumn<uint32_t>("capabilities");

  constexpr uint32_t withScroll = kInputCapabilityProtocolV19 | kInputCapabilityCrossOSScroll;
  QTest::newRow("windows without the scroll capability")
      << static_cast<int>(InputPlatform::Windows) << static_cast<uint32_t>(kInputCapabilityProtocolV19);
  QTest::newRow("macOS destination") << static_cast<int>(InputPlatform::MacOS) << withScroll;
  QTest::newRow("linux destination") << static_cast<int>(InputPlatform::Linux) << withScroll;
  QTest::newRow("unknown destination") << static_cast<int>(InputPlatform::Unknown) << withScroll;
}

void ClientProxyTests::scrollScope_ineligibleLinks_keepLegacyBytes()
{
  QFETCH(int, platform);
  QFETCH(uint32_t, capabilities);

  V19Link link({static_cast<InputPlatform>(platform), capabilities});
  QVERIFY(link.destinationParsed);

  link.proxy->setOptions(
      {kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 1, kOptionCrossOSInvertScope, kMouseWheelOnly}
  );
  (void)link.written();
  for (const auto source : {ScrollSource::Unknown, ScrollSource::Mouse, ScrollSource::Trackpad}) {
    link.proxy->mouseWheel(120, -120, source);
    QCOMPARE(link.written(), wheelBytes(120, -120));
  }
}

void ClientProxyTests::rightCommand_bytesMatchOriginalServer_data()
{
  QTest::addColumn<int>("minor");
  QTest::addColumn<int>("destinationPlatform");
  QTest::addColumn<uint32_t>("destinationCapabilities");
  QTest::addColumn<bool>("everyOptionOn");

  constexpr auto windows = static_cast<int>(InputPlatform::Windows);
  constexpr uint32_t everything = kInputCapabilityProtocolV19 | kInputCapabilityHangul | kInputCapabilityCrossOSScroll |
                                  kInputCapabilitySemanticMedia;

  for (const auto minor : {0, 1, 3, 6, 7, 8}) {
    QTest::addRow("1.%d", minor) << minor << windows << uint32_t{0} << false;
  }
  QTest::newRow("1.9, windows with every capability, options off") << 9 << windows << everything << false;
  QTest::newRow("1.9, windows with every capability, every option on") << 9 << windows << everything << true;
  QTest::newRow("1.9, windows without capabilities, every option on")
      << 9 << windows << static_cast<uint32_t>(kInputCapabilityProtocolV19) << true;
  QTest::newRow("1.9, macOS destination, every option on")
      << 9 << static_cast<int>(InputPlatform::MacOS) << everything << true;
  QTest::newRow("1.9, linux destination, every option on")
      << 9 << static_cast<int>(InputPlatform::Linux) << everything << true;
  QTest::newRow("1.9, unknown destination, every option on")
      << 9 << static_cast<int>(InputPlatform::Unknown) << everything << true;
}

// Command is the Windows key on every link: whatever Left or Right Command events reach a proxy are written exactly
// as they were before the cross-OS input feature existed, with no Right-to-Left merging, no state of its own, and no
// effect of any cross-OS option.
void ClientProxyTests::rightCommand_bytesMatchOriginalServer()
{
  QFETCH(int, minor);
  QFETCH(int, destinationPlatform);
  QFETCH(uint32_t, destinationCapabilities);
  QFETCH(bool, everyOptionOn);

  std::unique_ptr<ProxyUnderTest> legacy;
  std::unique_ptr<V19Link> link;
  ClientProxy *proxy = nullptr;
  CapturingStream *stream = nullptr;
  if (minor == 9) {
    link = std::make_unique<V19Link>(
        InputCapabilities{static_cast<InputPlatform>(destinationPlatform), destinationCapabilities}
    );
    QVERIFY(link->destinationParsed);
    proxy = link->proxy.get();
    stream = link->stream;
    if (everyOptionOn) {
      proxy->setOptions(
          {kOptionCrossOSF18Key, kHangulAction, kOptionCrossOSInvertScrollX, 1, kOptionCrossOSInvertScrollY, 1,
           kOptionCrossOSSemanticMedia, 1}
      );
    }
  } else {
    legacy = std::make_unique<ProxyUnderTest>(minor);
    proxy = legacy->proxy.get();
    stream = legacy->stream;
  }
  (void)stream->take();

  struct Step
  {
    KeyEvent event;
    KeyID id;
    KeyButton button;
  };
  // Right Command alone, then both Command keys held in both orders, then Left Command alone
  const std::array steps = {
      Step{KeyEvent::Down, kKeySuper_R, kRightCommandButton}, Step{KeyEvent::Repeat, kKeySuper_R, kRightCommandButton},
      Step{KeyEvent::Up, kKeySuper_R, kRightCommandButton},   Step{KeyEvent::Down, kKeySuper_R, kRightCommandButton},
      Step{KeyEvent::Down, kKeySuper_L, kLeftCommandButton},  Step{KeyEvent::Repeat, kKeySuper_L, kLeftCommandButton},
      Step{KeyEvent::Up, kKeySuper_R, kRightCommandButton},   Step{KeyEvent::Up, kKeySuper_L, kLeftCommandButton},
      Step{KeyEvent::Down, kKeySuper_L, kLeftCommandButton},  Step{KeyEvent::Down, kKeySuper_R, kRightCommandButton},
      Step{KeyEvent::Up, kKeySuper_L, kLeftCommandButton},    Step{KeyEvent::Up, kKeySuper_R, kRightCommandButton},
  };
  for (const auto &step : steps) {
    sendKeyEvent(*proxy, step.event, step.id, KeyModifierSuper, step.button);
    QCOMPARE(stream->take(), originalKeyBytes(minor, step.event, step.id, KeyModifierSuper, step.button));
  }

  // and the keys around them keep their repeat and their modifiers
  for (const auto event : {KeyEvent::Down, KeyEvent::Repeat, KeyEvent::Up}) {
    sendKeyEvent(*proxy, event, 'c', KeyModifierSuper, kButton);
    QCOMPARE(stream->take(), originalKeyBytes(minor, event, 'c', KeyModifierSuper, kButton));
  }
}

void ClientProxyTests::f18_macToWindows_sendsHangulWithSourceButtonAndModifiers()
{
  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  enableF18Hangul(link);
  MacSource mac(*link.proxy);

  // the source's KeyButton is what pairs the release with the press on the destination, so it travels unchanged,
  // and so do the modifiers that were held.  The release is the way macOS sends it: no key id, only the button
  constexpr KeyModifierMask held = KeyModifierShift | KeyModifierControl;
  mac.press(kKeyF18, kF18Button, held);
  QCOMPARE(link.written(), hangulDownBytes(held));
  mac.release(kF18Button, held);
  QCOMPARE(link.written(), hangulUpBytes(held));

  // a second press is a second toggle
  mac.press(kKeyF18, kF18Button);
  QCOMPARE(link.written(), hangulDownBytes());
  mac.release(kF18Button);
  QCOMPARE(link.written(), hangulUpBytes());
}

void ClientProxyTests::f18_consecutivePresses_eachToggleOnce()
{
  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  enableF18Hangul(link);
  MacSource mac(*link.proxy);

  // every tap of the key must toggle the destination's input mode, not just the first one after the cursor arrived
  for (const KeyModifierMask mask : {KeyModifierMask(0), KeyModifierShift, KeyModifierMask(0), KeyModifierSuper}) {
    mac.press(kKeyF18, kF18Button, mask);
    QCOMPARE(link.written(), hangulDownBytes(mask));
    mac.release(kF18Button, mask);
    QCOMPARE(link.written(), hangulUpBytes(mask));
  }
}

void ClientProxyTests::f18_heldKey_togglesOnceAndSuppressesRepeat()
{
  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  enableF18Hangul(link);
  MacSource mac(*link.proxy);

  mac.press(kKeyF18, kF18Button);
  QCOMPARE(link.written(), hangulDownBytes());

  // holding the key repeats it (with the key's own id), and a misreported second press of the held key counts as a
  // repeat: none of them may reach the destination, where each would toggle the input mode again
  for (int i = 0; i < 3; ++i) {
    mac.autoRepeat(kKeyF18, kF18Button);
    QVERIFY(link.written().isEmpty());
  }
  mac.press(kKeyF18, kF18Button);
  QVERIFY(link.written().isEmpty());

  mac.release(kF18Button);
  QCOMPARE(link.written(), hangulUpBytes());
}

void ClientProxyTests::f18_holdWithAutoRepeat_thenRelease_thenNewPress()
{
  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  enableF18Hangul(link);
  MacSource mac(*link.proxy);

  // however long the key was held, its release ends the hold and the next press toggles again
  for (const int repeats : {4, 0, 1}) {
    mac.press(kKeyF18, kF18Button, KeyModifierShift);
    QCOMPARE(link.written(), hangulDownBytes(KeyModifierShift));
    for (int i = 0; i < repeats; ++i) {
      mac.autoRepeat(kKeyF18, kF18Button, KeyModifierShift);
      QVERIFY(link.written().isEmpty());
    }
    mac.release(kF18Button, KeyModifierShift);
    QCOMPARE(link.written(), hangulUpBytes(KeyModifierShift));
  }
}

void ClientProxyTests::f18_releaseIsMatchedByButton_data()
{
  QTest::addColumn<KeyID>("releaseId");

  QTest::newRow("macOS: a release carries no key id") << kKeyNone;
  QTest::newRow("a release that still names F18") << kKeyF18;
}

void ClientProxyTests::f18_releaseIsMatchedByButton()
{
  QFETCH(KeyID, releaseId);

  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  enableF18Hangul(link);

  link.proxy->keyDown(kKeyF18, 0, kF18Button, kLang);
  QCOMPARE(link.written(), hangulDownBytes());
  link.proxy->keyUp(releaseId, 0, kF18Button);
  QCOMPARE(link.written(), hangulUpBytes());

  // the key is free for the next press
  link.proxy->keyDown(kKeyF18, 0, kF18Button, kLang);
  QCOMPARE(link.written(), hangulDownBytes());
  link.proxy->keyUp(releaseId, 0, kF18Button);
  QCOMPARE(link.written(), hangulUpBytes());
}

void ClientProxyTests::f18_optionTurnedOffMidHold_data()
{
  QTest::addColumn<bool>("reset");
  QTest::addColumn<int>("value");

  QTest::newRow("options reset") << true << 0;
  QTest::newRow("pass-through value") << false << static_cast<int>(F18Action::PassThrough);
  QTest::newRow("unknown value") << false << 7;
}

void ClientProxyTests::f18_optionTurnedOffMidHold()
{
  QFETCH(bool, reset);
  QFETCH(int, value);

  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  enableF18Hangul(link);
  MacSource mac(*link.proxy);

  mac.press(kKeyF18, kF18Button);
  QCOMPARE(link.written(), hangulDownBytes());

  if (reset) {
    link.proxy->resetOptions();
  } else {
    link.proxy->setOptions({kOptionCrossOSF18Key, static_cast<uint32_t>(value)});
  }
  (void)link.written();

  // the destination still has Hangul down: the key's repeats stay suppressed and its release lets go of Hangul
  mac.autoRepeat(kKeyF18, kF18Button);
  QVERIFY(link.written().isEmpty());
  mac.release(kF18Button);
  QCOMPARE(link.written(), hangulUpBytes());

  // from then on F18 is a plain key again
  mac.press(kKeyF18, kF18Button);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Down, kKeyF18, 0, kF18Button));
  mac.autoRepeat(kKeyF18, kF18Button);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Repeat, kKeyF18, 0, kF18Button, 1));
  mac.release(kF18Button);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Up, kKeyNone, 0, kF18Button));
}

void ClientProxyTests::f18_optionTurnedOnMidHold_releaseStaysPlain()
{
  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  MacSource mac(*link.proxy);

  mac.press(kKeyF18, kF18Button);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Down, kKeyF18, 0, kF18Button));

  enableF18Hangul(link);

  // the destination never saw Hangul go down, so it must not get a Hangul release
  mac.release(kF18Button);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Up, kKeyNone, 0, kF18Button));

  // and the next press maps
  mac.press(kKeyF18, kF18Button);
  QCOMPARE(link.written(), hangulDownBytes());
  mac.release(kF18Button);
  QCOMPARE(link.written(), hangulUpBytes());
}

void ClientProxyTests::f18_leaveMidHold_forgetsTheHeldKey()
{
  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  enableF18Hangul(link);
  MacSource mac(*link.proxy);

  mac.press(kKeyF18, kF18Button);
  (void)link.written();
  QVERIFY(link.proxy->leave());
  (void)link.written();
  link.proxy->enter(0, 0, 0, 0, false);
  (void)link.written();

  // the destination released everything when the cursor left: the repeats of the old hold stay suppressed, and a
  // late release of the old press is not translated into a Hangul key-up for a key that is no longer down there
  mac.autoRepeat(kKeyF18, kF18Button);
  QVERIFY(link.written().isEmpty());
  mac.release(kF18Button);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Up, kKeyNone, 0, kF18Button));

  // a new press maps again, and so does the one after it
  for (int i = 0; i < 2; ++i) {
    mac.press(kKeyF18, kF18Button);
    QCOMPARE(link.written(), hangulDownBytes());
    mac.release(kF18Button);
    QCOMPARE(link.written(), hangulUpBytes());
  }
}

void ClientProxyTests::f18_otherKeysWhileHeld_doNotDisturbTheHeldKey()
{
  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  enableF18Hangul(link);
  MacSource mac(*link.proxy);

  mac.press(kKeyF18, kF18Button);
  QCOMPARE(link.written(), hangulDownBytes());

  // typing while F18 is down: an ordinary key (released with kKeyNone) and Left Command (a modifier, released with
  // its own id) travel untouched, and releasing them is not mistaken for releasing F18
  mac.press('a', kButton);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Down, 'a', 0, kButton));
  mac.release(kButton);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Up, kKeyNone, 0, kButton));
  mac.press(kKeySuper_L, kLeftCommandButton, KeyModifierSuper);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Down, kKeySuper_L, KeyModifierSuper, kLeftCommandButton));
  mac.releaseModifier(kKeySuper_L, kLeftCommandButton);
  QCOMPARE(link.written(), originalKeyBytes(9, KeyEvent::Up, kKeySuper_L, 0, kLeftCommandButton));

  // F18 is still held
  mac.autoRepeat(kKeyF18, kF18Button);
  QVERIFY(link.written().isEmpty());
  mac.release(kF18Button);
  QCOMPARE(link.written(), hangulUpBytes());

  mac.press(kKeyF18, kF18Button);
  QCOMPARE(link.written(), hangulDownBytes());
  mac.release(kF18Button);
  QCOMPARE(link.written(), hangulUpBytes());
}

void ClientProxyTests::f18_ineligibleLinks_passThrough_data()
{
  QTest::addColumn<int>("sourcePlatform");
  QTest::addColumn<int>("destinationPlatform");
  QTest::addColumn<uint32_t>("destinationCapabilities");

  constexpr uint32_t withHangul = kInputCapabilityProtocolV19 | kInputCapabilityHangul;
  constexpr uint32_t withoutHangul =
      kInputCapabilityProtocolV19 | kInputCapabilityCrossOSScroll | kInputCapabilitySemanticMedia;

  const auto row = [](const char *name, InputPlatform source, InputPlatform destination, uint32_t capabilities) {
    QTest::newRow(name) << static_cast<int>(source) << static_cast<int>(destination) << capabilities;
  };
  row("windows destination without the Hangul capability", InputPlatform::MacOS, InputPlatform::Windows, withoutHangul);
  row("windows destination without any capability", InputPlatform::MacOS, InputPlatform::Windows, 0);
  row("macOS destination", InputPlatform::MacOS, InputPlatform::MacOS, withHangul);
  row("linux destination", InputPlatform::MacOS, InputPlatform::Linux, withHangul);
  row("other destination", InputPlatform::MacOS, InputPlatform::Other, withHangul);
  row("unknown destination", InputPlatform::MacOS, InputPlatform::Unknown, withHangul);
  row("windows source", InputPlatform::Windows, InputPlatform::Windows, withHangul);
  row("linux source", InputPlatform::Linux, InputPlatform::Windows, withHangul);
  row("unknown source", InputPlatform::Unknown, InputPlatform::Windows, withHangul);
}

void ClientProxyTests::f18_ineligibleLinks_passThrough()
{
  QFETCH(int, sourcePlatform);
  QFETCH(int, destinationPlatform);
  QFETCH(uint32_t, destinationCapabilities);

  V19Link link(
      {static_cast<InputPlatform>(destinationPlatform), destinationCapabilities},
      static_cast<InputPlatform>(sourcePlatform)
  );
  QVERIFY(link.destinationParsed);
  link.proxy->setOptions({kOptionCrossOSF18Key, kHangulAction});
  (void)link.written();

  for (const auto event : {KeyEvent::Down, KeyEvent::Repeat, KeyEvent::Up}) {
    const KeyID id = macKeyId(event, kKeyF18);
    sendKeyEvent(*link.proxy, event, id, KeyModifierShift, kF18Button);
    QCOMPARE(link.written(), originalKeyBytes(9, event, id, KeyModifierShift, kF18Button));
  }
}

void ClientProxyTests::f18_legacyLinks_passThrough_data()
{
  QTest::addColumn<int>("minor");

  for (const auto minor : {0, 1, 3, 6, 7, 8}) {
    QTest::addRow("1.%d", minor) << minor;
  }
}

void ClientProxyTests::f18_legacyLinks_passThrough()
{
  QFETCH(int, minor);

  // a link below 1.9 has no capability exchange, so even a configured option cannot change what is written
  ProxyUnderTest test(minor);
  test.proxy->setOptions({kOptionCrossOSF18Key, kHangulAction});
  (void)test.stream->take();

  for (const auto event : {KeyEvent::Down, KeyEvent::Repeat, KeyEvent::Up}) {
    const KeyID id = macKeyId(event, kKeyF18);
    sendKeyEvent(*test.proxy, event, id, KeyModifierShift, kF18Button);
    QCOMPARE(test.stream->take(), originalKeyBytes(minor, event, id, KeyModifierShift, kF18Button));
  }
}

void ClientProxyTests::f18_otherKeys_passThrough_data()
{
  QTest::addColumn<KeyID>("key");

  const auto row = [](const char *name, KeyID key) { QTest::newRow(name) << key; };
  row("F1", kKeyF1);
  row("F13", kKeyF13);
  row("F14", kKeyF14);
  row("F15", kKeyF15);
  row("F16", kKeyF16);
  row("F17", kKeyF17);
  row("F19", kKeyF19);
  row("F20", kKeyF20);
  row("F24", kKeyF24);
  row("Hangul itself", kKeyHangul);
  row("Kana", kKeyKana);
  row("Left Command", kKeySuper_L);
  row("Right Command", kKeySuper_R);
  row("Shift", kKeyShift_L);
  row("a", 'a');
}

void ClientProxyTests::f18_otherKeys_passThrough()
{
  QFETCH(KeyID, key);

  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);
  link.proxy->setOptions({kOptionCrossOSF18Key, kHangulAction});
  (void)link.written();

  // an ordinary key releases with kKeyNone and a modifier with its own id; both must travel untouched
  const std::pair<KeyEvent, KeyID> events[] = {
      {KeyEvent::Down, key}, {KeyEvent::Repeat, key}, {KeyEvent::Up, kKeyNone}, {KeyEvent::Up, key}
  };
  for (const auto &[event, id] : events) {
    sendKeyEvent(*link.proxy, event, id, KeyModifierSuper, kButton);
    QCOMPARE(link.written(), originalKeyBytes(9, event, id, KeyModifierSuper, kButton));
  }
}

void ClientProxyTests::f18_optionOffInvalidOrReset_passesThrough()
{
  V19Link link(kWindowsWithHangul);
  QVERIFY(link.destinationParsed);

  const auto expectPassThrough = [&](const char *when) {
    for (const auto event : {KeyEvent::Down, KeyEvent::Repeat, KeyEvent::Up}) {
      const KeyID id = macKeyId(event, kKeyF18);
      sendKeyEvent(*link.proxy, event, id, 0, kF18Button);
      QVERIFY2(
          link.written() == originalKeyBytes(9, event, id, 0, kF18Button),
          qPrintable(QStringLiteral("F18 was not passed through %1").arg(QString::fromUtf8(when)))
      );
    }
  };

  // no option at all is the default
  expectPassThrough("by default");

  // an explicit pass-through value and a value nobody defined both mean pass through
  link.proxy->setOptions({kOptionCrossOSF18Key, static_cast<OptionValue>(F18Action::PassThrough)});
  (void)link.written();
  expectPassThrough("with the pass-through value");
  link.proxy->setOptions({kOptionCrossOSF18Key, 7});
  (void)link.written();
  expectPassThrough("with an unknown value");

  // and a reset switches a configured mapping off again
  link.proxy->setOptions({kOptionCrossOSF18Key, kHangulAction});
  (void)link.written();
  link.proxy->resetOptions();
  (void)link.written();
  expectPassThrough("after the options were reset");
}

void ClientProxyTests::semanticMedia_v19EnabledSuppressesRepeat()
{
  RecordingEventQueue events;
  auto *stream = new CapturingStream;
  auto *server = reinterpret_cast<Server *>(0x1);
  TestClientProxy1_9 proxy("client", stream, server, &events);
  (void)stream->take();

  const InputCapabilities capabilities{
      InputPlatform::Windows, kInputCapabilityProtocolV19 | kInputCapabilitySemanticMedia
  };
  CapturingStream capabilityPacket;
  ProtocolUtil::writef(
      &capabilityPacket, kMsgCInputCapabilities, static_cast<uint8_t>(capabilities.platform), capabilities.capabilities
  );
  stream->pushInput(capabilityPacket.take());
  stream->read(nullptr, 4);
  QVERIFY(proxy.parseHandshakeMessageForTest(reinterpret_cast<const uint8_t *>(kMsgCInputCapabilities)));

  proxy.setOptions({kOptionCrossOSSemanticMedia, 1});
  (void)stream->take();
  proxy.keyDown(kKeyAudioPlay, 0, 0, kLang);
  CapturingStream expectedDown;
  ProtocolUtil::writef(&expectedDown, kMsgDKeyDown, kKeyAudioPlay, 0, 0, &kLang);
  QCOMPARE(stream->take(), expectedDown.take());

  proxy.keyRepeat(kKeyAudioPlay, 0, 1, 0, kLang);
  QVERIFY(stream->take().isEmpty());

  proxy.keyUp(kKeyAudioPlay, 0, 0);
  CapturingStream expectedUp;
  ProtocolUtil::writef(&expectedUp, kMsgDKeyUp, kKeyAudioPlay, 0, 0);
  QCOMPARE(stream->take(), expectedUp.take());
}

void ClientProxyTests::semanticMedia_v18AndDisabledPreserveWireValues()
{
  ProxyUnderTest legacy(8);
  legacy.proxy->keyRepeat(kKeyAudioPlay, 0, 1, 0, kLang);
  CapturingStream legacyExpected;
  ProtocolUtil::writef(&legacyExpected, kMsgDKeyRepeat, kKeyAudioPlay, 0, 1, 0, &kLang);
  QCOMPARE(legacy.stream->take(), legacyExpected.take());

  RecordingEventQueue events;
  auto *stream = new CapturingStream;
  auto *server = reinterpret_cast<Server *>(0x1);
  TestClientProxy1_9 proxy("client", stream, server, &events);
  (void)stream->take();
  const InputCapabilities capabilities{
      InputPlatform::Windows, kInputCapabilityProtocolV19 | kInputCapabilitySemanticMedia
  };
  CapturingStream capabilityPacket;
  ProtocolUtil::writef(
      &capabilityPacket, kMsgCInputCapabilities, static_cast<uint8_t>(capabilities.platform), capabilities.capabilities
  );
  stream->pushInput(capabilityPacket.take());
  stream->read(nullptr, 4);
  QVERIFY(proxy.parseHandshakeMessageForTest(reinterpret_cast<const uint8_t *>(kMsgCInputCapabilities)));

  proxy.setOptions({kOptionCrossOSSemanticMedia, 0});
  (void)stream->take();
  proxy.keyRepeat(kKeyAudioPlay, 0, 1, 0, kLang);
  CapturingStream disabledExpected;
  ProtocolUtil::writef(&disabledExpected, kMsgDKeyRepeat, kKeyAudioPlay, 0, 1, 0, &kLang);
  QCOMPARE(stream->take(), disabledExpected.take());
}

QTEST_MAIN(ClientProxyTests)
