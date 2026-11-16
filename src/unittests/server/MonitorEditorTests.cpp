/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorEditorTests.h"

#include "server/MonitorEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace deskflow::server;
using namespace deskflow::server::editor;

namespace {

Monitor mon(double x, double y, double w, double h, double scale = 100)
{
  Monitor m;
  m.x = x;
  m.y = y;
  m.w = w;
  m.h = h;
  m.scale = scale;
  return m;
}

Layout layoutOf(std::vector<Monitor> monitors)
{
  return Layout{std::move(monitors)};
}

QString text(const std::string &s)
{
  return QString::fromStdString(s);
}

bool hasOverlap(const Layout &layout)
{
  const auto problems = check(layout);
  return std::ranges::any_of(problems, [](const Problem &p) { return p.kind == Problem::Kind::Overlap; });
}

MonitorRect boxOf(const std::vector<MonitorRect> &rects)
{
  int32_t left = rects.front().x, top = rects.front().y, right = rects.front().right(), bottom = rects.front().bottom();
  for (const auto &r : rects) {
    left = std::min(left, r.x);
    top = std::min(top, r.y);
    right = std::max(right, r.right());
    bottom = std::max(bottom, r.bottom());
  }
  return {left, top, right - left, bottom - top};
}

// -- cases of check()

struct CheckCase
{
  const char *name;
  Layout layout;
  std::vector<Problem::Kind> expected;
};

const std::vector<CheckCase> &checkCases()
{
  static const auto cases = [] {
    std::vector<Monitor> seventeen;
    for (int i = 0; i < 17; ++i)
      seventeen.push_back(mon(i * 1920, 0, 1920, 1080));
    using enum Problem::Kind;
    return std::vector<CheckCase>{
        {"two monitors that touch", layoutOf({mon(0, 0, 1920, 1080), mon(1920, 0, 1920, 1080)}), {}},
        {"the T of the server", layoutOf({mon(0, 0, 3440, 1440), mon(753, 1440, 2064, 1347)}), {}},
        {"no monitors", layoutOf({}), {NoMonitors}},
        {"overlap", layoutOf({mon(0, 0, 1920, 1080), mon(1000, 0, 1920, 1080)}), {Overlap}},
        {"a gap", layoutOf({mon(0, 0, 1920, 1080), mon(2000, 0, 1920, 1080)}), {NotConnected}},
        {"touching at a corner only", layoutOf({mon(0, 0, 1920, 1080), mon(1920, 1080, 1920, 1080)}), {NotConnected}},
        {"too small", layoutOf({mon(0, 0, 100, 100)}), {BadSize}},
        {"too big", layoutOf({mon(0, 0, 50000, 1080)}), {BadSize}},
        {"a scale that is not one", layoutOf({mon(0, 0, 1920, 1080, 10)}), {BadScale}},
        {"seventeen monitors", layoutOf(seventeen), {TooMany}},
    };
  }();
  return cases;
}

// -- the cases of realise()

struct RealiseCase
{
  const char *name;
  Layout layout;
  std::optional<MonitorRect> reported;
  Fit fit;
  const char *text;
  double zoom;
};

const std::vector<RealiseCase> &realiseCases()
{
  static const std::vector<RealiseCase> cases = {
      {"one monitor, nothing known", layoutOf({mon(0, 0, 1920, 1080)}), std::nullopt, Fit::NotVerified, "0,0,1920,1080",
       1.0},
      {"one monitor at 150 %, nothing known", layoutOf({mon(40, 70, 1280, 720, 150)}), std::nullopt, Fit::NotVerified,
       "0,0,1920,1080", 100.0 / 150.0},
      {"one monitor that the computer reports", layoutOf({mon(0, 0, 1920, 1080)}), MonitorRect{0, 0, 1920, 1080},
       Fit::Matches, "0,0,1920,1080", 1.0},
      {"drawn at 150 % for a screen of its pixels", layoutOf({mon(0, 0, 1280, 720, 150)}),
       MonitorRect{0, 0, 1920, 1080}, Fit::Matches, "0,0,1920,1080", 100.0 / 150.0},
      {"the Windows PC of two monitors, bottoms in line, at 100 %",
       layoutOf({mon(500, 300, 2560, 1440), mon(-1420, 660, 1920, 1080)}), MonitorRect{-1920, 0, 4480, 1440},
       Fit::Matches, "0,0,2560,1440;-1920,360,1920,1080", 1.0},
      {"2560x1440 at 150 % and 1920x1080 at 100 % on its left, for a system that scales it all by the first",
       layoutOf({mon(0, 0, 2560 / 1.5, 960, 150), mon(-1920, -120, 1920, 1080)}), MonitorRect{-2880, -180, 5440, 1620},
       Fit::Matches, "0,0,2560,1440;-2880,-180,2880,1620", 1.0 / 1.5},
      {"the same, with the pixels of each monitor reported: the shape of the drawing is not the shape of the screen",
       layoutOf({mon(0, 0, 2560 / 1.5, 960, 150), mon(-1920, -120, 1920, 1080)}), MonitorRect{-1920, 0, 4480, 1440},
       Fit::Differs, "0,0,2560,1440;-2880,-180,2880,1620", 1.0 / 1.5},
      {"two stacked", layoutOf({mon(0, 0, 1920, 1080), mon(0, 1080, 1920, 1080)}), MonitorRect{0, 0, 1920, 2160},
       Fit::Matches, "0,0,1920,1080;0,1080,1920,1080", 1.0},
      {"a landscape and a portrait monitor", layoutOf({mon(0, 0, 2560, 1440), mon(2560, 0, 1080, 1920)}),
       MonitorRect{0, 0, 3640, 1920}, Fit::Matches, "0,0,2560,1440;2560,0,1080,1920", 1.0},
      {"three side by side, a pixel more than their scale makes",
       layoutOf({mon(0, 0, 1536, 864, 125), mon(1536, 0, 1536, 864, 125), mon(3072, 0, 1536, 864, 125)}),
       MonitorRect{0, 0, 5761, 1080}, Fit::Matches, "0,0,1920,1080;1920,0,1921,1080;3841,0,1920,1080",
       1.0 / std::sqrt((5761.0 / 4608.0) * 1.25)},
      {"three side by side, as many pixels as their scale makes",
       layoutOf({mon(0, 0, 1536, 864, 125), mon(1536, 0, 1536, 864, 125), mon(3072, 0, 1536, 864, 125)}),
       MonitorRect{0, 0, 5760, 1080}, Fit::Matches, "0,0,1920,1080;1920,0,1920,1080;3840,0,1920,1080", 0.8},
  };
  return cases;
}

// -- a layout that is drawn at random: monitors of the usual sizes, each one touching one that is there

Layout randomLayout(std::mt19937 &rng)
{
  static const std::array<std::pair<int, int>, 6> sizes = {
      {{1920, 1080}, {2560, 1440}, {1080, 1920}, {3440, 1440}, {1280, 720}, {1512, 982}}
  };
  static const std::array<double, 5> scales = {100, 125, 150, 175, 200};
  const auto make = [&] {
    const auto [w, h] = sizes[rng() % sizes.size()];
    Monitor m;
    m.scale = scales[rng() % scales.size()];
    m.w = w * 100.0 / m.scale;
    m.h = h * 100.0 / m.scale;
    return m;
  };

  Layout layout;
  layout.monitors.push_back(make());
  const size_t wanted = 1 + rng() % 4;
  for (int attempt = 0; attempt < 60 && layout.monitors.size() < wanted; ++attempt) {
    const auto base = layout.monitors[rng() % layout.monitors.size()];
    auto m = make();
    const auto side = rng() % 4;
    const auto align = rng() % 3;
    const auto along = [&](double baseStart, double baseLength, double length) {
      return align == 0   ? baseStart
             : align == 1 ? baseStart + baseLength - length
                          : baseStart + (baseLength - length) / 2;
    };
    if (side < 2) {
      m.x = side == 0 ? base.x + base.w : base.x - m.w;
      m.y = along(base.y, base.h, m.h);
    } else {
      m.y = side == 2 ? base.y + base.h : base.y - m.h;
      m.x = along(base.x, base.w, m.w);
    }
    Layout candidate = layout;
    candidate.monitors.push_back(m);
    if (check(candidate).empty())
      layout = candidate;
  }
  return layout;
}

} // namespace

