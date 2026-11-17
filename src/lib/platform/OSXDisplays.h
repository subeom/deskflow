/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/MonitorReport.h"
#include "server/MonitorGeometry.h"

#include <vector>
#include <string>
#include <cstdint>

namespace deskflow::platform {

//! The active displays of this Mac with what a client reports of them
/*!
The rectangles are those of \c osxActiveDisplays(). The scale is the number of pixels of a display for each of its
points, in percent (200 on a Retina display, 100 on a display of one pixel to the point), from the mode that the display
is in; it is 0 when the system does not tell the mode. The main display is the primary one.
*/
std::vector<deskflow::monitors::MonitorInfo> osxActiveDisplayInfo();

//! The active displays of this Mac, in points
/*!
The coordinates are those of Quartz, the ones the pointer is in: the origin is the top left of the main display, and a
display that is on its left or above it has a negative position. These are the numbers that Deskflow's shape for this
computer is made of. A display that mirrors another has the same rectangle (it is one monitor, for the layout). Empty
when there is no display to ask, which is the case of a process that has no window server.
*/
std::vector<deskflow::server::MonitorRect> osxActiveDisplays();

//! A display of this Mac with what the system says it is (read only; used to remember layouts by display situation)
struct OsxDisplayDetail
{
  //! The rectangle, the scale and whether it is the main display, as \c osxActiveDisplayInfo() says them
  deskflow::monitors::MonitorInfo info;
  //! The maker, the model and the serial number of the monitor, 0 when the system does not say
  uint32_t vendor = 0;
  uint32_t model = 0;
  uint32_t serial = 0;
  //! The UUID that the system keeps for the display; empty when it does not have one
  std::string uuid;
  bool builtIn = false;
};

//! The displays of this Mac that are online, with what identifies each
/*!
Not only the active ones: a display that is asleep is online but not active, and the situation of the displays must not
change when the screens go to sleep. A display that mirrors another is online with the rectangle of the one it mirrors.
Nothing is changed: the displays are only asked about. Empty when there is no display to ask.
*/
std::vector<OsxDisplayDetail> osxOnlineDisplayDetails();

} // namespace deskflow::platform
