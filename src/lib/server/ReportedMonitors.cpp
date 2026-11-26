/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ReportedMonitors.h"

#include <algorithm>
#include <format>

namespace deskflow::server {

ReportCheck checkReport(const deskflow::monitors::ReceivedReport &report, const MonitorRect &shape)
{
  using deskflow::monitors::kMaxReported;

  ReportCheck check;
  if (report.version != deskflow::monitors::kReportVersion) {
    check.problem = std::format("version {} of the report is not known", report.version);
    return check;
  }
  if (report.count == 0) {
    check.forget = true;
    return check;
  }
  if (report.monitors.size() > kMaxReported) {
    check.problem = std::format("{} monitors, a report has at most {}", report.monitors.size(), kMaxReported);
    return check;
  }

  // a mirrored display is the rectangle of another one: one monitor, primary if either is
  std::vector<deskflow::monitors::MonitorEntry> entries;
  std::vector<MonitorRect> rects;
  for (const auto &monitor : report.monitors) {
    if (monitor.w <= 0 || monitor.h <= 0) {
      check.problem = std::format("a monitor has no size ({},{} {}x{})", monitor.x, monitor.y, monitor.w, monitor.h);
      return check;
    }
    const auto rect = monitor.rect();
    const auto same = std::ranges::find(rects, rect);
    if (same == rects.end()) {
      rects.push_back(rect);
      entries.push_back(monitor);
    } else {
      auto &kept = entries[static_cast<size_t>(same - rects.begin())];
      kept.primary = kept.primary || monitor.primary;
      kept.logicalUnits = kept.logicalUnits || monitor.logicalUnits;
    }
  }

  auto made = MonitorLayout::create(rects);
  if (!made) {
    check.problem = made.error;
    return check;
  }

  const auto box = made.layout->boundingBox();
  if (box.x != shape.x || box.y != shape.y || box.w != shape.w || box.h != shape.h) {
    check.problem = std::format(
        "the monitors cover {},{} {}x{} but the screen that the computer reported is {},{} {}x{}", box.x, box.y, box.w,
        box.h, shape.x, shape.y, shape.w, shape.h
    );
    return check;
  }

  check.reported = ReportedMonitors{report.generation, std::move(entries), *made.layout};
  return check;
}

std::string loggableName(std::string_view name)
{
  std::string clean(name);
  for (auto &c : clean) {
    if (c == '"' || static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
      c = '?';
    }
  }
  return clean;
}

} // namespace deskflow::server
