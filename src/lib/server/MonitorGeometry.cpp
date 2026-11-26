/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/MonitorGeometry.h"

#include "server/ServerCursorEdges.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <format>
#include <limits>

namespace deskflow::server {

namespace {

// a computer reports the position and the size of its shape with 16 bits each
constexpr int32_t kShortMin = std::numeric_limits<int16_t>::min();
constexpr int32_t kShortMax = std::numeric_limits<int16_t>::max();
// a number that has more digits than this is out of range, whatever it is
constexpr size_t kMaxDigits = 6;

bool isSpace(char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

std::string_view trim(std::string_view text)
{
  while (!text.empty() && isSpace(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && isSpace(text.back()))
    text.remove_suffix(1);
  return text;
}

std::string_view withoutByteOrderMark(std::string_view text)
{
  constexpr std::string_view bom = "\xEF\xBB\xBF";
  return text.starts_with(bom) ? text.substr(bom.size()) : text;
}

std::vector<std::string_view> split(std::string_view text, char separator)
{
  std::vector<std::string_view> parts;
  size_t start = 0;
  while (true) {
    const auto end = text.find(separator, start);
    if (end == std::string_view::npos) {
      parts.push_back(text.substr(start));
      return parts;
    }
    parts.push_back(text.substr(start, end - start));
    start = end + 1;
  }
}

// whether \p line starts with \p label, which is in lower case, whatever the case of the line is
bool startsWithLabel(std::string_view line, std::string_view label)
{
  if (line.size() < label.size())
    return false;
  for (size_t i = 0; i < label.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(line[i])) != label[i])
      return false;
  }
  return true;
}

// whether \p line starts with the word \p word, which is in lower case
bool startsWithWord(std::string_view line, std::string_view word)
{
  return startsWithLabel(line, word) && (line.size() == word.size() || isSpace(line[word.size()]));
}

bool containsNoCase(std::string_view text, std::string_view what)
{
  const auto found =
      std::ranges::search(text, what, [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; });
  return !found.empty();
}

enum class NumberError
{
  None,
  NotANumber,
  OutOfRange
};

// a whole number of the shape of a computer: an optional minus sign and digits
NumberError parseNumber(std::string_view field, int32_t &value)
{
  field = trim(field);
  const bool negative = field.starts_with('-');
  const auto digits = negative ? field.substr(1) : field;
  if (digits.empty() || !std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; }))
    return NumberError::NotANumber;
  if (digits.size() > kMaxDigits)
    return NumberError::OutOfRange;
  int32_t magnitude = 0;
  for (const char c : digits)
    magnitude = magnitude * 10 + (c - '0');
  value = negative ? -magnitude : magnitude;
  return value < kShortMin || value > kShortMax ? NumberError::OutOfRange : NumberError::None;
}

bool inShape(const MonitorRect &monitor)
{
  return monitor.x >= kShortMin && monitor.x <= kShortMax && monitor.y >= kShortMin && monitor.y <= kShortMax &&
         monitor.w <= kShortMax && monitor.h <= kShortMax;
}

bool overlap(const MonitorRect &a, const MonitorRect &b)
{
  return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

// the monitors share a part of an edge
bool adjoin(const MonitorRect &a, const MonitorRect &b)
{
  const bool besideEachOther =
      (a.right() == b.x || b.right() == a.x) && std::max(a.y, b.y) < std::min(a.bottom(), b.bottom());
  const bool aboveEachOther =
      (a.bottom() == b.y || b.bottom() == a.y) && std::max(a.x, b.x) < std::min(a.right(), b.right());
  return besideEachOther || aboveEachOther;
}

MonitorLayoutResult failure(std::string error, size_t duplicates = 0)
{
  return {std::nullopt, std::move(error), duplicates};
}

// what may follow the numbers on a monitor line of the block to paste: scale=... and primary
bool isKnownMonitorWord(std::string_view word)
{
  return startsWithLabel(word, "scale=") || startsWithWord(word, "primary");
}

// the lines of a text, without the carriage returns of a text that has Windows line ends
std::vector<std::string_view> linesOf(std::string_view text)
{
  auto lines = split(withoutByteOrderMark(text), '\n');
  for (auto &line : lines)
    line = trim(line);
  return lines;
}

bool isMonitorLine(std::string_view line)
{
  return startsWithWord(line, "monitor") && line.size() > 7;
}

} // namespace

