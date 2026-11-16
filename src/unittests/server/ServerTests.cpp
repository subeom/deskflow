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

// The pointer pins: a real Server whose primary and remote computers have shapes of their own, and which records where
// the pointer goes

struct Box
{
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;

  bool operator==(const Box &) const = default;
};

struct Pos
{
  int32_t x;
  int32_t y;

  bool operator==(const Pos &) const = default;
};

// The primary computer's platform with a shape of its own
class ShapedPlatform : public MockPlatformComputer
{
public:
  using MockPlatformComputer::MockPlatformComputer;

  void getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const override
  {
    x = shape.x;
    y = shape.y;
    w = shape.w;
    h = shape.h;
  }

  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = shape.x + shape.w / 2;
    y = shape.y + shape.h / 2;
  }

  Box shape{0, 0, 1728, 1117};
};

// Remembers where the pointer entered the primary computer
class PointerPrimary : public PrimaryClient
{
public:
  using PrimaryClient::PrimaryClient;

  void enter(int32_t x, int32_t y, uint32_t seqNum, KeyModifierMask mask, bool forScreensaver) override
  {
    entered.push_back({x, y});
    PrimaryClient::enter(x, y, seqNum, mask, forScreensaver);
  }

  std::vector<Pos> entered;
};

// A remote computer with a shape of its own, which remembers where the pointer entered it and every move it was told
class PointerClient : public RecordingClient
{
public:
  PointerClient(const std::string &name, const Box &box) : RecordingClient(name), shape(box)
  {
    cursor = {box.x + box.w / 2, box.y + box.h / 2};
  }

  void getShape(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const override
  {
    x = shape.x;
    y = shape.y;
    w = shape.w;
    h = shape.h;
  }

  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = cursor.x;
    y = cursor.y;
  }

  void enter(int32_t x, int32_t y, uint32_t, KeyModifierMask, bool) override
  {
    entered.push_back({x, y});
  }

  void mouseMove(int32_t x, int32_t y) override
  {
    moved.push_back({x, y});
  }

  Box shape;
  Pos cursor;
  std::vector<Pos> entered;
  std::vector<Pos> moved;
};

constexpr auto kSteps = 6000;

struct PointerRig
{
  DispatchingEventQueue events;
  deskflow::server::Config config{&events};
  ShapedPlatform *platform = nullptr;
  std::unique_ptr<deskflow::Computer> computer;
  std::unique_ptr<PointerPrimary> primary;
  std::unique_ptr<Server> server;
  // the Server owns the remote computers from the moment it adopts them
  std::map<std::string, PointerClient *> clients;

  struct Remote
  {
    std::string name;
    Box shape;
    // configured and not connected
    bool offline = false;
  };

  explicit PointerRig(const Box &primaryShape, const std::vector<Remote> &remotes = {})
  {
    config.addComputer("primary");
    for (const auto &remote : remotes)
      config.addComputer(remote.name);

    platform = new ShapedPlatform(&events);
    platform->shape = primaryShape;
    computer = std::make_unique<deskflow::Computer>(platform, &events);
    primary = std::make_unique<PointerPrimary>("primary", computer.get());
    server = std::make_unique<Server>(config, primary.get(), computer.get(), &events);

    for (const auto &remote : remotes) {
      if (remote.offline)
        continue;
      auto *client = new PointerClient(remote.name, remote.shape);
      clients[remote.name] = client;
      server->adoptClient(client);
    }
  }

  // the two computers are neighbours: \p b is on the \p side of \p a
  void link(const std::string &a, Direction side, const std::string &b)
  {
    using enum Direction;
    const auto opposite = side == Left ? Right : side == Right ? Left : side == Top ? Bottom : Top;
    config.connect(a, side, 0.0f, 1.0f, b, 0.0f, 1.0f);
    config.connect(b, opposite, 0.0f, 1.0f, a, 0.0f, 1.0f);
  }

  PointerClient &client(const std::string &name) const
  {
    return *clients.at(name);
  }

  // the primary computer reports the pointer at \p x, \p y
  void moveOnPrimary(int32_t x, int32_t y)
  {
    send(EventTypes::PrimaryComputerMotionOnPrimary, x, y);
  }

  // the pointer is moved by \p dx, \p dy while it is on a remote computer
  void moveOnSecondary(int32_t dx, int32_t dy)
  {
    send(EventTypes::PrimaryComputerMotionOnSecondary, dx, dy);
  }

  // the cursor jumps to the computer called \p name
  bool switchTo(const char *name)
  {
    auto info = std::make_unique<Server::SwitchToComputerInfo>(name);
    const Event event(
        EventTypes::ServerSwitchToComputer, config.getInputFilter(), info.get(), Event::EventFlags::DontFreeData
    );
    return events.dispatchEvent(event);
  }

  // the remote computer \p name is on the \p x, \p y of its screen, and says so
  void placeCursor(const std::string &name, int32_t x, int32_t y)
  {
    auto &remote = client(name);
    remote.cursor = {x, y};
    reportShape(remote);
  }

