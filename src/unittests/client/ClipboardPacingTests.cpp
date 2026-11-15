/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ClipboardPacingTests.h"

#include "../deskflow/ClipboardTestSupport.h"
#include "../deskflow/MockPlatformComputer.h"
#include "client/Client.h"
#include "client/ServerProxy1_9.h"
#include "common/ClipboardCapabilities.h"
#include "common/InputCapabilities.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "deskflow/AppUtil.h"
#include "deskflow/ClipboardIdPolicy.h"
#include "deskflow/Computer.h"
#include "deskflow/IComputer.h"
#include "deskflow/ProtocolUtil.h"
#include "deskflow/StreamChunker.h"
#include "net/ISocketFactory.h"
#include "server/ClientProxy1_8.h"
#include "server/ClientProxy1_9.h"
#include "server/Config.h"
#include "server/PrimaryClient.h"
#include "server/Server.h"

#include <cstring>
#include <memory>

using namespace clipboardtest;

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

class NoSockets : public ISocketFactory
{
public:
  IDataSocket *create(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    return nullptr;
  }

  IListenSocket *createListen(IArchNetwork::AddressFamily, SecurityLevel) const override
  {
    return nullptr;
  }
};

//! A platform computer that remembers the clipboards it was given
class RecordingPlatformComputer : public MockPlatformComputer
{
public:
  using MockPlatformComputer::MockPlatformComputer;

  bool setClipboard(ClipboardID id, const IClipboard *clipboard) override
  {
    if (clipboard != nullptr) {
      m_received[id] = IClipboard::marshall(clipboard);
    }
    return true;
  }

  bool hasReceived(ClipboardID id) const
  {
    return m_received.contains(id);
  }

  const std::string &received(ClipboardID id) const
  {
    return m_received.at(id);
  }

private:
  std::map<ClipboardID, std::string> m_received;
};

//! The client side of a link: a real Client around a recording computer, and a real protocol 1.9 ServerProxy
class ClientSide
{
public:
  class Proxy : public ServerProxy1_9
  {
  public:
    using ServerProxy1_9::ServerProxy1_9;

    bool parseMessageForTest(const char *code)
    {
      return parseMessage(reinterpret_cast<const uint8_t *>(code)) == ConnectionResult::Okay;
    }

    bool parseHandshakeMessageForTest(const char *code)
    {
      return parseHandshakeMessage(reinterpret_cast<const uint8_t *>(code)) == ConnectionResult::Okay;
    }
  };

  ClientSide()
  {
    auto *platform = new RecordingPlatformComputer(&events);
    computer = std::make_unique<deskflow::Computer>(platform, &events);
    this->platform = platform;
    client = std::make_unique<Client>(&events, "client", NetworkAddress(), new NoSockets, computer.get());
    proxy = std::make_unique<Proxy>(client.get(), &stream, &events);
  }

  //! The server's DIPC: its platform and capabilities
  void receiveDipc(InputPlatform platform, InputCapabilityMask capabilities)
  {
    TestStream packet;
    ProtocolUtil::writef(&packet, kMsgDInputCapabilities, static_cast<uint8_t>(platform), capabilities);
    stream.feed(packet.packets().front().substr(4));
    QVERIFY(proxy->parseHandshakeMessageForTest(kMsgDInputCapabilities));
    stream.takePackets(); // the CIPC reply
  }

  //! Hands the packets another proxy wrote to the proxy as messages from the server
  void deliver(const std::vector<std::string> &packets)
  {
    for (const auto &packet : packets) {
      QVERIFY(packet.size() >= 4);
      stream.feed(packet.substr(4));
      QVERIFY(proxy->parseMessageForTest(packet.substr(0, 4).c_str()));
    }
  }

  void sendClipboard(ClipboardID id, const std::string &marshalled)
  {
    Clipboard clipboard;
    clipboard.unmarshall(marshalled, 0);
    proxy->onClipboardChanged(id, &clipboard);
  }

  TestEventQueue events;
  TestStream stream;
  RecordingPlatformComputer *platform = nullptr;
  std::unique_ptr<deskflow::Computer> computer;
  std::unique_ptr<Client> client;
  std::unique_ptr<Proxy> proxy;
};

