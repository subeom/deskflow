/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ArrangementTests.h"

#include "common/Settings.h"
#include "server/Arrangement.h"
#include "server/Config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace deskflow::server;
using namespace deskflow::server::arrangement;

namespace {

constexpr unsigned kSeed = 20261116;

// the screen of the MacBook Pro of the user below a DELL ultrawide, as the operating system gives it, in points
constexpr auto kServerShape = "0,0,3440,1440;753,1440,2064,1347";
// a Windows desktop: a 2560x1440 primary monitor with a 1920x1080 one on its left, bottoms in line
constexpr auto kWindowsShape = "-1920,360,1920,1080;0,0,2560,1440";
constexpr auto kHd = "0,0,1920,1080";

Computer pc(std::string_view name, std::string_view monitors, double x, double y, double zoom = 1.0)
{
  const auto layout = MonitorLayout::parse(monitors);
  if (!layout)
    qFatal("the monitors \"%s\" were not accepted: %s", std::string(monitors).c_str(), layout.error.c_str());
  Computer computer;
  computer.name = std::string(name);
  computer.monitors = layout.layout->monitors();
  computer.originX = x;
  computer.originY = y;
  computer.zoom = zoom;
  return computer;
}

Computer server(std::string_view name, std::string_view monitors, double x, double y)
{
  auto computer = pc(name, monitors, x, y);
  computer.isServer = true;
  return computer;
}

// every link on a line: "A right(0,75) = B"
std::string dump(const DerivedLinks &derived)
{
  std::string text;
  for (const auto &link : derived.links)
    text += std::format(
        "{} {}{} = {}{}\n", link.from, directionName(link.side), formatInterval(link.srcStart, link.srcEnd), link.to,
        formatInterval(link.dstStart, link.dstEnd)
    );
  return text;
}

std::string describe(const Issue &issue)
{
  static constexpr std::array<const char *, 9> kinds = {"NameIsEmpty",  "DuplicateName",   "InvalidMonitors",
                                                        "InvalidZoom",  "InvalidPosition", "Overlap",
                                                        "NotConnected", "AmbiguousSide",   "EdgeInTheSameRows"};
  std::string text = kinds.at(static_cast<size_t>(issue.kind));
  text += " " + issue.computer;
  if (!issue.other.empty())
    text += " " + issue.other;
  if (issue.side != Direction::NoDirection)
    text += std::string(" ") + directionName(issue.side);
  return text;
}

std::string describe(const std::vector<Issue> &issues)
{
  std::string text;
  for (const auto &issue : issues)
    text += describe(issue) + "\n";
  return text;
}

bool hasErrors(const std::vector<Issue> &issues)
{
  return std::ranges::any_of(issues, [](const Issue &i) { return i.severity() == Issue::Severity::Error; });
}

struct Case
{
  const char *name;
  Arrangement arrangement;
  std::string expected;
};

Direction opposite(Direction side)
{
  using enum Direction;
  return side == Left ? Right : side == Right ? Left : side == Top ? Bottom : Top;
}

//
// The table of layouts
//

const std::vector<Case> &layoutCases()
{
  static const std::vector<Case> cases = [] {
    std::vector<Case> c;
    c.push_back({
        "a row of three equal computers",
        {{pc("A", kHd, 0, 0), pc("B", kHd, 1920, 0), pc("C", kHd, 3840, 0)}},
        "A right = B\nB right = C\nB left = A\nC left = B\n",
    });
    c.push_back({
        "one above the other",
        {{pc("A", kHd, 0, 0), pc("B", kHd, 0, 1080)}},
        "A down = B\nB up = A\n",
    });
    c.push_back({
        "a smaller one, top aligned",
        {{pc("A", kHd, 0, 0), pc("B", "0,0,2560,1440", 1920, 0)}},
        "A right = B(0,75)\nB left(0,75) = A\n",
    });
    c.push_back({
        "a bigger one, bottom aligned",
        {{pc("A", kHd, 0, 0), pc("B", "0,0,2560,1440", 1920, -360)}},
        "A right = B(25,100)\nB left(25,100) = A\n",
    });
    c.push_back({
        "a bigger one, centred",
        {{pc("A", kHd, 0, 0), pc("B", "0,0,2560,1440", 1920, -180)}},
        "A right = B(12.5,87.5)\nB left(12.5,87.5) = A\n",
    });
    c.push_back({
        "staggered by half",
        {{pc("A", kHd, 0, 0), pc("B", kHd, 1920, 540)}},
        "A right(50,100) = B(0,50)\nB left(0,50) = A(50,100)\n",
    });
    c.push_back({
        "two computers on one side",
        {{pc("A", "0,0,1920,2160", 0, 0), pc("B", kHd, 1920, 0), pc("C", kHd, 1920, 1080)}},
        "A right(0,50) = B\nA right(50,100) = C\nB left = A(0,50)\nB down = C\nC left = A(50,100)\nC up = B\n",
    });
    c.push_back({
        "two monitors one above the other are one edge",
        {{pc("A", "0,0,1920,1080;0,1080,1920,1080", 0, 0), pc("B", "0,0,1920,2160", 1920, 0)}},
        "A right = B\nB left = A\n",
    });
    c.push_back({
        "negative origins",
        {{pc("A", kHd, -5000, -3000), pc("B", kHd, -3080, -3000)}},
        "A right = B\nB left = A\n",
    });
    c.push_back({
        "a gap",
        {{pc("A", kHd, 0, 0), pc("B", kHd, 1970, 0)}},
        "",
    });
    c.push_back({
        "a corner is no edge",
        {{pc("A", kHd, 0, 0), pc("B", kHd, 1920, 1080)}},
        "",
    });
    c.push_back({
        "the screen of the server with a Windows PC on its left and a Mac mini on its right",
        {{server("srv", kServerShape, 0, 0), pc("PC1", kWindowsShape, -2560, 0), pc("MINI", kHd, 3440, 0)}},
        "srv right(0,38.7513) = MINI\nsrv left(0,51.6685) = PC1\nPC1 right = srv(0,51.6685)\nMINI left = "
        "srv(0,38.7513)\n",
    });
    c.push_back({
        "a Windows PC at 150 % next to a Mac",
        {{pc("MAC", "0,0,1728,1117", 0, 0), pc("WIN", "0,0,2560,1440", 1728, 0, 2.0 / 3.0)}},
        "MAC right(0,85.9445) = WIN\nWIN left = MAC(0,85.9445)\n",
    });
    return c;
  }();
  return cases;
}

//
// Random arrangements, from the classic grid: the sizes and the zooms are what is random
//

constexpr std::array<const char *, 9> kShapes = {
    "0,0,1920,1080",
    "0,0,2560,1440",
    "0,0,1728,1117",
    "0,0,3440,1440",
    kWindowsShape,
    kServerShape,
    "0,0,1920,1080;1920,540,1280,720",
    "0,0,1920,1080;0,1080,1920,1080",
    "0,-1080,2560,1080;0,0,2560,1440",
};
constexpr std::array<double, 4> kZooms = {1.0, 2.0 / 3.0, 0.8, 0.5};

std::vector<GridCell> randomGrid(std::mt19937 &random, bool equal)
{
  const auto between = [&random](int low, int high) { return std::uniform_int_distribution<int>(low, high)(random); };
  std::vector<int> cells(15);
  for (int i = 0; i < 15; ++i)
    cells[static_cast<size_t>(i)] = i;
  std::ranges::shuffle(cells, random);
  const auto count = between(2, 9);
  std::vector<GridCell> grid;
  for (int i = 0; i < count; ++i) {
    const auto cell = cells[static_cast<size_t>(i)];
    GridCell gridCell;
    gridCell.column = cell % 5;
    gridCell.row = cell / 5;
    gridCell.computer = equal ? pc(std::format("c{}", cell), kHd, 0, 0)
                              : pc(std::format("c{}", cell), kShapes.at(static_cast<size_t>(between(0, 8))), 0, 0,
                                   kZooms.at(static_cast<size_t>(between(0, 3))));
    grid.push_back(gridCell);
  }
  return grid;
}

// what the classic grid writes for \p cells: ServerConfig's operator<<
std::string classicLinks(const std::vector<GridCell> &cells)
{
  auto sorted = cells;
  std::ranges::sort(sorted, [](const GridCell &a, const GridCell &b) {
    return std::tie(a.row, a.column) < std::tie(b.row, b.column);
  });
  const auto nameAt = [&](int column, int row) -> const std::string * {
    if (column < 0 || column >= 5 || row < 0 || row >= 3)
      return nullptr;
    for (const auto &cell : sorted) {
      if (cell.column == column && cell.row == row)
        return &cell.computer.name;
    }
    return nullptr;
  };
  std::string text = "section: links\n";
  for (const auto &cell : sorted) {
    text += std::format("\t{}:\n", cell.computer.name);
    const std::array<std::tuple<const char *, int, int>, 4> directions = {
        {{"right", 1, 0}, {"left", -1, 0}, {"up", 0, -1}, {"down", 0, 1}}
    };
    for (const auto &[name, dx, dy] : directions) {
      if (const auto *other = nameAt(cell.column + dx, cell.row + dy))
        text += std::format("\t\t{} = {}\n", name, *other);
    }
  }
  return text + "end\n\n";
}

Arrangement serverWithThreePcs()
{
  return {{
      server("srv", kServerShape, 0, 0),
      pc("PC1", kWindowsShape, -2560, 0),
      pc("MINI", kHd, 3440, 0),
      pc("PC2", "0,0,1600,900", -847, 1440),
  }};
}

// the physical position along the edge of \p side of the computer \p box of the fraction \p fraction of it
double physicalAlong(const Rect &box, Direction side, double fraction)
{
  const bool horizontal = side == Direction::Left || side == Direction::Right;
  return (horizontal ? box.y : box.x) + fraction * (horizontal ? box.h : box.w);
}

} // namespace

void ArrangementTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
}

//
// Derivation
//

void ArrangementTests::derive_layouts_data()
{
  QTest::addColumn<int>("index");
  for (size_t i = 0; i < layoutCases().size(); ++i)
    QTest::newRow(layoutCases().at(i).name) << static_cast<int>(i);
}

void ArrangementTests::derive_layouts()
{
  QFETCH(int, index);
  const auto &c = layoutCases().at(static_cast<size_t>(index));
  QVERIFY2(!hasErrors(validate(c.arrangement)), qPrintable(QString::fromStdString(describe(validate(c.arrangement)))));
  QCOMPARE(QString::fromStdString(dump(deriveLinks(c.arrangement))), QString::fromStdString(c.expected));
}

void ArrangementTests::derive_theServerOfTheUserWithThreePcs()
{
  const auto arrangement = serverWithThreePcs();
  // the PC beside the MacBook is under the DELL part and next to the MacBook part: two ways in, and one next to the
  // Windows PC on the left
  QCOMPARE(describe(check(arrangement)), std::string());
  QCOMPARE(
      QString::fromStdString(dump(deriveLinks(arrangement))), QStringLiteral(
                                                                  "srv right(0,38.7513) = MINI\n"
                                                                  "srv left(0,51.6685) = PC1\n"
                                                                  "srv left(51.6685,83.9612) = PC2\n"
                                                                  "srv down(0,21.8895) = PC2(52.9375,100)\n"
                                                                  "PC1 right = srv(0,51.6685)\n"
                                                                  "PC1 down(81.0938,100) = PC2(0,52.9375)\n"
                                                                  "MINI left = srv(0,38.7513)\n"
                                                                  "PC2 right = srv(51.6685,83.9612)\n"
                                                                  "PC2 up(0,52.9375) = PC1(81.0938,100)\n"
                                                                  "PC2 up(52.9375,100) = srv(0,21.8895)\n"
                                                              )
  );

  // where they touch, for drawing: five stretches, one of them the MacBook part's edge beside the PC at x = 753
  const auto derived = deriveLinks(arrangement);
  QCOMPARE(derived.contacts.size(), size_t(5));
  const auto beside = std::ranges::find_if(derived.contacts, [](const Contact &c) {
    return c.a == "srv" && c.b == "PC2" && c.sideA == Direction::Left;
  });
  QVERIFY(beside != derived.contacts.end());
  QCOMPARE(beside->line, 753.0);
  QCOMPARE(beside->from, 1440.0);
  QCOMPARE(beside->to, 2340.0);

  // and the text of the configuration is the same as the classic grid writes it
  const auto text = formatLinks(arrangement, deriveLinks(arrangement));
  QVERIFY(text.starts_with("section: links\n\tsrv:\n\t\tright(0,38.7513) = MINI\n"));
  QVERIFY(text.ends_with("\t\tup(52.9375,100) = srv(0,21.8895)\nend\n\n"));
}