  // the remote computer says that its shape changed, to \p box if given
  void reportShape(PointerClient &remote, std::optional<Box> box = std::nullopt)
  {
    if (box)
      remote.shape = *box;
    events.dispatchEvent(Event(EventTypes::ComputerShapeChanged, remote.getEventTarget()));
  }

private:
  void send(EventTypes type, int32_t a, int32_t b)
  {
    const Event event(type, primary->getEventTarget(), IPrimaryComputer::MotionInfo::alloc(a, b));
    events.dispatchEvent(event);
    Event::deleteData(event);
  }
};

// A summary of many numbers that changes when any of them does
class Digest
{
public:
  void add(int32_t value)
  {
    auto bits = static_cast<uint32_t>(value);
    for (int i = 0; i < 4; ++i) {
      m_value ^= (bits >> (8 * i)) & 0xff;
      m_value *= 1099511628211ull;
    }
  }

  void add(const Pos &pos)
  {
    add(pos.x);
    add(pos.y);
  }

  uint64_t value() const
  {
    return m_value;
  }

private:
  uint64_t m_value = 14695981039346656037ull;
};

constexpr std::array<Direction, 4> kSides = {Direction::Left, Direction::Right, Direction::Top, Direction::Bottom};

const char *sideName(Direction side)
{
  using enum Direction;
  return side == Left ? "left" : side == Right ? "right" : side == Top ? "top" : "bottom";
}

bool isHorizontal(Direction side)
{
  return side == Direction::Left || side == Direction::Right;
}

// the screens of the primary computer: a MacBook Pro 14", a MacBook Pro 16" and a MacBook Air
constexpr std::array<Box, 3> kPrimaryShapes = {{{0, 0, 1728, 1117}, {0, 0, 2064, 1347}, {0, 0, 1512, 982}}};

// the screens of a remote computer: plain monitors, the desktop of a Windows PC with a monitor on its left, one with
// a monitor above, a portrait monitor, an ultrawide one, and a laptop
constexpr std::array<Box, 9> kRemoteShapes = {{
    {0, 0, 1920, 1080},
    {0, 0, 2560, 1440},
    {0, 0, 3840, 2160},
    {-1920, 0, 4480, 1440},
    {0, -1080, 2560, 2520},
    {0, 0, 1080, 1920},
    {-2560, -360, 2560, 1440},
    {0, 0, 5120, 1440},
    {0, 0, 1728, 1117},
}};

// the pixel of the edge of \p box on its \p side, \p index pixels along it
Pos edgePixel(const Box &box, Direction side, int32_t index)
{
  using enum Direction;
  switch (side) {
  case Left:
    return {box.x, box.y + index};
  case Right:
    return {box.x + box.w - 1, box.y + index};
  case Top:
    return {box.x + index, box.y};
  default:
    return {box.x + index, box.y + box.h - 1};
  }
}

int32_t edgeLength(const Box &box, Direction side)
{
  return isHorizontal(side) ? box.h : box.w;
}

// the places along an edge of \p length pixels that are looked at one by one
std::array<int32_t, 5> samplesAlong(int32_t length)
{
  return {0, length / 4, length / 2, (length * 3) / 4, length - 1};
}

// "primary" and "C" are neighbours, C being on the \p side of the primary
std::unique_ptr<PointerRig> neighbours(const Box &primary, const Box &remote, Direction side)
{
  auto rig = std::make_unique<PointerRig>(primary, std::vector<PointerRig::Remote>{{"C", remote}});
  rig->link("primary", side, "C");
  return rig;
}

// The pointer goes through the \p side edge of the primary computer, \p index pixels along it: where it enters C, if it
// does, after which it goes back
std::optional<Pos> crossToC(PointerRig &rig, Direction side, int32_t index)
{
  auto &remote = rig.client("C");
  const auto before = remote.entered.size();
  const auto at = edgePixel(rig.platform->shape, side, index);
  rig.moveOnPrimary(at.x, at.y);
  std::optional<Pos> entered;
  if (remote.entered.size() > before)
    entered = remote.entered.back();
  rig.switchTo("primary");
  return entered;
}

// how the pointer got out of C
struct Exit
{
  // the number of moves it took
  int moves = 0;
  // where it entered the primary computer
  Pos entered{0, 0};
  uint64_t everyMove = 0;

  bool operator==(const Exit &) const = default;
};