//! The server side of a link: a real Server and a real protocol 1.9 ClientProxy
class ServerSide
{
public:
  class Proxy : public ClientProxy1_9
  {
  public:
    using ClientProxy1_9::ClientProxy1_9;

    bool parseHandshakeMessageForTest(const char *code)
    {
      return parseHandshakeMessage(reinterpret_cast<const uint8_t *>(code));
    }
  };

  ServerSide()
  {
    config.addComputer("primary");
    computer = std::make_unique<deskflow::Computer>(new MockPlatformComputer(&events), &events);
    primary = std::make_unique<PrimaryClient>("primary", computer.get());
    server = std::make_unique<Server>(config, primary.get(), computer.get(), &events);
    stream = new TestStream; // owned by the proxy
    proxy = std::make_unique<Proxy>("client", stream, server.get(), &events);
    stream->takePackets(); // DIPC and QINF
  }

  //! The client's CIPC: its platform and capabilities
  void receiveCipc(InputPlatform platform, InputCapabilityMask capabilities)
  {
    TestStream packet;
    ProtocolUtil::writef(&packet, kMsgCInputCapabilities, static_cast<uint8_t>(platform), capabilities);
    stream->feed(packet.packets().front().substr(4));
    QVERIFY(proxy->parseHandshakeMessageForTest(kMsgCInputCapabilities));
  }

  TestEventQueue events;
  deskflow::server::Config config{&events};
  std::unique_ptr<deskflow::Computer> computer;
  std::unique_ptr<PrimaryClient> primary;
  std::unique_ptr<Server> server;
  TestStream *stream = nullptr;
  std::unique_ptr<Proxy> proxy;
};

void flushEvent(TestEventQueue &events, deskflow::IStream &stream)
{
  events.dispatchEvent(Event(EventTypes::StreamOutputFlushed, stream.getEventTarget()));
}

size_t clipboardSendingEvents(const TestEventQueue &events)
{
  size_t count = 0;
  for (const auto &event : events.addedEvents()) {
    count += event.getType() == EventTypes::ClipboardSending ? 1 : 0;
  }
  return count;
}

//! What the original code wrote for the clipboard: StreamChunker's events handled in order
std::vector<std::string> originalBytes(const std::string &marshalled, ClipboardID id, uint32_t sequence)
{
  TestEventQueue events;
  TestStream stream;
  int target = 0;
  StreamChunker::sendClipboard(marshalled, marshalled.size(), id, sequence, &events, &target);
  for (const auto &event : events.addedEvents()) {
    ClipboardChunk::send(&stream, event.getDataObject());
  }
  return stream.packets();
}

//! Writes the clipboard events a proxy queued, as the event loop would
std::vector<std::string> handleQueuedChunks(TestEventQueue &events, void *target, TestStream &stream)
{
  for (const auto &event : events.addedEvents()) {
    if (event.getType() == EventTypes::ClipboardSending) {
      events.dispatchEvent(Event(event.getType(), target, event.getDataObject()));
    }
  }
  return stream.takePackets();
}

struct ClipboardOf : Clipboard
{
  explicit ClipboardOf(const std::string &marshalled)
  {
    unmarshall(marshalled, 0);
  }
};

constexpr InputCapabilityMask kPeerCapabilities = kInputCapabilityProtocolV19 | kClipboardCapabilityPaced;

} // namespace

void ClipboardPacingTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  static TestAppUtil util;
  m_log.setFilter(LogLevel::Level::Warning);
}

void ClipboardPacingTests::handshake_bothSidesAdvertiseTheClipboardCapability()
{
  // the capability travels in the mask of DIPC and CIPC, whose layout does not change
  TestEventQueue serverEvents;
  auto *serverStream = new TestStream;
  ClientProxy1_9 serverProxy("client", serverStream, reinterpret_cast<Server *>(0x1), &serverEvents);
  const auto dipc = serverStream->takePackets();
  TestStream expectedDipc;
  const auto local = deskflow::platform::currentInputCapabilities();
  ProtocolUtil::writef(
      &expectedDipc, kMsgDInputCapabilities, static_cast<uint8_t>(local.platform),
      local.capabilities | kClipboardCapabilityPaced | kClipboardCapabilitySingleId
  );
  QVERIFY(std::ranges::find(dipc, expectedDipc.packets().front()) != dipc.end());

  ClientSide client;
  TestStream dipcPacket;
  ProtocolUtil::writef(&dipcPacket, kMsgDInputCapabilities, static_cast<uint8_t>(InputPlatform::MacOS), uint32_t{1});
  client.stream.feed(dipcPacket.packets().front().substr(4));
  QVERIFY(client.proxy->parseHandshakeMessageForTest(kMsgDInputCapabilities));
  const auto cipc = client.stream.takePackets();
  QCOMPARE(cipc.size(), size_t{1});
  // CIPC, platform, then the mask in network byte order: bit 0 (protocol 1.9) is in the last byte, bits 8 and 9 in the
  // one before
  QCOMPARE(static_cast<uint8_t>(cipc.front().at(8)), uint8_t{0x01});
  QCOMPARE(static_cast<uint8_t>(cipc.front().at(7)), uint8_t{0x03});
}