MonitorLayoutResult MonitorLayout::create(std::vector<MonitorRect> monitors)
{
  size_t duplicates = 0;
  std::vector<MonitorRect> unique;
  unique.reserve(monitors.size());
  for (const auto &monitor : monitors) {
    // a mirrored display has the bounds of the display that it mirrors
    if (std::ranges::find(unique, monitor) != unique.end()) {
      ++duplicates;
    } else {
      unique.push_back(monitor);
      if (unique.size() > kMaxMonitors)
        break; // refused below: no need to look through the rest of a list that is far too long
    }
  }

  if (unique.empty())
    return failure("no monitor listed", duplicates);
  if (unique.size() > kMaxMonitors)
    return failure(std::format("at most {} monitors are supported", kMaxMonitors), duplicates);

  for (size_t i = 0; i < unique.size(); ++i) {
    const auto &monitor = unique[i];
    if (monitor.w <= 0 || monitor.h <= 0)
      return failure(std::format("monitor {}: the width and the height must be greater than 0", i + 1), duplicates);
    if (!inShape(monitor)) {
      return failure(
          std::format("monitor {}: a number is out of range ({} to {})", i + 1, kShortMin, kShortMax), duplicates
      );
    }
  }

  for (size_t i = 0; i < unique.size(); ++i) {
    for (size_t j = i + 1; j < unique.size(); ++j) {
      if (overlap(unique[i], unique[j]))
        return failure(std::format("monitors {} and {} overlap", i + 1, j + 1), duplicates);
    }
  }

  // the monitors that can be reached from the first one by going over shared parts of an edge
  std::vector<bool> reached(unique.size(), false);
  std::vector<size_t> pending{0};
  reached[0] = true;
  while (!pending.empty()) {
    const auto current = pending.back();
    pending.pop_back();
    for (size_t i = 0; i < unique.size(); ++i) {
      if (!reached[i] && adjoin(unique[current], unique[i])) {
        reached[i] = true;
        pending.push_back(i);
      }
    }
  }
  for (size_t i = 1; i < unique.size(); ++i) {
    if (!reached[i]) {
      return failure(
          std::format("monitor {} is not connected to the others (monitors have to share part of an edge)", i + 1),
          duplicates
      );
    }
  }

  int64_t left = std::numeric_limits<int64_t>::max();
  int64_t top = left;
  int64_t right = std::numeric_limits<int64_t>::min();
  int64_t bottom = right;
  int64_t area = 0;
  for (const auto &monitor : unique) {
    left = std::min<int64_t>(left, monitor.x);
    top = std::min<int64_t>(top, monitor.y);
    right = std::max<int64_t>(right, monitor.right());
    bottom = std::max<int64_t>(bottom, monitor.bottom());
    area += static_cast<int64_t>(monitor.w) * monitor.h;
  }
  if (right - left > kShortMax || bottom - top > kShortMax) {
    return failure(
        std::format(
            "the monitors do not fit in the shape of a computer, which is at most {} pixels wide or high", kShortMax
        ),
        duplicates
    );
  }

  MonitorLayout layout;
  layout.m_monitors = std::move(unique);
  layout.m_box = {
      static_cast<int32_t>(left), static_cast<int32_t>(top), static_cast<int32_t>(right - left),
      static_cast<int32_t>(bottom - top)
  };
  layout.m_plain = area == (right - left) * (bottom - top);

  MonitorLayoutResult result;
  result.layout = std::move(layout);
  result.duplicatesRemoved = duplicates;
  return result;
}

