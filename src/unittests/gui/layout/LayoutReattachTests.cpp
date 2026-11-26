/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutReattachTests.h"

#include "gui/layout/Reattach.h"

#include <algorithm>
#include <random>

using namespace deskflow::gui::layout;
using namespace deskflow::server;
using namespace deskflow::server::arrangement;

Q_DECLARE_METATYPE(Direction)

namespace {

Computer pc(
    const std::string &name, double originX, double originY, std::vector<MonitorRect> monitors, double zoom = 1.0,
    bool server = false
)
{
  Computer computer;
  computer.name = name;
  computer.originX = originX;
  computer.originY = originY;
  computer.monitors = std::move(monitors);
  computer.zoom = zoom;
  computer.isServer = server;
  return computer;
}

Computer serverOf(std::vector<MonitorRect> monitors)
{
  return pc("srv", 0, 0, std::move(monitors), 1.0, true);
}

const Computer &get(const Arrangement &arrangement, const std::string &name)
{
  return arrangement.computers[static_cast<size_t>(arrangement.indexOf(name))];
}

// whether the computer is against the server on that side, as the derivation of the links says
bool touchesServer(const Arrangement &arrangement, const std::string &name)
{
  for (const auto &contact : deriveLinks(arrangement).contacts) {
    if ((contact.a == "srv" && contact.b == name) || (contact.b == "srv" && contact.a == name))
      return true;
  }
  return false;
}

bool touchesServerOn(const Arrangement &arrangement, const std::string &name, Direction serverSide)
{
  for (const auto &contact : deriveLinks(arrangement).contacts) {
    if (contact.a == "srv" && contact.b == name && contact.sideA == serverSide)
      return true;
    if (contact.b == "srv" && contact.a == name && contact.sideB == serverSide)
      return true;
  }
  return false;
}

int errorsOf(const Arrangement &arrangement)
{
  int errors = 0;
  for (const auto &issue : validate(arrangement)) {
    if (issue.severity() == Issue::Severity::Error)
      ++errors;
  }
  return errors;
}

std::vector<std::string> sorted(std::vector<std::string> names)
{
  std::ranges::sort(names);
  return names;
}

// the T of the user: the ultrawide is the main display, the laptop screen is below it, and the computers around
Arrangement theT()
{
  Arrangement arrangement;
  arrangement.computers = {
      serverOf({{0, 0, 3440, 1440}, {964, 1440, 1512, 982}}),
      pc("PC1", -1920, 0, {{0, 0, 1920, 1080}}),
      pc("PC2", 3440, 0, {{0, 0, 1920, 1080}}),
      pc("MINI", 964, 2422, {{0, 0, 1920, 1080}}),
  };
  return arrangement;
}

const std::vector<MonitorRect> kLaptopOnly{{0, 0, 1512, 982}};

} // namespace

void LayoutReattachTests::theUsersT_goesToTheLaptop_andTheComputersFollowTheirSides()
{
  const auto before = theT();
  // the fixture is what it says: every computer is against the server
  QVERIFY(touchesServer(before, "PC1") && touchesServer(before, "PC2") && touchesServer(before, "MINI"));

  const auto result = reattachToServer(before, kLaptopOnly);
  QVERIFY(result.changed());
  QCOMPARE(sorted(result.reattached), (std::vector<std::string>{"MINI", "PC2"}));
  QVERIFY(result.notReattached.empty());
  // PC1 is still against the left edge of the laptop screen: it stays
  QCOMPARE(get(result.arrangement, "PC1").originX, -1920.0);
  QCOMPARE(get(result.arrangement, "PC1").originY, 0.0);
  QVERIFY(std::ranges::find(result.moved, "PC1") == result.moved.end());

  QVERIFY(touchesServerOn(result.arrangement, "PC2", Direction::Right));
  QCOMPARE(get(result.arrangement, "PC2").box().x, 1512.0);
  QVERIFY(touchesServerOn(result.arrangement, "MINI", Direction::Bottom));
  QCOMPARE(get(result.arrangement, "MINI").box().y, 982.0);
  QVERIFY(touchesServer(result.arrangement, "PC1"));

  // the displays of the server are the new ones, and nothing overlaps
  QVERIFY(result.arrangement.computers[0].monitors == kLaptopOnly);
  QCOMPARE(errorsOf(result.arrangement), 0);
  // the input was not touched
  QCOMPARE(get(before, "PC2").originX, 3440.0);
}

