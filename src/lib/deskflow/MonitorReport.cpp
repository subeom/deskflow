/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/MonitorReport.h"

#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"

#include <algorithm>
#include <format>
#include <limits>

namespace deskflow::monitors {

namespace {

void put16(std::vector<uint8_t> &out, uint16_t value)
{
  out.push_back(static_cast<uint8_t>(value >> 8));
  out.push_back(static_cast<uint8_t>(value & 0xff));
}

bool fits16(int64_t value)
{
  return value >= std::numeric_limits<int16_t>::min() && value <= std::numeric_limits<int16_t>::max();
}

} // namespace

std::vector<uint8_t> encodeReport(const MonitorReport &report)
{
  const auto count = static_cast<uint8_t>(std::min<size_t>(report.monitors.size(), 255));

  std::vector<uint8_t> out;
  out.reserve(8 + count * kRecordSize);
  out.insert(out.end(), kMsgDMonitors, kMsgDMonitors + 4);
  out.push_back(kReportVersion);
  put16(out, report.generation);
  out.push_back(count);
  for (size_t i = 0; i < count; ++i) {
    const auto &monitor = report.monitors[i];
    put16(out, static_cast<uint16_t>(monitor.x));
    put16(out, static_cast<uint16_t>(monitor.y));
    put16(out, static_cast<uint16_t>(monitor.w));
    put16(out, static_cast<uint16_t>(monitor.h));
    put16(out, monitor.scalePercent);
    out.push_back(
        static_cast<uint8_t>((monitor.primary ? kFlagPrimary : 0) | (monitor.logicalUnits ? kFlagLogicalUnits : 0))
    );
  }
  return out;
}

namespace {

// reads \p bytes and drops them: what was not understood is not left to be read as the code of a message
void discard(deskflow::IStream *stream, uint32_t bytes)
{
  while (bytes > 0) {
    const auto got = stream->read(nullptr, bytes);
    if (got == 0) {
      return;
    }
    bytes -= got;
  }
}

} // namespace

std::optional<ReceivedReport> readReport(deskflow::IStream *stream, uint32_t bytes)
{
  constexpr uint32_t kHeaderSize = 4;
  if (bytes < kHeaderSize) {
    discard(stream, bytes);
    return std::nullopt;
  }

  ReceivedReport report;
  if (!ProtocolUtil::readf(stream, kMsgDMonitors + 4, &report.version, &report.generation, &report.count)) {
    return std::nullopt;
  }

  // the length is the one of the packet; the count that the header says has to be the number of records that it holds
  const uint32_t records = bytes - kHeaderSize;
  if (records != static_cast<uint32_t>(report.count) * kRecordSize) {
    discard(stream, records);
    return std::nullopt;
  }

  report.monitors.reserve(report.count);
  for (int i = 0; i < report.count; ++i) {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t scale = 0;
    uint8_t flags = 0;
    if (!ProtocolUtil::readf(stream, kMsgDMonitorsRecord, &x, &y, &w, &h, &scale, &flags)) {
      return std::nullopt;
    }
    report.monitors.push_back(
        {static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h), scale,
         (flags & kFlagPrimary) != 0, (flags & kFlagLogicalUnits) != 0}
    );
  }
  return report;
}

std::string formatMonitorEntries(const std::vector<MonitorEntry> &monitors)
{
  std::string text;
  for (const auto &monitor : monitors) {
    if (!text.empty()) {
      text += ';';
    }
    text += std::format("{},{},{},{}", monitor.x, monitor.y, monitor.w, monitor.h);
    if (monitor.scalePercent != 0) {
      text += std::format(" scale={}", monitor.scalePercent);
    }
    if (monitor.primary) {
      text += " primary";
    }
    if (monitor.logicalUnits) {
      text += " logical";
    }
  }
  return text;
}

PreparedMonitors prepareMonitors(const std::vector<MonitorInfo> &monitors, const deskflow::server::MonitorRect &shape)
{
  PreparedMonitors prepared;
  if (monitors.empty()) {
    prepared.problem = "the platform lists no monitors";
    return prepared;
  }

  // a mirrored display is the rectangle of the one that it mirrors: one monitor, which is primary if either is
  std::vector<MonitorInfo> unique;
  for (const auto &monitor : monitors) {
    const auto same = std::ranges::find_if(unique, [&](const auto &u) { return u.rect == monitor.rect; });
    if (same == unique.end()) {
      unique.push_back(monitor);
      continue;
    }
    same->primary = same->primary || monitor.primary;
    same->logicalUnits = same->logicalUnits || monitor.logicalUnits;
    if (same->scalePercent == 0) {
      same->scalePercent = monitor.scalePercent;
    }
    ++prepared.duplicatesRemoved;
  }

  if (unique.size() > kMaxReported) {
    prepared.problem = std::format("{} monitors, a report has at most {}", unique.size(), kMaxReported);
    prepared.duplicatesRemoved = 0;
    return prepared;
  }

  for (const auto &monitor : unique) {
    const auto &r = monitor.rect;
    if (r.w <= 0 || r.h <= 0) {
      prepared.problem = std::format("a monitor has no size ({},{} {}x{})", r.x, r.y, r.w, r.h);
      prepared.duplicatesRemoved = 0;
      return prepared;
    }
    if (!fits16(r.x) || !fits16(r.y) || !fits16(static_cast<int64_t>(r.x) + r.w - 1) ||
        !fits16(static_cast<int64_t>(r.y) + r.h - 1) || !fits16(r.w) || !fits16(r.h)) {
      prepared.problem = std::format(
          "a monitor ({},{} {}x{}) does not fit in 16 bits, as a report and a screen say it", r.x, r.y, r.w, r.h
      );
      prepared.duplicatesRemoved = 0;
      return prepared;
    }
  }

  for (size_t i = 0; i < unique.size(); ++i) {
    for (size_t j = i + 1; j < unique.size(); ++j) {
      const auto &a = unique[i].rect;
      const auto &b = unique[j].rect;
      if (a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom()) {
        prepared.problem =
            std::format("monitors overlap ({},{} {}x{} and {},{} {}x{})", a.x, a.y, a.w, a.h, b.x, b.y, b.w, b.h);
        prepared.duplicatesRemoved = 0;
        return prepared;
      }
    }
  }

  // the shape of the computer is what the server checks them against
  int64_t left = unique.front().rect.x;
  int64_t top = unique.front().rect.y;
  int64_t right = unique.front().rect.right();
  int64_t bottom = unique.front().rect.bottom();
  for (const auto &monitor : unique) {
    left = std::min<int64_t>(left, monitor.rect.x);
    top = std::min<int64_t>(top, monitor.rect.y);
    right = std::max<int64_t>(right, monitor.rect.right());
    bottom = std::max<int64_t>(bottom, monitor.rect.bottom());
  }
  if (left != shape.x || top != shape.y || right - left != shape.w || bottom - top != shape.h) {
    prepared.problem = std::format(
        "the monitors cover {},{} {}x{} but the screen of the computer is {},{} {}x{}", left, top, right - left,
        bottom - top, shape.x, shape.y, shape.w, shape.h
    );
    prepared.duplicatesRemoved = 0;
    return prepared;
  }

  for (const auto &monitor : unique) {
    prepared.monitors.push_back(
        {static_cast<int16_t>(monitor.rect.x), static_cast<int16_t>(monitor.rect.y),
         static_cast<int16_t>(monitor.rect.w), static_cast<int16_t>(monitor.rect.h), monitor.scalePercent,
         monitor.primary, monitor.logicalUnits}
    );
  }
  return prepared;
}

} // namespace deskflow::monitors
