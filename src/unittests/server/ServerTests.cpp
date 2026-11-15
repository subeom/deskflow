/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerTests.h"

#include "../deskflow/MockEventQueue.h"
#include "../deskflow/MockPlatformComputer.h"
#include "base/Event.h"
#include "base/ILogOutputter.h"
#include "common/Settings.h"
#include "deskflow/Clipboard.h"
#include "deskflow/ClipboardSharePolicy.h"
#include "deskflow/Computer.h"
#include "deskflow/IPrimaryComputer.h"
#include "deskflow/ipc/CoreIpcServer.h"
#include "io/IStream.h"
#include "server/ClientProxy.h"
#include "server/Config.h"
#include "server/PrimaryClient.h"
#include "server/Server.h"
#include "server/ServerCursorEdges.h"
#include "server/ServerNeighborTraversal.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {

// Hands events to the handlers the code under test registered, like the real queue does
class DispatchingEventQueue : public MockEventQueue
{
public:
  void addHandler(EventTypes type, void *target, const EventHandler &handler) override
  {
    m_handlers[{type, target}] = handler;
  }

  void removeHandler(EventTypes type, void *target) override
  {
    m_handlers.erase({type, target});
  }

  bool dispatchEvent(const Event &event) override
  {
    const auto handler = m_handlers.find({event.getType(), event.getTarget()});
    if (handler == m_handlers.end()) {
      return false;
    }
    handler->second(event);
    return true;
  }

private:
  std::map<std::pair<EventTypes, void *>, EventHandler> m_handlers;
};

// The active client of a new Server is its primary client, so it sees every wheel event the Server relays
class RecordingPrimaryClient : public PrimaryClient
{
public:
  struct Wheel
  {
    int32_t x;
    int32_t y;
    ScrollSource source;
    // arrived through the overload that carries the device kind
    bool withSource;
  };

  using PrimaryClient::PrimaryClient;

  void mouseWheel(int32_t xDelta, int32_t yDelta) override
  {
    m_wheels.push_back({xDelta, yDelta, ScrollSource::Unknown, false});
  }

  void mouseWheel(int32_t xDelta, int32_t yDelta, ScrollSource source) override
  {
    m_wheels.push_back({xDelta, yDelta, source, true});
  }

  const std::vector<Wheel> &wheels() const
  {
    return m_wheels;
  }

private:
  std::vector<Wheel> m_wheels;
};

// A real Server on an inert platform computer
struct ServerRig
{
  DispatchingEventQueue events;
  deskflow::server::Config config{&events};
  std::unique_ptr<deskflow::Computer> computer;
  std::unique_ptr<RecordingPrimaryClient> primary;
  std::unique_ptr<Server> server;

  ServerRig()
  {
    config.addComputer("primary");
    computer = std::make_unique<deskflow::Computer>(new MockPlatformComputer(&events), &events);
    primary = std::make_unique<RecordingPrimaryClient>("primary", computer.get());
    server = std::make_unique<Server>(config, primary.get(), computer.get(), &events);
  }

  // What a primary computer does for a wheel event: queue it for the Server. Takes ownership of \p info.
  bool sendWheel(IPrimaryComputer::WheelInfo *info)
  {
    const Event wheel(EventTypes::PrimaryComputerWheel, primary->getEventTarget(), info);
    const auto dispatched = events.dispatchEvent(wheel);
    Event::deleteData(wheel);
    return dispatched;
  }
};

struct FreeDeleter
{
  void operator()(void *data) const
  {
    std::free(data);
  }
};
using WheelInfoPtr = std::unique_ptr<IPrimaryComputer::WheelInfo, FreeDeleter>;

constexpr auto kUnknown = static_cast<int>(ScrollSource::Unknown);
constexpr auto kMouse = static_cast<int>(ScrollSource::Mouse);
constexpr auto kTrackpad = static_cast<int>(ScrollSource::Trackpad);

constexpr int kNoText = -1;
constexpr int kTextOnlyLimit = static_cast<int>(ClipboardSharePolicy::kTextOnlyMaxTextBytes);

// Fills \p clipboard with text of \p textBytes bytes (none for kNoText), and HTML and a bitmap if asked for
void fill(Clipboard &clipboard, int textBytes, bool html = false, bool bitmap = false)
{
  clipboard.open(0);
  clipboard.empty();
  if (textBytes != kNoText)
    clipboard.add(IClipboard::Format::Text, std::string(static_cast<size_t>(textBytes), 'a'));
  if (html)
    clipboard.add(IClipboard::Format::HTML, "<b>bold</b>");
  if (bitmap)
    clipboard.add(IClipboard::Format::Bitmap, std::string(4096, 'x'));
  clipboard.close();
}

// What a clipboard that holds this is when it is marshalled
std::string marshalled(int textBytes, bool html = false, bool bitmap = false)
{
  Clipboard clipboard;
  fill(clipboard, textBytes, html, bitmap);
  return clipboard.marshall();
}

// The clipboard size option (CLSZ) a server reads from a configuration of \p configuredMiB MiB
OptionValue sizeOption(uint32_t configuredMiB, bool textOnly)
{
  return static_cast<OptionValue>(ClipboardSharePolicy::sizeOptionKiB(configuredMiB, textOnly));
}

// A clipboard that counts what is read from it
class CountingClipboard : public Clipboard
{
public:
  bool has(Format format) const override
  {
    ++m_has[static_cast<size_t>(format)];
    return Clipboard::has(format);
  }

  std::string get(Format format) const override
  {
    ++m_get[static_cast<size_t>(format)];
    return Clipboard::get(format);
  }

  int hasCalls(Format format) const
  {
    return m_has[static_cast<size_t>(format)];
  }

  int getCalls(Format format) const
  {
    return m_get[static_cast<size_t>(format)];
  }

private:
  mutable std::array<int, static_cast<size_t>(Format::TotalFormats)> m_has{};
  mutable std::array<int, static_cast<size_t>(Format::TotalFormats)> m_get{};
};