void ArrangementTests::derive_tolerance_andTheShortestLink()
{
  const auto linked = [](double gap, double shift) {
    return !deriveLinks({{pc("A", kHd, 0, 0), pc("B", kHd, 1920 + gap, shift)}}).links.empty();
  };
  QVERIFY(linked(0.0, 0));
  QVERIFY(linked(0.4, 0));
  QVERIFY(linked(-0.4, 0));
  QVERIFY(!linked(0.6, 0));
  // shorter than a unit is not a link, a unit is
  QVERIFY(!linked(0.0, 1079.5));
  QVERIFY(linked(0.0, 1079.0));
}

void ArrangementTests::derive_aNotchIsNoOverlap_andIsLinkedOnBothSides()
{
  const Arrangement arrangement{{server("srv", kServerShape, 0, 0), pc("PC2", "0,0,1600,900", -847, 1440)}};
  // the box of the server has the PC in it, its monitors do not
  QVERIFY(!hasErrors(validate(arrangement)));
  const auto derived = deriveLinks(arrangement);
  QCOMPARE(derived.of("srv", Direction::Left).size(), size_t(1));
  QCOMPARE(derived.of("srv", Direction::Bottom).size(), size_t(1));
  QCOMPARE(derived.of("PC2", Direction::Right).size(), size_t(1));
  QCOMPARE(derived.of("PC2", Direction::Top).size(), size_t(1));

  // a PC that is moved into the MacBook is an overlap
  const Arrangement overlapping{{server("srv", kServerShape, 0, 0), pc("PC2", "0,0,1600,900", -800, 1440)}};
  QCOMPARE(describe(validate(overlapping)), std::string("Overlap srv PC2\n"));
}

void ArrangementTests::derive_aRowWithTwoExposedEdges_keepsTheOuterOneAndWarns()
{
  // a U: two arms and a base. A computer in the gap touches the inner edge of the left arm, and one beside the right
  // arm touches its outer edge, in the same rows of the right side of the U.
  const Arrangement arrangement{{
      pc("U", "0,0,1000,1000;0,1000,3000,500;2000,0,1000,1000", 0, 0),
      pc("B", "0,0,1000,800", 1000, 0),
      pc("C", "0,0,1000,800", 3000, 0),
  }};
  const auto derived = deriveLinks(arrangement);
  QCOMPARE(
      QString::fromStdString(dump(derived)),
      QStringLiteral("U right(0,53.3333) = C\nU left(0,53.3333) = B\nB right = U(0,53.3333)\nC left = U(0,53.3333)\n")
  );
  // the right side is ambiguous; the left edge of the left arm is in the rows of the link of the notch
  QCOMPARE(describe(derived.issues), std::string("AmbiguousSide U B right\nEdgeInTheSameRows U B left\n"));
  QVERIFY(std::ranges::none_of(derived.issues, [](const Issue &i) { return i.severity() == Issue::Severity::Error; }));
}

void ArrangementTests::derive_linksOfComputersThatAreNotValid_areLeftOut()
{
  Computer nothing;
  nothing.name = "none";
  auto noZoom = pc("zoom", kHd, 1920, 0);
  noZoom.zoom = 0;
  auto nan = pc("nan", kHd, 0, 1080);
  nan.originX = std::nan("");
  auto apart = pc("apart", "0,0,100,100", -600, 0);
  apart.monitors.push_back({500, 0, 100, 100});
  const auto derived = deriveLinks({{pc("A", kHd, 0, 0), nothing, noZoom, nan, apart}});
  QVERIFY(derived.links.empty());
}

//
// Validation
//

void ArrangementTests::validation_data()
{
  QTest::addColumn<QString>("name");
  QTest::addColumn<QString>("expected");
  // the cases are in the function, the rows only name them
  for (const auto *name :
       {"fine", "overlap", "duplicate name that differs in case", "no name", "no monitors", "monitors that are apart",
        "zoom 0", "zoom 100", "a position that is not a number", "far away"})
    QTest::newRow(name) << name << QString();
}

