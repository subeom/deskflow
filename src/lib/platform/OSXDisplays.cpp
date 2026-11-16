/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXDisplays.h"

#include <CoreGraphics/CoreGraphics.h>

namespace deskflow::platform {

std::vector<deskflow::server::MonitorRect> osxActiveDisplays()
{
  CGDisplayCount count = 0;
  if (CGGetActiveDisplayList(0, nullptr, &count) != kCGErrorSuccess || count == 0)
    return {};

  std::vector<CGDirectDisplayID> displays(count);
  if (CGGetActiveDisplayList(count, displays.data(), &count) != kCGErrorSuccess)
    return {};
  displays.resize(count);

  // truncated the way the shape of the computer is, so that the box of the monitors is that shape
  std::vector<deskflow::server::MonitorRect> monitors;
  for (const auto display : displays) {
    const auto bounds = CGDisplayBounds(display);
    monitors.push_back({
        static_cast<int32_t>(bounds.origin.x),
        static_cast<int32_t>(bounds.origin.y),
        static_cast<int32_t>(bounds.size.width),
        static_cast<int32_t>(bounds.size.height),
    });
  }
  return monitors;
}

} // namespace deskflow::platform