// -- validation

void MonitorEditorTests::validation_data()
{
  QTest::addColumn<int>("index");
  for (size_t i = 0; i < checkCases().size(); ++i)
    QTest::newRow(checkCases()[i].name) << static_cast<int>(i);
}

void MonitorEditorTests::validation()
{
  QFETCH(int, index);
  const auto &c = checkCases()[static_cast<size_t>(index)];
  std::vector<Problem::Kind> kinds;
  for (const auto &problem : editor::check(c.layout)) {
    kinds.push_back(problem.kind);
    QVERIFY(!describe(problem).empty());
  }
  QCOMPARE(kinds, c.expected);
}

void MonitorEditorTests::presets_andAspects()
{
  std::set<std::pair<int, int>> seen;
  for (const auto &preset : presets()) {
    QVERIFY(preset.w > 0 && preset.h > 0);
    QVERIFY(std::string(preset.name).size() > 3);
    QVERIFY(seen.insert({preset.w, preset.h}).second);
  }
  for (const auto &[w, h] : std::vector<std::pair<int, int>>{
           {1280, 720}, {1920, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160}, {5120, 1440}, {1512, 982}, {1728, 1117}
       })
    QVERIFY2(presetOf(w, h) != nullptr, qPrintable(QStringLiteral("%1x%2").arg(w).arg(h)));
  QVERIFY(presetOf(2561, 1440) == nullptr);

  using Case = std::tuple<double, double, const char *>;
  for (const auto &[w, h, name] : std::vector<Case>{
           {2560, 1440, "16:9"},
           {1440, 2560, "9:16"},
           {3440, 1440, "21:9"},
           {2560, 1080, "21:9"},
           {5120, 1440, "32:9"},
           {1600, 1200, "4:3"},
           {3000, 2000, "3:2"},
           {1920, 1200, "16:10"},
           {1280, 1024, "5:4"},
           {1000, 777, ""},
       })
    QCOMPARE(text(aspectNameOf(w, h)), QString::fromLatin1(name));
  for (const auto scale : commonScales())
    QVERIFY(scale >= kMinScale && scale <= kMaxScale);
  QVERIFY(std::ranges::find(commonScales(), 150) != commonScales().end());
}

void MonitorEditorTests::withScale_keepsThePixels_andTheSeamThatCanBeKept()
{
  const auto two = layoutOf({mon(0, 0, 1920, 1080), mon(1920, 0, 2560, 1440)});

  // the pixels are the same, the size in the drawing is not; the seam on the left of it stays
  auto scaled = withScale(two, 1, 150);
  QCOMPARE(scaled.monitors[1].pixelsW(), 2560);
  QCOMPARE(scaled.monitors[1].pixelsH(), 1440);
  QCOMPARE(scaled.monitors[1].x, 1920.0);
  QVERIFY(std::abs(scaled.monitors[1].w - 2560 / 1.5) < 1e-9);
  QVERIFY(std::abs(scaled.monitors[1].h - 960) < 1e-9);
  QVERIFY(check(scaled).empty());

  // the first one gets smaller, and keeps the seam on its right
  scaled = withScale(two, 0, 200);
  QCOMPARE(scaled.monitors[0].w, 960.0);
  QCOMPARE(scaled.monitors[0].h, 540.0);
  QCOMPARE(scaled.monitors[0].x, 960.0);
  QVERIFY(check(scaled).empty());

  // a scale that cannot be one is the nearest one that can
  QCOMPARE(withScale(two, 0, 10).monitors[0].scale, kMinScale);
  QCOMPARE(withScale(two, 0, 900).monitors[0].scale, kMaxScale);
  // and a monitor that is not there is left alone
  QCOMPARE(withScale(two, 5, 150), two);
}

