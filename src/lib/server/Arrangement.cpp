/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/Arrangement.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <format>
#include <limits>
#include <map>
#include <queue>
#include <string>
#include <tuple>
#include <utility>

namespace deskflow::server::arrangement {

namespace {

// the order that the classic grid writes the links of a computer in
constexpr std::array<Direction, 4> kLinkOrder = {Direction::Right, Direction::Left, Direction::Top, Direction::Bottom};

// what is left of a number that should be the same
constexpr double kSame = 1.0e-6;
// Stretches of an edge that overlap, or are apart, by less than this are the same place: positions are kept to three
// decimals, which makes edges that were snapped to each other miss by up to 5e-4
constexpr double kHair = 0.01;

bool isHorizontalSide(Direction side)
{
  return side == Direction::Left || side == Direction::Right;
}

Direction oppositeOf(Direction side)
{
  using enum Direction;
  switch (side) {
  case Left:
    return Right;
  case Right:
    return Left;
  case Top:
    return Bottom;
  case Bottom:
    return Top;
  default:
    return NoDirection;
  }
}

size_t sideRank(Direction side)
{
  return static_cast<size_t>(std::ranges::find(kLinkOrder, side) - kLinkOrder.begin());
}

bool sameName(std::string_view a, std::string_view b)
{
  return std::ranges::equal(a, b, [](char x, char y) {
    return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
  });
}

bool isNumber(double value)
{
  return std::isfinite(value);
}

bool overlap(const Rect &a, const Rect &b)
{
  const auto ix = std::min(a.right(), b.right()) - std::max(a.x, b.x);
  const auto iy = std::min(a.bottom(), b.bottom()) - std::max(a.y, b.y);
  return ix > kTouchTolerance && iy > kTouchTolerance;
}

// an edge of a monitor in the coordinates of its computer
struct IntSegment
{
  Direction side;
  int32_t line;
  int32_t from;
  int32_t to;
};

// the parts of the edge of every monitor of \p monitors that no other monitor is right behind
std::vector<IntSegment> exposedIntSegments(const std::vector<MonitorRect> &monitors)
{
  std::vector<IntSegment> all;
  for (const auto &m : monitors) {
    for (const auto side : kLinkOrder) {
      const bool horizontal = isHorizontalSide(side);
      const int32_t line = side == Direction::Right    ? m.right()
                           : side == Direction::Left   ? m.x
                           : side == Direction::Bottom ? m.bottom()
                                                       : m.y;
      const int32_t from = horizontal ? m.y : m.x;
      const int32_t to = horizontal ? m.bottom() : m.right();

      // the stretches that another monitor is behind
      std::vector<std::pair<int32_t, int32_t>> covered;
      for (const auto &n : monitors) {
        if (&n == &m)
          continue;
        int32_t behind;
        switch (side) {
        case Direction::Right:
          behind = n.x;
          break;
        case Direction::Left:
          behind = n.right();
          break;
        case Direction::Bottom:
          behind = n.y;
          break;
        default:
          behind = n.bottom();
          break;
        }
        if (behind != line)
          continue;
        covered.emplace_back(horizontal ? n.y : n.x, horizontal ? n.bottom() : n.right());
      }
      std::ranges::sort(covered);

      int32_t at = from;
      for (const auto &[start, end] : covered) {
        if (start > at)
          all.push_back({side, line, at, std::min(start, to)});
        at = std::max(at, end);
        if (at >= to)
          break;
      }
      if (at < to)
        all.push_back({side, line, at, to});
    }
  }

  // collinear edges that continue each other are one
  std::ranges::sort(all, [](const IntSegment &a, const IntSegment &b) {
    return std::tie(a.side, a.line, a.from, a.to) < std::tie(b.side, b.line, b.from, b.to);
  });
  std::vector<IntSegment> merged;
  for (const auto &segment : all) {
    if (segment.to <= segment.from)
      continue;
    if (!merged.empty() && merged.back().side == segment.side && merged.back().line == segment.line &&
        segment.from <= merged.back().to) {
      merged.back().to = std::max(merged.back().to, segment.to);
    } else {
      merged.push_back(segment);
    }
  }
  return merged;
}

// an exposed edge in the arrangement
struct Segment
{
  Direction side;
  double line;
  double from;
  double to;
};

std::vector<Segment> exposedSegments(const Computer &computer)
{
  std::vector<Segment> segments;
  for (const auto &s : exposedIntSegments(computer.distinctMonitors())) {
    const bool horizontal = isHorizontalSide(s.side);
    const auto lineOrigin = horizontal ? computer.originX : computer.originY;
    const auto alongOrigin = horizontal ? computer.originY : computer.originX;
    segments.push_back({
        s.side,
        lineOrigin + computer.zoom * s.line,
        alongOrigin + computer.zoom * s.from,
        alongOrigin + computer.zoom * s.to,
    });
  }
  return segments;
}

// how far the exposed edge is from the bounding box on its side
double depthOf(const Rect &box, const Segment &segment)
{
  switch (segment.side) {
  case Direction::Left:
    return segment.line - box.x;
  case Direction::Right:
    return box.right() - segment.line;
  case Direction::Top:
    return segment.line - box.y;
  default:
    return box.bottom() - segment.line;
  }
}

bool isUsable(const Computer &computer)
{
  if (computer.name.empty() || !isNumber(computer.zoom) || computer.zoom < kMinZoom || computer.zoom > kMaxZoom)
    return false;
  if (!isNumber(computer.originX) || !isNumber(computer.originY) || std::abs(computer.originX) > kMaxCoordinate ||
      std::abs(computer.originY) > kMaxCoordinate)
    return false;
  return static_cast<bool>(MonitorLayout::create(computer.monitors));
}

// one stretch of edges that touch, seen from both computers
struct Candidate
{
  size_t a;
  Direction sideA;
  size_t b;
  Direction sideB;
  double from;
  double to;
  // how far in from the edge of their computers the touching edges are: the outer ones win
  double depth;
  // where the edges are
  double line;