// The primary computer's platform: its pasteboard is what the Server reads, and what it writes is recorded
class ClipboardPlatform : public MockPlatformComputer
{
public:
  using MockPlatformComputer::MockPlatformComputer;

  bool getClipboard(ClipboardID, IClipboard *dst) const override
  {
    lastSizeLimit = dst->sizeLimit();
    return IClipboard::copy(dst, &pasteboard);
  }

  bool setClipboard(ClipboardID id, const IClipboard *src) override
  {
    if (src != nullptr)
      received[id] = IClipboard::marshall(src);
    return true;
  }

  CountingClipboard pasteboard;
  std::array<std::optional<std::string>, kClipboardEnd> received;
  // the size limit the Server told the last clipboard it read to, which a platform can use to not read what is too
  // large
  mutable size_t lastSizeLimit = 0;
};

// A stream that goes nowhere: the Server writes to the stream of a client it closes
class NullStream : public deskflow::IStream
{
public:
  void close() override
  {
  }
  uint32_t read(void *, uint32_t) override
  {
    return 0;
  }
  void write(const void *, uint32_t) override
  {
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
    return const_cast<NullStream *>(this);
  }
  bool isReady() const override
  {
    return false;
  }
  uint32_t getSize() const override
  {
    return 0;
  }
};

// A remote computer: its own clipboard is what the Server reads when it is said to have changed, and what the Server
// asks it to set is recorded
class RecordingClient : public ClientProxy
{
public:
  explicit RecordingClient(const std::string &name) : ClientProxy(name, new NullStream)
  {
  }

  Clipboard &own(ClipboardID id)
  {
    return m_own[id];
  }

  // what the Server last asked this computer to put on its clipboard, marshalled
  const std::optional<std::string> &received(ClipboardID id) const
  {
    return m_received[id];
  }

  bool getClipboard(ClipboardID id, IClipboard *dst) const override
  {
    return IClipboard::copy(dst, &m_own[id]);
  }

  void setClipboard(ClipboardID id, const IClipboard *src) override
  {
    // like a proxy, a computer is only sent a clipboard it does not have yet
    if (m_dirty[id]) {
      m_dirty[id] = false;
      m_received[id] = IClipboard::marshall(src);
    }
  }

  void grabClipboard(ClipboardID id) override
  {
    m_dirty[id] = true;
    ++m_grabs[id];
  }

  // how many times the Server told this computer that another one took the clipboard
  int grabs(ClipboardID id) const
  {
    return m_grabs[id];
  }

  // the options the Server last sent, as id and value pairs
  const OptionsList &options() const
  {
    return m_options;
  }

  void setClipboardDirty(ClipboardID id, bool dirty) override
  {
    m_dirty[id] = dirty;
  }

  void getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const override
  {
    x = 0;
    y = 0;
    width = 1920;
    height = 1080;
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = 960;
    y = 540;
  }
  void enter(int32_t, int32_t, uint32_t, KeyModifierMask, bool) override
  {
  }
  bool leave() override
  {
    return true;
  }
  void keyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override
  {
  }
  void keyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override
  {
  }
  void keyUp(KeyID, KeyModifierMask, KeyButton) override
  {
  }
  void mouseDown(ButtonID) override
  {
  }
  void mouseUp(ButtonID) override
  {
  }
  void mouseMove(int32_t, int32_t) override
  {
  }
  void mouseRelativeMove(int32_t, int32_t) override
  {
  }
  void mouseWheel(int32_t, int32_t) override
  {
  }
  void screensaver(bool) override
  {
  }
  void resetOptions() override
  {
  }
  void setOptions(const OptionsList &options) override
  {
    m_options = options;
  }
  void sendDragInfo(uint32_t, const char *, size_t) override
  {
  }
  void fileChunkSending(uint8_t, char *, size_t) override
  {
  }
  void secureInputNotification(const std::string &) const override
  {
  }
  std::string getSecureInputApp() const override
  {
    return {};
  }

private:
  OptionsList m_options;
  std::array<int, kClipboardEnd> m_grabs{};
  std::array<Clipboard, kClipboardEnd> m_own;
  std::array<bool, kClipboardEnd> m_dirty{};
  std::array<std::optional<std::string>, kClipboardEnd> m_received;
};

// Keeps the INFO lines that are logged
class InfoLines : public ILogOutputter
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
    if (level == LogLevel::Level::Info)
      lines.append(message);
    return false;
  }

  int count(const QString &text) const
  {
    return static_cast<int>(std::count_if(lines.begin(), lines.end(), [&](const QString &line) {
      return line.contains(text);
    }));
  }

  QStringList lines;
};

// A real Server whose primary computer has a pasteboard to read, with two remote computers connected, "A" and "B"
struct ClipboardRig
{
  DispatchingEventQueue events;
  deskflow::server::Config config{&events};
  ClipboardPlatform *platform = nullptr;
  std::unique_ptr<deskflow::Computer> computer;
  std::unique_ptr<PrimaryClient> primary;
  std::unique_ptr<Server> server;
  // the Server owns the remote computers from the moment it adopts them
  RecordingClient *a = nullptr;
  RecordingClient *b = nullptr;

  // \p sizeOption is the clipboard size option the server is configured with, if any
  // \p clipboardOff names the computers that are set to not share their clipboard
  explicit ClipboardRig(
      std::optional<OptionValue> sizeOption = std::nullopt, std::initializer_list<const char *> clipboardOff = {}
  )
  {
    config.addComputer("primary");
    config.addComputer("A");
    config.addComputer("B");
    if (sizeOption)
      config.addOption("", kOptionClipboardSharingSize, *sizeOption);
    for (const char *name : clipboardOff)
      config.addOption(name, kOptionClipboardSharing, 0);

    platform = new ClipboardPlatform(&events);
    computer = std::make_unique<deskflow::Computer>(platform, &events);
    primary = std::make_unique<PrimaryClient>("primary", computer.get());
    server = std::make_unique<Server>(config, primary.get(), computer.get(), &events);

    a = new RecordingClient("A");
    b = new RecordingClient("B");
    server->adoptClient(a);
    server->adoptClient(b);
  }

