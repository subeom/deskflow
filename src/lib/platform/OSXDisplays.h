/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/MonitorGeometry.h"

#include <vector>

namespace deskflow::platform {

//! The active displays of this Mac, in points
/*!
The coordinates are those of Quartz, the ones the pointer is in: the origin is the top left of the main display, and a
display that is on its left or above it has a negative position. These are the numbers that Deskflow's shape for this
computer is made of. A display that mirrors another has the same rectangle (it is one monitor, for the layout). Empty
when there is no display to ask, which is the case of a process that has no window server.
*/
std::vector<deskflow::server::MonitorRect> osxActiveDisplays();

} // namespace deskflow::platform