// The pointer is at \p start on C, which has the primary on its \p side, and is pushed that way by 17 pixels at a time
// and by \p drift pixels along the edge
Exit pushOutOfC(PointerRig &rig, Direction side, const Pos &start, int32_t drift)
{
  auto &remote = rig.client("C");
  rig.switchTo("C");
  rig.placeCursor("C", start.x, start.y);
  remote.moved.clear();
  const auto before = rig.primary->entered.size();
  using enum Direction;
  const int32_t along = side == Left || side == Top ? -17 : 17;
  int moves = 0;
  while (moves < kSteps && rig.primary->entered.size() == before) {
    if (isHorizontal(side))
      rig.moveOnSecondary(along, drift);
    else
      rig.moveOnSecondary(drift, along);
    ++moves;
  }
  Exit exit;
  exit.moves = moves;
  if (rig.primary->entered.size() > before)
    exit.entered = rig.primary->entered.back();
  Digest digest;
  for (const auto &move : remote.moved)
    digest.add(move);
  exit.everyMove = digest.value();
  return exit;
}

// the places on \p box that the pointer starts from
std::array<Pos, 3> startsOn(const Box &box)
{
  return {{
      {box.x + box.w / 2, box.y + box.h / 2},
      {box.x + box.w / 5, box.y + (box.h * 4) / 5},
      {box.x + (box.w * 17) / 20, box.y + box.h / 7},
  }};
}

// every entry into C through every edge pixel of the primary computer, for each of the shapes of C
uint64_t everyEntryDigest(const Box &primary, Direction side)
{
  Digest digest;
  for (const auto &remote : kRemoteShapes) {
    auto rig = neighbours(primary, remote, side);
    for (int32_t index = 0; index < edgeLength(primary, side); ++index) {
      const auto entered = crossToC(*rig, side, index);
      digest.add(entered ? 1 : 0);
      if (entered)
        digest.add(*entered);
    }
  }
  return digest.value();
}

// every way out of C, for each of the shapes of C, toward a primary computer that is on its \p side
uint64_t everyExitDigest(const Box &primary, Direction side)
{
  Digest digest;
  for (const auto &remote : kRemoteShapes) {
    PointerRig rig(primary, {{"C", remote}});
    rig.link("C", side, "primary");
    for (const auto &start : startsOn(remote)) {
      for (const int32_t drift : {0, 4, -4}) {
        const auto exit = pushOutOfC(rig, side, start, drift);
        digest.add(exit.moves);
        digest.add(exit.entered);
        digest.add(static_cast<int32_t>(exit.everyMove & 0x7fffffff));
      }
    }
  }
  return digest.value();
}

// where the pointer is after something moved it
struct Landing
{
  std::string computer;
  Pos at;

  bool operator==(const Landing &) const = default;
};

// The computer that the pointer entered since the watch began, and where. Nothing if it did not enter another one.
class EntryWatch
{
public:
  explicit EntryWatch(PointerRig &rig) : m_rig(rig)
  {
    m_primary = rig.primary->entered.size();
    for (const auto &[name, client] : rig.clients)
      m_clients[name] = client->entered.size();
  }

  std::optional<Landing> landing() const
  {
    if (m_rig.primary->entered.size() > m_primary)
      return Landing{"primary", m_rig.primary->entered.back()};
    for (const auto &[name, client] : m_rig.clients) {
      if (client->entered.size() > m_clients.at(name))
        return Landing{name, client->entered.back()};
    }
    return std::nullopt;
  }

private:
  PointerRig &m_rig;
  size_t m_primary = 0;
  std::map<std::string, size_t> m_clients;
};

// A computer "A", with "B" on its right and the primary computer on its left; the pointer starts at \p start on "A" and
// is moved by \p dx, \p dy at once
std::optional<Landing> flick(const Pos &start, int32_t dx, int32_t dy)
{
  PointerRig rig(kPrimaryShapes[0], {{"A", {0, 0, 1920, 1080}}, {"B", {-100, 50, 2560, 1440}}});
  rig.link("primary", Direction::Right, "A");
  rig.link("A", Direction::Right, "B");
  rig.switchTo("A");
  rig.placeCursor("A", start.x, start.y);
  const EntryWatch watch(rig);
  rig.moveOnSecondary(dx, dy);
  return watch.landing();
}

// The pointer goes through the right edge of the primary computer, \p index pixels down it, toward "A" with "X", which
// is not connected, between them
std::optional<Landing> acrossAnOfflineComputer(int32_t index)
{
  PointerRig rig(kPrimaryShapes[1], {{"X", {0, 0, 800, 600}, true}, {"A", {0, 0, 1920, 1080}}});
  rig.link("primary", Direction::Right, "X");
  rig.link("X", Direction::Right, "A");
  const EntryWatch watch(rig);
  const auto at = edgePixel(rig.platform->shape, Direction::Right, index);
  rig.moveOnPrimary(at.x, at.y);
  return watch.landing();
}

