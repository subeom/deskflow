/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorGeometryTests.h"

#include "server/MonitorGeometry.h"
#include "server/ServerCursorEdges.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <vector>

using namespace deskflow::server;

namespace {

constexpr std::array<Direction, 4> kSides = {Direction::Left, Direction::Right, Direction::Top, Direction::Bottom};

bool isHorizontal(Direction side)
{
  return side == Direction::Left || side == Direction::Right;
}

Direction opposite(Direction side)
{
  using enum Direction;
  return side == Left ? Right : side == Right ? Left : side == Top ? Bottom : Top;
}

MonitorLayout layoutOf(std::string_view text)
{
  auto result = MonitorLayout::parse(text);
  if (!result)
    qFatal("the layout \"%s\" was not accepted: %s", std::string(text).c_str(), result.error.c_str());
  return *result.layout;
}

MonitorLayout layoutOf(const std::vector<MonitorRect> &monitors)
{
  auto result = MonitorLayout::create(monitors);
  if (!result)
    qFatal("the monitors were not accepted: %s", result.error.c_str());
  return *result.layout;
}

// where a pointer is along the side: a y for the left and right side, an x for the top and bottom side
int32_t along(Direction side, const MonitorPoint &point)
{
  return isHorizontal(side) ? point.y : point.x;
}

int32_t originOf(Direction side, const MonitorRect &box)
{
  return isHorizontal(side) ? box.y : box.x;
}

int32_t extentOf(Direction side, const MonitorRect &box)
{
  return isHorizontal(side) ? box.h : box.w;
}

uint32_t bitsOf(float value)
{
  return std::bit_cast<uint32_t>(value);
}

QString text(const MonitorPoint &point)
{
  return QStringLiteral("%1,%2").arg(point.x).arg(point.y);
}

QString text(const MonitorRect &rect)
{
  return QStringLiteral("%1,%2,%3,%4").arg(rect.x).arg(rect.y).arg(rect.w).arg(rect.h);
}

struct Catalogue
{
  const char *name;
  const char *list;
  MonitorRect box;
  bool plain;
  // a pixel of the bounding box that has no monitor
  MonitorPoint hole;
};

// What people have on their desks: the monitors in physical pixels, or in points on a Mac.
// Mixed DPI is the same thing: only the rectangles count, not the scale that a monitor has.
const std::array<Catalogue, 13> kCatalogue = {{
    {"a single monitor", "0,0,1920,1080", {0, 0, 1920, 1080}, true, {}},
    {"two monitors side by side", "0,0,1920,1080;1920,0,1920,1080", {0, 0, 3840, 1080}, true, {}},
    {"two monitors one above the other", "0,0,1920,1080;0,1080,1920,1080", {0, 0, 1920, 2160}, true, {}},
    {"two by two monitors",
     "0,0,1920,1080;1920,0,1920,1080;0,1080,1920,1080;1920,1080,1920,1080",
     {0, 0, 3840, 2160},
     true,
     {}},
    {"a smaller monitor on the right, top aligned",
     "0,0,2560,1440;2560,0,1920,1080",
     {0, 0, 4480, 1440},
     false,
     {3000, 1300}},
    {"a smaller monitor on the left, bottom aligned",
     "-1920,360,1920,1080;0,0,2560,1440",
     {-1920, 0, 4480, 1440},
     false,
     {-1000, 100}},
    {"a smaller monitor above, centred",
     "0,0,2560,1440;320,-1080,1920,1080",
     {0, -1080, 2560, 2520},
     false,
     {100, -500}},
    {"a portrait monitor on the right, higher",
     "0,0,2560,1440;2560,-480,1080,1920",
     {0, -480, 3640, 1920},
     false,
     {100, -300}},
    {"a portrait monitor next to a landscape one",
     "0,0,1920,1080;1920,-420,1080,1920",
     {0, -420, 3000, 1920},
     false,
     {100, -200}},
    {"an ultrawide monitor with a smaller one on the left",
     "-1920,360,1920,1080;0,0,3440,1440",
     {-1920, 0, 5360, 1440},
     false,
     {-1000, 100}},
    {"a hole in the middle of the top edge",
     "0,0,800,1000;800,600,800,400;1600,0,800,1000",
     {0, 0, 2400, 1000},
     false,
     {1200, 300}},
    {"a Mac with an external display", "0,0,1728,1117;1728,-200,2560,1440", {0, -200, 4288, 1440}, false, {100, -100}},
    {"monitors of mixed scales",
     "monitor 0,0,2560,1440 scale=150% primary\nmonitor 2560,0,1920,1080 scale=100%",
     {0, 0, 4480, 1440},
     false,
     {3000, 1300}},
}};

struct Invalid
{
  const char *name;
  const char *list;
  const char *error;
};

const std::array<Invalid, 18> kInvalid = {{
    {"nothing", "", "no monitor"},
    {"only separators", " ; ;\n ", "no monitor"},
    {"words", "abc", "not a whole number"},
    {"a decimal number", "0,0,1920.5,1080", "not a whole number"},
    {"too few numbers", "0,0,1920", "4 numbers"},
    {"too many numbers", "0,0,1920,1080,5", "4 numbers"},
    {"no width", "0,0,0,1080", "greater than 0"},
    {"a negative height", "0,0,1920,-1080", "greater than 0"},
    {"a huge number", "99999999999,0,10,10", "out of range"},
    {"a number that has too many digits", "0,0,00000000000000000000000001,10", "out of range"},
    {"a position that is not 16 bits", "-40000,0,100,100", "out of range"},
    {"a size that is not 16 bits", "0,0,40000,100", "out of range"},
    {"monitors that overlap", "0,0,1920,1080;1000,0,1920,1080", "overlap"},
    {"a monitor inside another", "0,0,1920,1080;100,100,200,200", "overlap"},
    {"monitors that do not touch", "0,0,100,100;200,200,100,100", "not connected"},
    {"monitors that touch at a corner", "0,0,100,100;100,100,100,100", "not connected"},
    {"an unknown word after a monitor", "monitor 0,0,100,100 wide", "wide"},
    {"a line that is not a monitor", "computer: X\nthe monitor is big", "line 2"},
}};

QString errorOf(const char *list)
{
  return QString::fromStdString(MonitorLayout::parse(list).error);
}

const char *kMacBlock =
    "== Paste-ready block (proposed experimental format; copy this whole block into the research thread) ==\n"
    "computer: subeoms-macbook-pro.local\n"
    "space: points x,y,w,h scale\n"
    "  monitor 0,0,2064,1347 scale=2.0 primary\n"
    "  monitor 2064,-300,2560,1440 scale=1.0\n"
    "\n"
    "Cross-check (also read-only):  system_profiler SPDisplaysDataType\n"
    "Nothing was changed, no event was posted, the cursor was not moved.\n";

const char *kWindowsBlock = "Deskflow processes (read-only)\n"
                            "deskflow-core.exe pid=4242  DPI awareness = PerMonitorV2  exe arch = x64\n"
                            "\n"
                            "Paste-ready block (proposed experimental format; copy it into the research thread):\n"
                            "computer: SUBEOM-COPILOT\r\n"
                            "space: virtual-desktop physical pixels x,y,w,h scale%\r\n"
                            "  monitor -1920,360,1920,1080 scale=100%\r\n"
                            "  monitor 0,0,2560,1440 scale=150% primary\r\n"
                            "\r\n"
                            "Nothing was changed, no input was sent, the cursor was not moved.\r\n";

// a random layout of one to six monitors that are connected, as people arrange them: each is put against a side of
// another one, anywhere along it
std::vector<MonitorRect> randomMonitors(std::mt19937 &random)
{
  const auto between = [&random](int32_t low, int32_t high) {
    return std::uniform_int_distribution<int32_t>(low, high)(random);
  };
  std::vector<MonitorRect> monitors{{between(-500, 500), between(-500, 500), between(300, 3000), between(300, 2000)}};
  const auto count = between(1, 6);
  for (int attempts = 0; static_cast<int>(monitors.size()) < count && attempts < 200; ++attempts) {
    const auto base = monitors[static_cast<size_t>(between(0, static_cast<int32_t>(monitors.size()) - 1))];
    MonitorRect next{0, 0, between(300, 3000), between(300, 2000)};
    switch (between(0, 3)) {
    case 0:
      next.x = base.right();
      next.y = between(base.y - next.h + 1, base.bottom() - 1);
      break;
    case 1:
      next.x = base.x - next.w;
      next.y = between(base.y - next.h + 1, base.bottom() - 1);
      break;
    case 2:
      next.y = base.bottom();
      next.x = between(base.x - next.w + 1, base.right() - 1);
      break;
    default:
      next.y = base.y - next.h;
      next.x = between(base.x - next.w + 1, base.right() - 1);
      break;
    }
    const bool overlaps = std::ranges::any_of(monitors, [&next](const MonitorRect &other) {
      return next.x < other.right() && other.x < next.right() && next.y < other.bottom() && other.y < next.bottom();
    });
    if (!overlaps)
      monitors.push_back(next);
  }
  return monitors;
}

constexpr uint32_t kSeed = 20260516;
constexpr int kLayouts = 150;

} // namespace

