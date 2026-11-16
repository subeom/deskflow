/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorEditing.h"

#include "server/Arrangement.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace deskflow::gui {

bool hasTypedMonitors(const ::Computer &computer)
{
  return !computer.monitorLayout().isEmpty() && server::MonitorLayout::parse(computer.monitorLayout().toStdString());
}

double effectiveZoom(const ::Computer &computer, const std::optional<ReportedMonitors> &reportedMonitors)
{
  if (!reportedMonitors || computer.isServer() || hasTypedMonitors(computer) || computer.arrangementZoom() != 1.0)
    return computer.arrangementZoom();
  const auto percent = reportedMonitors->primaryScalePercent();
  if (percent <= 0)
    return computer.arrangementZoom();
  return std::clamp(100.0 / percent, server::arrangement::kMinZoom, server::arrangement::kMaxZoom);
}

server::editor::Layout drawingOf(
    const ::Computer &computer, double originX, double originY, const std::optional<QRect> &reported,
    const std::optional<ReportedMonitors> &reportedMonitors
)
{
  using server::MonitorRect;
  std::vector<MonitorRect> rects;
  std::vector<double> scales(computer.monitorScales().begin(), computer.monitorScales().end());
  if (!computer.monitorLayout().isEmpty()) {
    if (const auto parsed = server::MonitorLayout::parse(computer.monitorLayout().toStdString()); parsed)
      rects = parsed.layout->monitors();
  }
  // what the user did not say, the computer did: its monitors at the scales that it said
  if (rects.empty() && reportedMonitors && !computer.isServer()) {
    rects = reportedMonitors->rects();
    scales.clear();
    for (const auto &monitor : reportedMonitors->monitors)
      scales.push_back(monitor.scalePercent > 0 ? monitor.scalePercent : std::nan(""));
  }
  if (rects.empty()) {
    if (computer.arrangementWidth() > 0 && computer.arrangementHeight() > 0)
      rects = {{0, 0, computer.arrangementWidth(), computer.arrangementHeight()}};
    else if (reported && reported->width() > 0 && reported->height() > 0)
      rects = {{reported->x(), reported->y(), reported->width(), reported->height()}};
    else
      rects = {{0, 0, 1920, 1080}};
  }
  return server::editor::fromStored(rects, effectiveZoom(computer, reportedMonitors), originX, originY, scales);
}

void apply(::Computer &computer, const server::editor::Realised &made)
{
  computer.setMonitorLayout(QString::fromStdString(made.text));
  computer.setMonitorScales(QList<double>(made.scales.begin(), made.scales.end()));
  computer.setArrangementZoom(made.zoom);
  computer.setArrangementOrigin(made.originX, made.originY);
}

} // namespace deskflow::gui