  // Delivers an event to the Server the way the event queue does; takes ownership of \p data
  bool dispatch(EventTypes type, void *target, void *data)
  {
    const Event event(type, target, data);
    const auto dispatched = events.dispatchEvent(event);
    Event::deleteData(event);
    return dispatched;
  }

  // The cursor moves to the computer called \p name
  bool switchTo(const char *name)
  {
    // the Server reads the data of this event as plain data, as the input filter sends it
    auto info = std::make_unique<Server::SwitchToComputerInfo>(name);
    const Event event(
        EventTypes::ServerSwitchToComputer, config.getInputFilter(), info.get(), Event::EventFlags::DontFreeData
    );
    return events.dispatchEvent(event);
  }

  void clipboardEvent(EventTypes type, BaseClientProxy &computer, ClipboardID id)
  {
    auto *info = static_cast<IComputer::ClipboardInfo *>(std::malloc(sizeof(IComputer::ClipboardInfo)));
    info->m_id = id;
    info->m_sequenceNumber = 0;
    QVERIFY2(dispatch(type, computer.getEventTarget(), info), "the Server did not handle the clipboard event");
  }

  // \p computer says it took the clipboard
  void grab(BaseClientProxy &computer, ClipboardID id)
  {
    clipboardEvent(EventTypes::ClipboardGrabbed, computer, id);
  }

  // \p computer says that what is on its clipboard changed
  void changed(BaseClientProxy &computer, ClipboardID id)
  {
    clipboardEvent(EventTypes::ClipboardChanged, computer, id);
  }
};

} // namespace

void ServerTests::initTestCase()
{
  // the Config in these tests reads per computer settings
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  m_ipcServer = new deskflow::core::ipc::CoreIpcServer(this);
}

void ServerTests::cleanup()
{
  Settings::setValue(Settings::Server::ClipboardTextOnly);
}

void ServerTests::SwitchToComputerInfo_alloc_computer()
{
  auto actual = new Server::SwitchToComputerInfo("test");
  QCOMPARE(actual->m_computer, "test");
  delete actual;
}

void ServerTests::KeyboardBroadcastInfo_alloc_stateAndComputers()
{
  auto info = new Server::KeyboardBroadcastInfo(Server::KeyboardBroadcastInfo::State::kOn, "test");
  QCOMPARE(info->m_state, Server::KeyboardBroadcastInfo::State::kOn);
  QCOMPARE(info->m_computers, "test");
  delete info;
}

void ServerTests::WheelInfo_alloc_defaultsToUnknownSource()
{
  const WheelInfoPtr info(IPrimaryComputer::WheelInfo::alloc(3, -5));
  QCOMPARE(info->m_xDelta, 3);
  QCOMPARE(info->m_yDelta, -5);
  QCOMPARE(static_cast<int>(info->m_source), kUnknown);
}

void ServerTests::WheelInfo_alloc_keepsExplicitSource_data()
{
  QTest::addColumn<int>("source");

  QTest::newRow("unknown") << kUnknown;
  QTest::newRow("mouse") << kMouse;
  QTest::newRow("trackpad") << kTrackpad;
}

void ServerTests::WheelInfo_alloc_keepsExplicitSource()
{
  QFETCH(int, source);

  const WheelInfoPtr info(IPrimaryComputer::WheelInfo::alloc(3, -5, static_cast<ScrollSource>(source)));
  QCOMPARE(info->m_xDelta, 3);
  QCOMPARE(info->m_yDelta, -5);
  QCOMPARE(static_cast<int>(info->m_source), source);
}

// The classification is made by the primary computer; losing it anywhere between its wheel event and the active
// client silently turns "mouse wheel only" scroll inversion off, because an unknown source is never inverted.
void ServerTests::wheelEvent_relaysScrollSourceToActiveClient_data()
{
  QTest::addColumn<bool>("withSource");
  QTest::addColumn<int>("source");
  QTest::addColumn<int>("expected");

  QTest::newRow("mouse") << true << kMouse << kMouse;
  QTest::newRow("trackpad") << true << kTrackpad << kTrackpad;
  QTest::newRow("explicit unknown") << true << kUnknown << kUnknown;
  QTest::newRow("two argument alloc") << false << kMouse << kUnknown;
}

void ServerTests::wheelEvent_relaysScrollSourceToActiveClient()
{
  QFETCH(bool, withSource);
  QFETCH(int, source);
  QFETCH(int, expected);

  ServerRig rig;
  QVERIFY2(
      rig.sendWheel(
          withSource ? IPrimaryComputer::WheelInfo::alloc(3, -5, static_cast<ScrollSource>(source))
                     : IPrimaryComputer::WheelInfo::alloc(3, -5)
      ),
      "the Server did not handle the primary computer's wheel event"
  );

  const auto &wheels = rig.primary->wheels();
  QCOMPARE(static_cast<int>(wheels.size()), 1);
  QCOMPARE(wheels.front().x, 3);
  QCOMPARE(wheels.front().y, -5);
  QVERIFY2(wheels.front().withSource, "the wheel event reached the client without its source");
  QCOMPARE(static_cast<int>(wheels.front().source), expected);
}

void ServerTests::edgeOvershoot_clampsRouteFraction()
{
  QCOMPARE(deskflow::server::getCursorEdgeFraction(-1, 0, 1080), 0.0f);
  QCOMPARE(deskflow::server::getCursorEdgeFraction(1080, 0, 1080), std::nextafter(1.0f, 0.0f));
  QCOMPARE(deskflow::server::getCursorEdgeFraction(540, 0, 1080), 540.5f / 1080.0f);
}

