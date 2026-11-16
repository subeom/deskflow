/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/config/Computer.h"
#include "gui/core/ReportedMonitors.h"
#include "server/MonitorEditor.h"

#include <QRect>

#include <optional>

namespace deskflow::gui {

/*!
 * The drawing of the monitors of \p computer for the monitor editor: its monitor layout, or else its size, or else the
 * monitors that it \p reportedMonitors (at the scales that it said), or else the screen that it \p reported, or else a
 * screen of 1920 x 1080; at the place of the arrangement where the origin of the computer is, \p originX, \p originY.
 */
server::editor::Layout drawingOf(
    const ::Computer &computer, double originX, double originY, const std::optional<QRect> &reported,
    const std::optional<ReportedMonitors> &reportedMonitors = std::nullopt
);

//! Whether the user has a monitor layout for \p computer (typed, imported or drawn) that is a layout
bool hasTypedMonitors(const ::Computer &computer);

/*!
 * How many units of the arrangement a unit of \p computer is. The arrangement is in logical pixels, so the physical
 * pixels of a Windows PC that reports its monitors at 150 % are 100 / 150 of a unit, unless the user scaled the
 * computer (a zoom that is exactly 1 is the one that nobody set) or drew its monitors himself.
 */
double effectiveZoom(const ::Computer &computer, const std::optional<ReportedMonitors> &reportedMonitors);

//! What the editor made becomes the monitors, the zoom, the scales and the place of \p computer
void apply(::Computer &computer, const server::editor::Realised &made);

inline std::optional<server::MonitorRect> toMonitorRect(const std::optional<QRect> &rect)
{
  if (!rect)
    return std::nullopt;
  return server::MonitorRect{rect->x(), rect->y(), rect->width(), rect->height()};
}

} // namespace deskflow::gui