void ArrangementTests::validation()
{
  QFETCH(QString, name);
  Arrangement arrangement{{pc("A", kHd, 0, 0), pc("B", kHd, 1920, 0)}};
  std::string expected;
  if (name == "overlap") {
    arrangement.computers[1].originX = 1900;
    expected = "Overlap A B\n";
  } else if (name == "duplicate name that differs in case") {
    arrangement.computers[1].name = "a";
    expected = "DuplicateName a A\n";
  } else if (name == "no name") {
    arrangement.computers[1].name.clear();
    expected = "NameIsEmpty \n";
  } else if (name == "no monitors") {
    arrangement.computers[1].monitors.clear();
    expected = "InvalidMonitors B\n";
  } else if (name == "monitors that are apart") {
    arrangement.computers[1].monitors = {{0, 0, 100, 100}, {500, 0, 100, 100}};
    expected = "InvalidMonitors B\n";
  } else if (name == "zoom 0") {
    arrangement.computers[1].zoom = 0;
    expected = "InvalidZoom B\n";
  } else if (name == "zoom 100") {
    arrangement.computers[1].zoom = 100;
    expected = "InvalidZoom B\n";
  } else if (name == "a position that is not a number") {
    arrangement.computers[1].originY = std::nan("");
    expected = "InvalidPosition B\n";
  } else if (name == "far away") {
    arrangement.computers[1].originX = 5.0e7;
    expected = "InvalidPosition B\n";
  }
  QCOMPARE(QString::fromStdString(describe(validate(arrangement))), QString::fromStdString(expected));
  QCOMPARE(hasErrors(validate(arrangement)), !expected.empty());
}

void ArrangementTests::check_aComputerThatNothingTouches_isNotConnected()
{
  const Arrangement arrangement{{server("srv", kHd, 0, 0), pc("B", kHd, 1920, 0), pc("C", kHd, 5000, 0)}};
  const auto issues = check(arrangement);
  QCOMPARE(describe(issues), std::string("NotConnected C\n"));
  QCOMPARE(issues.front().severity(), Issue::Severity::Warning);

  // a chain is connected from the server, whichever way its links go
  const Arrangement chain{{server("srv", kHd, 0, 0), pc("B", kHd, 1920, 0), pc("C", kHd, 3840, 0)}};
  QCOMPARE(describe(check(chain)), std::string());
}

//
// Snapping
//

void ArrangementTests::snap_touchesAndAligns_data()
{
  QTest::addColumn<double>("x");
  QTest::addColumn<double>("y");
  QTest::addColumn<double>("expectedX");
  QTest::addColumn<double>("expectedY");

  // A is at 0,0 and 1920x1080; B is 1920x1080 and is dragged
  QTest::newRow("touches on the right and aligns the tops") << 1930.0 << 8.0 << 1920.0 << 0.0;
  QTest::newRow("touches on the left") << -1915.0 << 10.0 << -1920.0 << 0.0;
  QTest::newRow("touches below, aligns the left edges") << 4.0 << 1090.0 << 0.0 << 1080.0;
  QTest::newRow("touches above") << 3.0 << -1070.0 << 0.0 << -1080.0;
  QTest::newRow("far from every edge it stays") << 4000.0 << 700.0 << 4000.0 << 700.0;
  QTest::newRow("snaps in one direction only") << 1925.0 << 300.0 << 1920.0 << 300.0;
}

void ArrangementTests::snap_touchesAndAligns()
{
  QFETCH(double, x);
  QFETCH(double, y);
  QFETCH(double, expectedX);
  QFETCH(double, expectedY);
  const Arrangement arrangement{{pc("A", kHd, 0, 0), pc("B", kHd, 9000, 9000)}};
  const auto snapped = snapMove(arrangement, 1, x, y, 20);
  QVERIFY(!snapped.refused);
  QCOMPARE(snapped.originX, expectedX);
  QCOMPARE(snapped.originY, expectedY);
}

void ArrangementTests::snap_aComputerOnAnotherIsPushedOff()
{
  const Arrangement arrangement{{pc("A", kHd, 0, 0), pc("B", kHd, 9000, 9000)}};
  const auto snapped = snapMove(arrangement, 1, 1500, 200, 20);
  QVERIFY(snapped.pushedOut);
  QVERIFY(!snapped.refused);
  // the nearest place that touches: on its right, 420 away
  QCOMPARE(snapped.originX, 1920.0);
  QCOMPARE(snapped.originY, 200.0);

  // the middles are in line too
  const auto smaller = Arrangement{{pc("A", kHd, 0, 0), pc("B", "0,0,1920,600", 9000, 9000)}};
  const auto middle = snapMove(smaller, 1, 1925, 225, 20);
  QCOMPARE(middle.originX, 1920.0);
  QCOMPARE(middle.originY, 240.0);
}

void ArrangementTests::snap_aGroupThatIsNotARectangle_snapsByAnyOfItsMonitors()
{
  // the PC is beside the MacBook, which is a long way from the box of the server group
  const Arrangement arrangement{{server("srv", kServerShape, 0, 0), pc("PC2", "0,0,1600,900", -847, 1440)}};
  const auto snapped = snapMove(arrangement, 0, 3, 4, 10);
  QCOMPARE(snapped.originX, 0.0);
  QCOMPARE(snapped.originY, 0.0);
  QVERIFY(!snapped.pushedOut);

  // the group cannot be dropped on the PC
  const auto onTop = snapMove(arrangement, 0, -400, 1200, 10);
  QVERIFY(onTop.pushedOut || onTop.refused);
  Arrangement moved = arrangement;
  moved.computers[0].originX = onTop.originX;
  moved.computers[0].originY = onTop.originY;
  QVERIFY(!hasErrors(validate(moved)));
}

void ArrangementTests::snap_whatCannotBeSnapped_isRefused()
{
  const Arrangement arrangement{{pc("A", kHd, 100, 200), pc("B", kHd, 5000, 5000)}};
  const auto none = snapMove(arrangement, 7, 0, 0, 20);
  QVERIFY(none.refused);
  const auto nan = snapMove(arrangement, 1, std::nan(""), 0, 20);
  QVERIFY(nan.refused);
  QCOMPARE(nan.originX, 5000.0);
  QCOMPARE(nan.originY, 5000.0);
}