void MonitorGeometryTests::layouts_data()
{
  QTest::addColumn<int>("index");
  for (size_t i = 0; i < kCatalogue.size(); ++i)
    QTest::newRow(kCatalogue.at(i).name) << static_cast<int>(i);
}

void MonitorGeometryTests::layouts()
{
  QFETCH(int, index);
  const auto &expected = kCatalogue.at(static_cast<size_t>(index));
  const auto layout = layoutOf(expected.list);

  QCOMPARE(text(layout.boundingBox()), text(expected.box));
  QCOMPARE(layout.isPlainRectangle(), expected.plain);
  for (const auto &monitor : layout.monitors()) {
    QVERIFY(layout.contains(monitor.x, monitor.y));
    QVERIFY(layout.contains(monitor.right() - 1, monitor.bottom() - 1));
  }
  if (!expected.plain) {
    QVERIFY(!layout.contains(expected.hole.x, expected.hole.y));
    QVERIFY(expected.box.contains(expected.hole.x, expected.hole.y));
  }
  QVERIFY(!layout.contains(expected.box.x - 1, expected.box.y));
  QVERIFY(!layout.contains(expected.box.right(), expected.box.y));
}

void MonitorGeometryTests::invalidLayouts_data()
{
  QTest::addColumn<int>("index");
  for (size_t i = 0; i < kInvalid.size(); ++i)
    QTest::newRow(kInvalid.at(i).name) << static_cast<int>(i);
}