void MonitorEditorTests::withResolution_andRotated_keepTheEdgesThatTouch()
{
  const auto two = layoutOf({mon(0, 0, 1920, 1080), mon(1920, 0, 1920, 1080)});

  const auto bigger = withResolution(two, 0, 2560, 1440);
  QCOMPARE(bigger.monitors[0].w, 2560.0);
  QCOMPARE(bigger.monitors[0].x, -640.0);
  QVERIFY(check(bigger).empty());

  const auto portrait = rotated(two, 1);
  QCOMPARE(portrait.monitors[1].pixelsW(), 1080);
  QCOMPARE(portrait.monitors[1].pixelsH(), 1920);
  QCOMPARE(portrait.monitors[1].x, 1920.0);
  QVERIFY(check(portrait).empty());
  QCOMPARE(rotated(portrait, 1), two);

  // a lone monitor rotates where it is
  const auto lone = rotated(layoutOf({mon(100, 50, 1920, 1080)}), 0);
  QCOMPARE(lone.monitors[0].x, 100.0);
  QCOMPARE(lone.monitors[0].w, 1080.0);
}

void MonitorEditorTests::addedAndRemoved_andMain()
{
  const auto one = layoutOf({mon(0, 0, 1920, 1080, 125)});
  const auto two = added(one);
  QCOMPARE(two.monitors.size(), size_t{2});
  QCOMPARE(two.monitors[1].x, 1920.0);
  QCOMPARE(two.monitors[1].y, 0.0);
  QCOMPARE(two.monitors[1].scale, 125.0);
  QCOMPARE(two.monitors[1].pixelsW(), two.monitors[0].pixelsW());
  QVERIFY(check(two).empty());
  QVERIFY(check(added(two)).empty());

  QCOMPARE(removed(two, 1), one);
  // the last one stays: a computer has a monitor
  QCOMPARE(removed(one, 0), one);

  const auto main = withMain(two, 1);
  QCOMPARE(main.monitors[0], two.monitors[1]);
  QCOMPARE(main.monitors[1], two.monitors[0]);
}

// -- move

void MonitorEditorTests::move_snapsToEdgesAndMiddles_data()
{
  QTest::addColumn<double>("wantX");
  QTest::addColumn<double>("wantY");
  QTest::addColumn<double>("x");
  QTest::addColumn<double>("y");
  QTest::addColumn<bool>("guided");
  // A is 0,0,1920,1080; B is 1280x720
  QTest::newRow("touching on the right, in line at the top") << 1925.0 << 5.0 << 1920.0 << 0.0 << true;
  QTest::newRow("touching on the right, in line at the bottom") << 1925.0 << 357.0 << 1920.0 << 360.0 << true;
  QTest::newRow("touching on the right, middles in line") << 1925.0 << 183.0 << 1920.0 << 180.0 << true;
  QTest::newRow("touching below, in line on the left") << 3.0 << 1085.0 << 0.0 << 1080.0 << true;
  QTest::newRow("on the left") << -1275.0 << 100.0 << -1280.0 << 100.0 << true;
  QTest::newRow("too far from everything") << 5000.0 << 3000.0 << 5000.0 << 3000.0 << false;
  QTest::newRow("a little far from the edge") << 1940.0 << 100.0 << 1940.0 << 100.0 << false;
}

void MonitorEditorTests::move_snapsToEdgesAndMiddles()
{
  QFETCH(double, wantX);
  QFETCH(double, wantY);
  QFETCH(double, x);
  QFETCH(double, y);
  QFETCH(bool, guided);
  const auto layout = layoutOf({mon(0, 0, 1920, 1080), mon(3000, 200, 1280, 720)});
  const auto moved = move(layout, 1, wantX, wantY, 8);
  QVERIFY(!moved.refused);
  QCOMPARE(moved.layout.monitors[1].x, x);
  QCOMPARE(moved.layout.monitors[1].y, y);
  QCOMPARE(moved.layout.monitors[1].w, 1280.0);
  QCOMPARE(moved.layout.monitors[0], layout.monitors[0]);
  QCOMPARE(!moved.guides.empty(), guided);
}

void MonitorEditorTests::move_neverGoesOverAnotherMonitor()
{
  const auto layout = layoutOf({mon(0, 0, 1920, 1080), mon(1920, 0, 1280, 720)});
  const auto moved = move(layout, 1, 500, 100, 8);
  QVERIFY(moved.refused);
  QCOMPARE(moved.layout, layout);
  // the obstacles, the other computers, are not gone over either
  const auto alone = layoutOf({mon(0, 0, 1920, 1080)});
  QVERIFY(move(alone, 0, 2010, 20, 8, {Rect{2000, 0, 500, 500}}).refused);
  // on its left, the right edge goes to the edge of the obstacle
  const auto snapped = move(alone, 0, 75, 20, 8, {Rect{2000, 0, 500, 500}});
  QVERIFY(!snapped.refused);
  QCOMPARE(snapped.layout.monitors[0].x, 80.0);
}

// -- resize

namespace {
struct HandleCase
{
  const char *name;
  Handle handle;
  double toX;
  double toY;
  Rect expected;
};

const std::vector<HandleCase> &handleCases()
{
  static const std::vector<HandleCase> cases = {
      {"left", Handle::Left, 800, 0, {800, 1000, 1200, 600}},
      {"right", Handle::Right, 2300, 0, {1000, 1000, 1300, 600}},
      {"top", Handle::Top, 0, 900, {1000, 900, 1000, 700}},
      {"bottom", Handle::Bottom, 0, 1800, {1000, 1000, 1000, 800}},
      {"top left", Handle::TopLeft, 900, 950, {900, 950, 1100, 650}},
      {"top right", Handle::TopRight, 2100, 950, {1000, 950, 1100, 650}},
      {"bottom left", Handle::BottomLeft, 900, 1700, {900, 1000, 1100, 700}},
      {"bottom right", Handle::BottomRight, 2100, 1700, {1000, 1000, 1100, 700}},
  };
  return cases;
}
} // namespace