void ClipboardPacingTests::serverToClient_notNegotiated_keepsTheLegacyBytes_data()
{
  QTest::addColumn<bool>("protocol19");
  QTest::newRow("protocol 1.8 proxy") << false;
  QTest::newRow("1.9 proxy before the destination reported its capabilities") << true;
}

void ClipboardPacingTests::serverToClient_notNegotiated_keepsTheLegacyBytes()
{
  QFETCH(bool, protocol19);

  TestEventQueue events;
  auto *stream = new TestStream;
  const auto marshalled = marshalledText(600 * 1024);
  const ClipboardOf clipboard(marshalled);

  std::unique_ptr<ClientProxy1_8> proxy;
  if (protocol19) {
    proxy = std::make_unique<ClientProxy1_9>("client", stream, reinterpret_cast<Server *>(0x1), &events);
  } else {
    proxy = std::make_unique<ClientProxy1_8>("client", stream, reinterpret_cast<Server *>(0x1), &events);
  }
  stream->takePackets();

  proxy->setClipboard(0, &clipboard);

  // the whole transfer is queued as events right away, in 512 KiB chunks, and nothing waits for a drain
  QCOMPARE(stream->packets().size(), size_t{0});
  QCOMPARE(clipboardSendingEvents(events), size_t{4});
  QCOMPARE(handleQueuedChunks(events, proxy.get(), *stream), originalBytes(marshalled, 0, 0));
  QVERIFY(stream->bulkSendCalls().empty());
}

void ClipboardPacingTests::serverToClient_negotiated_isPaced()
{
  ServerSide server;
  server.receiveCipc(InputPlatform::Windows, kInputCapabilityProtocolV19);
  const ClipboardOf clipboard(marshalledText(100 * 1024));

  // even a destination that was built before the clipboard capability existed is paced: it reassembles any chunk size
  server.proxy->setClipboard(0, &clipboard);

  QCOMPARE(clipboardSendingEvents(server.events), size_t{0});
  QCOMPARE(server.stream->packets().size(), size_t{2}); // start and the first 32 KiB chunk
  QCOMPARE(server.stream->bulkSendCalls(), std::vector<bool>({true}));
}

void ClipboardPacingTests::serverToClient_pacedStreamIsAcceptedByTheRealServerProxy()
{
  ServerSide server;
  // a destination on X11 gets both ids
  server.receiveCipc(InputPlatform::Linux, kInputCapabilityProtocolV19);
  const auto marshalled = marshalledText(200 * 1024);
  const ClipboardOf clipboard(marshalled);
  server.proxy->setClipboard(0, &clipboard);
  server.proxy->setClipboard(1, &clipboard);
  for (size_t i = 0; i < 100; ++i) {
    flushEvent(server.events, *server.stream);
  }

  ClientSide client;
  client.receiveDipc(InputPlatform::MacOS, kPeerCapabilities);
  client.deliver(server.stream->takePackets());

  QVERIFY(client.platform->hasReceived(0));
  QVERIFY(client.platform->hasReceived(1));
  QCOMPARE(client.platform->received(0), marshalled);
  QCOMPARE(client.platform->received(1), marshalled);
  for (const auto &event : client.events.addedEvents()) {
    QVERIFY(event.getType() != EventTypes::ClientDisconnectRequested);
  }
}