void MonitorGeometryTests::invalidLayouts()
{
  QFETCH(int, index);
  const auto &expected = kInvalid.at(static_cast<size_t>(index));
  const auto result = MonitorLayout::parse(expected.list);
  QVERIFY(!result);
  QVERIFY2(!result.error.empty(), "an error says what is wrong");
  QVERIFY2(QString::fromStdString(result.error).contains(expected.error), qPrintable(errorOf(expected.list)));
}

void MonitorGeometryTests::duplicates_areOneMonitor()
{
  // a mirrored display has the bounds of the display it mirrors
  const auto result = MonitorLayout::parse("0,0,1920,1080;0,0,1920,1080;1920,0,1280,720;0,0,1920,1080");
  QVERIFY(result);
  QCOMPARE(result.layout->monitors().size(), size_t{2});
  QCOMPARE(result.duplicatesRemoved, size_t{2});
  QCOMPARE(QString::fromStdString(result.layout->format()), QStringLiteral("0,0,1920,1080;1920,0,1280,720"));

  QVERIFY(!MonitorLayout::parse("0,0,1920,1080;0,0,1920,1081"));
}

void MonitorGeometryTests::entry_onAStaggeredDesktop_landsOnTheMonitorThatHasTheRow()
{
  // a 1080 high monitor on the right of a 1440 high one, top aligned: below it the right edge is the one of the left
  // one
  const auto layout = layoutOf("0,0,2560,1440;2560,0,1920,1080");
  for (int32_t row = 0; row < 1440; ++row) {
    const auto at = layout.entryPoint(Direction::Right, layout.edgeFraction(Direction::Right, row));
    QCOMPARE(text(at), text(MonitorPoint{row < 1080 ? 4479 : 2559, row}));
  }
  for (int32_t row = 0; row < 1440; ++row) {
    const auto at = layout.entryPoint(Direction::Left, layout.edgeFraction(Direction::Left, row));
    QCOMPARE(text(at), text(MonitorPoint{0, row}));
  }
  for (int32_t column = 0; column < 4480; ++column) {
    const auto at = layout.entryPoint(Direction::Bottom, layout.edgeFraction(Direction::Bottom, column));
    QCOMPARE(text(at), text(MonitorPoint{column, column < 2560 ? 1439 : 1079}));
  }

  // the left one, bottom aligned
  const auto left = layoutOf("-1920,360,1920,1080;0,0,2560,1440");
  QCOMPARE(text(left.entryPoint(Direction::Left, left.edgeFraction(Direction::Left, 100))), text(MonitorPoint{0, 100}));
  QCOMPARE(
      text(left.entryPoint(Direction::Left, left.edgeFraction(Direction::Left, 800))), text(MonitorPoint{-1920, 800})
  );
  QCOMPARE(
      text(left.entryPoint(Direction::Top, left.edgeFraction(Direction::Top, -1000))), text(MonitorPoint{-1000, 360})
  );
  QCOMPARE(text(left.entryPoint(Direction::Top, left.edgeFraction(Direction::Top, 500))), text(MonitorPoint{500, 0}));
}

void MonitorGeometryTests::entry_onAnLShapedDesktop_landsInsideAMonitor()
{
  // the portrait monitor on the right reaches 480 pixels higher than the left one
  const auto layout = layoutOf("0,0,2560,1440;2560,-480,1080,1920");
  QCOMPARE(
      text(layout.entryPoint(Direction::Left, layout.edgeFraction(Direction::Left, -100))),
      text(MonitorPoint{2560, -100})
  );
  QCOMPARE(
      text(layout.entryPoint(Direction::Left, layout.edgeFraction(Direction::Left, 700))), text(MonitorPoint{0, 700})
  );
  QCOMPARE(
      text(layout.entryPoint(Direction::Top, layout.edgeFraction(Direction::Top, 3000))), text(MonitorPoint{3000, -480})
  );
  QCOMPARE(
      text(layout.entryPoint(Direction::Top, layout.edgeFraction(Direction::Top, 100))), text(MonitorPoint{100, 0})
  );
  QCOMPARE(
      text(layout.entryPoint(Direction::Right, layout.edgeFraction(Direction::Right, -300))),
      text(MonitorPoint{3639, -300})
  );
}

