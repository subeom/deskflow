/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ServerCursorEdges.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace deskflow::server {

float getCursorEdgeFraction(int32_t cursorPosition, int32_t computerStart, int32_t computerSize)
{
  assert(computerSize > 0);

  const auto fraction = (static_cast<float>(cursorPosition) - static_cast<float>(computerStart) + 0.5f) /
                        static_cast<float>(computerSize);
  return std::clamp(fraction, 0.0f, std::nextafter(1.0f, 0.0f));
}

} // namespace deskflow::server