void ServerTests::namedTopology_topLeftOvershoot_selectsLeftComputer()
{
  using namespace deskflow::server;

  Config config(nullptr);
  QVERIFY(config.addComputer("Subeoms-MacBook-Pro.local"));
  QVERIFY(config.addComputer("SUBEOM-COPILOT"));
  QVERIFY(config.addComputer("9800X3D-5080"));
  QVERIFY(config.addComputer("Subeoms-Mac-mini.local"));
  QVERIFY(config.connect("Subeoms-MacBook-Pro.local", Direction::Left, 0.0f, 1.0f, "SUBEOM-COPILOT", 0.0f, 1.0f));
  QVERIFY(config.connect("Subeoms-MacBook-Pro.local", Direction::Top, 0.0f, 1.0f, "9800X3D-5080", 0.0f, 1.0f));
  QVERIFY(config.connect("9800X3D-5080", Direction::Right, 0.0f, 1.0f, "Subeoms-Mac-mini.local", 0.0f, 1.0f));

  const auto rawLeftPosition = -0.5f / 1080.0f;
  const auto rawTopPosition = 0.5f / 1920.0f;
  QCOMPARE(
      config.getNeighbor("Subeoms-MacBook-Pro.local", Direction::Left, rawLeftPosition, nullptr),
      std::string("SUBEOM-COPILOT")
  );
  QCOMPARE(
      config.getNeighbor("Subeoms-MacBook-Pro.local", Direction::Top, rawTopPosition, nullptr),
      std::string("9800X3D-5080")
  );

  const auto position = getCursorEdgeFraction(-1, 0, 1080);

  QCOMPARE(
      config.getNeighbor("Subeoms-MacBook-Pro.local", Direction::Left, position, nullptr), std::string("SUBEOM-COPILOT")
  );
}

void ServerTests::neighborTraversal_disconnectedCycleStops()
{
  using namespace deskflow::server;

  Config config(nullptr);
  QVERIFY(config.addComputer("A"));
  QVERIFY(config.addComputer("B"));
  QVERIFY(config.addComputer("C"));
  QVERIFY(config.connect("A", Direction::Right, 0.0f, 1.0f, "B", 0.0f, 1.0f));
  QVERIFY(config.connect("B", Direction::Right, 0.0f, 1.0f, "C", 0.0f, 1.0f));
  QVERIFY(config.connect("C", Direction::Right, 0.0f, 1.0f, "B", 0.0f, 1.0f));

  const auto result = traverseConnectedNeighbor(
      "A", 0.5f,
      [&config](const std::string &source, float position) -> std::optional<NeighborPosition> {
        float positionOut = 0.0f;
        const auto destination = config.getNeighbor(source, Direction::Right, position, &positionOut);
        if (destination.empty()) {
          return std::nullopt;
        }
        return NeighborPosition{destination, positionOut};
      },
      [](const std::string &computer) { return computer == "A"; }, [](const std::string &, const NeighborPosition &) {}
  );

  QCOMPARE(result.status, NeighborTraversalStatus::Cycle);
}

// What the Server shares is decided when the cursor leaves the primary computer or a computer says its clipboard
// changed, so the tests use the clipboards the way they are used: a computer says it took the clipboard (and the rest
// is told to give up theirs), then the cursor moves, or the computer says what is on its clipboard now.
namespace {

QByteArray bytes(const std::string &data)
{
  return QByteArray::fromStdString(data);
}

// What a computer was last asked to put on its clipboard, in a form that shows in a failure
QByteArray received(const std::optional<std::string> &data)
{
  return data ? bytes(*data) : QByteArray("<nothing>");
}

// Whether a computer was told anything that is a clipboard: it has not been, or it was asked to give up its own
bool neverReceivedAnything(const std::optional<std::string> &data)
{
  return !data || *data == std::string(4, '\0');
}

} // namespace

void ServerTests::clipboardOff_sharesEveryFormatOfThePrimary()
{
  ClipboardRig rig(sizeOption(3, false));
  fill(rig.platform->pasteboard, 40, true, true);
  rig.grab(*rig.primary, kClipboardClipboard);
  QVERIFY(rig.switchTo("A"));

  // off is what it always was: the clipboard goes as a whole, with every format it holds
  QCOMPARE(received(rig.a->received(kClipboardClipboard)), bytes(marshalled(40, true, true)));
  QCOMPARE(received(rig.a->received(kClipboardSelection)), bytes(marshalled(40, true, true)));
}

void ServerTests::clipboardOff_theSizeLimitStillApplies()
{
  // a limit of 1 KiB: a clipboard that fits is shared, and one that does not is not, as it always was
  {
    ClipboardRig rig(1);
    fill(rig.platform->pasteboard, 500, true, false);
    rig.grab(*rig.primary, kClipboardClipboard);
    QVERIFY(rig.switchTo("A"));
    QCOMPARE(received(rig.a->received(kClipboardClipboard)), bytes(marshalled(500, true, false)));
  }
  {
    ClipboardRig rig(1);
    fill(rig.platform->pasteboard, 2000, false, false);
    rig.grab(*rig.primary, kClipboardClipboard);
    QVERIFY(rig.switchTo("A"));
    QVERIFY(neverReceivedAnything(rig.a->received(kClipboardClipboard)));
  }
}

void ServerTests::clipboardTextOnly_primaryToClient_sharesTextUpToTheLimit_data()
{
  QTest::addColumn<int>("textBytes");

  QTest::newRow("text 0 B") << 0;
  QTest::newRow("text 1 B") << 1;
  QTest::newRow("text that marshals to exactly 10240 B") << kTextOnlyLimit;
}