void MonitorGeometryTests::entry_depth_staysOnTheMonitor()
{
  const auto layout = layoutOf("-1920,360,1920,1080;0,0,2560,1440");
  const auto fraction = layout.edgeFraction(Direction::Left, 800);
  QCOMPARE(text(layout.entryPoint(Direction::Left, fraction, 0)), text(MonitorPoint{-1920, 800}));
  QCOMPARE(text(layout.entryPoint(Direction::Left, fraction, 5)), text(MonitorPoint{-1915, 800}));
  // into the monitor next to it, where the row goes on
  QCOMPARE(text(layout.entryPoint(Direction::Left, fraction, 1925)), text(MonitorPoint{5, 800}));
  // and no further than the desktop goes
  QCOMPARE(text(layout.entryPoint(Direction::Left, fraction, 5000)), text(MonitorPoint{2559, 800}));

  // where the row ends at an edge, so does the depth
  const auto lshape = layoutOf("0,0,1000,1000;1000,500,1000,500");
  QCOMPARE(
      text(lshape.entryPoint(Direction::Right, lshape.edgeFraction(Direction::Right, 200), 5000)),
      text(MonitorPoint{0, 200})
  );
  QCOMPARE(
      text(lshape.entryPoint(Direction::Right, lshape.edgeFraction(Direction::Right, 200), 5)),
      text(MonitorPoint{994, 200})
  );
}

void MonitorGeometryTests::move_againstAnInnerEdge_isBlockedAndSlides()
{
  // an L: the second monitor is lower, so the first one has an edge on its right above it
  const auto layout = layoutOf("0,0,1000,1000;1000,500,1000,500");

  // above the second monitor the pointer cannot go on, and says how much of the move was left
  auto moved = layout.move({999, 200}, 5, 0);
  QCOMPARE(text(moved.position), text(MonitorPoint{999, 200}));
  QCOMPARE(moved.restX, 5);
  QCOMPARE(moved.restY, 0);

  // pushed against it while moving down, it slides down
  moved = layout.move({999, 200}, 5, 3);
  QCOMPARE(text(moved.position), text(MonitorPoint{999, 203}));
  QCOMPARE(moved.restX, 5);
  QCOMPARE(moved.restY, 0);

  // next to the second monitor it goes on
  moved = layout.move({999, 600}, 5, 0);
  QCOMPARE(text(moved.position), text(MonitorPoint{1004, 600}));
  QCOMPARE(moved.restX, 0);

  // a move to the left that does not go as far as the edge is not blocked, one that goes beyond it is
  moved = layout.move({3, 100}, -10, 0);
  QCOMPARE(text(moved.position), text(MonitorPoint{0, 100}));
  QCOMPARE(moved.restX, -7);

  // the corner of the first monitor, going toward the second one: x is blocked at the edge, y goes down onto it
  moved = layout.move({999, 499}, 5, 3);
  QCOMPARE(text(moved.position), text(MonitorPoint{999, 502}));
  QCOMPARE(moved.restX, 5);

  // the way up from the second monitor is blocked by the top of it, the way along it is not
  moved = layout.move({1500, 505}, 4, -20);
  QCOMPARE(text(moved.position), text(MonitorPoint{1504, 500}));
  QCOMPARE(moved.restX, 0);
  QCOMPARE(moved.restY, -15);
}

void MonitorGeometryTests::move_hasNoDeadTravel()
{
  const auto layout = layoutOf("0,0,1000,1000;1000,500,1000,500");

  // however much the pointer is pushed against an edge, it stays on it, and a move away from it goes at once
  MonitorPoint at{999, 200};
  for (int i = 0; i < 100; ++i) {
    const auto moved = layout.move(at, 5, 0);
    QCOMPARE(moved.restX, 5);
    at = moved.position;
  }
  QCOMPARE(text(at), text(MonitorPoint{999, 200}));
  const auto back = layout.move(at, -1, 0);
  QCOMPARE(text(back.position), text(MonitorPoint{998, 200}));
  QCOMPARE(back.restX, 0);

  // the same at the outer edge of the desktop
  const auto outer = layout.move({1999, 700}, 400, 0);
  QCOMPARE(text(outer.position), text(MonitorPoint{1999, 700}));
  QCOMPARE(outer.restX, 400);
  QCOMPARE(text(layout.move(outer.position, -1, 0).position), text(MonitorPoint{1998, 700}));
}

void MonitorGeometryTests::move_fromAHole_startsOnTheNearestMonitor()
{
  const auto layout = layoutOf("0,0,2560,1440;320,-1080,1920,1080");
  const auto moved = layout.move({100, -500}, 0, 0);
  QCOMPARE(text(moved.position), text(MonitorPoint{320, -500}));
  QCOMPARE(text(layout.nearest(100, -500)), text(MonitorPoint{320, -500}));
  QCOMPARE(text(layout.nearest(5000, 5000)), text(MonitorPoint{2559, 1439}));
  QCOMPARE(text(layout.nearest(10, 10)), text(MonitorPoint{10, 10}));
}

void MonitorGeometryTests::move_overAMonitorBoundary_goesOn()
{
  const auto layout = layoutOf("0,0,1920,1080;1920,0,1920,1080");
  auto moved = layout.move({1900, 500}, 50, 0);
  QCOMPARE(text(moved.position), text(MonitorPoint{1950, 500}));
  QCOMPARE(moved.restX, 0);
  moved = layout.move({1900, 500}, 4000, 0);
  QCOMPARE(text(moved.position), text(MonitorPoint{3839, 500}));
  QCOMPARE(moved.restX, 4000 - 1939);
}