void ClipboardPacingTests::serverToClient_pacedStreamWithInputInBetweenStillArrivesIntact()
{
  ServerSide server;
  server.receiveCipc(InputPlatform::Windows, kInputCapabilityProtocolV19);
  const auto marshalled = marshalledText(150 * 1024);
  const ClipboardOf clipboard(marshalled);
  server.proxy->setClipboard(0, &clipboard);

  // mouse moves and a keepalive are written while the transfer runs
  size_t flushes = 0;
  while (server.stream->packets().size() < 1000 && flushes++ < 100) {
    server.proxy->mouseMove(5, 6);
    flushEvent(server.events, *server.stream);
  }

  ClientSide client;
  client.receiveDipc(InputPlatform::MacOS, kPeerCapabilities);
  size_t chunks = 0;
  for (const auto &packet : server.stream->takePackets()) {
    if (std::memcmp(packet.data(), kMsgDClipboard, 4) == 0) {
      client.deliver({packet});
      ++chunks;
    }
  }
  QVERIFY(chunks > 4);
  QCOMPARE(client.platform->received(0), marshalled);
}

void ClipboardPacingTests::clientToServer_notNegotiated_keepsTheLegacyBytes()
{
  ClientSide client;
  const auto marshalled = marshalledText(600 * 1024);

  // the server has not sent its DIPC: this is a server that predates protocol 1.9
  client.sendClipboard(1, marshalled);

  QCOMPARE(client.stream.packets().size(), size_t{0});
  QCOMPARE(clipboardSendingEvents(client.events), size_t{4});
  QCOMPARE(handleQueuedChunks(client.events, client.proxy.get(), client.stream), originalBytes(marshalled, 1, 0));
}

void ClipboardPacingTests::clientToServer_negotiated_isPaced()
{
  ClientSide client;
  client.receiveDipc(InputPlatform::MacOS, kInputCapabilityProtocolV19);

  // a server of the previous 1.9 build, which does not know the capability, still reassembles the paced stream
  client.sendClipboard(0, marshalledText(100 * 1024));

  QCOMPARE(clipboardSendingEvents(client.events), size_t{0});
  QCOMPARE(client.stream.packets().size(), size_t{2});
}

void ClipboardPacingTests::clientToServer_pacedStreamIsAcceptedByTheRealClientProxy()
{
  ClientSide client;
  client.receiveDipc(InputPlatform::MacOS, kPeerCapabilities);
  const auto marshalled = marshalledText(200 * 1024);
  client.sendClipboard(0, marshalled);
  for (size_t i = 0; i < 100; ++i) {
    flushEvent(client.events, client.stream);
  }

  ServerSide server;
  server.receiveCipc(InputPlatform::Windows, kPeerCapabilities);
  for (const auto &packet : client.stream.takePackets()) {
    server.stream->feed(packet.substr(4));
    QVERIFY(server.proxy->recvClipboard());
  }

  Clipboard received;
  QVERIFY(server.proxy->getClipboard(0, &received));
  QCOMPARE(received.marshall(), marshalled);
  size_t changed = 0;
  for (const auto &event : server.events.addedEvents()) {
    changed += event.getType() == EventTypes::ClipboardChanged ? 1 : 0;
  }
  QCOMPARE(changed, size_t{1});
}

void ClipboardPacingTests::grabClipboard_dropsTheRunningTransferOfThatId()
{
  ServerSide server;
  server.receiveCipc(InputPlatform::Windows, kInputCapabilityProtocolV19);
  const ClipboardOf clipboard(marshalledText(200 * 1024));
  server.proxy->setClipboard(0, &clipboard);
  flushEvent(server.events, *server.stream);
  server.stream->takePackets();

  // the grab says the content is stale: its data must not follow it
  server.proxy->grabClipboard(0);
  for (size_t i = 0; i < 20; ++i) {
    flushEvent(server.events, *server.stream);
  }

  const auto packets = server.stream->takePackets();
  QCOMPARE(packets.size(), size_t{1});
  QVERIFY(std::memcmp(packets.front().data(), kMsgCClipboard, 4) == 0);
}

