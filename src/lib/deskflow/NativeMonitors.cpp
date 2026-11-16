/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/NativeMonitors.h"

#include <algorithm>
#include <limits>

namespace deskflow::monitors {

uint16_t scalePercentFromDpi(uint32_t dpi)
{
  // Windows says a scale of 100 % at 96 DPI; the rounding is that of a DPI that is not one of the steps
  const uint64_t percent = (static_cast<uint64_t>(dpi) * 100 + 48) / 96;
  return static_cast<uint16_t>(std::min<uint64_t>(percent, std::numeric_limits<uint16_t>::max()));
}

std::vector<MonitorInfo> monitorsFromNative(const std::vector<NativeMonitor> &native)
{
  std::vector<MonitorInfo> monitors;
  monitors.reserve(native.size());
  for (const auto &monitor : native) {
    monitors.push_back({
        {monitor.left, monitor.top, monitor.right - monitor.left, monitor.bottom - monitor.top},
        scalePercentFromDpi(monitor.dpi),
        monitor.primary,
    });
  }
  return monitors;
}

} // namespace deskflow::monitors