void MonitorGeometryTests::parse_data()
{
  QTest::addColumn<QString>("list");
  QTest::addColumn<QString>("expected");

  QTest::newRow("one monitor") << "0,0,1920,1080" << "0,0,1920,1080";
  QTest::newRow("a list") << "0,0,1920,1080;1920,0,1280,720" << "0,0,1920,1080;1920,0,1280,720";
  QTest::newRow("spaces") << " 0, 0 ,1920, 1080 ; 1920 ,0, 1280,720 " << "0,0,1920,1080;1920,0,1280,720";
  QTest::newRow("a separator at the end") << "0,0,1920,1080;1920,0,1280,720;" << "0,0,1920,1080;1920,0,1280,720";
  QTest::newRow("empty items") << ";;0,0,1920,1080;;1920,0,1280,720;;" << "0,0,1920,1080;1920,0,1280,720";
  QTest::newRow("a line for each monitor") << "0,0,1920,1080\n1920,0,1280,720\n" << "0,0,1920,1080;1920,0,1280,720";
  QTest::newRow("windows line ends") << "0,0,1920,1080\r\n1920,0,1280,720\r\n" << "0,0,1920,1080;1920,0,1280,720";
  QTest::newRow("tabs and blank lines") << "\n\t0,0,1920,1080\t\n\n\t1920,0,1280,720\n\n"
                                        << "0,0,1920,1080;1920,0,1280,720";
  QTest::newRow("a byte order mark") << QString::fromUtf8(
                                            "\xEF\xBB\xBF"
                                            "0,0,1920,1080"
                                        )
                                     << "0,0,1920,1080";
  QTest::newRow("negative numbers") << "-1920,-360,1920,1080;0,-1080,1920,1080"
                                    << "-1920,-360,1920,1080;0,-1080,1920,1080";
  QTest::newRow("a monitor line") << "monitor 0,0,1920,1080" << "0,0,1920,1080";
  QTest::newRow("monitor lines with what the tools print")
      << "monitor 0,0,2560,1440 scale=150% primary\nmonitor 2560,0,1920,1080 scale=100%"
      << "0,0,2560,1440;2560,0,1920,1080";
  QTest::newRow("a scale that is not whole") << "  Monitor 0,0,1920,1080 scale=1.5 PRIMARY" << "0,0,1920,1080";
  QTest::newRow("the biggest desktop") << "-32768,-32768,32767,32767" << "-32768,-32768,32767,32767";
  QTest::newRow("the whole block that a tool prints")
      << "computer: SUBEOM\nspace: points x,y,w,h scale\n  monitor 0,0,2064,1347 scale=2.0 primary\n"
      << "0,0,2064,1347";
}

void MonitorGeometryTests::parse()
{
  QFETCH(QString, list);
  QFETCH(QString, expected);
  const auto result = MonitorLayout::parse(list.toStdString());
  QVERIFY2(result, result.error.c_str());
  QCOMPARE(QString::fromStdString(result.layout->format()), expected);
}

void MonitorGeometryTests::parse_errors_data()
{
  QTest::addColumn<QString>("list");
  QTest::addColumn<QString>("error");

  QTest::newRow("a line that is not a monitor in a block")
      << "computer: X\nspace: y\n  monitor 0,0,100,100 scale=1.0\n  something else\n"
      << "line 4";
  QTest::newRow("the line of the monitor that is wrong") << "0,0,100,100\n100,0,x,100" << "line 2";
  QTest::newRow("the monitor that is wrong in a list") << "0,0,100,100;100,0,100,0" << "monitor 2";
  QTest::newRow("a monitor with no numbers") << "monitor" << "line 1";
  QTest::newRow("a minus sign alone") << "-,0,10,10" << "not a whole number";
  QTest::newRow("a plus sign") << "+5,0,10,10" << "not a whole number";
  QTest::newRow("an empty number") << "0,,10,10" << "not a whole number";
  QTest::newRow(
      "too many monitors"
  ) << "0,0,10,10;10,0,10,10;20,0,10,10;30,0,10,10;40,0,10,10;50,0,10,10;60,0,10,10;70,0,10,10;80,0,10,10;90,0,10,10;"
       "100,0,10,10;110,0,10,10;120,0,10,10;130,0,10,10;140,0,10,10;150,0,10,10;160,0,10,10"
    << "at most 16";
}

void MonitorGeometryTests::parse_errors()
{
  QFETCH(QString, list);
  QFETCH(QString, error);
  const auto result = MonitorLayout::parse(list.toStdString());
  QVERIFY(!result);
  QVERIFY2(QString::fromStdString(result.error).contains(error), result.error.c_str());
}

void MonitorGeometryTests::parse_aBlockToPaste_isTheSameAsAList()
{
  const auto fromBlock = layoutOf(MonitorLayout::extractPasteBlock(kMacBlock));
  const auto fromList = layoutOf("0,0,2064,1347;2064,-300,2560,1440");
  QCOMPARE(fromBlock.monitors().size(), fromList.monitors().size());
  QCOMPARE(QString::fromStdString(fromBlock.format()), QString::fromStdString(fromList.format()));

  // the whole output is not a block to paste, and says so
  QVERIFY(!MonitorLayout::parse(kMacBlock));
}