  double length() const
  {
    return to - from;
  }
};

// the sides of computers that two candidates both have source ranges on, where the ranges overlap
std::vector<std::pair<size_t, Direction>> clashingHalves(const Candidate &x, const Candidate &y)
{
  const auto halves = [](const Candidate &c) {
    return std::array<std::pair<size_t, Direction>, 2>{{{c.a, c.sideA}, {c.b, c.sideB}}};
  };
  std::vector<std::pair<size_t, Direction>> clashing;
  if (std::min(x.to, y.to) - std::max(x.from, y.from) <= kHair)
    return clashing;
  for (const auto &hx : halves(x)) {
    for (const auto &hy : halves(y)) {
      if (hx == hy)
        clashing.push_back(hx);
    }
  }
  return clashing;
}

double fractionOf(const Rect &box, Direction side, double position)
{
  const auto start = isHorizontalSide(side) ? box.y : box.x;
  const auto length = isHorizontalSide(side) ? box.h : box.w;
  const auto fraction = (position - start) / length;
  return std::clamp(std::round(fraction * 1.0e6) / 1.0e6, 0.0, 1.0);
}

Computer movedTo(const Computer &computer, double originX, double originY)
{
  auto moved = computer;
  moved.originX = originX;
  moved.originY = originY;
  return moved;
}

} // namespace

//
// Computer
//

std::vector<MonitorRect> Computer::distinctMonitors() const
{
  std::vector<MonitorRect> distinct;
  for (const auto &monitor : monitors) {
    if (std::ranges::find(distinct, monitor) == distinct.end())
      distinct.push_back(monitor);
  }
  return distinct;
}

std::vector<Rect> Computer::monitorRects() const
{
  std::vector<Rect> rects;
  for (const auto &m : distinctMonitors())
    rects.push_back({originX + zoom * m.x, originY + zoom * m.y, zoom * m.w, zoom * m.h});
  return rects;
}

Rect Computer::box() const
{
  const auto rects = monitorRects();
  if (rects.empty())
    return {originX, originY, 0, 0};
  double left = rects.front().x;
  double top = rects.front().y;
  double right = rects.front().right();
  double bottom = rects.front().bottom();
  for (const auto &r : rects) {
    left = std::min(left, r.x);
    top = std::min(top, r.y);
    right = std::max(right, r.right());
    bottom = std::max(bottom, r.bottom());
  }
  return {left, top, right - left, bottom - top};
}

MonitorRect Computer::shape() const
{
  const auto distinct = distinctMonitors();
  if (distinct.empty())
    return {};
  int32_t left = distinct.front().x;
  int32_t top = distinct.front().y;
  int32_t right = distinct.front().right();
  int32_t bottom = distinct.front().bottom();
  for (const auto &m : distinct) {
    left = std::min(left, m.x);
    top = std::min(top, m.y);
    right = std::max(right, m.right());
    bottom = std::max(bottom, m.bottom());
  }
  return {left, top, right - left, bottom - top};
}

//
// Arrangement
//

int Arrangement::indexOf(std::string_view name) const
{
  for (size_t i = 0; i < computers.size(); ++i) {
    if (sameName(computers[i].name, name))
      return static_cast<int>(i);
  }
  return -1;
}

int Arrangement::serverIndex() const
{
  for (size_t i = 0; i < computers.size(); ++i) {
    if (computers[i].isServer)
      return static_cast<int>(i);
  }
  return -1;
}

Issue::Severity Issue::severity() const
{
  return kind == Kind::NotConnected || kind == Kind::AmbiguousSide || kind == Kind::EdgeInTheSameRows
             ? Severity::Warning
             : Severity::Error;
}

std::vector<Link> DerivedLinks::of(std::string_view from, Direction side) const
{
  std::vector<Link> found;
  for (const auto &link : links) {
    if (link.side == side && sameName(link.from, from))
      found.push_back(link);
  }
  return found;
}

//
// Validation
//

std::vector<Issue> validate(const Arrangement &arrangement)
{
  using enum Issue::Kind;
  std::vector<Issue> issues;
  const auto &computers = arrangement.computers;

  std::vector<bool> usable(computers.size(), false);
  for (size_t i = 0; i < computers.size(); ++i) {
    const auto &c = computers[i];
    if (c.name.empty()) {
      issues.push_back({NameIsEmpty, c.name, {}, Direction::NoDirection, {}});
      continue;
    }
    for (size_t j = 0; j < i; ++j) {
      if (sameName(computers[j].name, c.name)) {
        issues.push_back({DuplicateName, c.name, computers[j].name, Direction::NoDirection, {}});
        break;
      }
    }
    const auto layout = MonitorLayout::create(c.monitors);
    if (!layout) {
      issues.push_back({InvalidMonitors, c.name, {}, Direction::NoDirection, layout.error});
      continue;
    }
    if (!isNumber(c.zoom) || c.zoom < kMinZoom || c.zoom > kMaxZoom) {
      issues.push_back({InvalidZoom, c.name, {}, Direction::NoDirection, {}});
      continue;
    }
    if (!isNumber(c.originX) || !isNumber(c.originY) || std::abs(c.originX) > kMaxCoordinate ||
        std::abs(c.originY) > kMaxCoordinate) {
      issues.push_back({InvalidPosition, c.name, {}, Direction::NoDirection, {}});
      continue;
    }
    const auto box = c.box();
    if (box.w > kMaxCoordinate || box.h > kMaxCoordinate || std::abs(box.x) > kMaxCoordinate ||
        std::abs(box.y) > kMaxCoordinate || std::abs(box.right()) > kMaxCoordinate ||
        std::abs(box.bottom()) > kMaxCoordinate) {
      issues.push_back({InvalidPosition, c.name, {}, Direction::NoDirection, {}});
      continue;
    }
    usable[i] = true;
  }

  for (size_t i = 0; i < computers.size(); ++i) {
    if (!usable[i])
      continue;
    const auto a = computers[i].monitorRects();
    for (size_t j = i + 1; j < computers.size(); ++j) {
      if (!usable[j])
        continue;
      const auto b = computers[j].monitorRects();
      const bool overlapping = std::ranges::any_of(a, [&](const Rect &ra) {
        return std::ranges::any_of(b, [&](const Rect &rb) { return overlap(ra, rb); });
      });
      if (overlapping)
        issues.push_back({Overlap, computers[i].name, computers[j].name, Direction::NoDirection, {}});
    }
  }
  return issues;
}

//
// Links
//

DerivedLinks deriveLinks(const Arrangement &arrangement)
{
  DerivedLinks result;
  const auto &computers = arrangement.computers;
  const auto count = computers.size();

  std::vector<bool> usable(count, false);
  std::vector<Rect> boxes(count);
  // the exposed edges of each computer, by side
  std::vector<std::array<std::vector<Segment>, 5>> segments(count);
  for (size_t i = 0; i < count; ++i) {
    usable[i] = isUsable(computers[i]);
    if (!usable[i])
      continue;
    boxes[i] = computers[i].box();
    for (const auto &segment : exposedSegments(computers[i]))
      segments[i][static_cast<size_t>(segment.side)].push_back(segment);
  }

  // every place where an exposed edge of one computer touches an exposed edge of another
  std::vector<Candidate> candidates;
  for (size_t a = 0; a < count; ++a) {
    for (size_t b = a + 1; b < count; ++b) {
      if (!usable[a] || !usable[b])
        continue;
      for (const auto sideA : kLinkOrder) {
        const auto sideB = oppositeOf(sideA);
        for (const auto &sa : segments[a][static_cast<size_t>(sideA)]) {
          for (const auto &sb : segments[b][static_cast<size_t>(sideB)]) {
            if (std::abs(sa.line - sb.line) > kTouchTolerance)
              continue;
            const auto from = std::max(sa.from, sb.from);
            const auto to = std::min(sa.to, sb.to);
            if (to - from < kMinLink - kSame)
              continue;
            candidates.push_back(
                {a, sideA, b, sideB, from, to, depthOf(boxes[a], sa) + depthOf(boxes[b], sb), sa.line}
            );
          }
        }
      }
    }
  }

  // The source ranges of a side must not overlap. Where two touching edges are in the same rows of a side (the two
  // arms of a U), the one that is nearer to the edge of the computer is kept, together with its other half.
  std::ranges::stable_sort(candidates, [](const Candidate &x, const Candidate &y) {
    if (std::abs(x.depth - y.depth) > kSame)
      return x.depth < y.depth;
    if (std::abs(x.length() - y.length()) > kSame)
      return x.length() > y.length();
    return std::tie(x.a, x.b) < std::tie(y.a, y.b);
  });
  std::vector<Candidate> accepted;
  for (const auto &candidate : candidates) {
    bool clashed = false;
    for (const auto &kept : accepted) {
      for (const auto &[computer, side] : clashingHalves(candidate, kept)) {
        clashed = true;
        const auto other = computer == candidate.a ? candidate.b : candidate.a;
        const Issue issue{Issue::Kind::AmbiguousSide, computers[computer].name, computers[other].name, side, {}};
        if (std::ranges::find(result.issues, issue) == result.issues.end())
          result.issues.push_back(issue);
      }
    }
    if (!clashed)
      accepted.push_back(candidate);
  }

  // Links of one side that overlap by a hair meet in the middle, so that their intervals in the configuration do not
  // overlap (the core refuses a file in which they do)
  std::map<std::pair<size_t, Direction>, std::vector<size_t>> byHalf;
  for (size_t i = 0; i < accepted.size(); ++i) {
    byHalf[{accepted[i].a, accepted[i].sideA}].push_back(i);
    byHalf[{accepted[i].b, accepted[i].sideB}].push_back(i);
  }
  for (auto &[half, members] : byHalf) {
    std::ranges::sort(members, [&](size_t x, size_t y) { return accepted[x].from < accepted[y].from; });
    for (size_t k = 0; k + 1 < members.size(); ++k) {
      auto &first = accepted[members[k]];
      auto &second = accepted[members[k + 1]];
      if (second.from < first.to) {
        const auto middle = (second.from + first.to) / 2;
        first.to = middle;
        second.from = middle;
      }
    }
  }

  // The pointer leaves through any exposed edge of a side, so a link covers the rows of that side that it is on, also
  // where another edge of that side, that touches nothing, is in them
  for (const auto &c : accepted) {
    const std::array<std::pair<size_t, Direction>, 2> halves = {{{c.a, c.sideA}, {c.b, c.sideB}}};
    for (const auto &[computer, side] : halves) {
      const auto other = computer == c.a ? c.b : c.a;
      for (const auto &segment : segments[computer][static_cast<size_t>(side)]) {
        if (std::abs(segment.line - c.line) <= kTouchTolerance ||
            std::min(segment.to, c.to) - std::max(segment.from, c.from) <= kHair)
          continue;
        const Issue issue{Issue::Kind::EdgeInTheSameRows, computers[computer].name, computers[other].name, side, {}};
        const auto sameSide = [&](const Issue &i) { return i.computer == issue.computer && i.side == side; };
        if (std::ranges::find(result.issues, issue) == result.issues.end() &&
            std::ranges::none_of(result.issues, [&](const Issue &i) {
              return i.kind == Issue::Kind::AmbiguousSide && sameSide(i);
            }))
          result.issues.push_back(issue);
      }
    }
  }

  for (const auto &c : accepted)
    result.contacts.push_back({computers[c.a].name, c.sideA, computers[c.b].name, c.sideB, c.line, c.from, c.to});

  // stretches that continue each other, of the same two computers, are one link
  std::ranges::sort(accepted, [](const Candidate &x, const Candidate &y) {
    return std::tie(x.a, x.sideA, x.b, x.sideB, x.from) < std::tie(y.a, y.sideA, y.b, y.sideB, y.from);
  });
  std::vector<Candidate> merged;
  for (const auto &candidate : accepted) {
    if (!merged.empty() && merged.back().a == candidate.a && merged.back().sideA == candidate.sideA &&
        merged.back().b == candidate.b && merged.back().sideB == candidate.sideB &&
        std::abs(candidate.from - merged.back().to) <= kHair) {
      merged.back().to = candidate.to;
    } else {
      merged.push_back(candidate);
    }
  }

  struct Half
  {
    size_t from;
    Direction side;
    Link link;
  };
  std::vector<Half> halves;
  for (const auto &c : merged) {
    const auto srcA = std::pair{fractionOf(boxes[c.a], c.sideA, c.from), fractionOf(boxes[c.a], c.sideA, c.to)};
    const auto srcB = std::pair{fractionOf(boxes[c.b], c.sideB, c.from), fractionOf(boxes[c.b], c.sideB, c.to)};
    if (srcA.first >= srcA.second || srcB.first >= srcB.second)
      continue;
    halves.push_back(
        {c.a,
         c.sideA,
         {computers[c.a].name, c.sideA, srcA.first, srcA.second, computers[c.b].name, srcB.first, srcB.second}}
    );
    halves.push_back(
        {c.b,
         c.sideB,
         {computers[c.b].name, c.sideB, srcB.first, srcB.second, computers[c.a].name, srcA.first, srcA.second}}
    );
  }
  std::ranges::sort(halves, [](const Half &x, const Half &y) {
    return std::tuple(x.from, sideRank(x.side), x.link.srcStart) <
           std::tuple(y.from, sideRank(y.side), y.link.srcStart);
  });
  for (const auto &half : halves)
    result.links.push_back(half.link);

  // what the links do not reach from the server
  std::vector<bool> reached(count, false);
  int start = arrangement.serverIndex();
  if (start < 0 || !usable[static_cast<size_t>(start)]) {
    start = -1;
    for (size_t i = 0; i < count; ++i) {
      if (usable[i]) {
        start = static_cast<int>(i);
        break;
      }
    }
  }
  if (start >= 0) {
    std::queue<size_t> pending;
    pending.push(static_cast<size_t>(start));
    reached[static_cast<size_t>(start)] = true;
    while (!pending.empty()) {
      const auto at = pending.front();
      pending.pop();
      for (const auto &half : halves) {
        if (half.from != at)
          continue;
        const auto to = static_cast<size_t>(arrangement.indexOf(half.link.to));
        if (!reached[to]) {
          reached[to] = true;
          pending.push(to);
        }
      }
    }
    for (size_t i = 0; i < count; ++i) {
      if (usable[i] && !reached[i])
        result.issues.push_back({Issue::Kind::NotConnected, computers[i].name, {}, Direction::NoDirection, {}});
    }
  }
  return result;
}

std::vector<Issue> check(const Arrangement &arrangement)
{
  auto issues = validate(arrangement);
  const auto derived = deriveLinks(arrangement);
  issues.insert(issues.end(), derived.issues.begin(), derived.issues.end());
  return issues;
}

//
// Snapping
//

namespace {

// where the edges of \p moving are in line with those of \p others, as guides
std::vector<Guide> guidesFor(const std::vector<Rect> &moving, const std::vector<Rect> &others)
{
  std::vector<Guide> guides;
  const auto add = [&guides](bool vertical, double position, double from, double to, bool touching) {
    for (auto &g : guides) {
      if (g.vertical == vertical && g.touching == touching && std::abs(g.position - position) <= kSame) {
        g.from = std::min(g.from, from);
        g.to = std::max(g.to, to);
        return;
      }
    }
    guides.push_back({vertical, position, from, to, touching});
  };

  for (const auto &m : moving) {
    for (const auto &o : others) {
      const double top = std::min(m.y, o.y);
      const double bottom = std::max(m.bottom(), o.bottom());
      const double left = std::min(m.x, o.x);
      const double right = std::max(m.right(), o.right());
      if (std::abs(m.x - o.right()) <= kTouchTolerance)
        add(true, m.x, top, bottom, true);
      if (std::abs(m.right() - o.x) <= kTouchTolerance)
        add(true, m.right(), top, bottom, true);
      if (std::abs(m.x - o.x) <= kTouchTolerance)
        add(true, m.x, top, bottom, false);
      if (std::abs(m.right() - o.right()) <= kTouchTolerance)
        add(true, m.right(), top, bottom, false);
      if (std::abs(m.centerX() - o.centerX()) <= kTouchTolerance)
        add(true, m.centerX(), top, bottom, false);
      if (std::abs(m.y - o.bottom()) <= kTouchTolerance)
        add(false, m.y, left, right, true);
      if (std::abs(m.bottom() - o.y) <= kTouchTolerance)
        add(false, m.bottom(), left, right, true);
      if (std::abs(m.y - o.y) <= kTouchTolerance)
        add(false, m.y, left, right, false);
      if (std::abs(m.bottom() - o.bottom()) <= kTouchTolerance)
        add(false, m.bottom(), left, right, false);
      if (std::abs(m.centerY() - o.centerY()) <= kTouchTolerance)
        add(false, m.centerY(), left, right, false);
    }
  }
  return guides;
}

struct Snap
{
  double delta;
  int priority;
};

// the smallest move along an axis that puts an edge of a monitor in line with an edge of another, within \p radius
std::optional<double> bestSnap(const std::vector<Snap> &snaps, double radius)
{
  const Snap *best = nullptr;
  for (const auto &snap : snaps) {
    if (std::abs(snap.delta) > radius)
      continue;
    if (best == nullptr || std::abs(snap.delta) < std::abs(best->delta) - kSame ||
        (std::abs(std::abs(snap.delta) - std::abs(best->delta)) <= kSame && snap.priority < best->priority))
      best = &snap;
  }
  if (best == nullptr)
    return std::nullopt;
  return best->delta;
}

bool overlapsAny(const std::vector<Rect> &moving, const std::vector<Rect> &others)
{
  return std::ranges::any_of(moving, [&](const Rect &m) {
    return std::ranges::any_of(others, [&](const Rect &o) { return overlap(m, o); });
  });
}

std::vector<Rect> shifted(std::vector<Rect> rects, double dx, double dy)
{
  for (auto &r : rects) {
    r.x += dx;
    r.y += dy;
  }
  return rects;
}

} // namespace

SnapResult snapMove(const Arrangement &arrangement, size_t moving, double originX, double originY, double radius)
{
  SnapResult result;
  const auto &computers = arrangement.computers;
  if (moving >= computers.size() || !isNumber(originX) || !isNumber(originY)) {
    result.originX = moving < computers.size() ? computers[moving].originX : 0;
    result.originY = moving < computers.size() ? computers[moving].originY : 0;
    result.refused = true;
    return result;
  }

  if (!isUsable(computers[moving])) {
    result.originX = isNumber(computers[moving].originX) ? computers[moving].originX : 0;
    result.originY = isNumber(computers[moving].originY) ? computers[moving].originY : 0;
    result.refused = true;
    return result;
  }

  const auto &computer = computers[moving];
  std::vector<Rect> others;
  for (size_t i = 0; i < computers.size(); ++i) {
    if (i == moving || !isUsable(computers[i]))
      continue;
    const auto rects = computers[i].monitorRects();
    others.insert(others.end(), rects.begin(), rects.end());
  }

  const auto at = movedTo(computer, originX, originY).monitorRects();

  std::vector<Snap> xs;
  std::vector<Snap> ys;
  for (const auto &m : at) {
    for (const auto &o : others) {
      const bool nearInY = m.y - radius < o.bottom() && o.y < m.bottom() + radius;
      const bool nearInX = m.x - radius < o.right() && o.x < m.right() + radius;
      if (nearInY) {
        xs.push_back({o.right() - m.x, 0});
        xs.push_back({o.x - m.right(), 0});
      }
      xs.push_back({o.x - m.x, 1});
      xs.push_back({o.right() - m.right(), 1});
      xs.push_back({o.centerX() - m.centerX(), 2});
      if (nearInX) {
        ys.push_back({o.bottom() - m.y, 0});
        ys.push_back({o.y - m.bottom(), 0});
      }
      ys.push_back({o.y - m.y, 1});
      ys.push_back({o.bottom() - m.bottom(), 1});
      ys.push_back({o.centerY() - m.centerY(), 2});
    }
  }
  const double dx = bestSnap(xs, radius).value_or(0.0);
  const double dy = bestSnap(ys, radius).value_or(0.0);
  result.originX = originX + dx;
  result.originY = originY + dy;

  auto final = shifted(at, dx, dy);
  if (overlapsAny(final, others)) {
    // The nearest place that touches what it overlaps and overlaps nothing
    std::optional<std::pair<double, double>> best;
    double bestDistance = std::numeric_limits<double>::max();
    const auto consider = [&](double sx, double sy) {
      if (overlapsAny(shifted(final, sx, sy), others))
        return;
      const auto distance = std::hypot(sx, sy);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = std::pair{sx, sy};
      }
    };
    for (const auto &m : final) {
      for (const auto &o : others) {
        if (!overlap(m, o))
          continue;
        consider(o.x - m.right(), 0);
        consider(o.right() - m.x, 0);
        consider(0, o.y - m.bottom());
        consider(0, o.bottom() - m.y);
      }
    }
    if (!best) {
      // a monitor that was pushed off one may be on another: the edges of every one of them are tried
      for (const auto &m : final) {
        for (const auto &o : others) {
          consider(o.x - m.right(), 0);
          consider(o.right() - m.x, 0);
          consider(0, o.y - m.bottom());
          consider(0, o.bottom() - m.y);
        }
      }
    }
    if (!best) {
      result.originX = computer.originX;
      result.originY = computer.originY;
      result.refused = true;
      result.guides = guidesFor(computer.monitorRects(), others);
      return result;
    }
    result.originX += best->first;
    result.originY += best->second;
    final = shifted(final, best->first, best->second);
    result.pushedOut = true;
  }
  result.guides = guidesFor(final, others);
  return result;
}