void LayoutReattachTests::aServerThatKeepsItsDisplays_changesNothing()
{
  const auto before = theT();
  const auto result = reattachToServer(before, before.computers[0].monitors);
  QVERIFY(!result.changed());
  QVERIFY(result.reattached.empty());
  QVERIFY(result.notReattached.empty());
  for (size_t i = 0; i < before.computers.size(); ++i) {
    QCOMPARE(result.arrangement.computers[i].originX, before.computers[i].originX);
    QCOMPARE(result.arrangement.computers[i].originY, before.computers[i].originY);
  }
}

void LayoutReattachTests::aComputerThatNeverTouchedTheServer_staysWhereItIs()
{
  auto before = theT();
  before.computers.push_back(pc("FAR", 20000, 20000, {{0, 0, 1920, 1080}}));
  const auto result = reattachToServer(before, kLaptopOnly);
  QCOMPARE(get(result.arrangement, "FAR").originX, 20000.0);
  QCOMPARE(get(result.arrangement, "FAR").originY, 20000.0);
  QVERIFY(std::ranges::find(result.moved, "FAR") == result.moved.end());
  QVERIFY(std::ranges::find(result.notReattached, "FAR") == result.notReattached.end());
}

void LayoutReattachTests::computersThatTouchAMovedComputer_moveWithIt()
{
  auto before = theT();
  // PC3 is beside PC2, further from the server
  before.computers.push_back(pc("PC3", 3440 + 1920, 0, {{0, 0, 1920, 1080}}));
  QVERIFY(!touchesServer(before, "PC3"));

  const auto result = reattachToServer(before, kLaptopOnly);
  QCOMPARE(sorted(result.reattached), (std::vector<std::string>{"MINI", "PC2"}));
  QVERIFY(std::ranges::find(result.moved, "PC3") != result.moved.end());
  // they moved together: PC3 is still against PC2
  const auto &pc2 = get(result.arrangement, "PC2");
  const auto &pc3 = get(result.arrangement, "PC3");
  QCOMPARE(pc3.box().x, pc2.box().right());
  QCOMPARE(pc3.box().y, pc2.box().y);
  QCOMPARE(errorsOf(result.arrangement), 0);
}

void LayoutReattachTests::everySide_isPutAgainstTheSameSide_data()
{
  QTest::addColumn<Direction>("side");
  QTest::addColumn<double>("x");
  QTest::addColumn<double>("y");
  QTest::addColumn<double>("screenX");
  QTest::addColumn<double>("screenY");

  // the server is 2000 x 1000 and a computer of 800 x 600 is against it in the middle of the side; the server then has
  // a screen of 1000 x 500 that is at the far side from it, so that the computer is not against it any more
  QTest::newRow("right") << Direction::Right << 2000.0 << 200.0 << 0.0 << 0.0;
  QTest::newRow("left") << Direction::Left << -800.0 << 200.0 << 1000.0 << 500.0;
  QTest::newRow("top") << Direction::Top << 600.0 << -600.0 << 1000.0 << 500.0;
  QTest::newRow("bottom") << Direction::Bottom << 600.0 << 1000.0 << 0.0 << 0.0;
}

