/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorGeometry.h"
#include "MacInputIndicatorCatArtwork.h"

#include <algorithm>
#include <bit>
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

constexpr std::uint32_t kCatIdleTailMotionMask = 0x1f;
constexpr std::uint32_t kCatIdleTailSeedFallback =
    0x6d2b79f5;
constexpr double kPi = 3.14159265358979323846;
constexpr double kCatIdleTailVisibilityScale = 2.0;
constexpr double kCatStartleMinimumStartIntervalMilliseconds =
    50;
constexpr double kCatStartleDurationMilliseconds = 300;
constexpr double kCatStartleRecoilOffsetX = -0.45;
constexpr double kCatStartleHorizontalScale = 0.92;
constexpr double kCatStartleDorsalLift = 2.4;
constexpr double kCatStartleMaximumHaloScale = 1.12;
constexpr double kCatStartleMaximumHaloAlpha = 0.16;
constexpr double kCatWalkSpeedPointsPerSecond = 120;
constexpr double kCatRunSpeedPointsPerSecond = 720;
constexpr double kCatRunTargetSpeedPointsPerSecond =
    260;
constexpr double kCatRunDistance = 48;
constexpr double kCatRunHysteresisDistance = 12;
constexpr double kCatReachedDistance = 0.25;
constexpr double kCatPursuitFrameCapMilliseconds =
    8;
constexpr double kCatPursuitHeadingRadiansPerSecond =
    10;
constexpr double kCatPursuitSettleMilliseconds = 180;

std::uint32_t nextCatIdleTailSeed(std::uint32_t seed)
{
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed == 0 ? kCatIdleTailSeedFallback : seed;
}

std::uint32_t catIdleTailMotionBit(
    InputIndicatorCatIdleTailMotion motion
)
{
  return 1U << static_cast<unsigned char>(motion);
}

InputIndicatorCatIdleTailMotion catIdleTailMotionAt(
    unsigned int index
)
{
  return static_cast<InputIndicatorCatIdleTailMotion>(index);
}

InputIndicatorCatIdleTailMotionState selectCatIdleTailMotion(
    InputIndicatorCatIdleTailMotionState state
)
{
  if ((state.remainingMask & kCatIdleTailMotionMask) == 0) {
    state.remainingMask = kCatIdleTailMotionMask;
  }

  auto candidates =
      state.remainingMask & kCatIdleTailMotionMask;
  if (state.hasLastMotion &&
      std::popcount(candidates) > 1) {
    candidates &=
        ~catIdleTailMotionBit(state.lastMotion);
  }
  if (candidates == 0) {
    candidates =
        state.remainingMask & kCatIdleTailMotionMask;
  }

  state.seed = nextCatIdleTailSeed(state.seed);
  const auto candidateCount =
      static_cast<std::uint32_t>(
          std::popcount(candidates)
      );
  const auto selection =
      state.seed % candidateCount;
  std::uint32_t selectedIndex = 0;
  std::uint32_t candidateIndex = 0;
  for (unsigned int index = 0; index < 5; ++index) {
    const auto bit = 1U << index;
    if ((candidates & bit) == 0) {
      continue;
    }
    if (candidateIndex == selection) {
      selectedIndex = index;
      break;
    }
    ++candidateIndex;
  }

  state.lastMotion =
      catIdleTailMotionAt(selectedIndex);
  state.hasLastMotion = true;
  state.remainingMask &=
      ~catIdleTailMotionBit(state.lastMotion);
  return state;
}

double smoothstep01(double value)
{
  const auto clamped = std::clamp(value, 0.0, 1.0);
  return clamped * clamped * (3.0 - 2.0 * clamped);
}

double smoothWindow(double progress)
{
  return smoothstep01(progress / 0.22) *
         (1.0 - smoothstep01((progress - 0.78) / 0.22));
}

double pulseWindow(double progress, double start, double end)
{
  return smoothstep01((progress - start) / (end - start));
}

bool isExactInputIndicatorSource(
    deskflow::input::InputIndicatorSource source
)
{
  return source ==
             deskflow::input::InputIndicatorSource::LatinABC ||
         source ==
             deskflow::input::InputIndicatorSource::Korean2Set;
}

double normalizedAnimationTime(double milliseconds)
{
  return std::isfinite(milliseconds)
      ? std::max(milliseconds, 0.0)
      : 0.0;
}

bool finitePoint(InputIndicatorPoint point)
{
  return std::isfinite(point.x) &&
         std::isfinite(point.y);
}