void ArrangementTests::derive_anEdgeThatTouchesNothingInTheRowsOfALink_isWarnedOf()
{
  // a U with a computer in its notch only: the left edge of the left arm and the right edge of the right arm are in the
  // same rows as the links of the notch, and touch nothing, so the pointer would leave there to the computer in the
  // notch
  const Arrangement u{
      {pc("U", "0,0,1000,1000;0,1000,3000,500;2000,0,1000,1000", 0, 0), pc("B", "0,0,1000,800", 1000, 0)}
  };
  const auto derived = deriveLinks(u);
  const auto text = describe(derived.issues);
  QVERIFY2(text.find("EdgeInTheSameRows U B left") != std::string::npos, text.c_str());
  QVERIFY2(text.find("EdgeInTheSameRows U B right") != std::string::npos, text.c_str());
  QVERIFY(std::ranges::none_of(derived.issues, [](const Issue &i) { return i.severity() == Issue::Severity::Error; }));

  // the screen of the user is not one of these: its edges on a side are in rows of their own
  QCOMPARE(describe(check(serverWithThreePcs())), std::string());
}

void ArrangementTests::derive_neighboursStackedWithRoundedPositions_areTwoLinks()
{
  // B and C are stacked beside A. C is where B ends, to three decimals, which is how it is stored: 3 / 10000 of a unit
  // too high. That is not an overlap of the links, and not an ambiguity.
  const Arrangement arrangement{{
      pc("A", "0,0,1000,2000", 0, 0),
      pc("B", "0,0,1000,1001", 1000, 0, 2.0 / 3.0),
      pc("C", "0,0,1000,1001", 1000, 667.333, 2.0 / 3.0),
  }};
  const auto derived = deriveLinks(arrangement);
  QCOMPARE(describe(derived.issues), std::string());
  const auto links = derived.of("A", Direction::Right);
  QCOMPARE(links.size(), size_t(2));
  QVERIFY(links[0].srcEnd <= links[1].srcStart);
  QCOMPARE(links[0].to, std::string("B"));
  QCOMPARE(links[1].to, std::string("C"));
}

void ArrangementTests::snap_aComputerThatIsNotValid_isRefused()
{
  auto broken = pc("B", kHd, 5000, 5000);
  broken.zoom = std::nan("");
  const Arrangement arrangement{{pc("A", kHd, 0, 0), broken}};
  const auto snapped = snapMove(arrangement, 1, 100, 100, 20);
  QVERIFY(snapped.refused);
  QVERIFY(std::isfinite(snapped.originX) && std::isfinite(snapped.originY));
}

void ArrangementTests::snap_guides_saySnappedEdges()
{
  const Arrangement arrangement{{pc("A", kHd, 0, 0), pc("B", "0,0,1920,600", 9000, 9000)}};
  const auto snapped = snapMove(arrangement, 1, 1925, 4, 20);
  QCOMPARE(snapped.originX, 1920.0);
  QCOMPARE(snapped.originY, 0.0);
  bool touching = false;
  bool aligned = false;
  for (const auto &guide : snapped.guides) {
    if (guide.vertical && guide.touching && guide.position == 1920.0) {
      touching = true;
      QCOMPARE(guide.from, 0.0);
      QCOMPARE(guide.to, 1080.0);
    }
    if (!guide.vertical && !guide.touching && guide.position == 0.0)
      aligned = true;
  }
  QVERIFY(touching);
  QVERIFY(aligned);

  const auto alone = snapMove(arrangement, 1, 6000, 6000, 20);
  QVERIFY(alone.guides.empty());
}

//
// The classic grid
//

namespace {

struct GridCase
{
  const char *name;
  std::vector<std::pair<int, const char *>> cells;
  const char *server;
  const char *expected;
};

// the same texts as ArrangementConfigTests pins for the classic grid, which was recorded before this feature
const std::vector<GridCase> &gridCases()
{
  static const std::vector<GridCase> cases = {
      {"the row of four of the user",
       {{6, "SUBEOM-COPILOT"}, {7, "Subeoms-MacBook-Pro.local"}, {8, "9800X3D-5080"}, {9, "Subeoms-Mac-mini.local"}},
       "Subeoms-MacBook-Pro.local",
       "section: links\n"
       "\tSUBEOM-COPILOT:\n"
       "\t\tright = Subeoms-MacBook-Pro.local\n"
       "\tSubeoms-MacBook-Pro.local:\n"
       "\t\tright = 9800X3D-5080\n"
       "\t\tleft = SUBEOM-COPILOT\n"
       "\t9800X3D-5080:\n"
       "\t\tright = Subeoms-Mac-mini.local\n"
       "\t\tleft = Subeoms-MacBook-Pro.local\n"
       "\tSubeoms-Mac-mini.local:\n"
       "\t\tleft = 9800X3D-5080\n"
       "end\n\n"},
      {"a cross",
       {{2, "up"}, {6, "left"}, {7, "srv"}, {8, "right"}, {12, "down"}},
       "srv",
       "section: links\n"
       "\tup:\n"
       "\t\tdown = srv\n"
       "\tleft:\n"
       "\t\tright = srv\n"
       "\tsrv:\n"
       "\t\tright = right\n"
       "\t\tleft = left\n"
       "\t\tup = up\n"
       "\t\tdown = down\n"
       "\tright:\n"
       "\t\tleft = srv\n"
       "\tdown:\n"
       "\t\tup = srv\n"
       "end\n\n"},
      {"a 2 by 2 block",
       {{6, "a"}, {7, "b"}, {11, "c"}, {12, "d"}},
       "a",
       "section: links\n"
       "\ta:\n"
       "\t\tright = b\n"
       "\t\tdown = c\n"
       "\tb:\n"
       "\t\tleft = a\n"
       "\t\tdown = d\n"
       "\tc:\n"
       "\t\tright = d\n"
       "\t\tup = a\n"
       "\td:\n"
       "\t\tleft = c\n"
       "\t\tup = b\n"
       "end\n\n"},
      {"gaps and the ends of the rows",
       {{4, "end-of-row-0"}, {5, "start-of-row-1"}, {7, "x"}, {9, "y"}},
       "x",
       "section: links\n"
       "\tend-of-row-0:\n"
       "\t\tdown = y\n"
       "\tstart-of-row-1:\n"
       "\tx:\n"
       "\ty:\n"
       "\t\tup = end-of-row-0\n"
       "end\n\n"},
  };
  return cases;
}

std::vector<GridCell> cellsOf(const GridCase &c, const char *monitors = kHd)
{
  std::vector<GridCell> cells;
  for (const auto &[index, name] : c.cells) {
    GridCell cell;
    cell.column = index % 5;
    cell.row = index / 5;
    cell.computer = pc(name, monitors, 0, 0);
    cells.push_back(cell);
  }
  return cells;
}

} // namespace