void LayoutReattachTests::everySide_isPutAgainstTheSameSide()
{
  QFETCH(Direction, side);
  QFETCH(double, x);
  QFETCH(double, y);
  QFETCH(double, screenX);
  QFETCH(double, screenY);

  Arrangement before;
  before.computers = {serverOf({{0, 0, 2000, 1000}}), pc("C", x, y, {{0, 0, 800, 600}})};
  QVERIFY(touchesServerOn(before, "C", side));

  const auto result = reattachToServer(before, {{int(screenX), int(screenY), 1000, 500}});
  QVERIFY(result.changed());
  QCOMPARE(result.reattached, (std::vector<std::string>{"C"}));
  QVERIFY(touchesServerOn(result.arrangement, "C", side));
  const auto box = get(result.arrangement, "C").box();
  switch (side) {
  case Direction::Right:
    QCOMPARE(box.x, screenX + 1000.0);
    break;
  case Direction::Left:
    QCOMPARE(box.right(), screenX);
    break;
  case Direction::Top:
    QCOMPARE(box.bottom(), screenY);
    break;
  default:
    QCOMPARE(box.y, screenY + 500.0);
    break;
  }
  QCOMPARE(errorsOf(result.arrangement), 0);
}

void LayoutReattachTests::theFractionAlongTheSide_isKept()
{
  Arrangement before;
  // the middle of the computer is at 20 % of the height of the server
  before.computers = {serverOf({{0, 0, 2000, 1000}}), pc("C", 2000, 100, {{0, 0, 800, 200}})};
  const auto result = reattachToServer(before, {{0, 0, 1000, 500}});
  const auto box = get(result.arrangement, "C").box();
  QCOMPARE(box.x, 1000.0);
  QCOMPARE(box.centerY(), 100.0);
}

void LayoutReattachTests::aGroupThatCannotBePlaced_staysAndIsReported()
{
  Arrangement before;
  // Z is a big computer that is where the right side of the smaller screen is: nothing fits between it and the server
  before.computers = {
      serverOf({{0, 0, 2000, 1000}}), pc("C", 2000, 0, {{0, 0, 800, 1000}}),
      pc("Z", 1000, -5000, {{0, 0, 4000, 11000}}),
  };
  const auto result = reattachToServer(before, {{0, 0, 1000, 500}});
  QCOMPARE(result.notReattached, (std::vector<std::string>{"C"}));
  QVERIFY(result.reattached.empty());
  QVERIFY(!result.changed());
  QCOMPARE(get(result.arrangement, "C").originX, 2000.0);
  QCOMPARE(get(result.arrangement, "C").originY, 0.0);
}

void LayoutReattachTests::aZoomedComputer_isMovedByItsBox()
{
  Arrangement before;
  // 3840 x 2160 pixels drawn at half size: 1920 x 1080 units
  before.computers = {serverOf({{0, 0, 2000, 1000}}), pc("C", 2000, 0, {{0, 0, 3840, 2160}}, 0.5)};
  QVERIFY(touchesServerOn(before, "C", Direction::Right));
  const auto result = reattachToServer(before, {{0, 0, 1000, 500}});
  QVERIFY(touchesServerOn(result.arrangement, "C", Direction::Right));
  QCOMPARE(get(result.arrangement, "C").box().x, 1000.0);
  QCOMPARE(get(result.arrangement, "C").zoom, 0.5);
}

void LayoutReattachTests::noServerOrNoDisplays_changesNothing()
{
  Arrangement noServer;
  noServer.computers = {pc("A", 0, 0, {{0, 0, 100, 100}}), pc("B", 100, 0, {{0, 0, 100, 100}})};
  QVERIFY(!reattachToServer(noServer, kLaptopOnly).changed());

  const auto before = theT();
  const auto result = reattachToServer(before, {});
  QVERIFY(!result.changed());
  QVERIFY(result.arrangement.computers[0].monitors == before.computers[0].monitors);
}

void LayoutReattachTests::theOrderOfTheComputers_doesNotMatter()
{
  const auto before = theT();
  const auto expected = reattachToServer(before, kLaptopOnly);

  std::mt19937 random(5);
  for (int i = 0; i < 12; ++i) {
    auto shuffled = before;
    std::shuffle(shuffled.computers.begin(), shuffled.computers.end(), random);
    const auto result = reattachToServer(shuffled, kLaptopOnly);
    for (const auto &computer : expected.arrangement.computers) {
      QCOMPARE(get(result.arrangement, computer.name).originX, computer.originX);
      QCOMPARE(get(result.arrangement, computer.name).originY, computer.originY);
    }
    QCOMPARE(sorted(result.reattached), sorted(expected.reattached));
  }
}

