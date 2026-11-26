/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXDisplays.h"

#include <CoreGraphics/CoreGraphics.h>
#include <ApplicationServices/ApplicationServices.h>

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
        // Quartz says points: the scale of a Retina display is how sharp it is, not how big
        true,
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

std::vector<OsxDisplayDetail> osxOnlineDisplayDetails()
{
  CGDisplayCount count = 0;
  if (CGGetOnlineDisplayList(0, nullptr, &count) != kCGErrorSuccess || count == 0)
    return {};

  std::vector<CGDirectDisplayID> displays(count);
  if (CGGetOnlineDisplayList(count, displays.data(), &count) != kCGErrorSuccess)
    return {};
  displays.resize(count);

  std::vector<OsxDisplayDetail> details;
  for (const auto display : displays) {
    OsxDisplayDetail detail;
    // truncated the way the shape of the computer is, as the rectangles of the active displays are
    const auto bounds = CGDisplayBounds(display);
    detail.info = {
        {
            static_cast<int32_t>(bounds.origin.x),
            static_cast<int32_t>(bounds.origin.y),
            static_cast<int32_t>(bounds.size.width),
            static_cast<int32_t>(bounds.size.height),
        },
        displayScalePercent(display),
        CGDisplayIsMain(display) != 0,
        // Quartz says points: the scale of a Retina display is how sharp it is, not how big
        true,
    };
    detail.vendor = CGDisplayVendorNumber(display);
    detail.model = CGDisplayModelNumber(display);
    detail.serial = CGDisplaySerialNumber(display);
    // the system says "unknown" for a number that it does not know: the vendor 'unkn', the model 0xFFFFFFFF
    constexpr uint32_t kUnknownVendor = 0x756e6b6e;
    constexpr uint32_t kUnknownModel = 0xFFFFFFFF;
    if (detail.vendor == kUnknownVendor || detail.vendor == kUnknownModel)
      detail.vendor = 0;
    if (detail.model == kUnknownModel)
      detail.model = 0;
    detail.builtIn = CGDisplayIsBuiltin(display) != 0;

    if (const CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(display)) {
      if (const CFStringRef text = CFUUIDCreateString(kCFAllocatorDefault, uuid)) {
        char buffer[64] = {};
        if (CFStringGetCString(text, buffer, sizeof(buffer), kCFStringEncodingUTF8))
          detail.uuid = buffer;
        CFRelease(text);
      }
      CFRelease(uuid);
    }
    details.push_back(detail);
  }
  return details;
}

} // namespace deskflow::platform
