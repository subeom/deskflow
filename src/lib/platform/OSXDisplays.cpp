/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXDisplays.h"

#include <CoreGraphics/CoreGraphics.h>

#include <cmath>

namespace deskflow::platform {

namespace {

// the pixels of the mode of a display for each of its points, in percent
uint16_t displayScalePercent(CGDirectDisplayID display)
{
  CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display);
  if (mode == nullptr)
    return 0;

  const auto points = CGDisplayModeGetWidth(mode);
  const auto pixels = CGDisplayModeGetPixelWidth(mode);
  CGDisplayModeRelease(mode);
  if (points == 0 || pixels == 0)
    return 0;
  return static_cast<uint16_t>(std::lround(static_cast<double>(pixels) * 100.0 / static_cast<double>(points)));
}

} // namespace

std::vector<deskflow::monitors::MonitorInfo> osxActiveDisplayInfo()
{
  CGDisplayCount count = 0;
  if (CGGetActiveDisplayList(0, nullptr, &count) != kCGErrorSuccess || count == 0)
    return {};

  std::vector<CGDirectDisplayID> displays(count);
  if (CGGetActiveDisplayList(count, displays.data(), &count) != kCGErrorSuccess)
    return {};
  displays.resize(count);

  // truncated the way the shape of the computer is, so that the box of the monitors is that shape
  std::vector<deskflow::monitors::MonitorInfo> monitors;
  for (const auto display : displays) {
    const auto bounds = CGDisplayBounds(display);
    monitors.push_back({
        {
            static_cast<int32_t>(bounds.origin.x),
            static_cast<int32_t>(bounds.origin.y),
            static_cast<int32_t>(bounds.size.width),
            static_cast<int32_t>(bounds.size.height),
        },
        displayScalePercent(display),
        CGDisplayIsMain(display) != 0,
    });
  }
  return monitors;
}

std::vector<deskflow::server::MonitorRect> osxActiveDisplays()
{
  std::vector<deskflow::server::MonitorRect> monitors;
  for (const auto &display : osxActiveDisplayInfo())
    monitors.push_back(display.rect);
  return monitors;
}

} // namespace deskflow::platform