void ArrangementTests::grid_equalSizes_deriveTheLinksOfTheClassicGrid_data()
{
  QTest::addColumn<int>("index");
  for (size_t i = 0; i < gridCases().size(); ++i)
    QTest::newRow(gridCases().at(i).name) << static_cast<int>(i);
}

void ArrangementTests::grid_equalSizes_deriveTheLinksOfTheClassicGrid()
{
  QFETCH(int, index);
  const auto &c = gridCases().at(static_cast<size_t>(index));
  for (const auto *monitors : {kHd, "0,0,2560,1440", "0,0,1728,1117"}) {
    const auto conversion = arrangementFromGrid(cellsOf(c, monitors), c.server);
    QVERIFY(conversion.notKept.empty());
    QVERIFY(!hasErrors(validate(conversion.arrangement)));
    QCOMPARE(
        QString::fromStdString(formatLinks(conversion.arrangement, deriveLinks(conversion.arrangement))),
        QString::fromLatin1(c.expected)
    );
  }
}

void ArrangementTests::grid_theRowOfTheUser_isKept()
{
  // the Windows PC on the left is a 1920x1080 monitor at 150 %, the one on the right has two monitors at 150 %, the Mac
  // mini is a plain 1920x1080
  const auto &c = gridCases().at(0);
  auto cells = cellsOf(c);
  cells[0].computer = pc("SUBEOM-COPILOT", kHd, 0, 0, 2.0 / 3.0);
  cells[1].computer = pc("Subeoms-MacBook-Pro.local", kServerShape, 0, 0);
  cells[2].computer = pc("9800X3D-5080", kWindowsShape, 0, 0, 2.0 / 3.0);
  const auto conversion = arrangementFromGrid(cells, "Subeoms-MacBook-Pro.local");
  QVERIFY(conversion.notKept.empty());
  QCOMPARE(describe(check(conversion.arrangement)), std::string());
  const auto derived = deriveLinks(conversion.arrangement);
  for (const auto &[from, side, to] : std::vector<std::tuple<const char *, Direction, const char *>>{
           {"SUBEOM-COPILOT", Direction::Right, "Subeoms-MacBook-Pro.local"},
           {"Subeoms-MacBook-Pro.local", Direction::Left, "SUBEOM-COPILOT"},
           {"Subeoms-MacBook-Pro.local", Direction::Right, "9800X3D-5080"},
           {"9800X3D-5080", Direction::Left, "Subeoms-MacBook-Pro.local"},
           {"9800X3D-5080", Direction::Right, "Subeoms-Mac-mini.local"},
           {"Subeoms-Mac-mini.local", Direction::Left, "9800X3D-5080"},
       }) {
    const auto links = derived.of(from, side);
    QVERIFY2(!links.empty() && links.front().to == to, from);
  }
  // the server is where the origin is
  QCOMPARE(conversion.arrangement.computers[1].originX, 0.0);
  QCOMPARE(conversion.arrangement.computers[1].originY, 0.0);
  QVERIFY(conversion.arrangement.computers[1].isServer);
}

void ArrangementTests::grid_otherSizes_keepTheLinksAndPutThemWherePhysicsPutsThem()
{
  // B is bigger than the server A and beside it, C is smaller and below it: they are centred along the shared edge
  std::vector<GridCell> cells(3);
  cells[0] = {0, 0, pc("A", kHd, 0, 0)};
  cells[1] = {1, 0, pc("B", "0,0,2560,1440", 0, 0)};
  cells[2] = {0, 1, pc("C", "0,0,1280,720", 0, 0)};
  const auto conversion = arrangementFromGrid(cells, "A");
  QVERIFY(conversion.notKept.empty());
  QCOMPARE(
      QString::fromStdString(dump(deriveLinks(conversion.arrangement))),
      QStringLiteral(
          "A right = B(12.5,87.5)\nA down(16.6667,83.3333) = C\nB left(12.5,87.5) = A\nC up = A(16.6667,83.3333)\n"
      )
  );
}

