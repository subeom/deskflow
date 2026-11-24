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

std::optional<InputIndicatorPoint> followInputIndicator(
    InputIndicatorPoint previous, InputIndicatorPoint target,
    double catchUpFactor, double maximumLag
)
{
  if (!std::isfinite(previous.x) || !std::isfinite(previous.y) ||
      !std::isfinite(target.x) || !std::isfinite(target.y) ||
      !std::isfinite(catchUpFactor) || catchUpFactor <= 0 ||
      catchUpFactor > 1 || !std::isfinite(maximumLag) ||
      maximumLag < 0) {
    return std::nullopt;
  }

  const auto deltaX = target.x - previous.x;
  const auto deltaY = target.y - previous.y;
  const auto distance = std::hypot(deltaX, deltaY);
  if (!std::isfinite(distance)) {
    return std::nullopt;
  }
  if (distance == 0) {
    return target;
  }

  const InputIndicatorPoint eased{
      .x = previous.x + deltaX * catchUpFactor,
      .y = previous.y + deltaY * catchUpFactor,
  };
  const auto remaining =
      distance * (1.0 - catchUpFactor);
  if (remaining <= maximumLag) {
    return eased;
  }

  const auto lagScale = maximumLag / distance;
  return InputIndicatorPoint{
      .x = target.x - deltaX * lagScale,
      .y = target.y - deltaY * lagScale,
  };
}

std::optional<InputIndicatorPoint> inputIndicatorMotionOffset(
    InputIndicatorPoint previousTarget, InputIndicatorPoint target,
    double maximumLength, bool reducedMotion
)
{
  if (!std::isfinite(previousTarget.x) ||
      !std::isfinite(previousTarget.y) ||
      !std::isfinite(target.x) || !std::isfinite(target.y) ||
      !std::isfinite(maximumLength) || maximumLength < 0) {
    return std::nullopt;
  }
  if (reducedMotion) {
    return InputIndicatorPoint{};
  }

  const auto deltaX = target.x - previousTarget.x;
  const auto deltaY = target.y - previousTarget.y;
  const auto distance = std::hypot(deltaX, deltaY);
  if (!std::isfinite(distance)) {
    return std::nullopt;
  }
  if (distance <= maximumLength || distance == 0) {
    return InputIndicatorPoint{.x = deltaX, .y = deltaY};
  }
  const auto scale = maximumLength / distance;
  return InputIndicatorPoint{
      .x = deltaX * scale,
      .y = deltaY * scale,
  };
}

bool inputIndicatorCompanionFacesRight(
    bool previousFacesRight,
    double motionX
)
{
  if (!std::isfinite(motionX)) {
    return previousFacesRight;
  }
  constexpr double kFacingDeadZone = 0.25;
  if (std::abs(motionX) <= kFacingDeadZone) {
    return previousFacesRight;
  }
  return motionX > 0;
}

double inputIndicatorTransitionProgress(
    double elapsedMilliseconds, double durationMilliseconds,
    bool reducedMotion
)
{
  if (reducedMotion) {
    return 1;
  }
  if (!std::isfinite(elapsedMilliseconds) ||
      !std::isfinite(durationMilliseconds) ||
      durationMilliseconds <= 0) {
    return 1;
  }
  const auto progress = std::clamp(
      elapsedMilliseconds / durationMilliseconds, 0.0, 1.0
  );
  return progress * progress * (3.0 - 2.0 * progress);
}

InputIndicatorMotionState inputIndicatorMotionState(
    InputIndicatorMotionState previous,
    double pointerSpeedPointsPerSecond,
    double stoppedMilliseconds,
    bool reducedMotion
)
{
  if (reducedMotion) {
    return InputIndicatorMotionState::Idle;
  }
  if (!std::isfinite(pointerSpeedPointsPerSecond)) {
    pointerSpeedPointsPerSecond = 0;
  }
  if (!std::isfinite(stoppedMilliseconds)) {
    stoppedMilliseconds = 0;
  }
  constexpr double kWalkSpeed = 30;
  constexpr double kRunSpeed = 650;
  constexpr double kSettleMilliseconds = 180;
  if (pointerSpeedPointsPerSecond >= kRunSpeed) {
    return InputIndicatorMotionState::Run;
  }
  if (pointerSpeedPointsPerSecond >= kWalkSpeed) {
    return InputIndicatorMotionState::Walk;
  }
  if (stoppedMilliseconds >= kSettleMilliseconds) {
    return InputIndicatorMotionState::Idle;
  }
  return previous;
}