void ServerTests::clipboardTextOnly_primaryToClient_sharesTextUpToTheLimit()
{
  QFETCH(int, textBytes);

  // with the size option a real server sends to its peers, and without it: the policy does not rely on it
  for (const bool withSizeOption : {true, false}) {
    Settings::setValue(Settings::Server::ClipboardTextOnly, true);
    ClipboardRig rig(withSizeOption ? std::optional<OptionValue>(sizeOption(3, true)) : std::nullopt);
    fill(rig.platform->pasteboard, textBytes);
    rig.grab(*rig.primary, kClipboardClipboard);
    QVERIFY(rig.switchTo("A"));

    QCOMPARE(received(rig.a->received(kClipboardClipboard)), bytes(marshalled(textBytes)));
    QCOMPARE(received(rig.a->received(kClipboardSelection)), bytes(marshalled(textBytes)));
  }
}

void ServerTests::clipboardTextOnly_primaryToClient_sharesNothingElse_data()
{
  QTest::addColumn<int>("textBytes");
  QTest::addColumn<bool>("html");
  QTest::addColumn<bool>("bitmap");

  QTest::newRow("text of 10229 B") << kTextOnlyLimit + 1 << false << false;
  QTest::newRow("text of exactly 10240 B") << 10 * 1024 << false << false;
  QTest::newRow("text of 1 MiB with HTML and a bitmap") << 1024 * 1024 << true << true;
  QTest::newRow("bitmap only") << kNoText << false << true;
  QTest::newRow("HTML only") << kNoText << true << false;
  QTest::newRow("HTML and bitmap") << kNoText << true << true;
  QTest::newRow("nothing") << kNoText << false << false;
}

void ServerTests::clipboardTextOnly_primaryToClient_sharesNothingElse()
{
  QFETCH(int, textBytes);
  QFETCH(bool, html);
  QFETCH(bool, bitmap);

  for (const bool withSizeOption : {true, false}) {
    Settings::setValue(Settings::Server::ClipboardTextOnly, true);
    ClipboardRig rig(withSizeOption ? std::optional<OptionValue>(sizeOption(3, true)) : std::nullopt);
    fill(rig.platform->pasteboard, textBytes, html, bitmap);
    rig.grab(*rig.primary, kClipboardClipboard);
    QVERIFY(rig.switchTo("A"));

    for (const auto id : {kClipboardClipboard, kClipboardSelection})
      QVERIFY2(
          neverReceivedAnything(rig.a->received(id)), qPrintable(QString::fromLatin1(received(rig.a->received(id))))
      );
  }
}

void ServerTests::clipboardTextOnly_primaryToClient_stripsOtherFormats()
{
  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  ClipboardRig rig(sizeOption(3, true));
  fill(rig.platform->pasteboard, 40, true, true);
  rig.grab(*rig.primary, kClipboardClipboard);
  QVERIFY(rig.switchTo("A"));

  // the text goes, whole; the HTML and the bitmap stay where they are
  QCOMPARE(received(rig.a->received(kClipboardClipboard)), bytes(marshalled(40)));
  QCOMPARE(received(rig.a->received(kClipboardSelection)), bytes(marshalled(40)));
}

// A platform converts an image to a bitmap, which is far more than a text costs, only when it is asked for it
void ServerTests::clipboardTextOnly_primaryToClient_neverReadsHtmlOrBitmap()
{
  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  ClipboardRig rig(sizeOption(3, true));
  fill(rig.platform->pasteboard, 40, true, true);
  rig.grab(*rig.primary, kClipboardClipboard);
  QVERIFY(rig.switchTo("A"));

  const auto &pasteboard = rig.platform->pasteboard;
  QVERIFY(pasteboard.getCalls(IClipboard::Format::Text) > 0);
  QCOMPARE(pasteboard.getCalls(IClipboard::Format::HTML), 0);
  QCOMPARE(pasteboard.getCalls(IClipboard::Format::Bitmap), 0);
  QCOMPARE(pasteboard.hasCalls(IClipboard::Format::HTML), 0);
  QCOMPARE(pasteboard.hasCalls(IClipboard::Format::Bitmap), 0);
}

void ServerTests::clipboardOff_primaryToClient_readsEveryFormat()
{
  ClipboardRig rig(sizeOption(3, false));
  fill(rig.platform->pasteboard, 40, true, true);
  rig.grab(*rig.primary, kClipboardClipboard);
  QVERIFY(rig.switchTo("A"));

  const auto &pasteboard = rig.platform->pasteboard;
  QVERIFY(pasteboard.getCalls(IClipboard::Format::Text) > 0);
  QVERIFY(pasteboard.getCalls(IClipboard::Format::HTML) > 0);
  QVERIFY(pasteboard.getCalls(IClipboard::Format::Bitmap) > 0);
}

// An old client sends whatever it has that fits in the size option: HTML and a bitmap along with the text
void ServerTests::clipboardTextOnly_clientToPrimary_stripsOtherFormats()
{
  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  ClipboardRig rig(sizeOption(3, true));
  fill(rig.a->own(kClipboardClipboard), 30, true, true);
  rig.grab(*rig.a, kClipboardClipboard);
  rig.changed(*rig.a, kClipboardClipboard);

  // the pasteboard of the server only gets the text
  QCOMPARE(received(rig.platform->received[kClipboardClipboard]), bytes(marshalled(30)));

  // and so does the next computer the cursor enters
  QVERIFY(rig.switchTo("B"));
  QCOMPARE(received(rig.b->received(kClipboardClipboard)), bytes(marshalled(30)));
}

void ServerTests::clipboardTextOnly_clientToPrimary_sharesNothingElse_data()
{
  QTest::addColumn<int>("textBytes");
  QTest::addColumn<bool>("html");
  QTest::addColumn<bool>("bitmap");

  QTest::newRow("text of 10229 B") << kTextOnlyLimit + 1 << false << false;
  QTest::newRow("long text with HTML") << 1024 * 1024 << true << false;
  QTest::newRow("bitmap only") << kNoText << false << true;
  QTest::newRow("HTML only") << kNoText << true << false;
}