namespace {

constexpr InputCapabilityMask kSingleIdPeer = kClipboardCapabilityPaced | kClipboardCapabilitySingleId;

//! The clipboard ids of the start chunks in \p packets
std::vector<int> startedIds(const std::vector<std::string> &packets)
{
  std::vector<int> ids;
  for (const auto &packet : packets) {
    if (packet.size() > 9 && std::memcmp(packet.data(), kMsgDClipboard, 4) == 0 &&
        static_cast<uint8_t>(packet.at(9)) == ChunkType::DataStart) {
      ids.push_back(static_cast<uint8_t>(packet.at(4)));
    }
  }
  return ids;
}

void sendBothIds(ServerSide &server, const std::string &marshalled)
{
  const ClipboardOf clipboard(marshalled);
  server.proxy->setClipboard(0, &clipboard);
  server.proxy->setClipboard(1, &clipboard);
  for (size_t i = 0; i < 20; ++i) {
    flushEvent(server.events, *server.stream);
  }
}

} // namespace

void ClipboardPacingTests::idPolicy_onlyXWindowsStylePeersExchangeTheSelection_data()
{
  QTest::addColumn<int>("platform");
  QTest::addColumn<int>("id");
  QTest::addColumn<bool>("exchanged");

  QTest::newRow("windows clipboard") << static_cast<int>(InputPlatform::Windows) << 0 << true;
  QTest::newRow("windows selection") << static_cast<int>(InputPlatform::Windows) << 1 << false;
  QTest::newRow("macOS clipboard") << static_cast<int>(InputPlatform::MacOS) << 0 << true;
  QTest::newRow("macOS selection") << static_cast<int>(InputPlatform::MacOS) << 1 << false;
  QTest::newRow("linux selection") << static_cast<int>(InputPlatform::Linux) << 1 << true;
  QTest::newRow("other selection") << static_cast<int>(InputPlatform::Other) << 1 << true;
  QTest::newRow("unknown selection") << static_cast<int>(InputPlatform::Unknown) << 1 << true;
  QTest::newRow("unknown clipboard") << static_cast<int>(InputPlatform::Unknown) << 0 << true;
}

void ClipboardPacingTests::idPolicy_onlyXWindowsStylePeersExchangeTheSelection()
{
  QFETCH(int, platform);
  QFETCH(int, id);
  QFETCH(bool, exchanged);

  QCOMPARE(
      deskflow::clipboard::exchangesId(static_cast<InputPlatform>(platform), static_cast<ClipboardID>(id)), exchanged
  );
}

void ClipboardPacingTests::serverToClient_nonX11Peer_getsOnlyClipboardIdZeroData_data()
{
  QTest::addColumn<int>("platform");
  QTest::newRow("windows") << static_cast<int>(InputPlatform::Windows);
  QTest::newRow("macOS") << static_cast<int>(InputPlatform::MacOS);
}

void ClipboardPacingTests::serverToClient_nonX11Peer_getsOnlyClipboardIdZeroData()
{
  QFETCH(int, platform);

  ServerSide server;
  // a peer built before this capability existed still gets one id: its receiver ignores the id on these platforms
  server.receiveCipc(static_cast<InputPlatform>(platform), kInputCapabilityProtocolV19);
  const auto marshalled = marshalledText(70 * 1024);
  sendBothIds(server, marshalled);

  const auto packets = server.stream->takePackets();
  QCOMPARE(startedIds(packets), std::vector<int>({0}));
  ChunkReassembly receiver;
  receiver.feedAll(packets);
  QVERIFY(receiver.finished(0));
  QVERIFY(!receiver.finished(1));
  QCOMPARE(receiver.finishedData(0), marshalled);
}

void ClipboardPacingTests::serverToClient_x11OrUnknownPeer_getsBothIds_data()
{
  QTest::addColumn<int>("platform");
  QTest::newRow("linux") << static_cast<int>(InputPlatform::Linux);
  QTest::newRow("other") << static_cast<int>(InputPlatform::Other);
  QTest::newRow("unknown") << static_cast<int>(InputPlatform::Unknown);
}

void ClipboardPacingTests::serverToClient_x11OrUnknownPeer_getsBothIds()
{
  QFETCH(int, platform);

  ServerSide server;
  server.receiveCipc(static_cast<InputPlatform>(platform), kInputCapabilityProtocolV19);
  sendBothIds(server, marshalledText(70 * 1024));

  QCOMPARE(startedIds(server.stream->takePackets()), std::vector<int>({0, 1}));
}