MonitorLayoutResult MonitorLayout::parse(std::string_view text)
{
  std::vector<MonitorRect> monitors;
  std::string error;

  // adds the monitor of \p numbers, which are on the line \p lineNumber; says what is wrong if they are not
  const auto addMonitor = [&](std::string_view numbers, size_t lineNumber) {
    const auto fields = split(numbers, ',');
    std::array<int32_t, 4> values{};
    for (size_t i = 0; i < fields.size() && i < values.size(); ++i) {
      switch (parseNumber(fields[i], values[i])) {
      case NumberError::NotANumber:
        error = std::format("line {}: \"{}\" is not a whole number", lineNumber, trim(fields[i]));
        return false;
      case NumberError::OutOfRange:
        error = std::format(
            "line {}: \"{}\" is out of range ({} to {})", lineNumber, trim(fields[i]), kShortMin, kShortMax
        );
        return false;
      case NumberError::None:
        break;
      }
    }
    if (fields.size() != 4) {
      error = std::format("line {}: expected x,y,w,h (4 numbers), found {}", lineNumber, fields.size());
      return false;
    }
    if (values[2] <= 0 || values[3] <= 0) {
      error = std::format("monitor {}: the width and the height must be greater than 0", monitors.size() + 1);
      return false;
    }
    monitors.push_back({values[0], values[1], values[2], values[3]});
    return true;
  };

  size_t lineNumber = 0;
  for (const auto line : linesOf(text)) {
    ++lineNumber;
    if (line.empty() || startsWithLabel(line, "computer:") || startsWithLabel(line, "space:"))
      continue;

    if (startsWithWord(line, "monitor")) {
      // "monitor x,y,w,h scale=... primary": the numbers go on until a word starts
      const auto rest = trim(line.substr(7));
      const auto firstWord =
          std::ranges::find_if(rest, [](char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; });
      const auto numbers = rest.substr(0, static_cast<size_t>(firstWord - rest.begin()));
      if (trim(numbers).empty())
        return failure(std::format("line {}: a monitor needs x,y,w,h", lineNumber));
      if (!addMonitor(numbers, lineNumber))
        return failure(error);
      for (const auto word : split(rest.substr(numbers.size()), ' ')) {
        if (!trim(word).empty() && !isKnownMonitorWord(trim(word)))
          return failure(std::format("line {}: \"{}\" is not expected after a monitor", lineNumber, trim(word)));
      }
      continue;
    }

    for (const auto item : split(line, ';')) {
      if (!trim(item).empty() && !addMonitor(item, lineNumber))
        return failure(error);
    }
  }

  return create(std::move(monitors));
}

std::string MonitorLayout::extractPasteBlock(std::string_view diagnosticText)
{
  const auto lines = linesOf(diagnosticText);

  std::string block;
  bool hasMonitor = false;
  const auto heading =
      std::ranges::find_if(lines, [](std::string_view line) { return containsNoCase(line, "paste-ready block"); });
  if (heading != lines.end()) {
    // the block goes on until the first blank line
    for (auto line = heading + 1; line != lines.end() && !line->empty(); ++line) {
      block += std::string(*line) + "\n";
      hasMonitor = hasMonitor || isMonitorLine(*line);
    }
  }
  if (hasMonitor)
    return block;

  // no block that has monitors: every monitor line there is
  block.clear();
  for (const auto line : lines) {
    if (isMonitorLine(line))
      block += std::string(line) + "\n";
  }
  return block;
}

std::string MonitorLayout::format() const
{
  std::string text;
  for (const auto &monitor : m_monitors) {
    if (!text.empty())
      text += ';';
    text += std::format("{},{},{},{}", monitor.x, monitor.y, monitor.w, monitor.h);
  }
  return text;
}

const MonitorRect *MonitorLayout::monitorAt(int32_t x, int32_t y) const
{
  const auto found =
      std::ranges::find_if(m_monitors, [x, y](const MonitorRect &monitor) { return monitor.contains(x, y); });
  return found == m_monitors.end() ? nullptr : &*found;
}

bool MonitorLayout::contains(int32_t x, int32_t y) const
{
  return monitorAt(x, y) != nullptr;
}

