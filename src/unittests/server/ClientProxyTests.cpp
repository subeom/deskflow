/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClientProxyTests.h"

#include "../deskflow/MockEventQueue.h"
#include "common/InputCapabilities.h"
#include "common/PlatformInfo.h"
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

#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <QByteArray>
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

class TestClientProxy1_9 : public ClientProxy1_9
{
public:
  using ClientProxy1_9::ClientProxy1_9;

  bool parseHandshakeMessageForTest(const uint8_t *code)
  {
    return parseHandshakeMessage(code);
  }

  bool parseMessageForTest(const uint8_t *code)
  {
    return parseMessage(code);
  }
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
      deskflow::platform::currentInputCapabilities().capabilities
  );
  const auto actual = stream->take();
  const auto expectedBytes = expected.take();
  QVERIFY(actual.endsWith(expectedBytes));
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

QTEST_MAIN(ClientProxyTests)