void ArrangementTests::grid_whatCannotBeKept_isSaid()
{
  // a 2 by 2 block in which the computer c, below the server a, is too small to touch the computer d beside it
  std::vector<GridCell> cells(4);
  cells[0] = {0, 0, pc("a", "0,0,1000,1000", 0, 0)};
  cells[1] = {1, 0, pc("b", "0,0,1000,1000", 0, 0)};
  cells[2] = {0, 1, pc("c", "0,0,400,400", 0, 0)};
  cells[3] = {1, 1, pc("d", "0,0,1000,1000", 0, 0)};
  const auto conversion = arrangementFromGrid(cells, "a");
  QVERIFY(!hasErrors(validate(conversion.arrangement)));
  QCOMPARE(conversion.notKept.size(), size_t(2));
  const auto names = std::format(
      "{} {} {}|{} {} {}", conversion.notKept[0].from, directionName(conversion.notKept[0].side),
      conversion.notKept[0].to, conversion.notKept[1].from, directionName(conversion.notKept[1].side),
      conversion.notKept[1].to
  );
  QCOMPARE(names, std::string("c right d|d left c"));
}

void ArrangementTests::grid_computersThatTheGridDoesNotConnect_areBelow()
{
  std::vector<GridCell> cells(3);
  cells[0] = {0, 0, pc("A", kHd, 0, 0)};
  cells[1] = {2, 0, pc("B", kHd, 0, 0)};
  cells[2] = {2, 1, pc("C", kHd, 0, 0)};
  const auto conversion = arrangementFromGrid(cells, "A");
  QVERIFY(conversion.notKept.empty());
  QCOMPARE(QString::fromStdString(dump(deriveLinks(conversion.arrangement))), QStringLiteral("B down = C\nC up = B\n"));
  // below the server, where nothing touches it
  QVERIFY(conversion.arrangement.computers[1].box().y >= 1080);
  QCOMPARE(describe(check(conversion.arrangement)), std::string("NotConnected B\nNotConnected C\n"));
}

//
// Properties
//

void ArrangementTests::property_derivedLinks_areSymmetricPhysicalAndOnMonitors()
{
  std::mt19937 random(kSeed);
  size_t links = 0;
  for (int round = 0; round < 400; ++round) {
    const auto conversion = arrangementFromGrid(randomGrid(random, false), "c0");
    const auto &arrangement = conversion.arrangement;
    // the conversion never overlaps
    QVERIFY2(!hasErrors(validate(arrangement)), qPrintable(QString::fromStdString(describe(validate(arrangement)))));
    const auto derived = deriveLinks(arrangement);

    for (const auto &link : derived.links) {
      ++links;
      const auto &from = arrangement.computers[static_cast<size_t>(arrangement.indexOf(link.from))];
      const auto &to = arrangement.computers[static_cast<size_t>(arrangement.indexOf(link.to))];
      QVERIFY(link.srcStart >= 0 && link.srcEnd <= 1 && link.srcStart < link.srcEnd);
      QVERIFY(link.dstStart >= 0 && link.dstEnd <= 1 && link.dstStart < link.dstEnd);

      // A.right -> B.left implies B.left -> A.right, over the same part of the edge, in the other direction
      const auto back = derived.of(link.to, opposite(link.side));
      QVERIFY2(
          std::ranges::any_of(
              back,
              [&](const Link &b) {
                return b.to == link.from && b.srcStart == link.dstStart && b.srcEnd == link.dstEnd &&
                       b.dstStart == link.srcStart && b.dstEnd == link.srcEnd;
              }
          ),
          qPrintable(QString::fromStdString(link.from + " " + directionName(link.side) + " " + link.to))
      );

      for (const double t : {0.1, 0.5, 0.9}) {
        const auto p = link.srcStart + t * (link.srcEnd - link.srcStart);
        const auto q = link.dstStart + t * (link.dstEnd - link.dstStart);
        // the pointer leaves at a place of the edge and enters at the same place: the mapping that the core applies to
        // a position (the transform of the source interval, then the destination interval) is the physical one
        const auto physicalFrom = physicalAlong(from.box(), link.side, p);
        const auto physicalTo = physicalAlong(to.box(), opposite(link.side), q);
        QVERIFY2(std::abs(physicalFrom - physicalTo) < 0.05, qPrintable(QString::number(physicalFrom - physicalTo)));

        // it enters on a monitor, on the edge that touches the one it left
        const auto layout = MonitorLayout::create(to.monitors);
        QVERIFY(layout);
        const auto entered = opposite(link.side);
        const auto at = layout.layout->entryPoint(entered, static_cast<float>(q));
        QVERIFY(layout.layout->contains(at.x, at.y));
        const double edge = [&] {
          switch (entered) {
          case Direction::Left:
            return to.originX + to.zoom * at.x;
          case Direction::Right:
            return to.originX + to.zoom * (at.x + 1);
          case Direction::Top:
            return to.originY + to.zoom * at.y;
          default:
            return to.originY + to.zoom * (at.y + 1);
          }
        }();
        const bool horizontal = link.side == Direction::Left || link.side == Direction::Right;
        const bool touches = std::ranges::any_of(from.monitorRects(), [&](const Rect &m) {
          const double facing = link.side == Direction::Right    ? m.right()
                                : link.side == Direction::Left   ? m.x
                                : link.side == Direction::Bottom ? m.bottom()
                                                                 : m.y;
          const double start = horizontal ? m.y : m.x;
          const double end = horizontal ? m.bottom() : m.right();
          return std::abs(facing - edge) <= kTouchTolerance + 1.0e-6 && physicalFrom >= start - 1 &&
                 physicalFrom <= end + 1;
        });
        QVERIFY2(
            touches, qPrintable(QString::fromStdString(link.from + " " + directionName(link.side) + " " + link.to))
        );
      }
    }
  }
  // the rounds did look at links
  QVERIFY(links > 1000);
}