// The pointer goes through the right edge of the primary computer, \p index pixels down it, with "A" linked to the
// upper half of the edge and "B" to the lower half
std::optional<Landing> throughLinkIntervals(int32_t index)
{
  PointerRig rig(kPrimaryShapes[0], {{"A", {0, 0, 1920, 1080}}, {"B", {0, 0, 2560, 1440}}});
  rig.config.connect("primary", Direction::Right, 0.0f, 0.5f, "A", 0.0f, 1.0f);
  rig.config.connect("primary", Direction::Right, 0.5f, 1.0f, "B", 0.0f, 1.0f);
  rig.config.connect("A", Direction::Left, 0.0f, 1.0f, "primary", 0.0f, 0.5f);
  rig.config.connect("B", Direction::Left, 0.0f, 1.0f, "primary", 0.5f, 1.0f);
  const EntryWatch watch(rig);
  const auto at = edgePixel(rig.platform->shape, Direction::Right, index);
  rig.moveOnPrimary(at.x, at.y);
  return watch.landing();
}

// The pointer reaches a corner of the primary computer that has a computer on each of its sides
std::optional<Landing> intoACorner(Direction horizontal, Direction vertical)
{
  PointerRig rig(
      kPrimaryShapes[0],
      {{"L", {0, 0, 1920, 1080}}, {"R", {0, 0, 2560, 1440}}, {"T", {0, 0, 1280, 720}}, {"D", {0, 0, 3840, 2160}}}
  );
  rig.link("primary", Direction::Left, "L");
  rig.link("primary", Direction::Right, "R");
  rig.link("primary", Direction::Top, "T");
  rig.link("primary", Direction::Bottom, "D");
  const EntryWatch watch(rig);
  const auto &shape = rig.platform->shape;
  const int32_t x = horizontal == Direction::Left ? shape.x : shape.x + shape.w - 1;
  const int32_t y = vertical == Direction::Top ? shape.y : shape.y + shape.h - 1;
  rig.moveOnPrimary(x, y);
  return watch.landing();
}

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

// A computer that is not the owner of a clipboard any more may still send its data: a client that was not told that
// another computer took the selection claims it until it leaves. What it sends must not replace the owner's data.
void ServerTests::clipboardChanged_fromAComputerThatIsNotTheOwner_isIgnored()
{
  ClipboardRig rig;
  fill(rig.a->own(kClipboardClipboard), 30, false, false);
  rig.grab(*rig.a, kClipboardClipboard);
  rig.changed(*rig.a, kClipboardClipboard);

  fill(rig.b->own(kClipboardClipboard), 31, false, false);
  rig.grab(*rig.b, kClipboardClipboard);
  rig.changed(*rig.b, kClipboardClipboard);

  // A still thinks it owns what it had and says so
  rig.changed(*rig.a, kClipboardClipboard);

  QVERIFY(rig.switchTo("primary"));
  QCOMPARE(received(rig.platform->received[kClipboardClipboard]), bytes(marshalled(31)));
}

// What was dropped for its size is never sent, not even after the limit was raised: on a Mac what stays in its place
// is not the clipboard
void ServerTests::clipboardOverTheLimit_stillIsNotSentAfterTheLimitWasRaised()
{
  ClipboardRig rig(1);
  fill(rig.platform->pasteboard, 2000, false, false);
  rig.grab(*rig.primary, kClipboardClipboard);
  QVERIFY(rig.switchTo("A")); // read, and dropped for its size

  // the limit is raised in the configuration the Server uses (an option that is already in one is not replaced)
  deskflow::server::Config raised(&rig.events);
  raised.addComputer("primary");
  raised.addComputer("A");
  raised.addComputer("B");
  raised.addOption("", kOptionClipboardSharingSize, 100);
  rig.config = raised;
  QVERIFY(rig.server->setConfig(rig.config));
  QCOMPARE(rig.server->getMaximumClipboardSizeBytes(), size_t{100} * 1024);

  // the cursor goes on from A: the primary is not read again, and what the Server kept is not the clipboard
  QVERIFY(rig.switchTo("B"));
  QVERIFY(neverReceivedAnything(rig.b->received(kClipboardClipboard)));
}