void LayoutReattachTests::aBiggerScreen_keepsWhatStillTouches()
{
  Arrangement before;
  before.computers = {serverOf({{0, 0, 1000, 500}}), pc("C", 1000, 0, {{0, 0, 800, 500}})};
  // the server has a bigger screen: C is where it was, and is against the box of the new screen no more, but it is
  // inside the screen: that is not a change that this makes better, and nothing that touches is moved
  const auto result = reattachToServer(before, {{0, 0, 1000, 700}});
  QVERIFY(touchesServer(result.arrangement, "C"));
  QVERIFY(!result.changed());
}

void LayoutReattachTests::property_whatIsMovedTouchesTheServerAndOverlapsNothing()
{
  std::mt19937 random(2027);
  int checked = 0;
  int withMoves = 0;
  const auto range = [&](int low, int high) { return low + int(random() % unsigned(high - low + 1)); };
  for (int round = 0; round < 300; ++round) {
    Arrangement before;
    const int serverW = range(800, 3600);
    const int serverH = range(600, 1600);
    before.computers.push_back(serverOf({{0, 0, serverW, serverH}}));
    // one computer against each side, of a random size and a random place along the side
    const auto addAround = [&](const std::string &name, Direction side) {
      const int w = range(400, 2000);
      const int h = range(300, 1200);
      double x = 0;
      double y = 0;
      switch (side) {
      case Direction::Right:
        x = serverW;
        y = range(-h / 2, serverH - h / 2);
        break;
      case Direction::Left:
        x = -w;
        y = range(-h / 2, serverH - h / 2);
        break;
      case Direction::Top:
        y = -h;
        x = range(-w / 2, serverW - w / 2);
        break;
      default:
        y = serverH;
        x = range(-w / 2, serverW - w / 2);
        break;
      }
      before.computers.push_back(pc(name, x, y, {{0, 0, w, h}}));
    };
    addAround("R", Direction::Right);
    addAround("L", Direction::Left);
    addAround("T", Direction::Top);
    addAround("B", Direction::Bottom);
    // the sides do not overlap each other where a corner meets: skip a fixture that is not valid itself
    if (errorsOf(before) != 0)
      continue;

    const std::vector<MonitorRect> now{{0, 0, range(500, 3600), range(400, 1600)}};
    const auto result = reattachToServer(before, now);
    const auto again = reattachToServer(before, now);
    ++checked;
    if (result.changed())
      ++withMoves;

    QCOMPARE(sorted(result.moved), sorted(again.moved));
    QCOMPARE(sorted(result.reattached), sorted(again.reattached));
    for (const auto &name : result.reattached)
      QVERIFY2(touchesServer(result.arrangement, name), name.c_str());
    for (const auto &computer : before.computers) {
      if (computer.isServer || std::ranges::find(result.moved, computer.name) != result.moved.end())
        continue;
      QCOMPARE(get(result.arrangement, computer.name).originX, computer.originX);
      QCOMPARE(get(result.arrangement, computer.name).originY, computer.originY);
    }
    // nothing that touched the new displays before is moved
    Arrangement unmoved = before;
    unmoved.computers[0].monitors = now;
    for (const auto &name : result.moved)
      QVERIFY2(!touchesServer(unmoved, name), name.c_str());
    // what was moved overlaps nothing that it did not overlap before
    for (const auto &issue : validate(result.arrangement)) {
      if (issue.severity() != Issue::Severity::Error)
        continue;
      const bool involvesMoved = std::ranges::find(result.moved, issue.computer) != result.moved.end() ||
                                 std::ranges::find(result.moved, issue.other) != result.moved.end();
      QVERIFY2(!involvesMoved, (issue.computer + " " + issue.other).c_str());
    }
  }
  // the fixtures were not all skipped, and the displays were not always the same
  QVERIFY2(checked >= 100, qPrintable(QStringLiteral("only %1 fixtures were valid").arg(checked)));
  QVERIFY2(withMoves >= 30, qPrintable(QStringLiteral("only %1 rounds moved anything").arg(withMoves)));
}

QTEST_MAIN(LayoutReattachTests)