void ServerTests::clipboardTextOnly_clientToPrimary_sharesNothingElse()
{
  QFETCH(int, textBytes);
  QFETCH(bool, html);
  QFETCH(bool, bitmap);

  for (const bool withSizeOption : {true, false}) {
    Settings::setValue(Settings::Server::ClipboardTextOnly, true);
    ClipboardRig rig(withSizeOption ? std::optional<OptionValue>(sizeOption(3, true)) : std::nullopt);
    fill(rig.a->own(kClipboardClipboard), textBytes, html, bitmap);
    rig.grab(*rig.a, kClipboardClipboard);
    rig.changed(*rig.a, kClipboardClipboard);

    QVERIFY2(
        !rig.platform->received[kClipboardClipboard].has_value(),
        qPrintable(QString::fromLatin1(received(rig.platform->received[kClipboardClipboard])))
    );

    QVERIFY(rig.switchTo("B"));
    QVERIFY(neverReceivedAnything(rig.b->received(kClipboardClipboard)));
  }
}

void ServerTests::clipboardTextOnly_clientToClient_forwardsTheTextOnly()
{
  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  ClipboardRig rig(sizeOption(3, true));
  QVERIFY(rig.switchTo("B"));

  fill(rig.a->own(kClipboardClipboard), 25, true, true);
  rig.grab(*rig.a, kClipboardClipboard);
  rig.changed(*rig.a, kClipboardClipboard);

  // what one computer copies and the cursor is on another: the server passes on the text only
  QCOMPARE(received(rig.b->received(kClipboardClipboard)), bytes(marshalled(25)));
}

void ServerTests::clipboardTextOnly_clientToClient_forwardsNothingElse_data()
{
  QTest::addColumn<int>("textBytes");
  QTest::addColumn<bool>("html");
  QTest::addColumn<bool>("bitmap");

  QTest::newRow("text of 10229 B") << kTextOnlyLimit + 1 << false << false;
  QTest::newRow("long text with HTML and a bitmap") << 1024 * 1024 << true << true;
  QTest::newRow("bitmap only") << kNoText << false << true;
  QTest::newRow("HTML only") << kNoText << true << false;
}

void ServerTests::clipboardTextOnly_clientToClient_forwardsNothingElse()
{
  QFETCH(int, textBytes);
  QFETCH(bool, html);
  QFETCH(bool, bitmap);

  for (const bool withSizeOption : {true, false}) {
    Settings::setValue(Settings::Server::ClipboardTextOnly, true);
    ClipboardRig rig(withSizeOption ? std::optional<OptionValue>(sizeOption(3, true)) : std::nullopt);
    QVERIFY(rig.switchTo("B"));

    fill(rig.a->own(kClipboardClipboard), textBytes, html, bitmap);
    rig.grab(*rig.a, kClipboardClipboard);
    rig.changed(*rig.a, kClipboardClipboard);

    QVERIFY2(
        !rig.b->received(kClipboardClipboard).has_value(),
        qPrintable(QString::fromLatin1(received(rig.b->received(kClipboardClipboard))))
    );
  }
}

// The setting is read when the configuration is: a server that was sharing everything and is told to share text only
// does not hand out what it has kept
void ServerTests::clipboardTextOnly_turnedOnLater_stripsWhatIsCached()
{
  ClipboardRig rig(sizeOption(3, false));
  QVERIFY(rig.switchTo("A"));
  fill(rig.a->own(kClipboardClipboard), 30, true, true);
  rig.grab(*rig.a, kClipboardClipboard);
  rig.changed(*rig.a, kClipboardClipboard);

  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  QVERIFY(rig.server->setConfig(rig.config));

  QVERIFY(rig.switchTo("B"));
  QCOMPARE(received(rig.b->received(kClipboardClipboard)), bytes(marshalled(30)));
}