namespace {

// The pins below were recorded from the Server as it ships (fork commit 9fdd5d134), before the way it maps the pointer
// from one computer to another was changed, and are not to be re-recorded: a Server that is not set to use the monitor
// layout of a computer must keep moving the pointer exactly as it always did.

struct EntryPin
{
  size_t remote;
  Direction side;
  // where the pointer enters C, for each of the places samplesAlong() gives on the edge of the primary computer
  std::array<Pos, 5> entered;
};

// the primary computer is 1728x1117
const std::array<EntryPin, 36> kEntryPins = {{
    {0, Direction::Left, {{{1919, 0}, {1919, 270}, {1919, 540}, {1919, 809}, {1919, 1079}}}},
    {0, Direction::Right, {{{0, 0}, {0, 270}, {0, 540}, {0, 809}, {0, 1079}}}},
    {0, Direction::Top, {{{0, 1079}, {480, 1079}, {960, 1079}, {1440, 1079}, {1919, 1079}}}},
    {0, Direction::Bottom, {{{0, 0}, {480, 0}, {960, 0}, {1440, 0}, {1919, 0}}}},
    {1, Direction::Left, {{{2559, 0}, {2559, 360}, {2559, 720}, {2559, 1079}, {2559, 1439}}}},
    {1, Direction::Right, {{{0, 0}, {0, 360}, {0, 720}, {0, 1079}, {0, 1439}}}},
    {1, Direction::Top, {{{0, 1439}, {640, 1439}, {1280, 1439}, {1920, 1439}, {2559, 1439}}}},
    {1, Direction::Bottom, {{{0, 0}, {640, 0}, {1280, 0}, {1920, 0}, {2559, 0}}}},
    {2, Direction::Left, {{{3839, 0}, {3839, 540}, {3839, 1080}, {3839, 1619}, {3839, 2159}}}},
    {2, Direction::Right, {{{0, 0}, {0, 540}, {0, 1080}, {0, 1619}, {0, 2159}}}},
    {2, Direction::Top, {{{1, 2159}, {961, 2159}, {1921, 2159}, {2881, 2159}, {3838, 2159}}}},
    {2, Direction::Bottom, {{{1, 0}, {961, 0}, {1921, 0}, {2881, 0}, {3838, 0}}}},
    {3, Direction::Left, {{{2559, 0}, {2559, 360}, {2559, 720}, {2559, 1079}, {2559, 1439}}}},
    {3, Direction::Right, {{{-1920, 0}, {-1920, 360}, {-1920, 720}, {-1920, 1079}, {-1920, 1439}}}},
    {3, Direction::Top, {{{-1919, 1439}, {-799, 1439}, {321, 1439}, {1441, 1439}, {2558, 1439}}}},
    {3, Direction::Bottom, {{{-1919, 0}, {-799, 0}, {321, 0}, {1441, 0}, {2558, 0}}}},
    {4, Direction::Left, {{{2559, -1079}, {2559, -450}, {2559, 180}, {2559, 809}, {2559, 1438}}}},
    {4, Direction::Right, {{{0, -1079}, {0, -450}, {0, 180}, {0, 809}, {0, 1438}}}},
    {4, Direction::Top, {{{0, 1439}, {640, 1439}, {1280, 1439}, {1920, 1439}, {2559, 1439}}}},
    {4, Direction::Bottom, {{{0, -1080}, {640, -1080}, {1280, -1080}, {1920, -1080}, {2559, -1080}}}},
    {5, Direction::Left, {{{1079, 0}, {1079, 480}, {1079, 960}, {1079, 1439}, {1079, 1919}}}},
    {5, Direction::Right, {{{0, 0}, {0, 480}, {0, 960}, {0, 1439}, {0, 1919}}}},
    {5, Direction::Top, {{{0, 1919}, {270, 1919}, {540, 1919}, {810, 1919}, {1079, 1919}}}},
    {5, Direction::Bottom, {{{0, 0}, {270, 0}, {540, 0}, {810, 0}, {1079, 0}}}},
    {6, Direction::Left, {{{-1, -360}, {-1, 0}, {-1, 360}, {-1, 719}, {-1, 1079}}}},
    {6, Direction::Right, {{{-2560, -360}, {-2560, 0}, {-2560, 360}, {-2560, 719}, {-2560, 1079}}}},
    {6, Direction::Top, {{{-2560, 1079}, {-1920, 1079}, {-1280, 1079}, {-640, 1079}, {-1, 1079}}}},
    {6, Direction::Bottom, {{{-2560, -360}, {-1920, -360}, {-1280, -360}, {-640, -360}, {-1, -360}}}},
    {7, Direction::Left, {{{5119, 0}, {5119, 360}, {5119, 720}, {5119, 1079}, {5119, 1439}}}},
    {7, Direction::Right, {{{0, 0}, {0, 360}, {0, 720}, {0, 1079}, {0, 1439}}}},
    {7, Direction::Top, {{{1, 1439}, {1281, 1439}, {2561, 1439}, {3841, 1439}, {5118, 1439}}}},
    {7, Direction::Bottom, {{{1, 0}, {1281, 0}, {2561, 0}, {3841, 0}, {5118, 0}}}},
    {8, Direction::Left, {{{1727, 0}, {1727, 279}, {1727, 558}, {1727, 837}, {1727, 1116}}}},
    {8, Direction::Right, {{{0, 0}, {0, 279}, {0, 558}, {0, 837}, {0, 1116}}}},
    {8, Direction::Top, {{{0, 1116}, {432, 1116}, {864, 1116}, {1296, 1116}, {1727, 1116}}}},
    {8, Direction::Bottom, {{{0, 0}, {432, 0}, {864, 0}, {1296, 0}, {1727, 0}}}},
}};

struct ExitPin
{
  size_t remote;
  Direction side;
  int moves;
  Pos entered;
};

// the pointer starts in the middle of C and is pushed toward the primary computer, which is 1728x1117; it enters the
// primary computer a few pixels past its edge, where the last move took it
const std::array<ExitPin, 36> kExitPins = {{
    {0, Direction::Left, 57, {1719, 559}},  {0, Direction::Right, 57, {9, 559}},
    {0, Direction::Top, 32, {864, 1113}},   {0, Direction::Bottom, 32, {864, 4}},
    {1, Direction::Left, 76, {1716, 558}},  {1, Direction::Right, 76, {12, 558}},
    {1, Direction::Top, 43, {864, 1106}},   {1, Direction::Bottom, 43, {864, 11}},
    {2, Direction::Left, 113, {1726, 558}}, {2, Direction::Right, 113, {1, 558}},
    {2, Direction::Top, 64, {864, 1109}},   {2, Direction::Bottom, 64, {864, 8}},
    {3, Direction::Left, 132, {1724, 558}}, {3, Direction::Right, 132, {4, 558}},
    {3, Direction::Top, 43, {864, 1106}},   {3, Direction::Bottom, 43, {864, 11}},
    {4, Direction::Left, 76, {1716, 558}},  {4, Direction::Right, 76, {12, 558}},
    {4, Direction::Top, 75, {864, 1102}},   {4, Direction::Bottom, 75, {864, 15}},
    {5, Direction::Left, 32, {1724, 558}},  {5, Direction::Right, 32, {4, 558}},
    {5, Direction::Top, 57, {864, 1108}},   {5, Direction::Bottom, 57, {864, 9}},
    {6, Direction::Left, 76, {1716, 558}},  {6, Direction::Right, 76, {12, 558}},
    {6, Direction::Top, 43, {864, 1106}},   {6, Direction::Bottom, 43, {864, 11}},
    {7, Direction::Left, 151, {1721, 558}}, {7, Direction::Right, 151, {7, 558}},
    {7, Direction::Top, 43, {864, 1106}},   {7, Direction::Bottom, 43, {864, 11}},
    {8, Direction::Left, 51, {1725, 558}},  {8, Direction::Right, 51, {3, 558}},
    {8, Direction::Top, 33, {864, 1114}},   {8, Direction::Bottom, 33, {864, 2}},
}};

struct DigestPin
{
  size_t primary;
  Direction side;
  uint64_t digest;
};

// every edge pixel of the primary computer, for every shape of C
const std::array<DigestPin, 12> kEntryDigests = {{
    {0, Direction::Left, 0x71492ada34f91addull},
    {0, Direction::Right, 0xc806d57b0e308b4ull},
    {0, Direction::Top, 0x8c4a2030536c478dull},
    {0, Direction::Bottom, 0x1c09bf1382248c01ull},
    {1, Direction::Left, 0x403b67276f22496full},
    {1, Direction::Right, 0xd1cdb59a61926f92ull},
    {1, Direction::Top, 0x48833d173b3d9954ull},
    {1, Direction::Bottom, 0xcd16eba4fab9b2a8ull},
    {2, Direction::Left, 0xdf65221dada53c48ull},
    {2, Direction::Right, 0xfef1cb65e94be8c8ull},
    {2, Direction::Top, 0x863cbff7635589fdull},
    {2, Direction::Bottom, 0x352f10153d29c10dull},
}};

// from three places on C, with three drifts, for every shape of C
const std::array<DigestPin, 12> kExitDigests = {{
    {0, Direction::Left, 0x40f904cd49ae2fb0ull},
    {0, Direction::Right, 0x24e1a90a7ab387b2ull},
    {0, Direction::Top, 0x6fe8d62a81905edull},
    {0, Direction::Bottom, 0x76d684d94887d5e1ull},
    {1, Direction::Left, 0xb88513b3462cdc55ull},
    {1, Direction::Right, 0x568103f17b17767eull},
    {1, Direction::Top, 0x3f140dd43ec844f2ull},
    {1, Direction::Bottom, 0xddf3f31008dde695ull},
    {2, Direction::Left, 0xd50af0194067356ull},
    {2, Direction::Right, 0xa375097d2beea07bull},
    {2, Direction::Top, 0xdf92ea66068b2815ull},
    {2, Direction::Bottom, 0xd028bd7772d59641ull},
}};

struct FlickPin
{
  Pos start;
  int32_t dx;
  // the computer the pointer enters, none if it stays
  const char *computer;
  Pos entered;
};

// "A" is 1920x1080 with the primary computer on its left; "B" is 2560x1440 at -100,50 and on its right. A move to the
// right comes with a move of 7 pixels down, a move to the left with a move of 7 pixels up. How far past the edge the
// pointer went is how far it goes into the computer it enters, even out of it when that is a long way.
const std::array<FlickPin, 33> kFlickPins = {{
    {{1900, 500}, 25, "B", {-95, 726}},
    {{1900, 500}, 1000, "B", {880, 726}},
    {{1900, 500}, 2100, "B", {1980, 726}},
    {{1900, 500}, 3000, "B", {2880, 726}},
    {{1900, 500}, 4000, "B", {3880, 726}},
    {{1900, 500}, 9000, "B", {8880, 726}},
    {{1919, 0}, 25, "B", {-76, 60}},
    {{1919, 0}, 1000, "B", {899, 60}},
    {{1919, 0}, 2100, "B", {1999, 60}},
    {{1919, 0}, 3000, "B", {2899, 60}},
    {{1919, 0}, 4000, "B", {3899, 60}},
    {{1919, 0}, 9000, "B", {8899, 60}},
    {{1500, 1079}, 25, nullptr, {0, 0}},
    {{1500, 1079}, 1000, "B", {480, 1489}},
    {{1500, 1079}, 2100, "B", {1580, 1489}},
    {{1500, 1079}, 3000, "B", {2480, 1489}},
    {{1500, 1079}, 4000, "B", {3480, 1489}},
    {{1500, 1079}, 9000, "B", {8480, 1489}},
    {{20, 500}, -25, "primary", {1723, 510}},
    {{20, 500}, -1000, "primary", {748, 510}},
    {{20, 500}, -1800, "primary", {-52, 510}},
    {{20, 500}, -3000, "primary", {-1252, 510}},
    {{20, 500}, -5000, "primary", {-3252, 510}},
    {{0, 0}, -25, "primary", {1703, 0}},
    {{0, 0}, -1000, "primary", {728, 0}},
    {{0, 0}, -1800, "primary", {-72, 0}},
    {{0, 0}, -3000, "primary", {-1272, 0}},
    {{0, 0}, -5000, "primary", {-3272, 0}},
    {{300, 1079}, -25, nullptr, {0, 0}},
    {{300, 1079}, -1000, "primary", {1028, 1109}},
    {{300, 1079}, -1800, "primary", {228, 1109}},
    {{300, 1079}, -3000, "primary", {-972, 1109}},
    {{300, 1079}, -5000, "primary", {-2972, 1109}},
}};

struct IndexPin
{
  int32_t index;
  Landing landing;
};

// the primary computer is 2064x1347, "A" is 1920x1080
const std::array<IndexPin, 6> kOfflinePins = {{
    {0, {"A", {0, 0}}},
    {1, {"A", {0, 1}}},
    {337, {"A", {0, 270}}},
    {673, {"A", {0, 540}}},
    {1000, {"A", {0, 802}}},
    {1346, {"A", {0, 1079}}},
}};

// the primary computer is 1728x1117, "A" is 1920x1080 and "B" is 2560x1440
const std::array<IndexPin, 7> kIntervalPins = {{
    {0, {"A", {0, 0}}},
    {100, {"A", {0, 194}}},
    {557, {"A", {0, 1078}}},
    {558, {"B", {0, 0}}},
    {559, {"B", {0, 2}}},
    {800, {"B", {0, 623}}},
    {1116, {"B", {0, 1438}}},
}};

QString text(const Pos &pos)
{
  return QStringLiteral("%1,%2").arg(pos.x).arg(pos.y);
}

QString text(const std::optional<Landing> &landing)
{
  return landing ? QStringLiteral("%1 at %2").arg(QString::fromStdString(landing->computer), text(landing->at))
                 : QStringLiteral("stays");
}

QString shapeName(const Box &box)
{
  return QStringLiteral("%1x%2 at %3,%4").arg(box.w).arg(box.h).arg(box.x).arg(box.y);
}

// the INFO lines of a pin that moves the pointer thousands of times are noise
class QuietLog
{
public:
  QuietLog() : m_level(Log::getInstance()->getFilter())
  {
    Log::getInstance()->setFilter(LogLevel::Level::Error);
  }

