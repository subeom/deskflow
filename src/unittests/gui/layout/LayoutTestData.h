/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/layout/DisplaySignature.h"

#include <vector>

// Made-up displays for the tests of the layout profiles: no real names, serials or ids

namespace layout_test {

using deskflow::gui::layout::DisplayInfo;
using deskflow::gui::layout::DisplaySignature;

// a laptop screen, 1512 x 982 points on a Retina display
inline DisplayInfo builtIn(int x = 0, int y = 0, bool primary = false)
{
  DisplayInfo d;
  d.uuid = QStringLiteral("00000000-0000-4000-8000-0000000000A1");
  d.vendor = 0x0610;
  d.model = 0xA050;
  d.builtIn = true;
  d.primary = primary;
  d.x = x;
  d.y = y;
  d.w = 1512;
  d.h = 982;
  d.scalePercent = 200;
  d.label = QStringLiteral("Built-in");
  return d;
}

// a 34 inch ultrawide, 3440 x 1440 points
inline DisplayInfo ultrawide(int x = 0, int y = 0, bool primary = true)
{
  DisplayInfo d;
  d.uuid = QStringLiteral("00000000-0000-4000-8000-0000000000B2");
  d.vendor = 0x10AC;
  d.model = 0xA0C4;
  d.serial = 700001;
  d.primary = primary;
  d.x = x;
  d.y = y;
  d.w = 3440;
  d.h = 1440;
  d.scalePercent = 100;
  d.label = QStringLiteral("External");
  return d;
}

// another monitor with the same resolution and scale as the ultrawide
inline DisplayInfo otherUltrawide(int x = 0, int y = 0, bool primary = true)
{
  auto d = ultrawide(x, y, primary);
  d.uuid = QStringLiteral("00000000-0000-4000-8000-0000000000B3");
  d.serial = 700002;
  return d;
}

// a 24 inch monitor, 1920 x 1080 points
inline DisplayInfo fullHd(int x, int y, bool primary = false, const QString &uuid = QStringLiteral("00000000-0000-4000-8000-0000000000C4"))
{
  DisplayInfo d;
  d.uuid = uuid;
  d.vendor = 0x04CA;
  d.model = 0x1001;
  d.serial = 810001;
  d.primary = primary;
  d.x = x;
  d.y = y;
  d.w = 1920;
  d.h = 1080;
  d.scalePercent = 100;
  return d;
}

// a monitor that tells nothing about itself
inline DisplayInfo anonymous(int x, int y, int w, int h, bool primary = false)
{
  DisplayInfo d;
  d.primary = primary;
  d.x = x;
  d.y = y;
  d.w = w;
  d.h = h;
  d.scalePercent = 100;
  return d;
}

// the T of the user: the ultrawide is the main display, the laptop screen is below it
inline std::vector<DisplayInfo> tShape()
{
  return {ultrawide(0, 0, true), builtIn(964, 1440, false)};
}

// the laptop alone: it is the main display
inline std::vector<DisplayInfo> laptopOnly()
{
  return {builtIn(0, 0, true)};
}

inline DisplaySignature sig(const std::vector<DisplayInfo> &displays)
{
  return DisplaySignature::fromDisplays(displays);
}

} // namespace layout_test