void MonitorEditorTests::resize_everyHandle_keepsTheOppositeEdges_data()
{
  QTest::addColumn<int>("index");
  for (size_t i = 0; i < handleCases().size(); ++i)
    QTest::newRow(handleCases()[i].name) << static_cast<int>(i);
}

void MonitorEditorTests::resize_everyHandle_keepsTheOppositeEdges()
{
  QFETCH(int, index);
  const auto &c = handleCases()[static_cast<size_t>(index)];
  const auto layout = layoutOf({mon(1000, 1000, 1000, 600, 125)});
  Constraints none;
  none.snapAspect = false;
  none.snapPresets = false;
  const auto resized = resize(layout, 0, c.handle, c.toX, c.toY, 8, none);
  QVERIFY(!resized.refused);
  const auto r = resized.layout.monitors[0].rect();
  QCOMPARE(r, c.expected);
  QCOMPARE(resized.layout.monitors[0].scale, 125.0);
}

void MonitorEditorTests::resize_edgesSnapToTheEdgesOfTheOthers()
{
  Constraints none;
  none.snapAspect = false;
  none.snapPresets = false;
  const auto layout = layoutOf({mon(0, 0, 1920, 1080), mon(2100, 0, 1920, 1080)});

  // the left edge of B goes to the right edge of A, which it touches then
  auto resized = resize(layout, 1, Handle::Left, 1925, 40, 8, none);
  QCOMPARE(resized.layout.monitors[1].x, 1920.0);
  QCOMPARE(resized.layout.monitors[1].right(), 4020.0);
  QVERIFY(std::ranges::any_of(resized.guides, [](const Guide &g) { return g.vertical && g.position == 1920.0; }));
  QVERIFY(check(resized.layout).empty());

  // the bottom edge of B goes to the bottom edge of A when it is near
  resized = resize(layout, 1, Handle::Bottom, 2500, 1085, 8, none);
  QCOMPARE(resized.layout.monitors[1].bottom(), 1080.0);
  QVERIFY(std::ranges::any_of(resized.guides, [](const Guide &g) { return !g.vertical && g.position == 1080.0; }));

  // when it is not near, it is where it was put
  resized = resize(layout, 1, Handle::Bottom, 2500, 1200, 8, none);
  QCOMPARE(resized.layout.monitors[1].bottom(), 1200.0);
  QVERIFY(resized.guides.empty());
}

void MonitorEditorTests::resize_sidesSnapToTheCommonRatios()
{
  const auto layout = layoutOf({mon(0, 0, 1000, 563)});
  const auto h = static_cast<double>(563);

  auto resized = resize(layout, 0, Handle::Right, 1003, 0, 8);
  QVERIFY(std::abs(resized.layout.monitors[0].w - h * 16 / 9) < 1e-6);
  QCOMPARE(text(resized.aspect), QStringLiteral("16:9"));

  // not near a ratio
  resized = resize(layout, 0, Handle::Right, 1100, 0, 8);
  QCOMPARE(resized.layout.monitors[0].w, 1100.0);
  QVERIFY(resized.aspect.empty());

  // a portrait monitor: the height goes to the ratio
  const auto portrait = layoutOf({mon(0, 0, 563, 1000)});
  resized = resize(portrait, 0, Handle::Bottom, 0, 1004, 8);
  QVERIFY(std::abs(resized.layout.monitors[0].h - 563 * 16 / 9.0) < 1e-6);
  QCOMPARE(text(resized.aspect), QStringLiteral("9:16"));

  // a corner: the height follows the width
  resized = resize(layout, 0, Handle::BottomRight, 1003, 570, 8);
  QCOMPARE(resized.layout.monitors[0].w, 1003.0);
  QVERIFY(std::abs(resized.layout.monitors[0].h - 1003 * 9 / 16.0) < 1e-6);
  QCOMPARE(text(resized.aspect), QStringLiteral("16:9"));

  // asked not to
  Constraints none;
  none.snapAspect = false;
  none.snapPresets = false;
  resized = resize(layout, 0, Handle::Right, 1003, 0, 8, none);
  QCOMPARE(resized.layout.monitors[0].w, 1003.0);
}

void MonitorEditorTests::resize_sizesSnapToThePresetsAndTheTarget()
{
  Constraints presetsOnly;
  presetsOnly.snapAspect = false;

  auto resized = resize(layoutOf({mon(0, 0, 1900, 1070)}), 0, Handle::BottomRight, 1921, 1081, 8, presetsOnly);
  QCOMPARE(resized.layout.monitors[0].rect(), (Rect{0, 0, 1920, 1080}));
  QVERIFY(resized.preset);

  // at 150 %, the preset is the pixels: 1920 x 1080 is 1280 x 720 in the drawing
  resized = resize(layoutOf({mon(0, 0, 1270, 710, 150)}), 0, Handle::BottomRight, 1281, 721, 8, presetsOnly);
  QVERIFY(std::abs(resized.layout.monitors[0].w - 1280) < 1e-9);
  QVERIFY(std::abs(resized.layout.monitors[0].h - 720) < 1e-9);
  QVERIFY(resized.preset);

  // a size that is no preset is as it was dragged
  resized = resize(layoutOf({mon(0, 0, 1900, 1070)}), 0, Handle::BottomRight, 2111, 1301, 8, presetsOnly);
  QCOMPARE(resized.layout.monitors[0].w, 2111.0);
  QVERIFY(!resized.preset);

  // the screen that the computer reports, in the units of the drawing
  Constraints target;
  target.snapAspect = false;
  target.snapPresets = false;
  target.targetSize = std::make_pair(4480.0, 1440.0);
  resized = resize(layoutOf({mon(0, 0, 4000, 1440)}), 0, Handle::Right, 4470, 0, 8, target);
  QCOMPARE(resized.layout.monitors[0].w, 4480.0);
  QVERIFY(resized.target);
  resized = resize(layoutOf({mon(0, 0, 4000, 1440)}), 0, Handle::Right, 4300, 0, 8, target);
  QCOMPARE(resized.layout.monitors[0].w, 4300.0);
  QVERIFY(!resized.target);
}

