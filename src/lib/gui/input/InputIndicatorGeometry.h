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

enum class InputIndicatorMotionState
{
  Idle,
  Walk,
  Run
};

struct InputIndicatorMotionPose
{
  double bodyBob = 0;
  double bodyStretch = 0;
  double tailSway = 0;
};

struct InputIndicatorCatPose
{
  InputIndicatorRect torsoBounds;
  InputIndicatorRect headBounds;
  InputIndicatorRect frontPawBounds;
  InputIndicatorRect rearPawBounds;
  double bodyAngleDegrees = 0;
  double tailLength = 0;
  double tailStraightness = 0;
};

struct InputIndicatorTailPose
{
  InputIndicatorPoint root;
  InputIndicatorPoint proximalControl;
  InputIndicatorPoint middleIncomingControl;
  InputIndicatorPoint middle;
  InputIndicatorPoint middleOutgoingControl;
  InputIndicatorPoint tipControl;
  InputIndicatorPoint tip;
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

bool inputIndicatorCompanionFacesRight(
    bool previousFacesRight,
    double motionX
);

double inputIndicatorTransitionProgress(
    double elapsedMilliseconds, double durationMilliseconds,
    bool reducedMotion
);

InputIndicatorMotionState inputIndicatorMotionState(
    InputIndicatorMotionState previous,
    double pointerSpeedPointsPerSecond,
    double pointerDistance,
    double followerLag,
    double stoppedMilliseconds,
    bool reducedMotion
);

double inputIndicatorMotionPhase(
    InputIndicatorMotionState state,
    double elapsedMilliseconds
);

int inputIndicatorMotionFrameInterval(
    InputIndicatorMotionState state,
    bool reducedMotion
);

InputIndicatorMotionPose inputIndicatorMotionPose(
    InputIndicatorMotionState state,
    double phase,
    bool reducedMotion
);

InputIndicatorCatPose inputIndicatorCatPose(
    InputIndicatorMotionState state,
    double phase,
    bool reducedMotion
);

InputIndicatorTailPose inputIndicatorCatTailPose(
    InputIndicatorPoint root,
    double length,
    double phase,
    double velocityX,
    double straightness,
    bool reducedMotion
);

} // namespace deskflow::gui::input
