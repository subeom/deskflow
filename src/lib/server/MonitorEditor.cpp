/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/MonitorEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>

namespace deskflow::server::editor {

namespace {

using arrangement::kMaxZoom;
using arrangement::kMinZoom;

double overlapLength(double a0, double a1, double b0, double b1)
{
  return std::min(a1, b1) - std::max(a0, b0);
}

bool overlaps(const Rect &a, const Rect &b)
{
  return overlapLength(a.x, a.right(), b.x, b.right()) > kEdge &&
         overlapLength(a.y, a.bottom(), b.y, b.bottom()) > kEdge;
}

// how much of an edge the rectangles share, when they touch
double touchLength(const Rect &a, const Rect &b)
{
  if (std::abs(a.right() - b.x) <= kEdge || std::abs(b.right() - a.x) <= kEdge)
    return std::max(0.0, overlapLength(a.y, a.bottom(), b.y, b.bottom()));
  if (std::abs(a.bottom() - b.y) <= kEdge || std::abs(b.bottom() - a.y) <= kEdge)
    return std::max(0.0, overlapLength(a.x, a.right(), b.x, b.right()));
  return 0.0;
}

bool linked(const Rect &a, const Rect &b)
{
  return overlaps(a, b) || touchLength(a, b) > kEdge;
}

bool isNumber(double value)
{
  return std::isfinite(value) && std::abs(value) < arrangement::kMaxCoordinate;
}

const std::array<Preset, 24> kPresets = {{
    {"1280 × 720 (HD)", 1280, 720},
    {"1366 × 768", 1366, 768},
    {"1600 × 900", 1600, 900},
    {"1920 × 1080 (Full HD)", 1920, 1080},
    {"1920 × 1200 (16:10)", 1920, 1200},
    {"2560 × 1080 (UltraWide)", 2560, 1080},
    {"2560 × 1440 (QHD)", 2560, 1440},
    {"2560 × 1600", 2560, 1600},
    {"2880 × 1800", 2880, 1800},
    {"3000 × 2000 (3:2)", 3000, 2000},
    {"3440 × 1440 (UltraWide QHD)", 3440, 1440},
    {"3840 × 1080 (Dual Full HD)", 3840, 1080},
    {"3840 × 1600", 3840, 1600},
    {"3840 × 2160 (4K)", 3840, 2160},
    {"3840 × 2400", 3840, 2400},
    {"5120 × 1440 (Super UltraWide)", 5120, 1440},
    {"5120 × 2160", 5120, 2160},
    {"7680 × 4320 (8K)", 7680, 4320},
    {"MacBook Air 13″ (1470 × 956 pt)", 1470, 956},
    {"MacBook Air 15″ (1710 × 1107 pt)", 1710, 1107},
    {"MacBook Pro 14″ (1512 × 982 pt)", 1512, 982},
    {"MacBook Pro 16″ (1728 × 1117 pt)", 1728, 1117},
    {"iMac 24″ (2240 × 1260 pt)", 2240, 1260},
    {"MacBook Air 13″, older (1440 × 900 pt)", 1440, 900},
}};

const std::array<Aspect, 7> kAspects = {{
    {"16:9", 16, 9},
    {"16:10", 16, 10},
    // 21:9 monitors are 2560x1080 (2.37) and 3440x1440 (2.39)
    {"21:9", 64, 27},
    {"32:9", 32, 9},
    {"4:3", 4, 3},
    {"3:2", 3, 2},
    {"5:4", 5, 4},
}};

const std::array<int, 12> kScales = {100, 125, 150, 175, 200, 225, 250, 300, 350, 400, 450, 500};

std::string portraitName(const char *name)
{
  const std::string landscape(name);
  const auto colon = landscape.find(':');
  return landscape.substr(colon + 1) + ":" + landscape.substr(0, colon);
}

// the ratios as numbers, with the name, landscape first and then portrait
struct Ratio
{
  double value;
  std::string name;
};

const std::vector<Ratio> &ratios()
{
  static const auto all = [] {
    std::vector<Ratio> list;
    for (const auto &aspect : kAspects)
      list.push_back({static_cast<double>(aspect.w) / aspect.h, aspect.name});
    for (const auto &aspect : kAspects)
      list.push_back({static_cast<double>(aspect.h) / aspect.w, portraitName(aspect.name)});
    return list;
  }();
  return all;
}

std::vector<Rect> othersOf(const Layout &layout, size_t index, const std::vector<Rect> &obstacles)
{
  std::vector<Rect> others;
  for (size_t i = 0; i < layout.monitors.size(); ++i) {
    if (i != index)
      others.push_back(layout.monitors[i].rect());
  }
  others.insert(others.end(), obstacles.begin(), obstacles.end());
  return others;
}

bool overlapsAny(const Rect &rect, const std::vector<Rect> &others)
{
  return std::ranges::any_of(others, [&](const Rect &o) { return overlaps(rect, o); });
}

Guide verticalGuide(double position, const Rect &a, const Rect &b, bool touching)
{
  return {true, position, std::min(a.y, b.y), std::max(a.bottom(), b.bottom()), touching};
}

Guide horizontalGuide(double position, const Rect &a, const Rect &b, bool touching)
{
  return {false, position, std::min(a.x, b.x), std::max(a.right(), b.right()), touching};
}

struct Snapped
{
  double position = 0;
  bool snapped = false;
  bool touching = false;
  double guide = 0;
  Rect partner;
};

// the position of the start of a side of \p length that is nearest to \p wanted and within \p radius of an edge or
// the middle of one of the \p others
Snapped snapStart(double wanted, double length, bool xAxis, const std::vector<Rect> &others, double radius)
{
  Snapped best;
  double bestDistance = radius + 1e-9;
  const auto offer = [&](double position, bool touching, double guide, const Rect &partner) {
    const double distance = std::abs(position - wanted);
    if (distance < bestDistance || (distance == bestDistance && touching && best.snapped && !best.touching)) {
      best = {position, true, touching, guide, partner};
      bestDistance = distance;
    }
  };
  for (const auto &o : others) {
    const double start = xAxis ? o.x : o.y;
    const double end = xAxis ? o.right() : o.bottom();
    offer(end, true, end, o);
    offer(start - length, true, start, o);
    offer(start, false, start, o);
    offer(end - length, false, end, o);
    offer((start + end) / 2 - length / 2, false, (start + end) / 2, o);
  }
  return best;
}

// the corner that a monitor of a new size keeps, which is the one that keeps most of the edges that touch
Layout resizedKeepingContacts(const Layout &layout, size_t index, double w, double h)
{
  const auto &m = layout.monitors[index];
  const auto others = othersOf(layout, index, {});
  const std::array<std::pair<double, double>, 4> corners = {{
      {m.x, m.y},
      {m.right() - w, m.y},
      {m.x, m.bottom() - h},
      {m.right() - w, m.bottom() - h},
  }};
  double bestContact = -1;
  std::pair<double, double> chosen = corners[0];
  for (const auto &corner : corners) {
    const Rect candidate{corner.first, corner.second, w, h};
    if (overlapsAny(candidate, others))
      continue;
    double contact = 0;
    for (const auto &o : others)
      contact += touchLength(candidate, o);
    if (contact > bestContact + 1e-9) {
      bestContact = contact;
      chosen = corner;
    }
  }
  auto result = layout;
  result.monitors[index].x = chosen.first;
  result.monitors[index].y = chosen.second;
  result.monitors[index].w = w;
  result.monitors[index].h = h;
  return result;
}

// edges that are within kEdge of each other are one edge
std::vector<std::vector<double>> clusterOf(std::vector<double> values)
{
  std::ranges::sort(values);
  std::vector<std::vector<double>> clusters;
  for (const auto value : values) {
    if (clusters.empty() || value - clusters.back().back() > kEdge)
      clusters.push_back({value});
    else
      clusters.back().push_back(value);
  }
  return clusters;
}

size_t clusterIndex(const std::vector<std::vector<double>> &clusters, double value)
{
  size_t best = 0;
  double distance = std::numeric_limits<double>::max();
  for (size_t i = 0; i < clusters.size(); ++i) {
    const double d = std::min(std::abs(clusters[i].front() - value), std::abs(clusters[i].back() - value));
    if (d < distance) {
      distance = d;
      best = i;
    }
  }
  return best;
}

} // namespace

Rect Layout::box() const
{
  if (monitors.empty())
    return {};
  double left = monitors.front().x;
  double top = monitors.front().y;
  double right = monitors.front().right();
  double bottom = monitors.front().bottom();
  for (const auto &m : monitors) {
    left = std::min(left, m.x);
    top = std::min(top, m.y);
    right = std::max(right, m.right());
    bottom = std::max(bottom, m.bottom());
  }
  return {left, top, right - left, bottom - top};
}

std::vector<Problem> check(const Layout &layout)
{
  using enum Problem::Kind;
  std::vector<Problem> problems;
  const auto &monitors = layout.monitors;
  if (monitors.empty())
    return {{NoMonitors}};
  if (monitors.size() > MonitorLayout::kMaxMonitors)
    problems.push_back({TooMany});

  bool unusable = false;
  for (size_t i = 0; i < monitors.size(); ++i) {
    const auto &m = monitors[i];
    if (!isNumber(m.x) || !isNumber(m.y) || !std::isfinite(m.w) || !std::isfinite(m.h) || m.w < kMinSide ||
        m.h < kMinSide || m.w > kMaxSide || m.h > kMaxSide) {
      problems.push_back({BadSize, i});
      unusable = true;
    }
  }
  for (size_t i = 0; i < monitors.size(); ++i) {
    if (!std::isfinite(monitors[i].scale) || monitors[i].scale < kMinScale || monitors[i].scale > kMaxScale) {
      problems.push_back({BadScale, i});
      unusable = true;
    }
  }
  if (unusable)
    return problems;

  for (size_t i = 0; i < monitors.size(); ++i) {
    for (size_t j = i + 1; j < monitors.size(); ++j) {
      if (overlaps(monitors[i].rect(), monitors[j].rect()))
        problems.push_back({Overlap, i, j});
    }
  }

  std::vector<bool> reached(monitors.size(), false);
  std::vector<size_t> pending{0};
  reached[0] = true;
  while (!pending.empty()) {
    const auto current = pending.back();
    pending.pop_back();
    for (size_t i = 0; i < monitors.size(); ++i) {
      if (!reached[i] && linked(monitors[current].rect(), monitors[i].rect())) {
        reached[i] = true;
        pending.push_back(i);
      }
    }
  }
  for (size_t i = 1; i < monitors.size(); ++i) {
    if (!reached[i]) {
      problems.push_back({NotConnected, i});
      break;
    }
  }
  return problems;
}

std::string describe(const Problem &problem)
{
  using enum Problem::Kind;
  switch (problem.kind) {
  case NoMonitors:
    return "there is no monitor";
  case TooMany:
    return std::format("at most {} monitors are supported", MonitorLayout::kMaxMonitors);
  case BadSize:
    return std::format("monitor {} has a size that a monitor cannot have", problem.first + 1);
  case BadScale:
    return std::format(
        "monitor {} has a display scale that is not between {:.0f} % and {:.0f} %", problem.first + 1, kMinScale,
        kMaxScale
    );
  case Overlap:
    return std::format("monitors {} and {} overlap", problem.first + 1, problem.second + 1);
  case NotConnected:
    return std::format("monitor {} does not touch the others: monitors share part of an edge", problem.first + 1);
  }
  return {};
}

std::span<const Preset> presets()
{
  return kPresets;
}

const Preset *presetOf(int w, int h)
{
  const auto found = std::ranges::find_if(kPresets, [&](const Preset &p) { return p.w == w && p.h == h; });
  return found == kPresets.end() ? nullptr : &*found;
}

std::span<const Aspect> aspects()
{
  return kAspects;
}

std::string aspectNameOf(double w, double h)
{
  if (!(w > 0) || !(h > 0))
    return {};
  const double ratio = w / h;
  const Ratio *best = nullptr;
  double distance = kAspectSnap;
  for (const auto &candidate : ratios()) {
    const double d = std::abs(ratio - candidate.value) / candidate.value;
    if (d <= distance) {
      distance = d;
      best = &candidate;
    }
  }
  return best != nullptr ? best->name : std::string();
}

std::span<const int> commonScales()
{
  return kScales;
}

MoveResult
move(const Layout &layout, size_t index, double x, double y, double radius, const std::vector<Rect> &obstacles)
{
  if (index >= layout.monitors.size() || !std::isfinite(x) || !std::isfinite(y))
    return {layout, {}, true};
  const auto &m = layout.monitors[index];
  const auto others = othersOf(layout, index, obstacles);

  const auto snapX = snapStart(x, m.w, true, others, radius);
  const auto snapY = snapStart(y, m.h, false, others, radius);

  struct Try
  {
    double x;
    double y;
    bool useX;
    bool useY;
  };
  const std::array<Try, 4> tries = {{
      {snapX.snapped ? snapX.position : x, snapY.snapped ? snapY.position : y, snapX.snapped, snapY.snapped},
      {snapX.snapped ? snapX.position : x, y, snapX.snapped, false},
      {x, snapY.snapped ? snapY.position : y, false, snapY.snapped},
      {x, y, false, false},
  }};
  for (const auto &attempt : tries) {
    const Rect rect{attempt.x, attempt.y, m.w, m.h};
    if (overlapsAny(rect, others))
      continue;
    MoveResult result{layout, {}, false};
    result.layout.monitors[index].x = attempt.x;
    result.layout.monitors[index].y = attempt.y;
    if (attempt.useX)
      result.guides.push_back(verticalGuide(snapX.guide, rect, snapX.partner, snapX.touching));
    if (attempt.useY)
      result.guides.push_back(horizontalGuide(snapY.guide, rect, snapY.partner, snapY.touching));
    return result;
  }
  return {layout, {}, true};
}

ResizeResult resize(
    const Layout &layout, size_t index, Handle handle, double toX, double toY, double radius,
    const Constraints &constraints
)
{
  const ResizeResult refused{layout, {}, true, {}, false, false};
  if (index >= layout.monitors.size() || !std::isfinite(toX) || !std::isfinite(toY))
    return refused;
  const auto &m = layout.monitors[index];
  const auto others = othersOf(layout, index, constraints.obstacles);

  const bool dragL = handle == Handle::Left || handle == Handle::TopLeft || handle == Handle::BottomLeft;
  const bool dragR = handle == Handle::Right || handle == Handle::TopRight || handle == Handle::BottomRight;
  const bool dragT = handle == Handle::Top || handle == Handle::TopLeft || handle == Handle::TopRight;
  const bool dragB = handle == Handle::Bottom || handle == Handle::BottomLeft || handle == Handle::BottomRight;
  double left = m.x;
  double right = m.right();
  double top = m.y;
  double bottom = m.bottom();
  if (dragL)
    left = toX;
  if (dragR)
    right = toX;
  if (dragT)
    top = toY;
  if (dragB)
    bottom = toY;

  const auto enforceMinimum = [&] {
    if (right - left < kMinSide) {
      if (dragL)
        left = right - kMinSide;
      else
        right = left + kMinSide;
    }
    if (bottom - top < kMinSide) {
      if (dragT)
        top = bottom - kMinSide;
      else
        bottom = top + kMinSide;
    }
  };
  enforceMinimum();

  ResizeResult result{layout, {}, false, {}, false, false};

  // an edge that is dragged goes to the edge of another monitor that it is near
  struct EdgeSnap
  {
    bool snapped = false;
    double edge = 0;
    Rect partner;
  };
  const auto nearestEdge = [&](double edge, bool xAxis) {
    EdgeSnap found;
    double distance = radius + 1e-9;
    for (const auto &o : others) {
      for (const double candidate : xAxis ? std::array{o.x, o.right()} : std::array{o.y, o.bottom()}) {
        if (std::abs(candidate - edge) < distance) {
          distance = std::abs(candidate - edge);
          found = {true, candidate, o};
        }
      }
    }
    return found;
  };
  EdgeSnap snapX;
  EdgeSnap snapY;
  if (dragL || dragR) {
    snapX = nearestEdge(dragL ? left : right, true);
    if (snapX.snapped) {
      auto &edge = dragL ? left : right;
      const double previous = edge;
      edge = snapX.edge;
      if (right - left < kMinSide) {
        edge = previous;
        snapX.snapped = false;
      }
    }
  }
  if (dragT || dragB) {
    snapY = nearestEdge(dragT ? top : bottom, false);
    if (snapY.snapped) {
      auto &edge = dragT ? top : bottom;
      const double previous = edge;
      edge = snapY.edge;
      if (bottom - top < kMinSide) {
        edge = previous;
        snapY.snapped = false;
      }
    }
  }

  const auto setWidth = [&](double w) {
    if (dragL)
      left = right - w;
    else
      right = left + w;
  };
  const auto setHeight = [&](double h) {
    if (dragT)
      top = bottom - h;
    else
      bottom = top + h;
  };

  // the ratio is kept: the side that is not dragged follows the one that is
  const bool locked = constraints.lockAspect && *constraints.lockAspect > 0;
  if (locked) {
    const double ratio = *constraints.lockAspect;
    const bool horizontalOnly = (dragL || dragR) && !(dragT || dragB);
    const bool verticalOnly = (dragT || dragB) && !(dragL || dragR);
    const double w = right - left;
    const double h = bottom - top;
    const bool widthLeads = horizontalOnly || (!verticalOnly && std::abs(w - m.w) / m.w >= std::abs(h - m.h) / m.h);
    if (widthLeads)
      setHeight(w / ratio);
    else
      setWidth(h * ratio);
    const double newW = right - left;
    const double newH = bottom - top;
    if (newW < kMinSide || newH < kMinSide) {
      const double k = std::max(kMinSide / newW, kMinSide / newH);
      setWidth(newW * k);
      setHeight(newH * k);
    }
  }

  // a side goes to the ratio that the monitor is near
  if (!locked && constraints.snapAspect) {
    const bool horizontalOnly = (dragL || dragR) && !(dragT || dragB);
    const bool verticalOnly = (dragT || dragB) && !(dragL || dragR);
    const bool adjustWidth = horizontalOnly ? !snapX.snapped : (!verticalOnly && snapY.snapped && !snapX.snapped);
    const bool adjustHeight = verticalOnly ? !snapY.snapped : (!horizontalOnly && !snapY.snapped);
    if (adjustWidth || adjustHeight) {
      const double w = right - left;
      const double h = bottom - top;
      double bestDistance = kAspectSnap;
      const Ratio *best = nullptr;
      for (const auto &ratio : ratios()) {
        const double d = adjustHeight ? std::abs(h - w / ratio.value) / (w / ratio.value)
                                      : std::abs(w - h * ratio.value) / (h * ratio.value);
        if (d <= bestDistance) {
          bestDistance = d;
          best = &ratio;
        }
      }
      if (best != nullptr) {
        if (adjustHeight)
          setHeight(w / best->value);
        else
          setWidth(h * best->value);
        result.aspect = best->name;
        enforceMinimum();
      }
    }
  }

  // the pixels go to a preset when they are near it
  if (!locked && constraints.snapPresets && !snapX.snapped && !snapY.snapped) {
    const double pw = (right - left) * m.scale / 100.0;
    const double ph = (bottom - top) * m.scale / 100.0;
    for (const auto &preset : kPresets) {
      if (std::abs(pw - preset.w) <= kPresetSnap * preset.w && std::abs(ph - preset.h) <= kPresetSnap * preset.h) {
        // only the sides of the edges that are dragged: the other side is where the monitors that touch it need it
        if (dragL || dragR)
          setWidth(preset.w * 100.0 / m.scale);
        if (dragT || dragB)
          setHeight(preset.h * 100.0 / m.scale);
        result.preset = true;
        break;
      }
    }
  }

  // the screen that the computer reports
  if (!locked && constraints.targetSize) {
    const auto [tw, th] = *constraints.targetSize;
    if ((dragL || dragR) && !snapX.snapped && std::abs((right - left) - tw) <= kPresetSnap * tw) {
      setWidth(tw);
      result.target = true;
    }
    if ((dragT || dragB) && !snapY.snapped && std::abs((bottom - top) - th) <= kPresetSnap * th) {
      setHeight(th);
      result.target = true;
    }
  }

  const Rect rect{left, top, right - left, bottom - top};
  if (overlapsAny(rect, others))
    return refused;
  result.layout.monitors[index].x = left;
  result.layout.monitors[index].y = top;
  result.layout.monitors[index].w = rect.w;
  result.layout.monitors[index].h = rect.h;
  if (snapX.snapped)
    result.guides.push_back(verticalGuide(dragL ? left : right, rect, snapX.partner, false));
  if (snapY.snapped)
    result.guides.push_back(horizontalGuide(dragT ? top : bottom, rect, snapY.partner, false));
  return result;
}

Layout withResolution(const Layout &layout, size_t index, int pixelsW, int pixelsH)
{
  if (index >= layout.monitors.size() || pixelsW <= 0 || pixelsH <= 0)
    return layout;
  const auto scale = layout.monitors[index].scale;
  return resizedKeepingContacts(layout, index, pixelsW * 100.0 / scale, pixelsH * 100.0 / scale);
}

Layout withScale(const Layout &layout, size_t index, double scale)
{
  if (index >= layout.monitors.size() || !std::isfinite(scale))
    return layout;
  scale = std::clamp(scale, kMinScale, kMaxScale);
  const auto &m = layout.monitors[index];
  const double pixelsW = m.w * m.scale / 100.0;
  const double pixelsH = m.h * m.scale / 100.0;
  auto result = resizedKeepingContacts(layout, index, pixelsW * 100.0 / scale, pixelsH * 100.0 / scale);
  result.monitors[index].scale = scale;
  return result;
}

Layout rotated(const Layout &layout, size_t index)
{
  if (index >= layout.monitors.size())
    return layout;
  const auto &m = layout.monitors[index];
  return resizedKeepingContacts(layout, index, m.h, m.w);
}

Layout added(const Layout &layout)
{
  if (layout.monitors.size() >= MonitorLayout::kMaxMonitors)
    return layout;
  if (layout.monitors.empty())
    return Layout{{Monitor{}}};
  auto result = layout;
  Monitor next = layout.monitors.front();
  const auto *rightmost = &layout.monitors.front();
  for (const auto &m : layout.monitors) {
    if (m.right() >= rightmost->right())
      rightmost = &m;
  }
  next.x = rightmost->right();
  next.y = rightmost->y;
  result.monitors.push_back(next);
  return result;
}

Layout removed(const Layout &layout, size_t index)
{
  if (index >= layout.monitors.size() || layout.monitors.size() == 1)
    return layout;
  auto result = layout;
  result.monitors.erase(result.monitors.begin() + static_cast<std::ptrdiff_t>(index));
  return result;
}

Layout withMain(const Layout &layout, size_t index)
{
  if (index >= layout.monitors.size() || index == 0)
    return layout;
  auto result = layout;
  const auto main = result.monitors[index];
  result.monitors.erase(result.monitors.begin() + static_cast<std::ptrdiff_t>(index));
  result.monitors.insert(result.monitors.begin(), main);
  return result;
}

Layout fromStored(
    const std::vector<MonitorRect> &rects, double zoom, double originX, double originY,
    const std::vector<double> &scales
)
{
  const double z = std::isfinite(zoom) && zoom > 0 ? zoom : 1.0;
  const double zoomScale = std::clamp(100.0 / z, kMinScale, kMaxScale);
  Layout layout;
  for (size_t i = 0; i < rects.size(); ++i) {
    Monitor m;
    m.x = originX + z * rects[i].x;
    m.y = originY + z * rects[i].y;
    m.w = z * rects[i].w;
    m.h = z * rects[i].h;
    const double scale = i < scales.size() ? scales[i] : std::nan("");
    m.scale = std::isfinite(scale) && scale >= kMinScale && scale <= kMaxScale ? scale : zoomScale;
    layout.monitors.push_back(m);
  }
  return layout;
}

Realised realise(const Layout &layout, const std::optional<MonitorRect> &reported, bool stretch)
{
  Realised out;
  out.reported = reported;
  if (const auto problems = check(layout); !problems.empty()) {
    out.error = describe(problems.front());
    return out;
  }

  const auto box = layout.box();
  const auto &main = layout.monitors.front();
  const double k0 = main.scale / 100.0;
  out.drawingW = static_cast<int>(std::lround(box.w * k0));
  out.drawingH = static_cast<int>(std::lround(box.h * k0));

  double kx = k0;
  double ky = k0;
  bool fitted = false;
  if (reported && reported->w > 0 && reported->h > 0) {
    const double fx = static_cast<double>(reported->w) / box.w;
    const double fy = static_cast<double>(reported->h) / box.h;
    const double differs = std::abs(fy / fx - 1.0);
    if (differs <= kFitTolerance || stretch) {
      kx = fx;
      ky = fy;
      fitted = true;
    } else {
      out.differsPercent = differs * 100.0;
    }
  }

  std::vector<double> xs;
  std::vector<double> ys;
  for (const auto &m : layout.monitors) {
    xs.push_back(m.x);
    xs.push_back(m.right());
    ys.push_back(m.y);
    ys.push_back(m.bottom());
  }
  const auto xClusters = clusterOf(xs);
  const auto yClusters = clusterOf(ys);

  // Where each edge of the drawing is in the coordinates of the computer, rounded as an edge: the monitors that share
  // one go on sharing it. With the screen of the computer the first and the last edge are those of the screen.
  const auto place = [&](const std::vector<std::vector<double>> &clusters, double k, double start, double anchor,
                         long screenStart, long screenLength) {
    std::vector<long> edges(clusters.size());
    for (size_t i = 0; i < clusters.size(); ++i) {
      edges[i] = fitted ? screenStart + std::lround((clusters[i].front() - start) * k)
                        : std::lround((clusters[i].front() - anchor) * k);
    }
    if (fitted) {
      edges.front() = screenStart;
      edges.back() = screenStart + screenLength;
    } else {
      edges[clusterIndex(clusters, anchor)] = 0;
    }
    return edges;
  };
  const auto xEdges = place(xClusters, kx, box.x, main.x, fitted ? reported->x : 0, fitted ? reported->w : 0);
  const auto yEdges = place(yClusters, ky, box.y, main.y, fitted ? reported->y : 0, fitted ? reported->h : 0);

  std::vector<MonitorRect> rects;
  for (const auto &m : layout.monitors) {
    const auto left = xEdges[clusterIndex(xClusters, m.x)];
    const auto right = xEdges[clusterIndex(xClusters, m.right())];
    const auto top = yEdges[clusterIndex(yClusters, m.y)];
    const auto bottom = yEdges[clusterIndex(yClusters, m.bottom())];
    if (right <= left || bottom <= top) {
      out.error = "a monitor is too small at the size of this computer";
      return out;
    }
    if (std::abs(left) > 1000000 || std::abs(right) > 1000000 || std::abs(top) > 1000000 ||
        std::abs(bottom) > 1000000) {
      out.error = "the numbers are out of range";
      return out;
    }
    rects.push_back(
        {static_cast<int32_t>(left), static_cast<int32_t>(top), static_cast<int32_t>(right - left),
         static_cast<int32_t>(bottom - top)}
    );
  }

  const double zoom = 1.0 / std::sqrt(kx * ky);
  if (!(zoom >= kMinZoom && zoom <= kMaxZoom)) {
    out.error = "the drawing is too far from the size of the computer";
    return out;
  }
  const auto created = MonitorLayout::create(rects);
  if (!created) {
    out.error = created.error;
    return out;
  }

  out.monitors = rects;
  for (const auto &m : layout.monitors)
    out.scales.push_back(m.scale);
  out.zoom = zoom;
  const double anchorX = fitted ? box.x : main.x;
  const double anchorY = fitted ? box.y : main.y;
  out.originX = anchorX - static_cast<double>(xEdges[clusterIndex(xClusters, anchorX)]) / kx;
  out.originY = anchorY - static_cast<double>(yEdges[clusterIndex(yClusters, anchorY)]) / ky;
  out.text = created.layout->format();
  out.fit = fitted ? Fit::Matches : (reported ? Fit::Differs : Fit::NotVerified);
  return out;
}

History::History(Layout initial) : m_states{std::move(initial)}
{
}

void History::push(Layout layout)
{
  if (layout == current())
    return;
  m_states.resize(m_position + 1);
  m_states.push_back(std::move(layout));
  if (m_states.size() > kLimit)
    m_states.erase(m_states.begin(), m_states.begin() + static_cast<std::ptrdiff_t>(m_states.size() - kLimit));
  m_position = m_states.size() - 1;
}

void History::undo()
{
  if (canUndo())
    --m_position;
}

void History::redo()
{
  if (canRedo())
    ++m_position;
}

} // namespace deskflow::server::editor