namespace {
struct PresetEdgeCase
{
  const char *name;
  Layout layout;
  Handle handle;
  double toX;
  double toY;
  Rect expected;
};

// A is next to B on the side that its edge must not leave: B touches A along it
const std::vector<PresetEdgeCase> &presetEdgeCases()
{
  static const std::vector<PresetEdgeCase> cases = {
      {"right", layoutOf({mon(0, 0, 1920, 1090), mon(0, 1090, 1000, 500)}), Handle::Right, 1925, 0, {0, 0, 1920, 1090}},
      {"left",
       layoutOf({mon(100, 0, 1920, 1090), mon(300, 1090, 1000, 500)}),
       Handle::Left,
       95,
       0,
       {100, 0, 1920, 1090}},
      {"bottom",
       layoutOf({mon(0, 0, 1930, 1075), mon(1930, 20, 500, 500)}),
       Handle::Bottom,
       0,
       1081,
       {0, 0, 1930, 1080}},
      {"top", layoutOf({mon(0, 10, 1930, 1070), mon(1930, 300, 500, 400)}), Handle::Top, 0, 5, {0, 0, 1930, 1080}},
  };
  return cases;
}
} // namespace

void MonitorEditorTests::resize_aPresetSnapOfASingleEdge_movesOnlyThatSide_data()
{
  QTest::addColumn<int>("index");
  for (size_t i = 0; i < presetEdgeCases().size(); ++i)
    QTest::newRow(presetEdgeCases()[i].name) << static_cast<int>(i);
}

void MonitorEditorTests::resize_aPresetSnapOfASingleEdge_movesOnlyThatSide()
{
  QFETCH(int, index);
  const auto &c = presetEdgeCases()[static_cast<size_t>(index)];
  QVERIFY(check(c.layout).empty());
  // the pixels are near 1920 x 1080 in both sides, and only the side of the edge that is dragged goes to it
  Constraints presetsOnly;
  presetsOnly.snapAspect = false;
  auto resized = resize(c.layout, 0, c.handle, c.toX, c.toY, 8, presetsOnly);
  QVERIFY(!resized.refused);
  QVERIFY(resized.preset);
  QCOMPARE(resized.layout.monitors[0].rect(), c.expected);
  QVERIFY2(check(resized.layout).empty(), "the monitors still touch");
  // with the ratios too, as it is when the mouse is used
  resized = resize(c.layout, 0, c.handle, c.toX, c.toY, 8);
  QVERIFY(!resized.refused);
  QVERIFY2(check(resized.layout).empty(), "the monitors still touch, with the ratios");
}

void MonitorEditorTests::resize_withTheAspectLocked_theOtherSideFollows()
{
  const auto layout = layoutOf({mon(0, 0, 1600, 900)});
  const auto lock = [](double ratio) {
    Constraints c;
    c.lockAspect = ratio;
    return c;
  };
  const double ratio = 1600.0 / 900.0;
  const auto rectOf = [&](Handle handle, double x, double y) -> Rect {
    const auto r = resize(layoutOf({mon(0, 0, 1600, 900)}), 0, handle, x, y, 8, lock(ratio));
    // a refusal is a rectangle that nothing is equal to
    return r.refused ? Rect{-1e9, -1e9, -1, -1} : r.layout.monitors[0].rect();
  };
  const auto same = [](const Rect &a, const Rect &b) {
    return std::abs(a.x - b.x) < 1e-6 && std::abs(a.y - b.y) < 1e-6 && std::abs(a.w - b.w) < 1e-6 &&
           std::abs(a.h - b.h) < 1e-6;
  };

  // an edge: the opposite edge and the near edge of the other side stay
  QVERIFY(same(rectOf(Handle::Right, 2000, 0), Rect{0, 0, 2000, 1125}));
  QVERIFY(same(rectOf(Handle::Bottom, 0, 1350), Rect{0, 0, 2400, 1350}));
  QVERIFY(same(rectOf(Handle::Left, -400, 0), Rect{-400, 0, 2000, 1125}));
  QVERIFY(same(rectOf(Handle::Top, 0, -450), Rect{0, -450, 2400, 1350}));
  // a corner: the side that was changed the most is the one that is followed, the opposite corner stays
  QVERIFY(same(rectOf(Handle::BottomRight, 2000, 950), Rect{0, 0, 2000, 1125}));
  QVERIFY(same(rectOf(Handle::BottomRight, 1650, 1350), Rect{0, 0, 2400, 1350}));
  QVERIFY(same(rectOf(Handle::TopLeft, -400, -100), Rect{-400, -225, 2000, 1125}));
  QVERIFY(same(rectOf(Handle::TopRight, 2000, -100), Rect{0, -225, 2000, 1125}));
  QVERIFY(same(rectOf(Handle::BottomLeft, -400, 1000), Rect{-400, 0, 2000, 1125}));

  // the ratios and the presets are not snapped, whatever is asked
  const auto near = resize(layout, 0, Handle::Right, 1925, 0, 8, lock(ratio));
  QVERIFY(!near.preset && near.aspect.empty() && !near.target);
  QVERIFY(std::abs(near.layout.monitors[0].w - 1925) < 1e-9);
  QVERIFY(std::abs(near.layout.monitors[0].h - 1925 / ratio) < 1e-9);

  // not smaller than the minimum, in either side
  const auto small = resize(layout, 0, Handle::Right, 10, 0, 8, lock(ratio));
  QVERIFY(small.layout.monitors[0].w >= kMinSide - 1e-9 && small.layout.monitors[0].h >= kMinSide - 1e-9);
  QVERIFY(std::abs(small.layout.monitors[0].w / small.layout.monitors[0].h - ratio) < 1e-9);

  // an edge snaps to the edge of another monitor, and the other side follows
  const auto two = layoutOf({mon(0, 0, 1600, 900), mon(1700, 0, 500, 900)});
  const auto snapped = resize(two, 0, Handle::Right, 1695, 0, 8, lock(ratio));
  QVERIFY(!snapped.refused);
  QCOMPARE(snapped.layout.monitors[0].w, 1700.0);
  QVERIFY(std::abs(snapped.layout.monitors[0].h - 1700 / ratio) < 1e-9);
  QVERIFY(!snapped.guides.empty());

  // and it is never put over another monitor, nor over an obstacle
  QVERIFY(resize(two, 0, Handle::Right, 2000, 0, 8, lock(ratio)).refused);
  Constraints among = lock(ratio);
  among.obstacles = {Rect{0, 1000, 3000, 500}};
  QVERIFY(resize(layout, 0, Handle::Bottom, 0, 1300, 8, among).refused);
}