  ~QuietLog()
  {
    Log::getInstance()->setFilter(m_level);
  }

private:
  LogLevel::Level m_level;
};

} // namespace

void ServerTests::pointerPin_primaryToRemote_entersWhereItAlwaysDid_data()
{
  QTest::addColumn<int>("pin");
  for (size_t i = 0; i < kEntryPins.size(); ++i) {
    const auto &pin = kEntryPins.at(i);
    QTest::newRow(
        qPrintable(QStringLiteral("%1 through the %2").arg(shapeName(kRemoteShapes.at(pin.remote)), sideName(pin.side)))
    ) << static_cast<int>(i);
  }
}

void ServerTests::pointerPin_primaryToRemote_entersWhereItAlwaysDid()
{
  const QuietLog quiet;
  QFETCH(int, pin);
  const auto &expected = kEntryPins.at(static_cast<size_t>(pin));
  auto rig = neighbours(kPrimaryShapes[0], kRemoteShapes.at(expected.remote), expected.side);
  const auto samples = samplesAlong(edgeLength(kPrimaryShapes[0], expected.side));
  for (size_t i = 0; i < samples.size(); ++i) {
    const auto entered = crossToC(*rig, expected.side, samples.at(i));
    QVERIFY(entered);
    QCOMPARE(text(*entered), text(expected.entered.at(i)));
  }
}

