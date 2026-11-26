/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ipc/InputIndicatorIpc.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace deskflow::gui::input {

enum class InputIndicatorPosition
{
  CursorBottomRight,
  CursorTopRight,
  CursorBottomLeft,
  CursorTopLeft,
  CursorFollowApproach
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

enum class InputIndicatorCompanion
{
  Cat,
  Dog,
  Turtle
};

enum class InputIndicatorCompanionIdleMotion : unsigned char
{
  RelaxedSway,
  TipFlick,
  CurlUnfurl,
  LazySweep,
  QuestionLift
};

struct InputIndicatorCompanionIdleMotionState
{
  std::uint32_t seed = 0;
  std::uint32_t remainingMask = 0x1f;
  InputIndicatorCompanionIdleMotion lastMotion =
      InputIndicatorCompanionIdleMotion::RelaxedSway;
  bool hasLastMotion = false;
  bool running = false;
  double startedMilliseconds = 0;
};

struct InputIndicatorCompanionIdleFrame
{
  InputIndicatorCompanionIdleMotion motion =
      InputIndicatorCompanionIdleMotion::RelaxedSway;
  double progress = 0;
  bool active = false;
};

struct InputIndicatorCompanionIdleMotionStep
{
  InputIndicatorCompanionIdleMotionState state;
  InputIndicatorCompanionIdleFrame frame;
};

using InputIndicatorCatIdleTailMotion =
    InputIndicatorCompanionIdleMotion;
using InputIndicatorCatIdleTailMotionState =
    InputIndicatorCompanionIdleMotionState;
using InputIndicatorCatIdleTailFrame =
    InputIndicatorCompanionIdleFrame;
using InputIndicatorCatIdleTailMotionStep =
    InputIndicatorCompanionIdleMotionStep;

struct InputIndicatorCompanionSourceStartleState
{
  deskflow::input::InputIndicatorSource latestSource =
      deskflow::input::InputIndicatorSource::Unknown;
  bool active = false;
  bool pending = false;
  bool hasStarted = false;
  double startedMilliseconds = 0;
  double lastStartedMilliseconds = 0;
};

struct InputIndicatorCompanionStartleFrame
{
  bool active = false;
  double progress = 0;
  double recoilOffsetX = 0;
  double horizontalScale = 1;
  double dorsalLift = 0;
  double haloScale = 1;
  double haloAlpha = 0;
};

struct InputIndicatorCompanionSourceStartleStep
{
  InputIndicatorCompanionSourceStartleState state;
  InputIndicatorCompanionStartleFrame frame;
};

using InputIndicatorCatSourceStartleState =
    InputIndicatorCompanionSourceStartleState;
using InputIndicatorCatStartleFrame =
    InputIndicatorCompanionStartleFrame;
using InputIndicatorCatSourceStartleStep =
    InputIndicatorCompanionSourceStartleStep;

enum class InputIndicatorTargetKind : unsigned char
{
  Pointer,
  Caret
};

struct InputIndicatorTarget
{
  InputIndicatorPoint point;
  InputIndicatorTargetKind kind =
      InputIndicatorTargetKind::Pointer;
};

struct InputIndicatorCompanionPursuitState
{
  InputIndicatorPoint position;
  InputIndicatorPoint velocity;
  double headingRadians = 0;
  double stoppedMilliseconds = 0;
  InputIndicatorMotionState motionState =
      InputIndicatorMotionState::Idle;
  bool initialized = false;
};

struct InputIndicatorCompanionPursuitStep
{
  InputIndicatorCompanionPursuitState state;
  InputIndicatorPoint movement;
  InputIndicatorTargetKind targetKind =
      InputIndicatorTargetKind::Pointer;
  double distanceMoved = 0;
  double speedPointsPerSecond = 0;
  bool reached = false;
};

using InputIndicatorCatPursuitState =
    InputIndicatorCompanionPursuitState;
using InputIndicatorCatPursuitStep =
    InputIndicatorCompanionPursuitStep;

enum class InputIndicatorCatArtworkFrame
{
  IdleRise,
  IdleNeutral,
  IdleFall,
  WalkReach,
  WalkPass,
  WalkPush,
  RunCoil,
  RunAir,
  RunLand
};

struct InputIndicatorMotionPose
{
  double bodyBob = 0;
  double bodyStretch = 0;
  double tailSway = 0;
};

struct InputIndicatorCompanionMotionPose
{
  double bodyBob = 0;
  double bodyStretch = 0;
  double tailPhase = 0;
  double limbStride = 0;
  double bodyLead = 0;
};

struct InputIndicatorCatPose
{
  InputIndicatorCatArtworkFrame artworkFrame =
      InputIndicatorCatArtworkFrame::IdleNeutral;
  InputIndicatorRect silhouetteBounds;
  InputIndicatorRect torsoBounds;
  InputIndicatorRect headBounds;
  InputIndicatorRect frontPawBounds;
  InputIndicatorRect rearPawBounds;
  InputIndicatorRect languageBounds;
  InputIndicatorRect innerLanguageBounds;
  double torsoExpansion = 0;
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
    InputIndicatorPosition position, double gap, double margin,
    std::optional<double> approachHeadingRadians =
        std::nullopt
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

InputIndicatorPoint inputIndicatorCatTorsoPoint(
    const InputIndicatorCatPose &pose,
    InputIndicatorPoint point
);

InputIndicatorCatIdleTailMotionState
inputIndicatorCatIdleTailMotionInitialState(
    std::uint32_t seed
);

double inputIndicatorCatIdleTailMotionDuration(
    InputIndicatorCatIdleTailMotion motion
);

InputIndicatorCompanionIdleMotionState
inputIndicatorCompanionIdleMotionInitialState(
    std::uint32_t seed
);

double inputIndicatorCompanionIdleMotionDuration(
    InputIndicatorCompanionIdleMotion motion
);

InputIndicatorCatIdleTailMotionStep
inputIndicatorCatIdleTailMotionAdvance(
    const InputIndicatorCatIdleTailMotionState &state,
    InputIndicatorMotionState motionState,
    double nowMilliseconds,
    bool reducedMotion
);

InputIndicatorCompanionIdleMotionStep
inputIndicatorCompanionIdleMotionAdvance(
    const InputIndicatorCompanionIdleMotionState &state,
    InputIndicatorMotionState motionState,
    double nowMilliseconds,
    bool reducedMotion
);

InputIndicatorPoint inputIndicatorCatIdleTailOffset(
    InputIndicatorCatIdleTailMotion motion,
    double progress
);

InputIndicatorPoint inputIndicatorCatIdleTailPoint(
    InputIndicatorPoint point,
    const InputIndicatorCatIdleTailFrame &frame
);

InputIndicatorCatSourceStartleState
inputIndicatorCatSourceStartleTrigger(
    const InputIndicatorCatSourceStartleState &state,
    deskflow::input::InputIndicatorSource previousSource,
    deskflow::input::InputIndicatorSource source,
    double nowMilliseconds,
    bool reducedMotion
);

InputIndicatorCatSourceStartleStep
inputIndicatorCatSourceStartleAdvance(
    const InputIndicatorCatSourceStartleState &state,
    double nowMilliseconds,
    bool reducedMotion
);

InputIndicatorCompanionSourceStartleState
inputIndicatorCompanionSourceStartleTrigger(
    const InputIndicatorCompanionSourceStartleState &state,
    deskflow::input::InputIndicatorSource previousSource,
    deskflow::input::InputIndicatorSource source,
    double nowMilliseconds,
    bool reducedMotion
);

InputIndicatorCompanionSourceStartleStep
inputIndicatorCompanionSourceStartleAdvance(
    const InputIndicatorCompanionSourceStartleState &state,
    double nowMilliseconds,
    bool reducedMotion
);

InputIndicatorPoint inputIndicatorCatStartlePoint(
    InputIndicatorPoint point,
    const InputIndicatorCatStartleFrame &frame,
    bool dorsalContour
);

InputIndicatorPoint inputIndicatorCompanionArtworkPoint(
    InputIndicatorCompanion companion,
    InputIndicatorPoint point,
    InputIndicatorMotionState motionState,
    double motionPhase,
    const InputIndicatorCompanionIdleFrame &idleFrame,
    const InputIndicatorCompanionStartleFrame &startleFrame
);

InputIndicatorCompanionMotionPose
inputIndicatorCompanionMotionPose(
    InputIndicatorCompanion companion,
    InputIndicatorMotionState motionState,
    double motionPhase,
    bool reducedMotion
);

std::optional<InputIndicatorCatPursuitStep>
inputIndicatorCatPursuitAdvance(
    const InputIndicatorCatPursuitState &state,
    const InputIndicatorTarget &target,
    double targetSpeedPointsPerSecond,
    double elapsedMilliseconds,
    bool initialize,
    bool reducedMotion
);

std::optional<InputIndicatorCompanionPursuitStep>
inputIndicatorCompanionPursuitAdvance(
    const InputIndicatorCompanionPursuitState &state,
    const InputIndicatorTarget &target,
    double targetSpeedPointsPerSecond,
    double elapsedMilliseconds,
    bool initialize,
    bool reducedMotion,
    double speedMultiplier
);

double inputIndicatorNormalizedAngle(double radians);

InputIndicatorPoint inputIndicatorCatBodyPoint(
    InputIndicatorPoint point,
    bool facesRight,
    double facingHorizontalScale,
    double headingRadians
);

InputIndicatorPoint inputIndicatorCompanionBodyPoint(
    InputIndicatorPoint point,
    bool facesRight,
    double facingHorizontalScale,
    double headingRadians
);

constexpr InputIndicatorPoint inputIndicatorCatFacingPoint(
    InputIndicatorPoint point, bool facesRight
)
{
  return facesRight
      ? point
      : InputIndicatorPoint{.x = 30.0 - point.x, .y = point.y};
}

InputIndicatorTailPose inputIndicatorCatTailPose(
    InputIndicatorPoint root,
    double length,
    double phase,
    double velocityX,
    double straightness,
    bool reducedMotion
);

} // namespace deskflow::gui::input