void ArrangementTests::property_derivedLinks_areReadByTheCoreAsTheyAreMeant()
{
  std::mt19937 random(kSeed + 1);
  for (int cell = 0; cell < 15; ++cell) {
    const auto name = QStringLiteral("c%1").arg(cell);
    Settings::setValue(Settings::Computer::Name.arg(name), name);
  }
  for (int round = 0; round < 150; ++round) {
    const auto conversion = arrangementFromGrid(randomGrid(random, false), "c0");
    const auto &arrangement = conversion.arrangement;
    const auto derived = deriveLinks(arrangement);

    // the computers of the configuration are those of the settings
    Config config(nullptr);
    const auto text = formatLinks(arrangement, derived) + "section: options\nend\n";
    std::istringstream input(text);
    try {
      input >> config;
    } catch (const std::exception &e) {
      QFAIL(qPrintable(QStringLiteral("%1\n%2").arg(QString::fromUtf8(e.what()), QString::fromStdString(text))));
    }

    for (const auto &link : derived.links) {
      for (const double t : {0.0, 0.25, 0.5, 0.9}) {
        const auto p = static_cast<float>(link.srcStart + t * (link.srcEnd - link.srcStart));
        float out = -1;
        const auto neighbour = config.getNeighbor(link.from, link.side, p, &out);
        QVERIFY2(
            neighbour == link.to,
            qPrintable(QString::fromStdString(link.from + " " + directionName(link.side) + " " + link.to))
        );
        QVERIFY(std::abs(out - (link.dstStart + t * (link.dstEnd - link.dstStart))) < 1.0e-4);
      }
    }
  }
}

void ArrangementTests::property_equalGrids_areTheClassicGrid()
{
  std::mt19937 random(kSeed + 2);
  for (int round = 0; round < 300; ++round) {
    const auto cells = randomGrid(random, true);
    const auto conversion = arrangementFromGrid(cells, "c0");
    QVERIFY(conversion.notKept.empty());
    QVERIFY(!hasErrors(validate(conversion.arrangement)));
    QCOMPARE(
        QString::fromStdString(formatLinks(conversion.arrangement, deriveLinks(conversion.arrangement))),
        QString::fromStdString(classicLinks(cells))
    );
  }
}

void ArrangementTests::property_aSnappedComputer_neverOverlaps()
{
  std::mt19937 random(kSeed + 3);
  for (int round = 0; round < 300; ++round) {
    auto arrangement = arrangementFromGrid(randomGrid(random, false), "c0").arrangement;
    const auto moving = std::uniform_int_distribution<size_t>(0, arrangement.computers.size() - 1)(random);
    const auto x = std::uniform_real_distribution<double>(-6000, 9000)(random);
    const auto y = std::uniform_real_distribution<double>(-4000, 6000)(random);
    const auto snapped = snapMove(arrangement, moving, x, y, 25);
    arrangement.computers[moving].originX = snapped.originX;
    arrangement.computers[moving].originY = snapped.originY;
    QVERIFY2(!hasErrors(validate(arrangement)), qPrintable(QString::fromStdString(describe(validate(arrangement)))));
  }
}

//
// Text
//

void ArrangementTests::windowsDisplayScale_data()
{
  QTest::addColumn<QString>("text");
  QTest::addColumn<double>("scale");

  const auto windows = QStringLiteral(
      "computer: SUBEOM-COPILOT\r\nspace: virtual-desktop physical pixels x,y,w,h scale%\r\n"
      "  monitor -1920,360,1920,1080 scale=100%\r\n  monitor 0,0,2560,1440 scale=150% primary\r\n"
  );
  QTest::newRow("the primary monitor of a Windows block") << windows << 1.5;
  QTest::newRow("the first monitor when none is the primary")
      << QStringLiteral("  monitor 0,0,2560,1440 scale=125%\n  monitor 2560,0,1920,1080 scale=100%\n") << 1.25;
  QTest::newRow("a Mac has points, no zoom")
      << QStringLiteral("  monitor 0,0,2064,1347 scale=2.0 primary\n  monitor 2064,0,2560,1440 scale=1.0\n") << 0.0;
  QTest::newRow("no scale") << QStringLiteral("0,0,1920,1080\n") << 0.0;
  QTest::newRow("a scale that is not on a monitor line") << QStringLiteral("computer: x scale=150%\n") << 0.0;
}

void ArrangementTests::windowsDisplayScale()
{
  QFETCH(QString, text);
  QFETCH(double, scale);
  const auto found = deskflow::server::arrangement::windowsDisplayScale(text.toStdString());
  if (scale == 0.0) {
    QVERIFY(!found);
  } else {
    QVERIFY(found);
    QCOMPARE(*found, scale);
  }
  QCOMPARE(zoomForScale(100), 1.0);
  QVERIFY(std::abs(zoomForScale(150) - 2.0 / 3.0) < 1.0e-12);
  QCOMPARE(zoomForScale(200), 0.5);
  QCOMPARE(zoomForScale(0), 1.0);
  QCOMPARE(zoomForScale(5000), 1.0);
}

void ArrangementTests::formatInterval_data()
{
  QTest::addColumn<double>("start");
  QTest::addColumn<double>("end");
  QTest::addColumn<QString>("expected");
  QTest::newRow("the whole side") << 0.0 << 1.0 << QString();
  QTest::newRow("a part") << 0.25 << 0.5 << QStringLiteral("(25,50)");
  QTest::newRow("to the end") << 0.5 << 1.0 << QStringLiteral("(50,100)");
  QTest::newRow("four decimals") << 0.0 << 0.387513 << QStringLiteral("(0,38.7513)");
  QTest::newRow("a hair") << 0.000926 << 1.0 << QStringLiteral("(0.0926,100)");
}

void ArrangementTests::formatInterval()
{
  QFETCH(double, start);
  QFETCH(double, end);
  QFETCH(QString, expected);
  QCOMPARE(QString::fromStdString(deskflow::server::arrangement::formatInterval(start, end)), expected);
}

QTEST_MAIN(ArrangementTests)