//
// The classic grid
//

namespace {

constexpr double kGapBetweenIslands = 100.0;

struct Placement
{
  GridConversion &result;
  std::vector<bool> placed;
};

// the arrangement rectangles of every computer that is placed except \p self
std::vector<Rect> placedRects(const Placement &p, size_t self)
{
  std::vector<Rect> rects;
  for (size_t i = 0; i < p.placed.size(); ++i) {
    if (!p.placed[i] || i == self)
      continue;
    const auto r = p.result.arrangement.computers[i].monitorRects();
    rects.insert(rects.end(), r.begin(), r.end());
  }
  return rects;
}

// puts the computer \p index with the top left of its box at \p x, \p y
void putBoxAt(Placement &p, size_t index, double x, double y)
{
  auto &computer = p.result.arrangement.computers[index];
  const auto shape = computer.shape();
  computer.originX = x - computer.zoom * shape.x;
  computer.originY = y - computer.zoom * shape.y;
  p.placed[index] = true;
}

// Puts \p child on the \p side of \p parent, touching it
void placeNextTo(Placement &p, size_t parent, Direction side, size_t child)
{
  const auto parentBox = p.result.arrangement.computers[parent].box();
  auto &computer = p.result.arrangement.computers[child];
  const auto shape = computer.shape();
  const double w = computer.zoom * shape.w;
  const double h = computer.zoom * shape.h;
  const auto others = placedRects(p, child);
  const bool horizontal = isHorizontalSide(side);

  // where the box of the child is along the shared edge: in the middle, flush with either end of the parent, and
  // beside what is in the way
  const double length = horizontal ? h : w;
  const double parentStart = horizontal ? parentBox.y : parentBox.x;
  const double parentEnd = horizontal ? parentBox.bottom() : parentBox.right();
  std::vector<double> offers = {
      std::round((parentStart + parentEnd) / 2 - length / 2),
      parentStart,
      parentEnd - length,
  };
  for (const auto &o : others) {
    offers.push_back(horizontal ? o.bottom() : o.right());
    offers.push_back((horizontal ? o.y : o.x) - length);
  }
  const double middle = offers.front();

  const auto across = [&](double along) {
    switch (side) {
    case Direction::Right:
      return std::pair{parentBox.right(), along};
    case Direction::Left:
      return std::pair{parentBox.x - w, along};
    case Direction::Bottom:
      return std::pair{along, parentBox.bottom()};
    default:
      return std::pair{along, parentBox.y - h};
    }
  };

  std::optional<std::pair<double, double>> best;
  double bestDistance = std::numeric_limits<double>::max();
  for (const auto offer : offers) {
    const auto overlapOnEdge = std::min(offer + length, parentEnd) - std::max(offer, parentStart);
    if (overlapOnEdge < kMinLink)
      continue;
    const auto [x, y] = across(offer);
    auto trial = movedTo(computer, 0, 0);
    trial.originX = x - computer.zoom * shape.x;
    trial.originY = y - computer.zoom * shape.y;
    if (overlapsAny(trial.monitorRects(), others))
      continue;
    const auto distance = std::abs(offer - middle);
    if (distance < bestDistance - kSame) {
      bestDistance = distance;
      best = std::pair{x, y};
    }
  }
  if (!best) {
    // nothing that touches the parent is free: to the right of everything, which keeps no link
    double right = parentBox.right();
    for (const auto &o : others)
      right = std::max(right, o.right());
    best = std::pair{right + kGapBetweenIslands, parentBox.y};
  }
  putBoxAt(p, child, best->first, best->second);
}

} // namespace