void ServerTests::pointerPin_remoteToPrimary_leavesWhereItAlwaysDid_data()
{
  QTest::addColumn<int>("pin");
  for (size_t i = 0; i < kExitPins.size(); ++i) {
    const auto &pin = kExitPins.at(i);
    QTest::newRow(
        qPrintable(QStringLiteral("%1 toward its %2").arg(shapeName(kRemoteShapes.at(pin.remote)), sideName(pin.side)))
    ) << static_cast<int>(i);
  }
}

void ServerTests::pointerPin_remoteToPrimary_leavesWhereItAlwaysDid()
{
  const QuietLog quiet;
  QFETCH(int, pin);
  const auto &expected = kExitPins.at(static_cast<size_t>(pin));
  const auto &shape = kRemoteShapes.at(expected.remote);
  PointerRig rig(kPrimaryShapes[0], {{"C", shape}});
  rig.link("C", expected.side, "primary");
  const auto exit = pushOutOfC(rig, expected.side, startsOn(shape)[0], 0);
  QCOMPARE(exit.moves, expected.moves);
  QCOMPARE(text(exit.entered), text(expected.entered));
}

void ServerTests::pointerPin_everyEdgePixel_entersWhereItAlwaysDid_data()
{
  QTest::addColumn<int>("pin");
  for (size_t i = 0; i < kEntryDigests.size(); ++i) {
    const auto &pin = kEntryDigests.at(i);
    QTest::newRow(qPrintable(
        QStringLiteral("%1 through the %2").arg(shapeName(kPrimaryShapes.at(pin.primary)), sideName(pin.side))
    )) << static_cast<int>(i);
  }
}