void MonitorGeometryTests::parse_rejectsWhatIsNotAShapeThatCanBeReported()
{
  // 16 bits for the position and 16 bits for the size of the shape of a computer
  QVERIFY(MonitorLayout::parse("-32768,0,32767,100"));
  QVERIFY(!MonitorLayout::parse("-32769,0,100,100"));
  QVERIFY(!MonitorLayout::parse("32768,0,100,100"));
  QVERIFY(!MonitorLayout::parse("0,0,32768,100"));
  const auto wide = MonitorLayout::parse("0,0,20000,100;20000,0,20000,100");
  QVERIFY(!wide);
  QVERIFY2(QString::fromStdString(wide.error).contains("fit"), wide.error.c_str());
  QVERIFY(!MonitorLayout::parse("0,0,100,20000;0,20000,100,20000"));
}

void MonitorGeometryTests::parse_aHugeListIsRefusedAtOnce()
{
  // a file that is not a list of monitors at all must not hold the dialog up while it is looked through
  std::vector<MonitorRect> many;
  for (int i = 0; i < 100000; ++i)
    many.push_back({i % 1000, i / 1000, 1, 1});
  const auto begin = std::chrono::steady_clock::now();
  const auto result = MonitorLayout::create(many);
  const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin);
  QVERIFY(!result);
  QVERIFY2(QString::fromStdString(result.error).contains("at most 16"), result.error.c_str());
  QVERIFY2(took.count() < 500, qPrintable(QString::number(took.count())));
}

void MonitorGeometryTests::format_isWhatParseReads()
{
  for (const auto &entry : kCatalogue) {
    const auto layout = layoutOf(entry.list);
    const auto again = layoutOf(layout.format());
    QCOMPARE(again.monitors(), layout.monitors());
    QCOMPARE(QString::fromStdString(again.format()), QString::fromStdString(layout.format()));
  }
}

void MonitorGeometryTests::extractPasteBlock_data()
{
  QTest::addColumn<QString>("output");
  QTest::addColumn<QString>("expected");

  QTest::newRow("macOS") << kMacBlock << "0,0,2064,1347;2064,-300,2560,1440";
  QTest::newRow("windows") << kWindowsBlock << "-1920,360,1920,1080;0,0,2560,1440";
  QTest::newRow("a file that was saved with a byte order mark")
      << QString::fromUtf8("\xEF\xBB\xBF") + kWindowsBlock << "-1920,360,1920,1080;0,0,2560,1440";
  QTest::newRow("monitor lines and no heading") << "some text\n  monitor 0,0,1920,1080 scale=100% primary\nmore text"
                                                << "0,0,1920,1080";
  QTest::newRow("no monitors") << "computer: X\nnothing to see\n" << "";
  QTest::newRow("a list is not a block") << "0,0,1920,1080" << "";
}

void MonitorGeometryTests::extractPasteBlock()
{
  QFETCH(QString, output);
  QFETCH(QString, expected);
  const auto block = MonitorLayout::extractPasteBlock(output.toStdString());
  if (expected.isEmpty()) {
    QVERIFY(block.empty());
    return;
  }
  QVERIFY(!block.empty());
  const auto result = MonitorLayout::parse(block);
  QVERIFY2(result, result.error.c_str());
  QCOMPARE(QString::fromStdString(result.layout->format()), expected);
}

void MonitorGeometryTests::plainRectangle_isWhatDeskflowHasAlwaysDone_data()
{
  QTest::addColumn<QString>("shape");

  // the screens of the Macs, a few that are in the way of an off by one, and the ones of the PCs
  for (const char *shape :
       {"0,0,1728,1117", "0,0,2064,1347", "0,0,1512,982", "0,0,1440,900", "0,0,1,1", "0,0,2,3", "-5,-7,101,53",
        "-1920,0,4480,1440", "0,-1080,2560,2520", "0,0,3840,2160", "0,0,5120,1440", "100,200,1080,1920"})
    QTest::newRow(shape) << shape;
}