GridConversion arrangementFromGrid(const std::vector<GridCell> &cells, std::string_view serverName)
{
  GridConversion conversion;
  if (cells.empty())
    return conversion;

  // row by row, as the classic grid writes them
  std::vector<size_t> order(cells.size());
  for (size_t i = 0; i < order.size(); ++i)
    order[i] = i;
  std::ranges::stable_sort(order, [&](size_t a, size_t b) {
    return std::tie(cells[a].row, cells[a].column) < std::tie(cells[b].row, cells[b].column);
  });
  std::vector<GridCell> sorted;
  for (const auto i : order)
    sorted.push_back(cells[i]);

  std::map<std::pair<int, int>, size_t> at;
  for (size_t i = 0; i < sorted.size(); ++i) {
    at[{sorted[i].column, sorted[i].row}] = i;
    auto computer = sorted[i].computer;
    computer.originX = 0;
    computer.originY = 0;
    computer.isServer = sameName(computer.name, serverName);
    conversion.arrangement.computers.push_back(computer);
  }
  Placement p{conversion, std::vector<bool>(sorted.size(), false)};

  const auto neighbourOf = [&](size_t index, Direction side) -> std::optional<size_t> {
    const auto &cell = sorted[index];
    int column = cell.column;
    int row = cell.row;
    switch (side) {
    case Direction::Right:
      ++column;
      break;
    case Direction::Left:
      --column;
      break;
    case Direction::Top:
      --row;
      break;
    default:
      ++row;
      break;
    }
    const auto found = at.find({column, row});
    if (found == at.end())
      return std::nullopt;
    return found->second;
  };

  const auto placeComponent = [&](size_t start) {
    std::queue<size_t> pending;
    pending.push(start);
    while (!pending.empty()) {
      const auto parent = pending.front();
      pending.pop();
      for (const auto side : kLinkOrder) {
        const auto child = neighbourOf(parent, side);
        if (!child || p.placed[*child])
          continue;
        placeNextTo(p, parent, side, *child);
        pending.push(*child);
      }
    }
  };

  size_t start = 0;
  for (size_t i = 0; i < sorted.size(); ++i) {
    if (conversion.arrangement.computers[i].isServer) {
      start = i;
      break;
    }
  }
  p.placed[start] = true;
  placeComponent(start);

  // whatever the grid does not connect to the server is below the rest, each group of them in a row of its own
  for (size_t i = 0; i < sorted.size(); ++i) {
    if (p.placed[i])
      continue;
    double left = std::numeric_limits<double>::max();
    double bottom = std::numeric_limits<double>::lowest();
    for (const auto &r : placedRects(p, sorted.size())) {
      left = std::min(left, r.x);
      bottom = std::max(bottom, r.bottom());
    }
    putBoxAt(p, i, left, bottom + kGapBetweenIslands);
    placeComponent(i);
  }

  // what the grid links and the arrangement does not
  const auto derived = deriveLinks(conversion.arrangement);
  for (size_t i = 0; i < sorted.size(); ++i) {
    for (const auto side : kLinkOrder) {
      const auto neighbour = neighbourOf(i, side);
      if (!neighbour)
        continue;
      const auto &to = conversion.arrangement.computers[*neighbour].name;
      const auto links = derived.of(conversion.arrangement.computers[i].name, side);
      if (std::ranges::none_of(links, [&](const Link &l) { return sameName(l.to, to); }))
        conversion.notKept.push_back({conversion.arrangement.computers[i].name, side, 0, 1, to, 0, 1});
    }
  }
  return conversion;
}