void ServerTests::pointerPin_everyEdgePixel_entersWhereItAlwaysDid()
{
  const QuietLog quiet;
  QFETCH(int, pin);
  const auto &expected = kEntryDigests.at(static_cast<size_t>(pin));
  QCOMPARE(everyEntryDigest(kPrimaryShapes.at(expected.primary), expected.side), expected.digest);
}

void ServerTests::pointerPin_everyWayOutOfARemote_leavesWhereItAlwaysDid_data()
{
  QTest::addColumn<int>("pin");
  for (size_t i = 0; i < kExitDigests.size(); ++i) {
    const auto &pin = kExitDigests.at(i);
    QTest::newRow(qPrintable(
        QStringLiteral("toward %1 on its %2").arg(shapeName(kPrimaryShapes.at(pin.primary)), sideName(pin.side))
    )) << static_cast<int>(i);
  }
}

void ServerTests::pointerPin_everyWayOutOfARemote_leavesWhereItAlwaysDid()
{
  const QuietLog quiet;
  QFETCH(int, pin);
  const auto &expected = kExitDigests.at(static_cast<size_t>(pin));
  QCOMPARE(everyExitDigest(kPrimaryShapes.at(expected.primary), expected.side), expected.digest);
}

void ServerTests::pointerPin_aFlick_landsWhereItAlwaysDid_data()
{
  QTest::addColumn<int>("pin");
  for (size_t i = 0; i < kFlickPins.size(); ++i) {
    const auto &pin = kFlickPins.at(i);
    QTest::newRow(qPrintable(QStringLiteral("%1 by %2").arg(text(pin.start)).arg(pin.dx))) << static_cast<int>(i);
  }
}

void ServerTests::pointerPin_aFlick_landsWhereItAlwaysDid()
{
  const QuietLog quiet;
  QFETCH(int, pin);
  const auto &expected = kFlickPins.at(static_cast<size_t>(pin));
  const auto landing = flick(expected.start, expected.dx, expected.dx > 0 ? 7 : -7);
  const auto expectedLanding =
      expected.computer ? std::optional<Landing>(Landing{expected.computer, expected.entered}) : std::nullopt;
  QCOMPARE(text(landing), text(expectedLanding));
}

void ServerTests::pointerPin_acrossAnOfflineComputer_landsWhereItAlwaysDid()
{
  const QuietLog quiet;
  for (const auto &pin : kOfflinePins)
    QCOMPARE(text(acrossAnOfflineComputer(pin.index)), text(std::optional<Landing>(pin.landing)));
}

void ServerTests::pointerPin_linkIntervals_landWhereTheyAlwaysDid()
{
  const QuietLog quiet;
  for (const auto &pin : kIntervalPins)
    QCOMPARE(text(throughLinkIntervals(pin.index)), text(std::optional<Landing>(pin.landing)));
}

void ServerTests::pointerPin_cornersOfThePrimary_horizontalIsTriedFirst()
{
  const QuietLog quiet;
  QCOMPARE(text(intoACorner(Direction::Left, Direction::Top)), QStringLiteral("L at 1919,0"));
  QCOMPARE(text(intoACorner(Direction::Left, Direction::Bottom)), QStringLiteral("L at 1919,1079"));
  QCOMPARE(text(intoACorner(Direction::Right, Direction::Top)), QStringLiteral("R at 0,0"));
  QCOMPARE(text(intoACorner(Direction::Right, Direction::Bottom)), QStringLiteral("R at 0,1439"));
}

QTEST_MAIN(ServerTests)