double inputIndicatorMotionPhase(
    InputIndicatorMotionState state,
    double elapsedMilliseconds
)
{
  if (!std::isfinite(elapsedMilliseconds)) {
    return 0;
  }
  double period = 1200;
  switch (state) {
  case InputIndicatorMotionState::Idle:
    period = 1200;
    break;
  case InputIndicatorMotionState::Walk:
    period = 250;
    break;
  case InputIndicatorMotionState::Run:
    period = 1000.0 / 7.0;
    break;
  }
  return std::fmod(std::max(elapsedMilliseconds, 0.0), period) /
         period;
}

int inputIndicatorMotionFrameInterval(
    InputIndicatorMotionState state,
    bool reducedMotion
)
{
  if (reducedMotion) {
    return 0;
  }
  return state == InputIndicatorMotionState::Idle ? 75 : 8;
}

InputIndicatorMotionPose inputIndicatorMotionPose(
    InputIndicatorMotionState state,
    double phase,
    bool reducedMotion
)
{
  if (reducedMotion ||
      !std::isfinite(phase)) {
    return {};
  }
  const auto angle =
      std::clamp(phase, 0.0, 1.0) * 2.0 * M_PI;
  const auto sinPhase = std::sin(angle);
  switch (state) {
  case InputIndicatorMotionState::Idle:
    return {.tailSway = sinPhase * 2.0};
  case InputIndicatorMotionState::Walk:
    return {
        .bodyBob = sinPhase * 0.45,
        .bodyStretch = std::abs(sinPhase) * 0.025,
    };
  case InputIndicatorMotionState::Run:
    return {
        .bodyBob = sinPhase * 0.9,
        .bodyStretch = std::abs(sinPhase) * 0.07,
    };
  }
}

InputIndicatorTailPose inputIndicatorCatTailPose(
    InputIndicatorPoint root,
    double length,
    double phase,
    double velocityX,
    bool reducedMotion
)
{
  if (!std::isfinite(root.x) ||
      !std::isfinite(root.y)) {
    return {};
  }
  if (!std::isfinite(length) || length <= 0) {
    length = 10;
  }
  if (!std::isfinite(phase)) {
    phase = 0;
  }
  if (!std::isfinite(velocityX)) {
    velocityX = 0;
  }
  phase = std::fmod(std::fmod(phase, 1.0) + 1.0, 1.0);
  velocityX = std::clamp(velocityX, -8.0, 8.0);
  const double angle = phase * 2.0 * M_PI;
  const double middleSway =
      reducedMotion ? 0 : std::sin(angle) * 1.2;
  const double tipSway =
      reducedMotion
          ? 0
          : std::sin((phase - 0.18) * 2.0 * M_PI) * 0.8;
  const double inertia =
      reducedMotion ? 0 : -velocityX * 0.12;
  const auto middle = InputIndicatorPoint{
      .x = root.x + length * 0.52,
      .y = root.y + 1.8 + middleSway,
  };
  const auto incomingOffset = InputIndicatorPoint{
      .x = length * 0.18,
      .y = 0.6 + middleSway * 0.2,
  };
  const auto tip = InputIndicatorPoint{
      .x = root.x + length,
      .y = root.y + 3.0 + tipSway + inertia,
  };
  return {
      .root = root,
      .proximalControl = {
          .x = root.x + length * 0.2,
          .y = root.y + 0.5 + middleSway * 0.5,
      },
      .middleIncomingControl = {
          .x = middle.x - incomingOffset.x,
          .y = middle.y - incomingOffset.y,
      },
      .middle = middle,
      .middleOutgoingControl = {
          .x = middle.x + incomingOffset.x,
          .y = middle.y + incomingOffset.y,
      },
      .tipControl = {
          .x = tip.x - length * 0.18,
          .y = tip.y - 0.8 - tipSway * 0.15,
      },
      .tip = tip,
  };
}

} // namespace deskflow::gui::input
