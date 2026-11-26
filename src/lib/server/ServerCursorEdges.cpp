/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ServerCursorEdges.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace deskflow::server {

float clampCursorEdgeFraction(float position)
{
  return clampCursorEdgeFraction(position, 0.0f, 1.0f);
}

float clampCursorEdgeFraction(float position, float start, float end)
{
  assert(start < end);
  return std::clamp(position, start, std::nextafter(end, start));
}

float getCursorEdgeFraction(int32_t cursorPosition, int32_t computerStart, int32_t computerSize)
{
  assert(computerSize > 0);

  const auto fraction = (static_cast<float>(cursorPosition) - static_cast<float>(computerStart) + 0.5f) /
                        static_cast<float>(computerSize);
  return clampCursorEdgeFraction(fraction);
}

} // namespace deskflow::server