//
// Text
//

std::string formatInterval(double start, double end)
{
  if (start <= kSame && end >= 1.0 - kSame)
    return {};
  const auto percent = [](double fraction) {
    auto text = std::format("{:.4f}", std::round(fraction * 1.0e6) / 1.0e4);
    while (text.back() == '0')
      text.pop_back();
    if (text.back() == '.')
      text.pop_back();
    return text;
  };
  return std::format("({},{})", percent(start), percent(end));
}

std::string formatLinks(const Arrangement &arrangement, const DerivedLinks &links)
{
  std::string text = "section: links\n";
  for (const auto &computer : arrangement.computers) {
    if (computer.name.empty())
      continue;
    text += std::format("\t{}:\n", computer.name);
    for (const auto side : kLinkOrder) {
      for (const auto &link : links.of(computer.name, side)) {
        text += std::format(
            "\t\t{}{} = {}{}\n", directionName(side), formatInterval(link.srcStart, link.srcEnd), link.to,
            formatInterval(link.dstStart, link.dstEnd)
        );
      }
    }
  }
  text += "end\n\n";
  return text;
}

const char *directionName(Direction side)
{
  switch (side) {
  case Direction::Left:
    return "left";
  case Direction::Right:
    return "right";
  case Direction::Top:
    return "up";
  case Direction::Bottom:
    return "down";
  default:
    return "";
  }
}

std::optional<double> windowsDisplayScale(std::string_view diagnosticText)
{
  std::optional<double> first;
  size_t position = 0;
  while (position < diagnosticText.size()) {
    auto end = diagnosticText.find('\n', position);
    if (end == std::string_view::npos)
      end = diagnosticText.size();
    const auto line = diagnosticText.substr(position, end - position);
    position = end + 1;

    const auto word = line.find("monitor");
    const auto scale = line.find("scale=");
    if (word == std::string_view::npos || scale == std::string_view::npos || scale < word)
      continue;
    const std::string number(line.substr(scale + 6));
    char *stop = nullptr;
    const auto value = std::strtod(number.c_str(), &stop);
    if (stop == number.c_str() || *stop != '%' || !isNumber(value) || value <= 0)
      continue;
    if (line.find("primary", scale) != std::string_view::npos)
      return value / 100.0;
    if (!first)
      first = value / 100.0;
  }
  return first;
}

double zoomForScale(double percent)
{
  if (!isNumber(percent) || percent < 25 || percent > 500)
    return 1.0;
  return 100.0 / percent;
}

} // namespace deskflow::server::arrangement