// A computer set to not share its clipboard is left out of every clipboard path of the Server. What the others share
// among them is not touched.
void ServerTests::clipboardOffForAComputer_primaryToTheOthers_skipsOnlyThatComputer()
{
  ClipboardRig rig(std::nullopt, {"A"});
  fill(rig.platform->pasteboard, 40, true, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  rig.grab(*rig.primary, kClipboardSelection);

  // it is not told about the grab, and it gets no data when the cursor enters it
  QCOMPARE(rig.a->grabs(kClipboardClipboard), 0);
  QCOMPARE(rig.a->grabs(kClipboardSelection), 0);
  QCOMPARE(rig.b->grabs(kClipboardClipboard), 1);
  QVERIFY(rig.switchTo("A"));
  QVERIFY(neverReceivedAnything(rig.a->received(kClipboardClipboard)));
  QVERIFY(neverReceivedAnything(rig.a->received(kClipboardSelection)));

  // the primary's clipboard is still current when the cursor goes on to a computer that shares
  QVERIFY(rig.switchTo("B"));
  QCOMPARE(received(rig.b->received(kClipboardClipboard)), bytes(marshalled(40, true, false)));
}

void ServerTests::clipboardOffForAComputer_whatItCopies_isNeitherSharedNorAnnounced()
{
  ClipboardRig rig(std::nullopt, {"A"});
  fill(rig.a->own(kClipboardClipboard), 30, false, false);
  rig.grab(*rig.a, kClipboardClipboard);
  rig.changed(*rig.a, kClipboardClipboard);

  // nobody is told that it took the clipboard, and its data reaches nobody
  QCOMPARE(rig.b->grabs(kClipboardClipboard), 0);
  QVERIFY(!rig.platform->received[kClipboardClipboard].has_value());
  QVERIFY(rig.switchTo("B"));
  QVERIFY(neverReceivedAnything(rig.b->received(kClipboardClipboard)));
}

// The Server starts with empty clipboards that the primary computer owns, and a primary that does not share is never
// read, so those would be sent: and a computer that is sent an empty clipboard empties its own
void ServerTests::clipboardOffForAComputer_aPrimaryThatDoesNotShare_doesNotEmptyTheOthers()
{
  ClipboardRig rig(std::nullopt, {"primary"});
  // a computer that has just connected is sent what the Server has (as the proxy of a client is when it is created)
  for (ClipboardID id = 0; id < kClipboardEnd; ++id)
    rig.a->setClipboardDirty(id, true);
  QVERIFY(rig.switchTo("A"));
  QVERIFY(!rig.a->received(kClipboardClipboard).has_value());
  QVERIFY(!rig.a->received(kClipboardSelection).has_value());

  // the others still share among themselves
  fill(rig.a->own(kClipboardClipboard), 20, false, false);
  rig.grab(*rig.a, kClipboardClipboard);
  rig.changed(*rig.a, kClipboardClipboard);
  QVERIFY(rig.switchTo("B"));
  QCOMPARE(received(rig.b->received(kClipboardClipboard)), bytes(marshalled(20)));
}

void ServerTests::clipboardOffForAComputer_theOthersStillShareAmongThemselves()
{
  ClipboardRig rig(std::nullopt, {"A"});
  QVERIFY(rig.switchTo("B"));
  fill(rig.b->own(kClipboardClipboard), 25, true, false);
  rig.grab(*rig.b, kClipboardClipboard);
  rig.changed(*rig.b, kClipboardClipboard);

  // the primary gets it when the cursor comes back, and the computer that is left out never does
  QCOMPARE(rig.a->grabs(kClipboardClipboard), 0);
  QVERIFY(rig.switchTo("primary"));
  QCOMPARE(received(rig.platform->received[kClipboardClipboard]), bytes(marshalled(25, true, false)));
  QVERIFY(rig.switchTo("A"));
  QVERIFY(neverReceivedAnything(rig.a->received(kClipboardClipboard)));
}

void ServerTests::clipboardOffForAComputer_isOffForAllComputersWhenNoneIsSetOff()
{
  // the default: nothing is left out, and nothing about a computer is sent that was not before
  ClipboardRig rig;
  fill(rig.platform->pasteboard, 40, false, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  QCOMPARE(rig.a->grabs(kClipboardClipboard), 1);
  QCOMPARE(rig.b->grabs(kClipboardClipboard), 1);
  QVERIFY(rig.switchTo("A"));
  QCOMPARE(received(rig.a->received(kClipboardClipboard)), bytes(marshalled(40)));
}

void ServerTests::clipboardOffForAComputer_isToldWithAnOptionThatWinsOverTheGlobalOne()
{
  // the global option comes after the computer's own in what is sent, and the last one a client reads wins
  auto lastSharing = [](const OptionsList &options) {
    OptionValue value = -1;
    for (size_t i = 0; i + 1 < options.size(); i += 2) {
      if (options[i] == kOptionClipboardSharing)
        value = static_cast<OptionValue>(options[i + 1]);
    }
    return value;
  };

  OptionsList withOff;
  OptionsList withoutOff;
  {
    ClipboardRig rig(std::nullopt, {"A"});
    rig.config.addOption("", kOptionClipboardSharing, 1);
    QVERIFY(rig.server->setConfig(rig.config));
    withOff = rig.b->options();
    QCOMPARE(lastSharing(rig.a->options()), 0);
    QCOMPARE(lastSharing(rig.b->options()), 1);
  }
  {
    ClipboardRig rig;
    rig.config.addOption("", kOptionClipboardSharing, 1);
    QVERIFY(rig.server->setConfig(rig.config));
    withoutOff = rig.b->options();
  }
  // a computer that shares is sent exactly what it was before the setting existed
  QCOMPARE(withOff, withoutOff);
}

// Text only replaces the size box with a fixed limit, so a 0 in the greyed out box must not keep sharing off
void ServerTests::clipboardTextOnly_withASizeBoxOfZero_stillSharesText()
{
  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  ClipboardRig rig(sizeOption(0, true));
  fill(rig.platform->pasteboard, 30, true, true);
  rig.grab(*rig.primary, kClipboardClipboard);
  QVERIFY(rig.switchTo("A"));
  QCOMPARE(received(rig.a->received(kClipboardClipboard)), bytes(marshalled(30)));
}

void ServerTests::clipboardOff_withASizeBoxOfZero_sharesNothing()
{
  ClipboardRig rig(sizeOption(0, false));
  fill(rig.platform->pasteboard, 30, false, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  QVERIFY(rig.switchTo("A"));
  QVERIFY(neverReceivedAnything(rig.a->received(kClipboardClipboard)));
}

// A clipboard that is not shared is read again on every switch: it is reported once at INFO level, and for the
// clipboard only, because on Windows and macOS the selection is read from the same pasteboard and would repeat the line
void ServerTests::clipboardTextOnly_anEmptyClipboard_isReportedAtInfoLevelOnce()
{
  InfoLines info;
  m_log.insert(&info, true);
  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  ClipboardRig rig(sizeOption(3, true));
  fill(rig.platform->pasteboard, kNoText, false, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  rig.grab(*rig.primary, kClipboardSelection);
  QVERIFY(rig.switchTo("A"));
  QVERIFY(rig.switchTo("primary"));
  QVERIFY(rig.switchTo("B"));
  m_log.remove(&info);

  QCOMPARE(info.count(QStringLiteral("not sharing clipboard 0")), 1);
  QCOMPARE(info.count(QStringLiteral("not sharing clipboard 1")), 0);
}

void ServerTests::clipboardTextOnly_anImage_isReportedAtInfoLevelOnce()
{
  InfoLines info;
  m_log.insert(&info, true);
  Settings::setValue(Settings::Server::ClipboardTextOnly, true);
  ClipboardRig rig(sizeOption(3, true));
  fill(rig.platform->pasteboard, kNoText, false, true);
  rig.grab(*rig.primary, kClipboardClipboard);
  rig.grab(*rig.primary, kClipboardSelection);
  QVERIFY(rig.switchTo("A"));
  QVERIFY(rig.switchTo("primary"));
  QVERIFY(rig.switchTo("B"));
  m_log.remove(&info);

  QCOMPARE(info.count(QStringLiteral("not sharing clipboard 0")), 1);
  QCOMPARE(info.count(QStringLiteral("not sharing clipboard 1")), 0);
}

// Another program that writes the clipboard of the primary computer with what is already on it (Handoff and Universal
// Clipboard do this with what Deskflow wrote to the other Mac, a clipboard manager with what it saw) makes the computer
// grab the clipboard again. That is not a new copy: telling every computer to give up its clipboard and sending the
// same data to them again for it is what feeds the loop.
void ServerTests::clipboardRewrite_identicalWhileTheCursorIsAway_isIgnored()
{
  ClipboardRig rig;
  fill(rig.platform->pasteboard, 40, true, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  QVERIFY(rig.switchTo("A"));
  QCOMPARE(received(rig.a->received(kClipboardClipboard)), bytes(marshalled(40, true, false)));
  QCOMPARE(rig.a->grabs(kClipboardClipboard), 1);
  QCOMPARE(rig.b->grabs(kClipboardClipboard), 1);

  // the same content is written again
  InfoLines info;
  m_log.insert(&info, true);
  rig.grab(*rig.primary, kClipboardClipboard);
  m_log.remove(&info);

  QCOMPARE(rig.a->grabs(kClipboardClipboard), 1);
  QCOMPARE(rig.b->grabs(kClipboardClipboard), 1);
  QCOMPARE(info.count(QStringLiteral("clipboard 0 skipped for \"primary\": identical content")), 1);
}

void ServerTests::clipboardRewrite_withNewContentWhileTheCursorIsAway_isStillShared()
{
  ClipboardRig rig;
  fill(rig.platform->pasteboard, 40, true, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  QVERIFY(rig.switchTo("A"));

  fill(rig.platform->pasteboard, 41, true, false);
  rig.grab(*rig.primary, kClipboardClipboard);

  QCOMPARE(rig.a->grabs(kClipboardClipboard), 2);
  QCOMPARE(received(rig.a->received(kClipboardClipboard)), bytes(marshalled(41, true, false)));
}

void ServerTests::clipboardRewrite_rightAfterTheServerWroteTheClipboard_isIgnored()
{
  ClipboardRig rig;
  fill(rig.a->own(kClipboardClipboard), 30, false, false);
  rig.grab(*rig.a, kClipboardClipboard);
  rig.changed(*rig.a, kClipboardClipboard);
  QVERIFY(rig.switchTo("primary"));
  QCOMPARE(received(rig.platform->received[kClipboardClipboard]), bytes(marshalled(30)));
  const auto grabsOfB = rig.b->grabs(kClipboardClipboard);

  // the cursor is on the primary computer, so nothing else tells the rewrite from a copy but that Deskflow just wrote
  // it
  fill(rig.platform->pasteboard, 30, false, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  QCOMPARE(rig.b->grabs(kClipboardClipboard), grabsOfB);

  // something else is a copy
  fill(rig.platform->pasteboard, 31, false, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  QCOMPARE(rig.b->grabs(kClipboardClipboard), grabsOfB + 1);
}

void ServerTests::clipboardRewrite_afterTheWindowHasPassed_isACopy()
{
  ClipboardRig rig;
  rig.server->setClipboardEchoWindow(std::chrono::milliseconds::zero());
  fill(rig.a->own(kClipboardClipboard), 30, false, false);
  rig.grab(*rig.a, kClipboardClipboard);
  rig.changed(*rig.a, kClipboardClipboard);
  QVERIFY(rig.switchTo("primary"));
  const auto grabsOfB = rig.b->grabs(kClipboardClipboard);

  fill(rig.platform->pasteboard, 30, false, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  QCOMPARE(rig.b->grabs(kClipboardClipboard), grabsOfB + 1);
}

// The user copies the same text on another computer to take the clipboard: that is a grab like any other
void ServerTests::clipboardRewrite_theSameTextCopiedOnAnotherComputer_movesTheOwnership()
{
  ClipboardRig rig;
  fill(rig.a->own(kClipboardClipboard), 30, false, false);
  rig.grab(*rig.a, kClipboardClipboard);
  rig.changed(*rig.a, kClipboardClipboard);
  QVERIFY(rig.switchTo("B"));
  QCOMPARE(received(rig.b->received(kClipboardClipboard)), bytes(marshalled(30)));
  const auto grabsOfA = rig.a->grabs(kClipboardClipboard);

  fill(rig.b->own(kClipboardClipboard), 30, false, false);
  rig.grab(*rig.b, kClipboardClipboard);
  rig.changed(*rig.b, kClipboardClipboard);

  // A is told that it is not the owner any more, and the primary gets the text when the cursor comes back
  QCOMPARE(rig.a->grabs(kClipboardClipboard), grabsOfA + 1);
  QVERIFY(rig.switchTo("primary"));
  QCOMPARE(received(rig.platform->received[kClipboardClipboard]), bytes(marshalled(30)));
}

// A platform that has to convert an image to read it can skip an image that cannot be shared, if it is told how large a
// clipboard may be: the Server says so to the clipboard it reads into
void ServerTests::clipboardRead_tellsThePlatformTheSizeLimit()
{
  {
    ClipboardRig rig(sizeOption(3, false));
    fill(rig.platform->pasteboard, 40, false, false);
    rig.grab(*rig.primary, kClipboardClipboard);
    QVERIFY(rig.switchTo("A"));
    QCOMPARE(rig.platform->lastSizeLimit, size_t{3} * 1024 * 1024);
  }
  {
    Settings::setValue(Settings::Server::ClipboardTextOnly, true);
    ClipboardRig rig(sizeOption(3, true));
    fill(rig.platform->pasteboard, 40, false, false);
    rig.grab(*rig.primary, kClipboardClipboard);
    QVERIFY(rig.switchTo("A"));
    QCOMPARE(rig.platform->lastSizeLimit, size_t{10} * 1024);
  }
}

QTEST_MAIN(ServerTests)
