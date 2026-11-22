/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorGeometry.h"

#include <algorithm>
#include <cmath>

namespace deskflow::gui::input {

namespace {

bool validSize(const InputIndicatorSize &size)
{
  return std::isfinite(size.width) && std::isfinite(size.height) && size.width > 0 && size.height > 0;
}

bool validScreen(const InputIndicatorRect &screen)
{
  return std::isfinite(screen.x) && std::isfinite(screen.y) && std::isfinite(screen.width) &&
         std::isfinite(screen.height) && screen.width > 0 && screen.height > 0;
}

bool contains(const InputIndicatorRect &screen, const InputIndicatorPoint &point)
{
  return point.x >= screen.x && point.x <= screen.x + screen.width && point.y >= screen.y &&
         point.y <= screen.y + screen.height;
}

} // namespace

std::optional<InputIndicatorPoint> positionInputIndicator(
    InputIndicatorPoint cursor, InputIndicatorSize badge, const std::vector<InputIndicatorRect> &screens,
    InputIndicatorPosition position, double gap, double margin
)
{
  if (!std::isfinite(cursor.x) || !std::isfinite(cursor.y) || !validSize(badge) || !std::isfinite(gap) ||
      !std::isfinite(margin) || gap < 0 || margin < 0) {
    return std::nullopt;
  }

  const InputIndicatorRect *screen = nullptr;
  for (const auto &candidate : screens) {
    if (validScreen(candidate) && contains(candidate, cursor)) {
      screen = &candidate;
      break;
    }
  }
  if (!screen) {
    return std::nullopt;
  }

  const double minimumX = screen->x + margin;
  const double maximumX = screen->x + screen->width - margin - badge.width;
  const double minimumY = screen->y + margin;
  const double maximumY = screen->y + screen->height - margin - badge.height;
  if (minimumX > maximumX || minimumY > maximumY) {
    return std::nullopt;
  }

  InputIndicatorPoint result;
  const bool onLeft = position == InputIndicatorPosition::CursorBottomLeft ||
                      position == InputIndicatorPosition::CursorTopLeft;
  const bool above = position == InputIndicatorPosition::CursorTopLeft ||
                     position == InputIndicatorPosition::CursorTopRight;
  result.x = cursor.x + (onLeft ? -badge.width - gap : gap);
  result.y = cursor.y + (above ? gap : -badge.height - gap);
  result.x = std::clamp(result.x, minimumX, maximumX);
  result.y = std::clamp(result.y, minimumY, maximumY);
  return result;
}

} // namespace deskflow::gui::input
