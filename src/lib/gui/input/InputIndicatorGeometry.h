/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <optional>
#include <vector>

namespace deskflow::gui::input {

enum class InputIndicatorPosition
{
  CursorBottomRight,
  CursorTopRight,
  CursorBottomLeft,
  CursorTopLeft
};

struct InputIndicatorPoint
{
  double x = 0;
  double y = 0;
};

struct InputIndicatorSize
{
  double width = 0;
  double height = 0;
};

struct InputIndicatorRect
{
  double x = 0;
  double y = 0;
  double width = 0;
  double height = 0;
};

std::optional<InputIndicatorPoint> positionInputIndicator(
    InputIndicatorPoint cursor, InputIndicatorSize badge, const std::vector<InputIndicatorRect> &screens,
    InputIndicatorPosition position, double gap, double margin
);

std::optional<InputIndicatorPoint> followInputIndicator(
    InputIndicatorPoint previous, InputIndicatorPoint target,
    double catchUpFactor, double maximumLag
);

std::optional<InputIndicatorPoint> inputIndicatorMotionOffset(
    InputIndicatorPoint previousTarget, InputIndicatorPoint target,
    double maximumLength, bool reducedMotion
);

double inputIndicatorTransitionProgress(
    double elapsedMilliseconds, double durationMilliseconds,
    bool reducedMotion
);

} // namespace deskflow::gui::input