// The silhouette of a rectangle is its sides, so what a layout of a rectangle does has to be what
// Server::mapToFraction() and Server::mapToPixel() have always done, for every pixel of every edge
void MonitorGeometryTests::plainRectangle_isWhatDeskflowHasAlwaysDone()
{
  QFETCH(QString, shape);
  const auto layout = layoutOf(shape.toStdString());
  const auto box = layout.boundingBox();
  QVERIFY(layout.isPlainRectangle());

  for (const auto side : kSides) {
    const auto origin = originOf(side, box);
    const auto extent = extentOf(side, box);
    // the pixels of the edge and a few that are beyond it
    for (int32_t line = origin - 3; line < origin + extent + 3; ++line) {
      const float legacy = deskflow::server::getCursorEdgeFraction(line, origin, extent);
      QCOMPARE(bitsOf(layout.edgeFraction(side, line)), bitsOf(legacy));
    }
    for (int32_t line = origin; line < origin + extent; ++line) {
      const float fraction = deskflow::server::getCursorEdgeFraction(line, origin, extent);
      for (const int32_t depth : {0, 1, 2, 5}) {
        // Server::mapToNeighbor() takes it out of a computer that it does not go that far into
        if (depth >= (isHorizontal(side) ? box.w : box.h))
          continue;
        const auto at = layout.entryPoint(side, fraction, depth);
        // Server::mapToPixel() along the edge, Server::mapToNeighbor() across it
        QCOMPARE(along(side, at), static_cast<int32_t>(fraction * extent) + origin);
        QCOMPARE(along(side, at), line);
        using enum Direction;
        const int32_t across = side == Left    ? box.x + depth
                               : side == Right ? box.right() - 1 - depth
                               : side == Top   ? box.y + depth
                                               : box.bottom() - 1 - depth;
        QCOMPARE(isHorizontal(side) ? at.x : at.y, across);
      }
    }
    // fractions that no pixel gives: the ends and what is next to them, and a few in between
    std::vector<float> fractions = {0.0f,   std::nextafter(0.0f, 1.0f), 0.5f, std::nextafter(1.0f, 0.0f), 0.3333f,
                                    0.9999f};
    std::mt19937 random(kSeed);
    for (int i = 0; i < 500; ++i)
      fractions.push_back(std::uniform_real_distribution<float>(0.0f, 1.0f)(random));
    for (const float fraction : fractions) {
      const auto at = layout.entryPoint(side, std::min(fraction, std::nextafter(1.0f, 0.0f)));
      QCOMPARE(along(side, at), static_cast<int32_t>(std::min(fraction, std::nextafter(1.0f, 0.0f)) * extent) + origin);
    }
  }
}

void MonitorGeometryTests::plainRectangle_ofSeveralMonitors_isWhatDeskflowHasAlwaysDone()
{
  // monitors that make up a rectangle are one
  const auto layout = layoutOf("0,0,1920,1080;1920,0,1920,1080;0,1080,1920,1080;1920,1080,1920,1080");
  const auto whole = layoutOf("0,0,3840,2160");
  QVERIFY(layout.isPlainRectangle());
  for (const auto side : kSides) {
    const auto extent = extentOf(side, layout.boundingBox());
    for (int32_t line = 0; line < extent; ++line) {
      QCOMPARE(bitsOf(layout.edgeFraction(side, line)), bitsOf(whole.edgeFraction(side, line)));
      const auto fraction = whole.edgeFraction(side, line);
      QCOMPARE(text(layout.entryPoint(side, fraction, 3)), text(whole.entryPoint(side, fraction, 3)));
    }
  }
  // and the pointer moves as it does on the whole one
  std::mt19937 random(kSeed);
  MonitorPoint a{1900, 1000};
  MonitorPoint b = a;
  for (int i = 0; i < 5000; ++i) {
    const int32_t dx = std::uniform_int_distribution<int32_t>(-300, 300)(random);
    const int32_t dy = std::uniform_int_distribution<int32_t>(-300, 300)(random);
    const auto movedLayout = layout.move(a, dx, dy);
    const auto movedWhole = whole.move(b, dx, dy);
    QCOMPARE(text(movedLayout.position), text(movedWhole.position));
    QCOMPARE(movedLayout.restX, movedWhole.restX);
    QCOMPARE(movedLayout.restY, movedWhole.restY);
    a = movedLayout.position;
    b = movedWhole.position;
  }
}

void MonitorGeometryTests::property_anEntryIsOnAMonitor()
{
  std::mt19937 random(kSeed);
  for (int n = 0; n < kLayouts; ++n) {
    const auto layout = layoutOf(randomMonitors(random));
    for (const auto side : kSides) {
      for (int i = 0; i < 200; ++i) {
        const float fraction = std::uniform_real_distribution<float>(0.0f, 1.0f)(random);
        const int32_t depth = i % 4 == 0 ? 0 : std::uniform_int_distribution<int32_t>(0, 3000)(random);
        const auto at = layout.entryPoint(side, std::min(fraction, std::nextafter(1.0f, 0.0f)), depth);
        if (!layout.contains(at.x, at.y))
          QFAIL(qPrintable(QStringLiteral("layout %1: %2 is on no monitor").arg(n).arg(text(at))));
      }
    }
  }
}

void MonitorGeometryTests::property_anEntryIsOnTheEdge()
{
  // without depth, the pixel that the pointer enters on has nothing beyond it: the way it came in is closed
  std::mt19937 random(kSeed + 1);
  for (int n = 0; n < kLayouts; ++n) {
    const auto layout = layoutOf(randomMonitors(random));
    for (const auto side : kSides) {
      for (int i = 0; i < 100; ++i) {
        const float fraction = std::uniform_real_distribution<float>(0.0f, 0.9999f)(random);
        const auto at = layout.entryPoint(side, fraction);
        using enum Direction;
        const int32_t outX = at.x + (side == Left ? -1 : side == Right ? 1 : 0);
        const int32_t outY = at.y + (side == Top ? -1 : side == Bottom ? 1 : 0);
        if (layout.contains(outX, outY))
          QFAIL(qPrintable(QStringLiteral("layout %1: %2 is not on the edge").arg(n).arg(text(at))));
      }
    }
  }
}

