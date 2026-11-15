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
#include "common/Settings.h"
#include "deskflow/Computer.h"
#include "deskflow/IPrimaryComputer.h"
#include "server/Config.h"
#include "server/PrimaryClient.h"
#include "server/Server.h"
#include "server/ServerCursorEdges.h"
#include "server/ServerNeighborTraversal.h"

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

} // namespace

void ServerTests::initTestCase()
{
  // the Config in these tests reads per computer settings
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
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

QTEST_MAIN(ServerTests)
