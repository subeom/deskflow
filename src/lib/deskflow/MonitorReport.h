/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/MonitorGeometry.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deskflow {
class IStream;
}

//! The monitors that a computer reports to the server (DMON), see docs/dev/monitor-reporting.md
namespace deskflow::monitors {

inline constexpr uint8_t kReportVersion = 1;
//! What a valid report holds; the same as a MonitorLayout can have
inline constexpr size_t kMaxReported = 16;
//! The bytes of one monitor on the wire
inline constexpr size_t kRecordSize = 11;
//! The flags of a monitor on the wire: it is the primary monitor of its computer
inline constexpr uint8_t kFlagPrimary = 1u << 0;
//! The coordinates of the monitor are in logical units (the points of macOS), not in physical pixels (Windows): its
//! display scale does not change how big it is in them
inline constexpr uint8_t kFlagLogicalUnits = 1u << 1;

//! A monitor as a platform tells it: in the coordinates of the shape that the computer reports in DINF
struct MonitorInfo
{
  deskflow::server::MonitorRect rect;
  //! The display scale in percent (100, 150, 200 ...), 0 when it is not known
  uint16_t scalePercent = 0;
  bool primary = false;
  //! The rectangle is in logical units, so the scale says how sharp the monitor is and not how big it is
  bool logicalUnits = false;

  bool operator==(const MonitorInfo &) const = default;
};

//! A monitor as it is on the wire: all of it in 16 bits
struct MonitorEntry
{
  int16_t x = 0;
  int16_t y = 0;
  int16_t w = 0;
  int16_t h = 0;
  uint16_t scalePercent = 0;
  bool primary = false;
  bool logicalUnits = false;

  deskflow::server::MonitorRect rect() const
  {
    return {x, y, w, h};
  }
  bool operator==(const MonitorEntry &) const = default;
};

struct MonitorReport
{
  //! Counts the reports of one connection, from 1, wrapping at 65 535
  uint16_t generation = 0;
  std::vector<MonitorEntry> monitors;
};

//! The bytes of a DMON message of \p report, its code first: all of it, to be written at once
/*!
At most 255 monitors are written (the count is one byte); a valid report has at most \c kMaxReported.
*/
std::vector<uint8_t> encodeReport(const MonitorReport &report);

//! What a DMON message said, before anyone has looked at it
struct ReceivedReport
{
  uint8_t version = 0;
  uint16_t generation = 0;
  //! The count that the header said, which is also how many monitors were read
  uint8_t count = 0;
  std::vector<MonitorEntry> monitors;
};

//! Read what follows the code of a DMON message
/*!
The size of the message is the one that its header says (the count of monitors of \c kRecordSize bytes each), whatever
the values in it are: a report that is not valid is read in full as well, so the stream is where the sender left it.
Empty when the stream ended in the middle of the message.
*/
std::optional<ReceivedReport> readReport(deskflow::IStream *stream);

//! The monitors as the server writes them in its log: \c x,y,w,h, then \c scale=S unless it is not known, \c primary
//! and
//! \c logical
/*!
The items are joined by \c ; and have no space after it: \c "0,0,2560,1440 scale=150 primary;2560,0,1920,1080
scale=100". This is the text that the GUI reads from the log of the core.
*/
std::string formatMonitorEntries(const std::vector<MonitorEntry> &monitors);

//! What a computer can report of its monitors
struct PreparedMonitors
{
  std::vector<MonitorEntry> monitors;
  //! Why the monitors cannot be reported, when they cannot; empty when they can
  std::string problem;
  //! How many monitors were the same as another (a mirrored display) and left out
  size_t duplicatesRemoved = 0;

  bool ok() const
  {
    return problem.empty();
  }
};

//! The report of \p monitors, which must tile exactly \p shape, the shape that the computer reports in DINF
/*!
Monitors that are exactly the same rectangle are one monitor. A report has at most \c kMaxReported monitors of a size
that do not overlap, that fit in 16 bits, and whose bounding box is \p shape. When they cannot be made to, there is no
report and \c problem says why.
*/
PreparedMonitors prepareMonitors(const std::vector<MonitorInfo> &monitors, const deskflow::server::MonitorRect &shape);

} // namespace deskflow::monitors