void MonitorEditorTests::resize_withTheAspectLocked_aGuideIsOnlyOfAnEdgeThatIsWhereItSnapped()
{
  // a corner is dragged to near the left edge and the top edge of a neighbour: the lock lets the height lead, and the
  // width follows it away from the left edge of the neighbour, so only the top edge is where it snapped
  const auto layout = layoutOf({mon(0, 0, 1600, 900)});
  Constraints locked;
  locked.lockAspect = 1600.0 / 900.0;
  locked.obstacles = {Rect{2100, 1185, 100, 100}};
  const auto resized = resize(layout, 0, Handle::BottomRight, 2095, 1180, 8, locked);
  QVERIFY(!resized.refused);
  QCOMPARE(resized.layout.monitors[0].bottom(), 1185.0);
  QVERIFY(std::abs(resized.layout.monitors[0].w - 1185 * 16 / 9.0) < 1e-9);
  QCOMPARE(resized.guides.size(), size_t{1});
  QVERIFY(!resized.guides[0].vertical);
  QCOMPARE(resized.guides[0].position, 1185.0);

  // and an overlap is not refused when the caller says that it decides it
  Constraints notChecked;
  notChecked.obstacles = {Rect{1000, 0, 2000, 2000}};
  QVERIFY(resize(layout, 0, Handle::Right, 2000, 0, 8, notChecked).refused);
  notChecked.refuseOverlap = false;
  QVERIFY(!resize(layout, 0, Handle::Right, 2000, 0, 8, notChecked).refused);
}

void MonitorEditorTests::resize_neverSmallerThanTheMinimum_andNeverOverAnother()
{
  Constraints none;
  none.snapAspect = false;
  none.snapPresets = false;
  auto resized = resize(layoutOf({mon(0, 0, 1000, 600)}), 0, Handle::Right, 10, 0, 8, none);
  QCOMPARE(resized.layout.monitors[0].w, kMinSide);
  QCOMPARE(resized.layout.monitors[0].x, 0.0);
  resized = resize(layoutOf({mon(0, 0, 1000, 600)}), 0, Handle::TopLeft, 990, 590, 8, none);
  QCOMPARE(resized.layout.monitors[0].right(), 1000.0);
  QCOMPARE(resized.layout.monitors[0].bottom(), 600.0);
  QCOMPARE(resized.layout.monitors[0].w, kMinSide);
  QCOMPARE(resized.layout.monitors[0].h, kMinSide);

  const auto two = layoutOf({mon(0, 0, 1920, 1080), mon(1920, 0, 1920, 1080)});
  resized = resize(two, 0, Handle::Right, 2500, 0, 8, none);
  QVERIFY(resized.refused);
  QCOMPARE(resized.layout, two);
  // and a monitor that is not there
  QVERIFY(resize(two, 7, Handle::Right, 2500, 0, 8, none).refused);
}

void MonitorEditorTests::resize_aSingleRectangleAmongObstacles()
{
  // a computer that has no monitor list is one rectangle among the other computers
  Constraints among;
  among.snapAspect = false;
  among.snapPresets = false;
  among.obstacles = {Rect{1925, 0, 1920, 1080}};
  const auto layout = layoutOf({mon(0, 0, 1500, 1080)});

  auto resized = resize(layout, 0, Handle::Right, 1922, 0, 8, among);
  QVERIFY(!resized.refused);
  QCOMPARE(resized.layout.monitors[0].right(), 1925.0);
  QVERIFY(!resized.guides.empty());

  resized = resize(layout, 0, Handle::Right, 2500, 0, 8, among);
  QVERIFY(resized.refused);
  QCOMPARE(resized.layout, layout);
}

// -- realise

void MonitorEditorTests::realise_cases_data()
{
  QTest::addColumn<int>("index");
  for (size_t i = 0; i < realiseCases().size(); ++i)
    QTest::newRow(realiseCases()[i].name) << static_cast<int>(i);
}

void MonitorEditorTests::realise_cases()
{
  QFETCH(int, index);
  const auto &c = realiseCases()[static_cast<size_t>(index)];
  const auto made = realise(c.layout, c.reported);
  QVERIFY2(made.fit == c.fit, qPrintable(text(made.error)));
  QCOMPARE(text(made.text), QString::fromLatin1(c.text));
  QVERIFY(std::abs(made.zoom - c.zoom) < 1e-6);
  QCOMPARE(made.monitors.size(), c.layout.monitors.size());
  QCOMPARE(made.scales.size(), c.layout.monitors.size());
  // what the server reads is what is made
  const auto parsed = MonitorLayout::parse(made.text);
  QVERIFY2(static_cast<bool>(parsed), qPrintable(text(parsed.error)));
  QCOMPARE(parsed.layout->monitors(), made.monitors);
  if (c.fit == Fit::Matches)
    QCOMPARE(boxOf(made.monitors), *c.reported);
  QCOMPARE(made.reported.has_value(), c.reported.has_value());
}

