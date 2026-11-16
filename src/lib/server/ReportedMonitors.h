/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/MonitorReport.h"
#include "server/MonitorGeometry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deskflow::server {

//! The monitors that a computer reported, and that the server has checked
struct ReportedMonitors
{
  //! The generation of the report, as the computer counted it
  uint16_t generation = 0;
  //! As reported, a mirrored display once, in the order of the report
  std::vector<deskflow::monitors::MonitorEntry> monitors;
  //! The same monitors as a layout; its bounding box is the shape that the computer reported
  MonitorLayout layout;
};

//! What the server makes of a report
struct ReportCheck
{
  //! The report, when it is valid: its monitors are the ones to use
  std::optional<ReportedMonitors> reported;
  //! The computer says it has no monitors to report: what was reported before is not to be used any more
  bool forget = false;
  //! Why the report is not valid, when it is not
  std::string problem;
};

//! Check a report against the shape (DINF) that the same computer reported
/*!
A report is valid when it is of version 1, has 1 to 16 monitors of a size that do not overlap and make one connected
shape, and the bounding box of the shape is exactly \p shape. A report of no monitors is a valid way to say that there
are none. Monitors that are exactly the same rectangle (a mirrored display) are one. What is not valid is never used;
nothing here disconnects anyone.
*/
ReportCheck checkReport(const deskflow::monitors::ReceivedReport &report, const MonitorRect &shape);

} // namespace deskflow::server