bool validPursuitState(
    const InputIndicatorCatPursuitState &state
)
{
  return state.initialized &&
         finitePoint(state.position) &&
         finitePoint(state.velocity) &&
         std::isfinite(state.headingRadians) &&
         std::isfinite(state.stoppedMilliseconds) &&
         state.stoppedMilliseconds >= 0;
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
    double pointerDistance,
    double followerLag,
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
  if (!std::isfinite(pointerDistance)) {
    pointerDistance = 0;
  }
  if (!std::isfinite(followerLag)) {
    followerLag = 0;
  }
  if (!std::isfinite(stoppedMilliseconds)) {
    stoppedMilliseconds = 0;
  }
  constexpr double kWalkSpeed = 30;
  constexpr double kRunSpeed = 650;
  constexpr double kWalkDistance = 3;
  constexpr double kRunDistance = 48;
  constexpr double kRunFollowerLag = 24;
  constexpr double kSettleMilliseconds = 180;
  if (pointerSpeedPointsPerSecond >= kRunSpeed ||
      pointerDistance >= kRunDistance ||
      followerLag >= kRunFollowerLag) {
    return InputIndicatorMotionState::Run;
  }
  if (pointerSpeedPointsPerSecond >= kWalkSpeed ||
      pointerDistance >= kWalkDistance) {
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

InputIndicatorCompanionMotionPose
inputIndicatorCompanionMotionPose(
    InputIndicatorCompanion companion,
    InputIndicatorMotionState state,
    double motionPhase,
    bool reducedMotion
)
{
  if (reducedMotion ||
      !std::isfinite(motionPhase)) {
    return {};
  }
  const double phase =
      std::clamp(motionPhase, 0.0, 1.0);
  const double angle = phase * 2.0 * M_PI;
  const double wave = std::sin(angle);
  const double stride = std::sin(angle);
  if (companion == InputIndicatorCompanion::Cat) {
    switch (state) {
    case InputIndicatorMotionState::Idle:
      return {
          .tailPhase = wave,
      };
    case InputIndicatorMotionState::Walk:
      return {
          .bodyBob = wave * 0.45,
          .bodyStretch =
              std::abs(wave) * 0.025,
          .limbStride = stride * 0.9,
      };
    case InputIndicatorMotionState::Run:
      return {
          .bodyBob = wave * 0.9,
          .bodyStretch =
              std::abs(wave) * 0.07,
          .limbStride = stride * 1.5,
          .bodyLead = 0.6,
      };
    }
  }
  if (companion == InputIndicatorCompanion::Dog) {
    switch (state) {
    case InputIndicatorMotionState::Idle:
      return {};
    case InputIndicatorMotionState::Walk:
      return {
          .bodyBob = wave * 0.28,
          .bodyStretch =
              std::abs(wave) * 0.015,
          .tailPhase = wave,
          .limbStride = stride * 0.9,
      };
    case InputIndicatorMotionState::Run:
      return {
          .bodyBob = wave * 0.42,
          .bodyStretch =
              std::abs(wave) * 0.035,
          .tailPhase = wave * 1.4,
          .limbStride = stride * 1.5,
          .bodyLead = 0.8,
      };
    }
  }
  switch (state) {
  case InputIndicatorMotionState::Idle:
    return {};
  case InputIndicatorMotionState::Walk:
    return {
        .bodyBob = wave * 0.16,
        .bodyStretch =
            std::abs(wave) * 0.008,
        .tailPhase = wave,
        .limbStride = stride * 0.55,
    };
  case InputIndicatorMotionState::Run:
    return {
        .bodyBob = wave * 0.30,
        .bodyStretch =
            std::abs(wave) * 0.020,
        .tailPhase = wave,
        .limbStride = stride * 0.95,
        .bodyLead = 0.55,
    };
  }
}

InputIndicatorCatPose inputIndicatorCatPose(
    InputIndicatorMotionState state,
    double phase,
    bool reducedMotion
)
{
  if (reducedMotion ||
      !std::isfinite(phase)) {
    state = InputIndicatorMotionState::Idle;
    phase = 0;
  }
  phase =
      std::fmod(std::fmod(phase, 1.0) + 1.0, 1.0);

  InputIndicatorCatArtworkFrame artworkFrame =
      InputIndicatorCatArtworkFrame::IdleNeutral;
  switch (state) {
  case InputIndicatorMotionState::Idle:
    artworkFrame =
        std::sin(phase * 2.0 * M_PI) > 0.34
            ? InputIndicatorCatArtworkFrame::IdleRise
            : std::sin(phase * 2.0 * M_PI) < -0.34
                  ? InputIndicatorCatArtworkFrame::IdleFall
                  : InputIndicatorCatArtworkFrame::IdleNeutral;
    break;
  case InputIndicatorMotionState::Walk:
    artworkFrame =
        phase < 1.0 / 3.0
            ? InputIndicatorCatArtworkFrame::WalkReach
            : phase < 2.0 / 3.0
                  ? InputIndicatorCatArtworkFrame::WalkPass
                  : InputIndicatorCatArtworkFrame::WalkPush;
    break;
  case InputIndicatorMotionState::Run:
    artworkFrame =
        phase < 1.0 / 3.0
            ? InputIndicatorCatArtworkFrame::RunCoil
            : phase < 2.0 / 3.0
                  ? InputIndicatorCatArtworkFrame::RunAir
                  : InputIndicatorCatArtworkFrame::RunLand;
    break;
  }

  const auto artwork =
      macInputIndicatorCatArtwork(artworkFrame);
  const auto &artworkSilhouette =
      artwork.silhouetteBounds;
  const double labelCenterX =
      artwork.languageBounds.x +
      artwork.languageBounds.width / 2.0;
  const double labelCenterY =
      artwork.languageBounds.y +
      artwork.languageBounds.height / 2.0;
  const InputIndicatorRect innerLanguageBounds{
      .x = labelCenterX - 4.0,
      .y = labelCenterY - 3.5,
      .width = 8.0,
      .height = 7.0,
  };
  const double torsoExpansion =
      state == InputIndicatorMotionState::Idle
          ? 1.5
          : state == InputIndicatorMotionState::Walk
                ? 1.2
                : 1.5;
  const double torsoWidth =
      state == InputIndicatorMotionState::Idle
          ? 11.0
          : state == InputIndicatorMotionState::Walk
                ? 12.0
                : 13.0;
  const double torsoHeight =
      state == InputIndicatorMotionState::Idle
          ? 10.0
          : 8.0;
  const InputIndicatorRect torsoBounds{
      .x = labelCenterX - torsoWidth / 2.0,
      .y = labelCenterY - torsoHeight / 2.0,
      .width = torsoWidth,
      .height = torsoHeight,
  };
  const InputIndicatorRect silhouette{
      .x = artworkSilhouette.x,
      .y = artworkSilhouette.y - torsoExpansion,
      .width = artworkSilhouette.width,
      .height = artworkSilhouette.height +
          torsoExpansion * 2.0,
  };
  return {
      .artworkFrame = artworkFrame,
      .silhouetteBounds = silhouette,
      .torsoBounds = torsoBounds,
      .headBounds = {
          .x = silhouette.x + silhouette.width * 0.72,
          .y = silhouette.y + silhouette.height * 0.35,
          .width = silhouette.width * 0.28,
          .height = silhouette.height * 0.65,
      },
      .frontPawBounds = {
          .x = silhouette.x + silhouette.width * 0.64,
          .y = silhouette.y,
          .width = silhouette.width * 0.18,
          .height = std::min(silhouette.height, 3.0),
      },
      .rearPawBounds = {
          .x = silhouette.x + silhouette.width * 0.20,
          .y = silhouette.y,
          .width = silhouette.width * 0.18,
          .height = std::min(silhouette.height, 3.0),
      },
      .languageBounds = artwork.languageBounds,
      .innerLanguageBounds = innerLanguageBounds,
      .torsoExpansion = torsoExpansion,
      .bodyAngleDegrees =
          state == InputIndicatorMotionState::Run ? 4.5 : 0,
      .tailLength =
          state == InputIndicatorMotionState::Run ? 9.0 : 8.0,
      .tailStraightness =
          state == InputIndicatorMotionState::Run
              ? 0.85
              : state == InputIndicatorMotionState::Walk
                    ? 0.45
                    : 0.15,
  };
}

InputIndicatorPoint inputIndicatorCatTorsoPoint(
    const InputIndicatorCatPose &pose,
    InputIndicatorPoint point
)
{
  if (pose.torsoExpansion <= 0 ||
      !std::isfinite(point.x) ||
      !std::isfinite(point.y)) {
    return point;
  }

  const double centerX =
      pose.innerLanguageBounds.x +
      pose.innerLanguageBounds.width / 2.0;
  const double centerY =
      pose.innerLanguageBounds.y +
      pose.innerLanguageBounds.height / 2.0;
  constexpr double innerX = 4.5;
  constexpr double outerX = 6.5;
  constexpr double minimumY = 0.1;
  constexpr double fullY = 1.2;
  constexpr double maximumY = 4.0;
  const double dx = std::abs(point.x - centerX);
  const double dy = point.y - centerY;
  const double verticalDistance = std::abs(dy);
  if (dx >= outerX ||
      verticalDistance <= minimumY ||
      verticalDistance >= maximumY) {
    return point;
  }

  const auto smoothstep =
      [](double edge0, double edge1, double value) {
        if (edge0 == edge1) {
          return value < edge0 ? 0.0 : 1.0;
        }
        const double t =
            std::clamp(
                (value - edge0) / (edge1 - edge0),
                0.0,
                1.0
            );
        return t * t * (3.0 - 2.0 * t);
      };
  const double xWeight =
      1.0 -
      smoothstep(innerX, outerX, dx);
  const double growWeight =
      smoothstep(
          minimumY, fullY, verticalDistance
      );
  const double fadeWeight =
      1.0 -
      smoothstep(
          3.5, maximumY, verticalDistance
      );
  point.y +=
      (dy < 0 ? -1.0 : 1.0) *
      pose.torsoExpansion *
      xWeight *
      growWeight *
      fadeWeight;
  return point;
}

InputIndicatorCatIdleTailMotionState
inputIndicatorCatIdleTailMotionInitialState(
    std::uint32_t seed
)
{
  return {
      .seed = seed == 0 ? kCatIdleTailSeedFallback : seed,
      .remainingMask = kCatIdleTailMotionMask,
  };
}

double inputIndicatorCatIdleTailMotionDuration(
    InputIndicatorCatIdleTailMotion motion
)
{
  switch (motion) {
  case InputIndicatorCatIdleTailMotion::RelaxedSway:
    return 2400;
  case InputIndicatorCatIdleTailMotion::TipFlick:
    return 1800;
  case InputIndicatorCatIdleTailMotion::CurlUnfurl:
    return 2800;
  case InputIndicatorCatIdleTailMotion::LazySweep:
    return 3200;
  case InputIndicatorCatIdleTailMotion::QuestionLift:
    return 2400;
  }
}

InputIndicatorCatIdleTailMotionStep
inputIndicatorCatIdleTailMotionAdvance(
    const InputIndicatorCatIdleTailMotionState &state,
    InputIndicatorMotionState motionState,
    double nowMilliseconds,
    bool reducedMotion
)
{
  InputIndicatorCatIdleTailMotionStep result{.state = state};
  if (reducedMotion ||
      motionState != InputIndicatorMotionState::Idle) {
    result.state.running = false;
    return result;
  }

  const double now =
      std::isfinite(nowMilliseconds)
          ? std::max(nowMilliseconds, 0.0)
          : 0.0;
  if (!result.state.running) {
    result.state = selectCatIdleTailMotion(result.state);
    result.state.startedMilliseconds = now;
    result.state.running = true;
  } else {
    for (int advancement = 0; advancement < 1024;
         ++advancement) {
      const double duration =
          inputIndicatorCatIdleTailMotionDuration(
              result.state.lastMotion
          );
      const double completion =
          result.state.startedMilliseconds + duration;
      if (now < completion) {
        break;
      }
      result.state =
          selectCatIdleTailMotion(result.state);
      result.state.startedMilliseconds = completion;
      if (advancement == 1023) {
        result.state.startedMilliseconds = now;
      }
    }
  }

  const double duration =
      inputIndicatorCatIdleTailMotionDuration(
          result.state.lastMotion
      );
  result.frame = {
      .motion = result.state.lastMotion,
      .progress = std::clamp(
          (now - result.state.startedMilliseconds) /
              duration,
          0.0,
          1.0 - std::numeric_limits<double>::epsilon()
      ),
      .active = true,
  };
  return result;
}

InputIndicatorCompanionIdleMotionState
inputIndicatorCompanionIdleMotionInitialState(
    std::uint32_t seed
)
{
  return inputIndicatorCatIdleTailMotionInitialState(
      seed
  );
}

double inputIndicatorCompanionIdleMotionDuration(
    InputIndicatorCompanionIdleMotion motion
)
{
  return inputIndicatorCatIdleTailMotionDuration(
      motion
  );
}

InputIndicatorCompanionIdleMotionStep
inputIndicatorCompanionIdleMotionAdvance(
    const InputIndicatorCompanionIdleMotionState &state,
    InputIndicatorMotionState motionState,
    double nowMilliseconds,
    bool reducedMotion
)
{
  return inputIndicatorCatIdleTailMotionAdvance(
      state,
      motionState,
      nowMilliseconds,
      reducedMotion
  );
}

InputIndicatorPoint inputIndicatorCatIdleTailOffset(
    InputIndicatorCatIdleTailMotion motion,
    double progress
)
{
  if (!std::isfinite(progress) ||
      progress <= 0 ||
      progress >= 1) {
    return {};
  }
  progress = std::clamp(progress, 0.0, 1.0);
  const double window = smoothWindow(progress);
  switch (motion) {
  case InputIndicatorCatIdleTailMotion::RelaxedSway: {
    const double wave =
        std::sin(progress * 2.0 * kPi) * window;
    return {
        .x =
            0.18 * wave *
            kCatIdleTailVisibilityScale,
        .y =
            0.68 * wave *
            kCatIdleTailVisibilityScale,
    };
  }
  case InputIndicatorCatIdleTailMotion::TipFlick: {
    const double local =
        std::clamp(
            (progress - 0.30) / 0.40, 0.0, 1.0
        );
    if (local <= 0 || local >= 1) {
      return {};
    }
    const double wave =
        std::sin(local * 2.0 * kPi) *
        std::sin(local * kPi);
    return {
        .x =
            -0.22 * wave *
            kCatIdleTailVisibilityScale,
        .y =
            1.10 * wave *
            kCatIdleTailVisibilityScale,
    };
  }
  case InputIndicatorCatIdleTailMotion::CurlUnfurl: {
    const double curl =
        std::sin(progress * kPi);
    const double eased = curl * curl;
    return {
        .x =
            0.78 * eased *
            kCatIdleTailVisibilityScale,
        .y =
            -0.58 * eased *
            kCatIdleTailVisibilityScale,
    };
  }
  case InputIndicatorCatIdleTailMotion::LazySweep: {
    const double wave =
        std::sin(progress * 2.0 * kPi) * window;
    return {
        .x =
            -0.62 * wave *
            kCatIdleTailVisibilityScale,
        .y =
            0.42 * wave *
            kCatIdleTailVisibilityScale,
    };
  }
  case InputIndicatorCatIdleTailMotion::QuestionLift: {
    const double lift =
        pulseWindow(progress, 0.12, 0.44) *
        (1.0 - pulseWindow(progress, 0.70, 0.98));
    return {
        .x =
            0.50 * lift *
            kCatIdleTailVisibilityScale,
        .y =
            1.28 * lift *
            kCatIdleTailVisibilityScale,
    };
  }
  }
}

InputIndicatorPoint inputIndicatorCatIdleTailPoint(
    InputIndicatorPoint point,
    const InputIndicatorCatIdleTailFrame &frame
)
{
  if (!frame.active ||
      !std::isfinite(point.x) ||
      !std::isfinite(point.y) ||
      point.x >= 12.4) {
    return point;
  }

  const double raw =
      std::clamp((12.4 - point.x) / (12.4 - 2.8), 0.0, 1.0);
  const double influence = smoothstep01(raw);
  const auto offset = inputIndicatorCatIdleTailOffset(
      frame.motion, frame.progress
  );
  double xWeight = influence;
  double yWeight = influence;
  switch (frame.motion) {
  case InputIndicatorCatIdleTailMotion::RelaxedSway:
    yWeight =
        influence * (1.0 - 1.75 * influence);
    break;
  case InputIndicatorCatIdleTailMotion::TipFlick:
    xWeight = influence * influence * influence;
    yWeight = xWeight;
    break;
  case InputIndicatorCatIdleTailMotion::CurlUnfurl:
    xWeight = influence * std::sqrt(influence);
    yWeight = xWeight;
    break;
  case InputIndicatorCatIdleTailMotion::LazySweep:
    xWeight = std::sqrt(influence);
    yWeight = std::pow(influence, 0.75);
    break;
  case InputIndicatorCatIdleTailMotion::QuestionLift:
    xWeight = influence * influence;
    yWeight = xWeight;
    break;
  }
  point.x += offset.x * xWeight;
  point.y += offset.y * yWeight;
  return point;
}

InputIndicatorCatSourceStartleState
inputIndicatorCatSourceStartleTrigger(
    const InputIndicatorCatSourceStartleState &state,
    deskflow::input::InputIndicatorSource previousSource,
    deskflow::input::InputIndicatorSource source,
    double nowMilliseconds,
    bool reducedMotion
)
{
  auto next = state;
  if (reducedMotion) {
    next.active = false;
    next.pending = false;
    return next;
  }
  if (!isExactInputIndicatorSource(previousSource) ||
      !isExactInputIndicatorSource(source) ||
      previousSource == source) {
    return next;
  }

  const double now =
      normalizedAnimationTime(nowMilliseconds);
  next.latestSource = source;
  if (!next.hasStarted ||
      now - next.lastStartedMilliseconds >=
          kCatStartleMinimumStartIntervalMilliseconds) {
    next.active = true;
    next.pending = false;
    next.hasStarted = true;
    next.startedMilliseconds = now;
    next.lastStartedMilliseconds = now;
    return next;
  }

  next.pending = true;
  return next;
}

InputIndicatorCatSourceStartleStep
inputIndicatorCatSourceStartleAdvance(
    const InputIndicatorCatSourceStartleState &state,
    double nowMilliseconds,
    bool reducedMotion
)
{
  InputIndicatorCatSourceStartleStep result{
      .state = state,
  };
  if (reducedMotion) {
    result.state.active = false;
    result.state.pending = false;
    return result;
  }

  const double now =
      normalizedAnimationTime(nowMilliseconds);
  if (result.state.pending &&
      now - result.state.lastStartedMilliseconds >=
          kCatStartleMinimumStartIntervalMilliseconds) {
    result.state.active = true;
    result.state.pending = false;
    result.state.startedMilliseconds =
        result.state.lastStartedMilliseconds +
        kCatStartleMinimumStartIntervalMilliseconds;
    result.state.lastStartedMilliseconds =
        result.state.startedMilliseconds;
  }
  if (!result.state.active) {
    return result;
  }

  const double age =
      now - result.state.startedMilliseconds;
  if (age >= kCatStartleDurationMilliseconds) {
    result.state.active = false;
    result.state.pending = false;
    return result;
  }

  result.frame.active = true;
  result.frame.progress =
      std::clamp(
          age / kCatStartleDurationMilliseconds,
          0.0,
          1.0
      );
  const double haloEnvelope =
      std::sin(
          result.frame.progress * kPi
      );
  result.frame.haloScale =
      1.0 +
      (kCatStartleMaximumHaloScale - 1.0) *
          haloEnvelope;
  result.frame.haloAlpha =
      kCatStartleMaximumHaloAlpha *
      haloEnvelope;
  if (age <= 70) {
    const double attack =
        smoothstep01(age / 70.0);
    result.frame.recoilOffsetX =
        kCatStartleRecoilOffsetX * attack;
    result.frame.horizontalScale =
        1.0 -
        (1.0 - kCatStartleHorizontalScale) *
            attack;
    return result;
  }
  if (age <= 160) {
    const double rise =
        smoothstep01((age - 70.0) / 90.0);
    result.frame.recoilOffsetX =
        kCatStartleRecoilOffsetX *
        (1.0 - rise);
    result.frame.horizontalScale =
        kCatStartleHorizontalScale +
        (1.0 - kCatStartleHorizontalScale) *
            rise;
    result.frame.dorsalLift =
        kCatStartleDorsalLift * rise;
    return result;
  }

  const double settle =
      smoothstep01((age - 160.0) / 140.0);
  result.frame.dorsalLift =
      kCatStartleDorsalLift * (1.0 - settle);
  return result;
}

InputIndicatorPoint inputIndicatorCatStartlePoint(
    InputIndicatorPoint point,
    const InputIndicatorCatStartleFrame &frame,
    bool dorsalContour
)
{
  if (!frame.active ||
      !std::isfinite(point.x) ||
      !std::isfinite(point.y)) {
    return point;
  }

  const double x = point.x;
  const double y = point.y;
  point.x =
      15.0 +
      (x - 15.0) * frame.horizontalScale +
      frame.recoilOffsetX;
  if (!dorsalContour ||
      frame.dorsalLift <= 0) {
    return point;
  }

  const double horizontalWeight =
      smoothstep01((x - 11.5) / 2.5) *
      (1.0 - smoothstep01((x - 17.2) / 1.5));
  const double verticalWeight =
      smoothstep01((y - 9.5) / 2.0) *
      (1.0 - smoothstep01((y - 14.2) / 2.0));
  point.y +=
      frame.dorsalLift *
      horizontalWeight *
      verticalWeight;
  return point;
}

InputIndicatorCompanionSourceStartleState
inputIndicatorCompanionSourceStartleTrigger(
    const InputIndicatorCompanionSourceStartleState &state,
    deskflow::input::InputIndicatorSource previousSource,
    deskflow::input::InputIndicatorSource source,
    double nowMilliseconds,
    bool reducedMotion
)
{
  return inputIndicatorCatSourceStartleTrigger(
      state,
      previousSource,
      source,
      nowMilliseconds,
      reducedMotion
  );
}

InputIndicatorCompanionSourceStartleStep
inputIndicatorCompanionSourceStartleAdvance(
    const InputIndicatorCompanionSourceStartleState &state,
    double nowMilliseconds,
    bool reducedMotion
)
{
  return inputIndicatorCatSourceStartleAdvance(
      state,
      nowMilliseconds,
      reducedMotion
  );
}

InputIndicatorPoint inputIndicatorCompanionArtworkPoint(
    InputIndicatorCompanion companion,
    InputIndicatorPoint point,
    InputIndicatorMotionState motionState,
    double motionPhase,
    const InputIndicatorCompanionIdleFrame &idleFrame,
    const InputIndicatorCompanionStartleFrame &startleFrame
)
{
  if (!finitePoint(point)) {
    return point;
  }
  if (companion == InputIndicatorCompanion::Cat) {
    const auto tailPoint =
        inputIndicatorCatIdleTailPoint(
            point, idleFrame
        );
    return inputIndicatorCatStartlePoint(
        tailPoint, startleFrame, true
    );
  }

  const auto original = point;
  const auto pose =
      inputIndicatorCompanionMotionPose(
          companion, motionState, motionPhase, false
      );
  point.x =
      15.0 +
      (point.x - 15.0) *
          (1.0 + pose.bodyStretch);
  point.y += pose.bodyBob;

  const bool dog =
      companion == InputIndicatorCompanion::Dog;
  const double idleProgress =
      idleFrame.active
          ? std::clamp(
                idleFrame.progress, 0.0, 1.0
            )
          : 0;
  if (idleFrame.active) {
    const double window =
        smoothWindow(idleProgress);
    const double pulse =
        std::sin(idleProgress * kPi);
    const double wave =
        std::sin(idleProgress * 2.0 * kPi);
    if (dog) {
      const bool tail = original.x <= 7.0;
      const bool ear =
          original.x >= 18.0 &&
          original.y >= 15.0;
      const bool nose = original.x >= 24.0;
      switch (idleFrame.motion) {
      case InputIndicatorCompanionIdleMotion::RelaxedSway:
        if (ear) {
          point.y += 0.95 * pulse * window;
        }
        break;
      case InputIndicatorCompanionIdleMotion::TipFlick:
        if (tail) {
          point.y += 0.8 * wave * window;
        }
        break;
      case InputIndicatorCompanionIdleMotion::CurlUnfurl:
        if (ear) {
          point.y -= 0.65 * pulse * pulse;
        }
        break;
      case InputIndicatorCompanionIdleMotion::LazySweep:
        if (tail) {
          point.x += 0.45 * wave * window;
          point.y += 0.65 * wave * window;
        }
        break;
      case InputIndicatorCompanionIdleMotion::QuestionLift:
        if (nose) {
          point.y -=
              0.35 *
              pulseWindow(
                  idleProgress, 0.18, 0.55
              );
        }
        if (tail) {
          point.x +=
              0.55 *
              pulseWindow(
                  idleProgress, 0.45, 0.86
              );
          point.y +=
              0.45 *
              pulseWindow(
                  idleProgress, 0.45, 0.86
              );
        }
        break;
      }
    } else {
      const bool head = original.x >= 22.0;
      const bool shell =
          original.x >= 6.0 &&
          original.x <= 23.0;
      const bool foot = original.y <= 7.0;
      const bool tail = original.x <= 6.0;
      switch (idleFrame.motion) {
      case InputIndicatorCompanionIdleMotion::RelaxedSway:
        if (head) {
          point.x += 1.10 * pulse;
        }
        break;
      case InputIndicatorCompanionIdleMotion::TipFlick:
        if (shell) {
          point.y -= 0.25 * pulse;
        }
        break;
      case InputIndicatorCompanionIdleMotion::CurlUnfurl:
        if (foot) {
          point.y += 0.8 * pulse * pulse;
        }
        break;
      case InputIndicatorCompanionIdleMotion::LazySweep:
        if (head || tail) {
          point.y += 0.30 * wave * window;
        }
        break;
      case InputIndicatorCompanionIdleMotion::QuestionLift:
        if (head) {
          const double peek =
              pulseWindow(
                  idleProgress, 0.15, 0.50
              ) *
              (1.0 -
               0.65 *
                   pulseWindow(
                       idleProgress, 0.72, 0.98
                   ));
          point.x += 0.75 * peek;
        }
        break;
      }
    }
  }

  if (original.y <= 7.0) {
    const double side =
        original.x < (dog ? 15.0 : 14.0)
            ? -1.0
            : 1.0;
    point.x +=
        pose.limbStride * side;
    point.y +=
        std::abs(pose.limbStride) *
        (dog ? 0.28 : 0.38);
  }
  if (pose.bodyLead > 0 &&
      original.x >= 17.0) {
    point.x += pose.bodyLead;
  }
  if (dog && original.x <= 7.0) {
    point.y += pose.tailPhase * 0.35;
  } else if (!dog && original.x <= 6.0) {
    point.y += pose.tailPhase * 0.18;
  }

  if (!startleFrame.active) {
    return point;
  }
  point.x =
      15.0 +
      (point.x - 15.0) *
          startleFrame.horizontalScale +
      startleFrame.recoilOffsetX;
  if (dog) {
    if (original.x >= 18.0 &&
        original.y >= 15.0) {
      point.y +=
          startleFrame.dorsalLift * 0.45;
    }
    if (original.x <= 7.0) {
      point.x +=
          startleFrame.dorsalLift * 0.10;
      point.y +=
          startleFrame.dorsalLift * 0.30;
    }
  } else {
    if (original.x >= 22.0) {
      point.x +=
          startleFrame.dorsalLift * 0.55;
    }
    if (original.y >= 15.0) {
      point.y +=
          startleFrame.dorsalLift * 0.08;
    }
  }
  return point;
}

double inputIndicatorNormalizedAngle(double radians)
{
  if (!std::isfinite(radians)) {
    return 0;
  }
  const double fullTurn = 2.0 * kPi;
  radians =
      std::fmod(radians + kPi, fullTurn);
  if (radians < 0) {
    radians += fullTurn;
  }
  return radians - kPi;
}

std::optional<InputIndicatorCompanionPursuitStep>
inputIndicatorCompanionPursuitAdvance(
    const InputIndicatorCompanionPursuitState &state,
    const InputIndicatorTarget &target,
    double targetSpeedPointsPerSecond,
    double elapsedMilliseconds,
    bool initialize,
    bool reducedMotion,
    double speedMultiplier
)
{
  if (!finitePoint(target.point) ||
      !std::isfinite(targetSpeedPointsPerSecond) ||
      targetSpeedPointsPerSecond < 0 ||
      !std::isfinite(elapsedMilliseconds) ||
      elapsedMilliseconds < 0 ||
      !std::isfinite(speedMultiplier) ||
      speedMultiplier < 0.5 ||
      speedMultiplier > 2.0 ||
      (!initialize && !validPursuitState(state))) {
    return std::nullopt;
  }
  if (target.kind != InputIndicatorTargetKind::Pointer &&
      target.kind != InputIndicatorTargetKind::Caret) {
    return std::nullopt;
  }

  InputIndicatorCompanionPursuitStep result{
      .state = state,
      .targetKind = target.kind,
  };
  if (initialize || reducedMotion) {
    result.movement = {
        .x = target.point.x -
             (initialize
                  ? target.point.x
                  : state.position.x),
        .y = target.point.y -
             (initialize
                  ? target.point.y
                  : state.position.y),
    };
    result.state.position = target.point;
    result.state.velocity = {};
    result.state.stoppedMilliseconds = 0;
    result.state.motionState =
        InputIndicatorMotionState::Idle;
    result.state.initialized = true;
    result.distanceMoved =
        std::hypot(
            result.movement.x, result.movement.y
        );
    result.reached = true;
    return result;
  }

  const double deltaX =
      target.point.x - state.position.x;
  const double deltaY =
      target.point.y - state.position.y;
  const double distance =
      std::hypot(deltaX, deltaY);
  if (!std::isfinite(distance)) {
    return std::nullopt;
  }
  if (distance <= kCatReachedDistance) {
    result.movement = {
        .x = deltaX,
        .y = deltaY,
    };
    result.state.position = target.point;
    result.state.velocity = {};
    result.state.stoppedMilliseconds +=
        elapsedMilliseconds;
    if (result.state.stoppedMilliseconds >=
        kCatPursuitSettleMilliseconds) {
      result.state.motionState =
          InputIndicatorMotionState::Idle;
    }
    result.distanceMoved = distance;
    result.reached = true;
    return result;
  }

  const double simulatedMilliseconds =
      std::min(
          elapsedMilliseconds,
          kCatPursuitFrameCapMilliseconds
      );
  const bool run =
      distance >= kCatRunDistance ||
      targetSpeedPointsPerSecond >=
          kCatRunTargetSpeedPointsPerSecond ||
      (state.motionState ==
           InputIndicatorMotionState::Run &&
       distance > kCatRunHysteresisDistance);
  const double speed =
      (run
          ? kCatRunSpeedPointsPerSecond
          : kCatWalkSpeedPointsPerSecond) *
      speedMultiplier;
  const double stepDistance =
      std::min(
          distance,
          speed *
              (simulatedMilliseconds / 1000.0)
      );
  const double unitX = deltaX / distance;
  const double unitY = deltaY / distance;
  result.movement = {
      .x = unitX * stepDistance,
      .y = unitY * stepDistance,
  };
  result.state.position = {
      .x = state.position.x + result.movement.x,
      .y = state.position.y + result.movement.y,
  };
  result.distanceMoved = stepDistance;
  result.reached =
      distance - stepDistance <=
      kCatReachedDistance;
  if (result.reached) {
    result.movement = {
        .x = deltaX,
        .y = deltaY,
    };
    result.distanceMoved = distance;
    result.state.position = target.point;
  }

  if (simulatedMilliseconds > 0 &&
      result.distanceMoved > 0) {
    const double seconds =
        simulatedMilliseconds / 1000.0;
    result.state.velocity = {
        .x = result.movement.x / seconds,
        .y = result.movement.y / seconds,
    };
    const double requestedHeading =
        std::atan2(deltaY, deltaX);
    const double headingDelta =
        inputIndicatorNormalizedAngle(
            requestedHeading -
            state.headingRadians
        );
    const double maximumTurn =
        kCatPursuitHeadingRadiansPerSecond *
        seconds;
    result.state.headingRadians =
        inputIndicatorNormalizedAngle(
            state.headingRadians +
            std::clamp(
                headingDelta,
                -maximumTurn,
                maximumTurn
            )
        );
    result.state.motionState =
        run
            ? InputIndicatorMotionState::Run
            : InputIndicatorMotionState::Walk;
    result.state.stoppedMilliseconds = 0;
  } else {
    result.state.velocity = {};
    result.state.stoppedMilliseconds +=
        elapsedMilliseconds;
    if (result.state.stoppedMilliseconds >=
        kCatPursuitSettleMilliseconds) {
      result.state.motionState =
          InputIndicatorMotionState::Idle;
    }
  }
  result.speedPointsPerSecond =
      std::hypot(
          result.state.velocity.x,
          result.state.velocity.y
      );
  return result;
}

std::optional<InputIndicatorCatPursuitStep>
inputIndicatorCatPursuitAdvance(
    const InputIndicatorCatPursuitState &state,
    const InputIndicatorTarget &target,
    double targetSpeedPointsPerSecond,
    double elapsedMilliseconds,
    bool initialize,
    bool reducedMotion
)
{
  return inputIndicatorCompanionPursuitAdvance(
      state,
      target,
      targetSpeedPointsPerSecond,
      elapsedMilliseconds,
      initialize,
      reducedMotion,
      1.0
  );
}

InputIndicatorPoint inputIndicatorCatBodyPoint(
    InputIndicatorPoint point,
    bool facesRight,
    double facingHorizontalScale,
    double headingRadians
)
{
  if (!finitePoint(point) ||
      !std::isfinite(facingHorizontalScale) ||
      facingHorizontalScale < 0 ||
      !std::isfinite(headingRadians)) {
    return point;
  }
  if (!facesRight) {
    point.x = 30.0 - point.x;
  }
  point.x =
      15.0 +
      (point.x - 15.0) *
          facingHorizontalScale;
  const double relativeHeading =
      inputIndicatorNormalizedAngle(
          facesRight
              ? headingRadians
              : headingRadians - kPi
      );
  const double cosine =
      std::cos(relativeHeading);
  const double sine =
      std::sin(relativeHeading);
  const double x = point.x - 15.0;
  const double y = point.y - 14.0;
  return {
      .x = 15.0 + x * cosine - y * sine,
      .y = 14.0 + x * sine + y * cosine,
  };
}

InputIndicatorPoint inputIndicatorCompanionBodyPoint(
    InputIndicatorPoint point,
    bool facesRight,
    double facingHorizontalScale,
    double headingRadians
)
{
  return inputIndicatorCatBodyPoint(
      point,
      facesRight,
      facingHorizontalScale,
      headingRadians
  );
}

InputIndicatorTailPose inputIndicatorCatTailPose(
    InputIndicatorPoint root,
    double length,
    double phase,
    double velocityX,
    double straightness,
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
  if (!std::isfinite(straightness)) {
    straightness = 0.15;
  }
  phase = std::fmod(std::fmod(phase, 1.0) + 1.0, 1.0);
  velocityX = std::clamp(velocityX, -8.0, 8.0);
  straightness = std::clamp(straightness, 0.0, 1.0);
  const double curl = 1.0 - straightness;
  const double angle = phase * 2.0 * M_PI;
  const double middleSway =
      reducedMotion
          ? 0
          : std::sin(angle) * (0.25 + curl * 0.95);
  const double tipSway =
      reducedMotion
          ? 0
          : std::sin((phase - 0.18) * 2.0 * M_PI) *
                (0.2 + curl * 0.6);
  const double inertia =
      reducedMotion
          ? 0
          : -velocityX * (0.05 + curl * 0.07);
  const double middleLift = 0.6 + curl * 1.2;
  const double tipLift = 0.9 + curl * 2.1;
  const auto middle = InputIndicatorPoint{
      .x = root.x + length * 0.52,
      .y = root.y + middleLift + middleSway,
  };
  const auto incomingOffset = InputIndicatorPoint{
      .x = length * 0.18,
      .y = 0.6 + middleSway * 0.2,
  };
  const auto tip = InputIndicatorPoint{
      .x = root.x + length,
      .y = root.y + tipLift + tipSway + inertia,
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
