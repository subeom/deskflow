/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/MonitorReport.h"

#include <cstdint>
#include <vector>

namespace deskflow::monitors {

//! A monitor as an operating system lists it: plain numbers, so that the conversion is the same code everywhere
/*!
This is what Windows says (\c EnumDisplayMonitors, \c GetMonitorInfo and \c GetDpiForMonitor): the rectangle of the
monitor in pixels of the virtual desktop, whose right and bottom are the first pixel outside it, the effective DPI,
and whether it is the primary monitor.
*/
struct NativeMonitor
{
  int32_t left = 0;
  int32_t top = 0;
  int32_t right = 0;
  int32_t bottom = 0;
  //! The effective DPI of the monitor, which is 96 at a scale of 100 %; 0 when it is not known
  uint32_t dpi = 0;
  bool primary = false;
};

//! The display scale in percent that a DPI is: 96 is 100, 120 is 125, 144 is 150, 192 is 200; 0 for an unknown DPI
uint16_t scalePercentFromDpi(uint32_t dpi);

//! The monitors that a computer reports, from what the system lists, in the order that it lists them
/*!
The rectangle is made of the corners, not copied: a monitor that has no size or is upside down is left as it is and
is for \c prepareMonitors() to refuse, so that nothing is hidden here. Sizes and positions that do not fit in 32 bits
are not a thing that a screen has.
*/
std::vector<MonitorInfo> monitorsFromNative(const std::vector<NativeMonitor> &native);

} // namespace deskflow::monitors