MonitorPoint MonitorLayout::nearest(int32_t x, int32_t y) const
{
  MonitorPoint best{x, y};
  int64_t bestDistance = std::numeric_limits<int64_t>::max();
  for (const auto &monitor : m_monitors) {
    const MonitorPoint clamped{
        std::clamp(x, monitor.x, monitor.right() - 1), std::clamp(y, monitor.y, monitor.bottom() - 1)
    };
    const int64_t dx = clamped.x - static_cast<int64_t>(x);
    const int64_t dy = clamped.y - static_cast<int64_t>(y);
    const int64_t distance = dx * dx + dy * dy;
    if (distance < bestDistance) {
      bestDistance = distance;
      best = clamped;
    }
  }
  return best;
}

float MonitorLayout::edgeFraction(Direction side, int32_t line) const
{
  const bool alongY = side == Direction::Left || side == Direction::Right;
  return getCursorEdgeFraction(line, alongY ? m_box.y : m_box.x, alongY ? m_box.h : m_box.w);
}

MonitorPoint MonitorLayout::entryPoint(Direction side, float fraction, int32_t depth) const
{
  const bool alongY = side == Direction::Left || side == Direction::Right;
  const int32_t origin = alongY ? m_box.y : m_box.x;
  const int32_t extent = alongY ? m_box.h : m_box.w;

  // the same arithmetic as Server::mapToPixel(), so that a rectangle is mapped as it always was
  const int32_t offset = std::clamp(static_cast<int32_t>(fraction * extent), 0, extent - 1);
  const int32_t line = origin + offset;

  // the outermost pixel of the row or column
  std::optional<int32_t> edge;
  for (const auto &monitor : m_monitors) {
    const bool onTheLine =
        alongY ? (line >= monitor.y && line < monitor.bottom()) : (line >= monitor.x && line < monitor.right());
    if (!onTheLine)
      continue;
    switch (side) {
      using enum Direction;
    case Left:
      edge = edge ? std::min(*edge, monitor.x) : monitor.x;
      break;
    case Right:
      edge = edge ? std::max(*edge, monitor.right() - 1) : monitor.right() - 1;
      break;
    case Top:
      edge = edge ? std::min(*edge, monitor.y) : monitor.y;
      break;
    default:
      edge = edge ? std::max(*edge, monitor.bottom() - 1) : monitor.bottom() - 1;
      break;
    }
  }
  if (!edge) {
    // a row of the box that has no monitor cannot be, the monitors are connected
    return alongY ? nearest(m_box.x, line) : nearest(line, m_box.y);
  }

  MonitorPoint at = alongY ? MonitorPoint{*edge, line} : MonitorPoint{line, *edge};
  if (depth > 0) {
    const bool fromTheStart = side == Direction::Left || side == Direction::Top;
    slide(at, fromTheStart ? depth : -depth, alongY);
  }
  return at;
}

int32_t MonitorLayout::slide(MonitorPoint &at, int32_t delta, bool horizontal) const
{
  if (delta == 0)
    return 0;
  const int32_t step = delta > 0 ? 1 : -1;
  int32_t remaining = std::abs(delta);
  int32_t &coordinate = horizontal ? at.x : at.y;

  while (remaining > 0) {
    const auto *monitor = monitorAt(at.x, at.y);
    if (monitor == nullptr)
      break;
    const int32_t limit =
        horizontal ? (step > 0 ? monitor->right() - 1 : monitor->x) : (step > 0 ? monitor->bottom() - 1 : monitor->y);
    const int32_t travel = std::min(std::abs(limit - coordinate), remaining);
    coordinate += step * travel;
    remaining -= travel;
    if (remaining == 0)
      break;

    // at the edge of this monitor: on to the one that has the next pixel, if there is one
    const int32_t nextX = horizontal ? at.x + step : at.x;
    const int32_t nextY = horizontal ? at.y : at.y + step;
    if (!contains(nextX, nextY))
      break;
    at = {nextX, nextY};
    --remaining;
  }
  return step * remaining;
}

MonitorMove MonitorLayout::move(MonitorPoint from, int32_t dx, int32_t dy) const
{
  MonitorPoint at = contains(from.x, from.y) ? from : nearest(from.x, from.y);
  MonitorMove moved;
  // each way in turn, as far as the monitors go: a pointer that is pushed against an edge goes along it
  moved.restX = slide(at, dx, true);
  moved.restY = slide(at, dy, false);
  moved.position = at;
  return moved;
}

} // namespace deskflow::server