void MonitorEditorTests::realise_aShapeThatDiffers_isSaid_andCanBeStretched()
{
  const auto layout = layoutOf({mon(0, 0, 2560 / 1.5, 960, 150), mon(-1920, -120, 1920, 1080)});
  const MonitorRect reported{-1920, 0, 4480, 1440};

  const auto differs = realise(layout, reported);
  QCOMPARE(static_cast<int>(differs.fit), static_cast<int>(Fit::Differs));
  QVERIFY2(std::abs(differs.differsPercent - 7.9) < 0.1, qPrintable(QString::number(differs.differsPercent)));
  // the drawing is 5440 x 1620 in the units of its main display, the computer is 4480 x 1440
  QCOMPARE(differs.drawingW, 5440);
  QCOMPARE(differs.drawingH, 1620);
  QCOMPARE(*differs.reported, reported);
  // the layout is still one that can be saved
  QVERIFY(static_cast<bool>(MonitorLayout::parse(differs.text)));

  const auto stretched = realise(layout, reported, true);
  QCOMPARE(static_cast<int>(stretched.fit), static_cast<int>(Fit::Matches));
  QCOMPARE(boxOf(stretched.monitors), reported);
  QVERIFY(static_cast<bool>(MonitorLayout::create(stretched.monitors)));

  // inside the tolerance it is fitted without being asked
  const auto close = realise(layoutOf({mon(0, 0, 1920, 1080)}), MonitorRect{0, 0, 1920, 1100});
  QCOMPARE(static_cast<int>(close.fit), static_cast<int>(Fit::Matches));
  QCOMPARE(text(close.text), QStringLiteral("0,0,1920,1100"));
}

void MonitorEditorTests::realise_keepsTheDrawingWhereItWas()
{
  for (const auto &c : realiseCases()) {
    const auto made = realise(c.layout, c.reported);
    QVERIFY(made.fit != Fit::Invalid);
    for (size_t i = 0; i < made.monitors.size(); ++i) {
      // the arrangement position of a monitor of the computer is that of the drawing, up to the rounding to pixels
      const auto x = made.originX + made.zoom * made.monitors[i].x;
      const auto y = made.originY + made.zoom * made.monitors[i].y;
      const double tolerance = 1.0 + made.zoom;
      QVERIFY2(std::abs(x - c.layout.monitors[i].x) <= tolerance, c.name);
      QVERIFY2(std::abs(y - c.layout.monitors[i].y) <= tolerance, c.name);
    }
  }
}

void MonitorEditorTests::realise_aLayoutThatIsNotValid_isNotMade()
{
  const auto overlapping = realise(layoutOf({mon(0, 0, 1920, 1080), mon(1000, 0, 1920, 1080)}), std::nullopt);
  QCOMPARE(static_cast<int>(overlapping.fit), static_cast<int>(Fit::Invalid));
  QVERIFY(!overlapping.error.empty());
  QVERIFY(overlapping.monitors.empty());
  QCOMPARE(static_cast<int>(realise(layoutOf({}), std::nullopt).fit), static_cast<int>(Fit::Invalid));
}

void MonitorEditorTests::realise_aZoomThatCannotBeUsed_isNotMade()
{
  // a screen of 40000 pixels is not one that a layout can have, and a drawing that has to be 24 times as big is not
  // a drawing of it
  const auto small = layoutOf({mon(0, 0, 160, 160)});
  QCOMPARE(static_cast<int>(realise(small, MonitorRect{0, 0, 3840, 3840}).fit), static_cast<int>(Fit::Invalid));
  QCOMPARE(static_cast<int>(realise(small, MonitorRect{0, 0, 40000, 40000}).fit), static_cast<int>(Fit::Invalid));
  QCOMPARE(static_cast<int>(realise(small, MonitorRect{0, 0, 1920, 1920}).fit), static_cast<int>(Fit::Matches));
}

void MonitorEditorTests::fromStored_usesTheScalesThatAreStored_andTheZoomForTheRest()
{
  const std::vector<MonitorRect> rects = {{0, 0, 2560, 1440}, {-1920, 360, 1920, 1080}};
  const auto layout = fromStored(rects, 0.5, 100, 200, {150});
  QCOMPARE(layout.monitors.size(), size_t{2});
  QCOMPARE(layout.monitors[0].rect(), (Rect{100, 200, 1280, 720}));
  QCOMPARE(layout.monitors[0].scale, 150.0);
  QCOMPARE(layout.monitors[0].pixelsW(), 1920);
  QCOMPARE(layout.monitors[1].rect(), (Rect{-860, 380, 960, 540}));
  // the scale of the zoom: a unit of the computer is half a unit of the drawing
  QCOMPARE(layout.monitors[1].scale, 200.0);

  // scales that are not scales are the scale of the zoom
  const auto odd = fromStored(rects, 1.0, 0, 0, {0, std::nan("")});
  QCOMPARE(odd.monitors[0].scale, 100.0);
  QCOMPARE(odd.monitors[1].scale, 100.0);
  // and a zoom that cannot be one is none
  QCOMPARE(fromStored(rects, 0.0, 0, 0, {}).monitors[0].w, 2560.0);
}

// -- properties

void MonitorEditorTests::property_fitToTheReportedBox_isExact_andHasNoGapAndNoOverlap()
{
  std::mt19937 rng(20261116);
  int rounds = 0;
  int touching = 0;
  for (int round = 0; round < 600; ++round) {
    const auto layout = randomLayout(rng);
    if (!check(layout).empty())
      continue;
    const auto box = layout.box();
    const double k = 0.4 + (rng() % 2600) / 1000.0;
    const double stretch = 1.0 + (static_cast<int>(rng() % 201) - 100) / 10000.0;
    const MonitorRect reported{
        static_cast<int32_t>(static_cast<int>(rng() % 6001) - 3000),
        static_cast<int32_t>(static_cast<int>(rng() % 6001) - 3000), static_cast<int32_t>(std::lround(box.w * k)),
        static_cast<int32_t>(std::lround(box.h * k * stretch))
    };
    const auto made = realise(layout, reported);
    QVERIFY2(made.fit == Fit::Matches, qPrintable(text(made.error)));
    ++rounds;

    // the box of the monitors is the screen, exactly, and the server reads the monitors
    QCOMPARE(boxOf(made.monitors), reported);
    QVERIFY(static_cast<bool>(MonitorLayout::create(made.monitors)));
    QVERIFY(static_cast<bool>(MonitorLayout::parse(made.text)));

    // the monitors that shared an edge in the drawing share it, with no gap and no overlap, after the rounding
    for (size_t i = 0; i < layout.monitors.size(); ++i) {
      for (size_t j = 0; j < layout.monitors.size(); ++j) {
        if (i == j)
          continue;
        const auto a = layout.monitors[i].rect();
        const auto b = layout.monitors[j].rect();
        const auto overlapY = std::min(a.bottom(), b.bottom()) - std::max(a.y, b.y);
        const auto overlapX = std::min(a.right(), b.right()) - std::max(a.x, b.x);
        if (a.right() == b.x && overlapY > 1.0) {
          ++touching;
          QCOMPARE(made.monitors[i].right(), made.monitors[j].x);
        }
        if (a.bottom() == b.y && overlapX > 1.0) {
          ++touching;
          QCOMPARE(made.monitors[i].bottom(), made.monitors[j].y);
        }
      }
    }
  }
  QVERIFY(rounds > 400);
  QVERIFY(touching > 400);
}