void MonitorGeometryTests::property_entryIsMonotone()
{
  std::mt19937 random(kSeed + 2);
  for (int n = 0; n < kLayouts; ++n) {
    const auto layout = layoutOf(randomMonitors(random));
    for (const auto side : kSides) {
      std::vector<float> fractions;
      for (int i = 0; i < 300; ++i)
        fractions.push_back(std::uniform_real_distribution<float>(0.0f, 0.99999f)(random));
      std::ranges::sort(fractions);
      int32_t last = std::numeric_limits<int32_t>::min();
      for (const float fraction : fractions) {
        const auto line = along(side, layout.entryPoint(side, fraction));
        if (line < last)
          QFAIL(qPrintable(QStringLiteral("layout %1 is not monotone").arg(n)));
        last = line;
      }
    }
  }
}

void MonitorGeometryTests::property_fractionAndLineMapToEachOtherExactly()
{
  std::mt19937 random(kSeed + 3);
  for (int n = 0; n < kLayouts; ++n) {
    const auto layout = layoutOf(randomMonitors(random));
    const auto box = layout.boundingBox();
    for (const auto side : kSides) {
      const auto origin = originOf(side, box);
      const auto extent = extentOf(side, box);
      for (int32_t line = origin; line < origin + extent; ++line) {
        // the fraction a pointer leaves at is the line it enters on
        const auto at = layout.entryPoint(side, layout.edgeFraction(side, line));
        if (along(side, at) != line)
          QFAIL(
              qPrintable(QStringLiteral("layout %1: line %2 comes back as %3").arg(n).arg(line).arg(along(side, at)))
          );
        // and an entry that is read as a fraction again is the same entry
        const auto again = layout.entryPoint(side, layout.edgeFraction(side, along(side, at)));
        if (!(again == at))
          QFAIL(qPrintable(QStringLiteral("layout %1: %2 is not the same entry again").arg(n).arg(text(at))));
      }
    }
  }
}

void MonitorGeometryTests::property_aRandomWalkNeverLeavesTheDesktop()
{
  std::mt19937 random(kSeed + 4);
  for (int n = 0; n < kLayouts; ++n) {
    const auto layout = layoutOf(randomMonitors(random));
    const auto box = layout.boundingBox();
    MonitorPoint at = layout.nearest(box.x + box.w / 2, box.y + box.h / 2);
    for (int i = 0; i < 1500; ++i) {
      const bool big = i % 25 == 0;
      const int32_t reach = big ? 1500 : 60;
      const int32_t dx = std::uniform_int_distribution<int32_t>(-reach, reach)(random);
      const int32_t dy = std::uniform_int_distribution<int32_t>(-reach, reach)(random);
      const auto moved = layout.move(at, dx, dy);
      if (!layout.contains(moved.position.x, moved.position.y))
        QFAIL(qPrintable(QStringLiteral("layout %1 lost the pointer at %2").arg(n).arg(text(moved.position))));
      // what was not done has the sign of the move and is not more than it
      QVERIFY(moved.restX == 0 || (moved.restX > 0) == (dx > 0));
      QVERIFY(moved.restY == 0 || (moved.restY > 0) == (dy > 0));
      QVERIFY(std::abs(moved.restX) <= std::abs(dx) && std::abs(moved.restY) <= std::abs(dy));
      // nothing is lost: what was done and what was not make the move
      QCOMPARE(moved.position.x - at.x + moved.restX, dx);
      QCOMPARE(moved.position.y - at.y + moved.restY, dy);
      // and a pointer that was not blocked is where the move puts it
      if (moved.restX == 0 && moved.restY == 0) {
        QCOMPARE(moved.position.x, at.x + dx);
        QCOMPARE(moved.position.y, at.y + dy);
      }
      at = moved.position;
    }
  }
}

void MonitorGeometryTests::property_goingThereAndBackComesBackToTheSameRow()
{
  std::mt19937 random(kSeed + 5);
  for (int n = 0; n < kLayouts; ++n) {
    const auto a = layoutOf(randomMonitors(random));
    const auto b = layoutOf(randomMonitors(random));
    for (const auto side : kSides) {
      const auto boxA = a.boundingBox();
      const auto boxB = b.boundingBox();
      const auto extentA = extentOf(side, boxA);
      const auto extentB = extentOf(side, boxB);
      // as many lines of A as there are in a line of B
      const int32_t slack = (extentA + extentB - 1) / extentB;
      for (int i = 0; i < 100; ++i) {
        const int32_t line = originOf(side, boxA) + std::uniform_int_distribution<int32_t>(0, extentA - 1)(random);
        // the pointer leaves A at its line, comes in on B, and goes straight back
        const auto in = b.entryPoint(opposite(side), a.edgeFraction(side, line));
        const auto back = a.entryPoint(side, b.edgeFraction(opposite(side), along(side, in)));
        const int32_t distance = std::abs(along(side, back) - line);
        if (distance > slack)
          QFAIL(
              qPrintable(QStringLiteral("layout %1: line %2 comes back as %3").arg(n).arg(line).arg(along(side, back)))
          );
      }
    }
  }
}

QTEST_MAIN(MonitorGeometryTests)