void ClipboardPacingTests::serverToClient_nonX11Peer_stillGetsTheGrabOfBothIds()
{
  ServerSide server;
  server.receiveCipc(InputPlatform::Windows, kInputCapabilityProtocolV19);

  // ownership bookkeeping of the peer must not change: a grab is a few bytes
  server.proxy->grabClipboard(0);
  server.proxy->grabClipboard(1);
  const auto packets = server.stream->takePackets();
  QCOMPARE(packets.size(), size_t{2});
  QCOMPARE(static_cast<uint8_t>(packets[0].at(4)), uint8_t{0});
  QCOMPARE(static_cast<uint8_t>(packets[1].at(4)), uint8_t{1});
}

void ClipboardPacingTests::serverToClient_beforeTheCapabilities_keepsBothIds()
{
  TestEventQueue events;
  auto *stream = new TestStream;
  ClientProxy1_9 proxy("client", stream, reinterpret_cast<Server *>(0x1), &events);
  stream->takePackets();
  const ClipboardOf clipboard(marshalledText(100));

  proxy.setClipboard(0, &clipboard);
  proxy.setClipboard(1, &clipboard);

  // nothing about the peer is known yet, so nothing is dropped: both transfers are queued as before
  QCOMPARE(clipboardSendingEvents(events), size_t{6});
}

void ClipboardPacingTests::clientToServer_nonX11ServerThatMirrors_getsOnlyClipboardIdZeroData()
{
  ClientSide client;
  client.receiveDipc(InputPlatform::MacOS, kSingleIdPeer);
  const auto marshalled = marshalledText(50 * 1024);

  client.sendClipboard(0, marshalled);
  client.sendClipboard(1, marshalled);
  for (size_t i = 0; i < 20; ++i) {
    flushEvent(client.events, client.stream);
  }

  QCOMPARE(startedIds(client.stream.takePackets()), std::vector<int>({0}));
}

void ClipboardPacingTests::clientToServer_serverWithoutTheCapabilityOrX11Server_getsBothIds_data()
{
  QTest::addColumn<int>("platform");
  QTest::addColumn<uint>("capabilities");

  // an older 1.9 server on a Mac or Windows PC may serve X11 clients that need the selection, and does not mirror
  QTest::newRow("macOS server without the capability")
      << static_cast<int>(InputPlatform::MacOS) << static_cast<uint>(kClipboardCapabilityPaced);
  QTest::newRow("windows server without the capability")
      << static_cast<int>(InputPlatform::Windows) << static_cast<uint>(kInputCapabilityProtocolV19);
  QTest::newRow("linux server") << static_cast<int>(InputPlatform::Linux) << static_cast<uint>(kSingleIdPeer);
  QTest::newRow("unknown server") << static_cast<int>(InputPlatform::Unknown) << static_cast<uint>(kSingleIdPeer);
}

void ClipboardPacingTests::clientToServer_serverWithoutTheCapabilityOrX11Server_getsBothIds()
{
  QFETCH(int, platform);
  QFETCH(uint, capabilities);

  ClientSide client;
  client.receiveDipc(static_cast<InputPlatform>(platform), capabilities);
  const auto marshalled = marshalledText(50 * 1024);
  client.sendClipboard(0, marshalled);
  client.sendClipboard(1, marshalled);
  for (size_t i = 0; i < 20; ++i) {
    flushEvent(client.events, client.stream);
  }

  QCOMPARE(startedIds(client.stream.takePackets()), std::vector<int>({0, 1}));
}

void ClipboardPacingTests::clientToServer_skippedIdIsReportedByTheProxy()
{
  ClientSide client;
  QVERIFY(client.proxy->sendsClipboardId(1)); // nothing known yet
  client.receiveDipc(InputPlatform::Windows, kSingleIdPeer);
  QVERIFY(client.proxy->sendsClipboardId(0));
  QVERIFY(!client.proxy->sendsClipboardId(1));
}