void MonitorEditorTests::property_realise_isIdempotent_andRoundTrips()
{
  std::mt19937 rng(20261117);
  for (int round = 0; round < 400; ++round) {
    const auto layout = randomLayout(rng);
    if (!check(layout).empty())
      continue;
    const auto box = layout.box();
    const double k = 0.5 + (rng() % 2000) / 1000.0;
    const std::optional<MonitorRect> reported =
        rng() % 3 == 0 ? std::nullopt
                       : std::optional<MonitorRect>(MonitorRect{
                             static_cast<int32_t>(static_cast<int>(rng() % 4001) - 2000),
                             static_cast<int32_t>(static_cast<int>(rng() % 4001) - 2000),
                             static_cast<int32_t>(std::lround(box.w * k)), static_cast<int32_t>(std::lround(box.h * k))
                         });
    const auto first = realise(layout, reported);
    QVERIFY(first.fit == Fit::Matches || first.fit == Fit::NotVerified);

    // drawn again from what was stored, and made again, it is what it was
    const auto again = fromStored(first.monitors, first.zoom, first.originX, first.originY, first.scales);
    QCOMPARE(again.monitors.size(), layout.monitors.size());
    for (size_t i = 0; i < again.monitors.size(); ++i) {
      QCOMPARE(again.monitors[i].scale, layout.monitors[i].scale);
      // the pixels of a monitor are what they were, up to the rounding of the screen
      QVERIFY(std::abs(again.monitors[i].pixelsW() - layout.monitors[i].pixelsW()) <= 2.0 * first.zoom + 1.0);
    }
    const auto second = realise(again, reported);
    QCOMPARE(second.monitors, first.monitors);
    QCOMPARE(text(second.text), text(first.text));
    QCOMPARE(static_cast<int>(second.fit), static_cast<int>(first.fit));
    QVERIFY(std::abs(second.zoom - first.zoom) < 1e-9);
  }
}

void MonitorEditorTests::property_editing_neverCreatesAnOverlap()
{
  std::mt19937 rng(20261118);
  static const std::array<Handle, 8> handles = {Handle::Left,       Handle::Right,      Handle::Top,
                                                Handle::Bottom,     Handle::TopLeft,    Handle::TopRight,
                                                Handle::BottomLeft, Handle::BottomRight};
  int refused = 0;
  int applied = 0;
  for (int round = 0; round < 300; ++round) {
    auto layout = randomLayout(rng);
    if (layout.monitors.size() < 2 || !check(layout).empty())
      continue;
    for (int step = 0; step < 12; ++step) {
      const auto index = rng() % layout.monitors.size();
      const auto x = static_cast<double>(static_cast<int>(rng() % 9000) - 3000);
      const auto y = static_cast<double>(static_cast<int>(rng() % 6000) - 2000);
      if (rng() % 2 == 0) {
        const auto moved = move(layout, index, x, y, 12);
        QVERIFY(!hasOverlap(moved.layout));
        if (moved.refused)
          QCOMPARE(moved.layout, layout);
        moved.refused ? ++refused : ++applied;
        layout = moved.layout;
      } else {
        const auto resized = resize(layout, index, handles[rng() % handles.size()], x, y, 12);
        QVERIFY(!hasOverlap(resized.layout));
        for (const auto &m : resized.layout.monitors)
          QVERIFY(m.w >= kMinSide - 1e-9 && m.h >= kMinSide - 1e-9);
        if (resized.refused)
          QCOMPARE(resized.layout, layout);
        resized.refused ? ++refused : ++applied;
        layout = resized.layout;
      }
    }
  }
  QVERIFY(refused > 100);
  QVERIFY(applied > 100);
}

// -- history

void MonitorEditorTests::history_goesBackAndForward()
{
  const auto a = layoutOf({mon(0, 0, 1920, 1080)});
  const auto b = added(a);
  const auto c = added(b);
  History history(a);
  QVERIFY(!history.canUndo() && !history.canRedo());

  history.push(b);
  history.push(b); // the same again is nothing
  history.push(c);
  QCOMPARE(history.current(), c);
  history.undo();
  QCOMPARE(history.current(), b);
  history.undo();
  QCOMPARE(history.current(), a);
  QVERIFY(!history.canUndo());
  history.undo();
  QCOMPARE(history.current(), a);
  history.redo();
  QCOMPARE(history.current(), b);
  QVERIFY(history.canRedo());

  // what is done after going back is the new end
  history.push(rotated(b, 1));
  QVERIFY(!history.canRedo());
  history.undo();
  QCOMPARE(history.current(), b);

  // it does not grow without end
  History long_(a);
  for (int i = 0; i < 1000; ++i)
    long_.push(layoutOf({mon(i, 0, 1920, 1080)}));
  int undone = 0;
  while (long_.canUndo()) {
    long_.undo();
    ++undone;
  }
  QVERIFY(undone <= 200 && undone > 50);
}

QTEST_APPLESS_MAIN(MonitorEditorTests)