void ClipboardPacingTests::mirror_nonX11ClientThatSendsOneId_isAlsoTheSelection()
{
  ClientSide client;
  client.receiveDipc(InputPlatform::MacOS, kSingleIdPeer);
  const auto marshalled = marshalledText(20 * 1024);
  client.sendClipboard(0, marshalled);
  for (size_t i = 0; i < 20; ++i) {
    flushEvent(client.events, client.stream);
  }

  ServerSide server;
  server.receiveCipc(InputPlatform::Windows, kSingleIdPeer);
  for (const auto &packet : client.stream.takePackets()) {
    server.stream->feed(packet.substr(4));
    QVERIFY(server.proxy->recvClipboard());
  }

  // the server sees the selection change too, as it did when the peer sent both ids
  std::vector<int> changed;
  for (const auto &event : server.events.addedEvents()) {
    if (event.getType() == EventTypes::ClipboardChanged) {
      changed.push_back(static_cast<const IComputer::ClipboardInfo *>(event.getData())->m_id);
    }
  }
  QCOMPARE(changed, std::vector<int>({0, 1}));
  Clipboard selection;
  QVERIFY(server.proxy->getClipboard(1, &selection));
  QCOMPARE(selection.marshall(), marshalled);
}

void ClipboardPacingTests::mirror_peerWithoutTheCapabilityOrX11Peer_isNotMirrored_data()
{
  QTest::addColumn<int>("platform");
  QTest::addColumn<uint>("capabilities");

  QTest::newRow("legacy peer sends both ids itself")
      << static_cast<int>(InputPlatform::Windows) << static_cast<uint>(kInputCapabilityProtocolV19);
  QTest::newRow("x11 peer") << static_cast<int>(InputPlatform::Linux) << static_cast<uint>(kSingleIdPeer);
}

void ClipboardPacingTests::mirror_peerWithoutTheCapabilityOrX11Peer_isNotMirrored()
{
  QFETCH(int, platform);
  QFETCH(uint, capabilities);

  const auto marshalled = marshalledText(2000);
  TestStream chunks;
  ClipboardChunk::write(&chunks, 0, 0, ChunkType::DataStart, std::to_string(marshalled.size()));
  ClipboardChunk::write(&chunks, 0, 0, ChunkType::DataChunk, marshalled);
  ClipboardChunk::write(&chunks, 0, 0, ChunkType::DataEnd, {});

  ServerSide server;
  server.receiveCipc(static_cast<InputPlatform>(platform), capabilities);
  for (const auto &packet : chunks.packets()) {
    server.stream->feed(packet.substr(4));
    QVERIFY(server.proxy->recvClipboard());
  }

  size_t changed = 0;
  for (const auto &event : server.events.addedEvents()) {
    changed += event.getType() == EventTypes::ClipboardChanged ? 1 : 0;
  }
  QCOMPARE(changed, size_t{1});
}

void ClipboardPacingTests::proofOfLife_negotiated_anyReceivedDataResetsTheAlarm()
{
  ClientSide client;
  client.receiveDipc(InputPlatform::MacOS, kPeerCapabilities);

  const auto before = client.events.timersCreated();
  client.stream.feed("CNOP");
  QVERIFY(client.events.dispatchEvent(Event(EventTypes::StreamInputReady, client.stream.getEventTarget())));
  QCOMPARE(client.events.timersCreated(), before + 1);
}

void ClipboardPacingTests::proofOfLife_notNegotiated_onlyKeepAlivesResetTheAlarm()
{
  ClientSide client;

  const auto before = client.events.timersCreated();
  client.stream.feed("CNOP");
  QVERIFY(client.events.dispatchEvent(Event(EventTypes::StreamInputReady, client.stream.getEventTarget())));
  QCOMPARE(client.events.timersCreated(), before);

  // a keepalive still resets it and is echoed
  client.stream.feed("CALV");
  QVERIFY(client.events.dispatchEvent(Event(EventTypes::StreamInputReady, client.stream.getEventTarget())));
  QCOMPARE(client.events.timersCreated(), before + 1);
  QVERIFY(!client.stream.takePackets().empty());
}

void ClipboardPacingTests::proofOfLife_peerWithoutTheCapability_isUnchanged()
{
  ClientSide client;
  client.receiveDipc(InputPlatform::MacOS, kInputCapabilityProtocolV19);

  const auto before = client.events.timersCreated();
  client.stream.feed("CNOP");
  QVERIFY(client.events.dispatchEvent(Event(EventTypes::StreamInputReady, client.stream.getEventTarget())));
  QCOMPARE(client.events.timersCreated(), before);
}

QTEST_MAIN(ClipboardPacingTests)
