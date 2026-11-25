/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "InputIndicatorGeometry.h"
#include "deskflow/ipc/InputIndicatorIpc.h"

#include <QString>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace deskflow::gui::input {

enum class InputIndicatorCompanion
{
  Cat,
  Dog,
  Turtle
};

enum class InputIndicatorRenderPrimitive
{
  SilhouetteUnion,
  LanguageGlyph
};

struct MacInputIndicatorRenderModel
{
  InputIndicatorCompanion companion =
      InputIndicatorCompanion::Cat;
  InputIndicatorRenderPrimitive silhouettePrimitive =
      InputIndicatorRenderPrimitive::SilhouetteUnion;
  InputIndicatorRenderPrimitive languagePrimitive =
      InputIndicatorRenderPrimitive::LanguageGlyph;
  bool usesEyePrimitives = false;
  bool usesSideProfile = false;
  bool mirrorsWithMotion = false;
  bool sourceFacesRight = false;
  bool languageMirrorsWithSilhouette = false;
  double totalWidth = 30;
  double totalHeight = 28;
  InputIndicatorRect silhouetteBounds;
  InputIndicatorRect headBounds;
  InputIndicatorRect torsoBounds;
  InputIndicatorRect pawBounds;
  InputIndicatorRect tailBounds;
  InputIndicatorRect languageBounds;
};

enum class InputIndicatorTone
{
  Automatic,
  Light,
  Dark
};

struct MacInputIndicatorLabelPreference
{
  double pointSize = 0;
  double inkYCorrection = 0;
};

struct MacInputIndicatorLabelMeasurement
{
  InputIndicatorPoint origin;
  InputIndicatorSize size;
};

struct MacInputIndicatorLabelLayout
{
  double fontSize = 0;
  InputIndicatorPoint drawingOrigin;
  InputIndicatorRect inkBounds;
};

constexpr MacInputIndicatorLabelPreference
macInputIndicatorLabelPreference(
    deskflow::input::InputIndicatorSource source,
    InputIndicatorMotionState state
)
{
  if (source ==
      deskflow::input::InputIndicatorSource::Korean2Set) {
    switch (state) {
    case InputIndicatorMotionState::Idle:
      return {.pointSize = 6.8, .inkYCorrection = -0.25};
    case InputIndicatorMotionState::Walk:
      return {.pointSize = 6.4, .inkYCorrection = -0.20};
    case InputIndicatorMotionState::Run:
      return {.pointSize = 6.1, .inkYCorrection = -0.15};
    }
  }
  return {
      .pointSize =
          state == InputIndicatorMotionState::Run ? 7.6 : 8.0,
      .inkYCorrection = 0,
  };
}

inline std::optional<MacInputIndicatorLabelLayout>
macInputIndicatorFitLabelLayout(
    double pointSize,
    double inkYCorrection,
    const InputIndicatorRect &container,
    const MacInputIndicatorLabelMeasurement &measurement
)
{
  if (!std::isfinite(pointSize) || pointSize <= 0 ||
      !std::isfinite(inkYCorrection) ||
      !std::isfinite(container.x) ||
      !std::isfinite(container.y) ||
      !std::isfinite(container.width) ||
      !std::isfinite(container.height) ||
      container.width <= 0 || container.height <= 0 ||
      !std::isfinite(measurement.origin.x) ||
      !std::isfinite(measurement.origin.y) ||
      !std::isfinite(measurement.size.width) ||
      !std::isfinite(measurement.size.height) ||
      measurement.size.width <= 0 ||
      measurement.size.height <= 0) {
    return std::nullopt;
  }

  const double scale = std::min({
      1.0,
      container.width / measurement.size.width,
      container.height / measurement.size.height,
  });
  if (!std::isfinite(scale) || scale <= 0) {
    return std::nullopt;
  }

  const double inkWidth =
      measurement.size.width * scale;
  const double inkHeight =
      measurement.size.height * scale;
  const double inkX =
      container.x + (container.width - inkWidth) / 2.0;
  const double centeredY =
      container.y + (container.height - inkHeight) / 2.0;
  const double inkY =
      std::clamp(
          centeredY + inkYCorrection * scale,
          container.y,
          container.y + container.height - inkHeight
      );
  return MacInputIndicatorLabelLayout{
      .fontSize = pointSize * scale,
      .drawingOrigin = {
          .x = inkX - measurement.origin.x * scale,
          .y = inkY - measurement.origin.y * scale,
      },
      .inkBounds = {
          .x = inkX,
          .y = inkY,
          .width = inkWidth,
          .height = inkHeight,
      },
  };
}

inline std::optional<MacInputIndicatorLabelLayout>
macInputIndicatorLabelLayout(
    deskflow::input::InputIndicatorSource source,
    InputIndicatorMotionState state,
    const InputIndicatorRect &container,
    const MacInputIndicatorLabelMeasurement &measurement
)
{
  if (source ==
      deskflow::input::InputIndicatorSource::Unknown) {
    return std::nullopt;
  }
  const auto preference =
      macInputIndicatorLabelPreference(source, state);
  return macInputIndicatorFitLabelLayout(
      preference.pointSize,
      preference.inkYCorrection,
      container,
      measurement
  );
}

struct MacInputIndicatorRgb
{
  double red = 0;
  double green = 0;
  double blue = 0;
};

struct MacInputIndicatorColorModel
{
  MacInputIndicatorRgb body;
  MacInputIndicatorRgb edge;
  MacInputIndicatorRgb latinGlyph;
  MacInputIndicatorRgb koreanGlyph;
};

constexpr MacInputIndicatorRgb macInputIndicatorBlendColor(
    MacInputIndicatorRgb from,
    MacInputIndicatorRgb to,
    double fraction
)
{
  fraction = std::clamp(fraction, 0.0, 1.0);
  return {
      .red = from.red + (to.red - from.red) * fraction,
      .green = from.green + (to.green - from.green) * fraction,
      .blue = from.blue + (to.blue - from.blue) * fraction,
  };
}

constexpr MacInputIndicatorColorModel macInputIndicatorColorModel(
    InputIndicatorTone tone
)
{
  constexpr MacInputIndicatorRgb body{
      .red = 242.0 / 255.0,
      .green = 243.0 / 255.0,
      .blue = 245.0 / 255.0,
  };
  constexpr MacInputIndicatorRgb edge{
      .red = 68.0 / 255.0,
      .green = 80.0 / 255.0,
      .blue = 95.0 / 255.0,
  };
  switch (tone) {
  case InputIndicatorTone::Light:
    return {
        .body = body,
        .edge = edge,
        .latinGlyph = {
            .red = 55.0 / 255.0,
            .green = 78.0 / 255.0,
            .blue = 105.0 / 255.0,
        },
        .koreanGlyph = {
            .red = 122.0 / 255.0,
            .green = 62.0 / 255.0,
            .blue = 82.0 / 255.0,
        },
    };
  case InputIndicatorTone::Dark:
    return {
        .body = body,
        .edge = edge,
        .latinGlyph = {
            .red = 48.0 / 255.0,
            .green = 66.0 / 255.0,
            .blue = 90.0 / 255.0,
        },
        .koreanGlyph = {
            .red = 102.0 / 255.0,
            .green = 49.0 / 255.0,
            .blue = 68.0 / 255.0,
        },
    };
  case InputIndicatorTone::Automatic:
    return {
        .body = body,
        .edge = edge,
        .latinGlyph = {
            .red = 74.0 / 255.0,
            .green = 96.0 / 255.0,
            .blue = 125.0 / 255.0,
        },
        .koreanGlyph = {
            .red = 136.0 / 255.0,
            .green = 78.0 / 255.0,
            .blue = 98.0 / 255.0,
        },
    };
  }
}

constexpr MacInputIndicatorRgb macInputIndicatorGlyphColor(
    deskflow::input::InputIndicatorSource source,
    InputIndicatorTone tone
)
{
  const auto model =
      macInputIndicatorColorModel(tone);
  return source == deskflow::input::InputIndicatorSource::Korean2Set
      ? model.koreanGlyph
      : model.latinGlyph;
}

constexpr MacInputIndicatorRgb macInputIndicatorBodyColor(
    deskflow::input::InputIndicatorSource source,
    InputIndicatorTone tone
)
{
  const auto model =
      macInputIndicatorColorModel(tone);
  return macInputIndicatorBlendColor(
      model.body,
      macInputIndicatorGlyphColor(source, tone),
      0.08
  );
}

struct MacInputIndicatorHaloPulse
{
  deskflow::input::InputIndicatorSource source =
      deskflow::input::InputIndicatorSource::Unknown;
  double startedMilliseconds = 0;
};

struct MacInputIndicatorHaloFrame
{
  deskflow::input::InputIndicatorSource source =
      deskflow::input::InputIndicatorSource::Unknown;
  double scale = 1;
  double alpha = 0;
  bool active = false;
};

inline constexpr double kMacInputIndicatorHaloDurationMilliseconds =
    280;
inline constexpr double kMacInputIndicatorHaloMinimumStartInterval =
    50;
inline constexpr size_t kMacInputIndicatorMaximumHaloWaves = 3;

inline bool macInputIndicatorAppendHaloPulse(
    std::vector<MacInputIndicatorHaloPulse> &pulses,
    deskflow::input::InputIndicatorSource source,
    double startedMilliseconds
)
{
  if (!std::isfinite(startedMilliseconds) ||
      startedMilliseconds < 0) {
    return false;
  }
  if (!pulses.empty() &&
      startedMilliseconds - pulses.back().startedMilliseconds <
          kMacInputIndicatorHaloMinimumStartInterval) {
    return false;
  }
  pulses.push_back({
      .source = source,
      .startedMilliseconds = startedMilliseconds,
  });
  if (pulses.size() > kMacInputIndicatorMaximumHaloWaves) {
    pulses.erase(
        pulses.begin(),
        pulses.begin() +
            static_cast<std::ptrdiff_t>(
                pulses.size() -
                kMacInputIndicatorMaximumHaloWaves
            )
    );
  }
  return true;
}

inline MacInputIndicatorHaloFrame macInputIndicatorHaloFrame(
    const MacInputIndicatorHaloPulse &pulse,
    double nowMilliseconds,
    bool reducedMotion
)
{
  if (reducedMotion ||
      !std::isfinite(nowMilliseconds) ||
      !std::isfinite(pulse.startedMilliseconds)) {
    return {};
  }
  const double age =
      nowMilliseconds - pulse.startedMilliseconds;
  if (age < 0 ||
      age >= kMacInputIndicatorHaloDurationMilliseconds) {
    return {};
  }
  constexpr double kPeakProgress = 0.18;
  constexpr double kMaximumAlpha = 0.34;
  constexpr double kMaximumScale = 1.22;
  constexpr double kPi = 3.14159265358979323846;
  const double progress =
      age / kMacInputIndicatorHaloDurationMilliseconds;
  const double easeOut =
      1.0 - std::pow(1.0 - progress, 3.0);
  const double alpha =
      progress < kPeakProgress
          ? kMaximumAlpha *
                std::sin(
                    (progress / kPeakProgress) *
                    kPi / 2.0
                )
          : kMaximumAlpha *
                std::pow(
                    1.0 -
                        (progress - kPeakProgress) /
                            (1.0 - kPeakProgress),
                    2.0
                );
  return {
      .source = pulse.source,
      .scale =
          1.0 + (kMaximumScale - 1.0) * easeOut,
      .alpha = alpha,
      .active = true,
  };
}

inline std::vector<MacInputIndicatorHaloFrame>
macInputIndicatorActiveHaloFrames(
    const std::vector<MacInputIndicatorHaloPulse> &pulses,
    double nowMilliseconds,
    bool reducedMotion
)
{
  std::vector<MacInputIndicatorHaloFrame> result;
  result.reserve(kMacInputIndicatorMaximumHaloWaves);
  for (const auto &pulse : pulses) {
    const auto frame =
        macInputIndicatorHaloFrame(
            pulse, nowMilliseconds, reducedMotion
        );
    if (frame.active) {
      result.push_back(frame);
    }
  }
  return result;
}

struct MacInputIndicatorPanelStyle
{
  double width = 0;
  double height = 0;
  double cursorGap = 0;
  double screenMargin = 0;
  double labelSize = 0;
  double maximumLag = 0;
  double maximumMotion = 0;
  double transitionMilliseconds = 0;
  int opacityPercent = 0;
};

struct MacInputIndicatorRenderAlpha
{
  double body = 0;
  double detail = 0;
};

constexpr MacInputIndicatorRenderAlpha macInputIndicatorRenderAlpha(
    int opacityPercent
)
{
  const int clamped =
      opacityPercent < 60
          ? 60
          : opacityPercent > 100
                ? 100
                : opacityPercent;
  const double body = clamped / 100.0;
  return {
      .body = body,
      .detail = body + 0.1 > 1.0 ? 1.0 : body + 0.1,
  };
}

constexpr MacInputIndicatorPanelStyle macInputIndicatorPanelStyle(
    InputIndicatorCompanion companion = InputIndicatorCompanion::Cat
)
{
  return {
      .width = 30,
      .height = 28,
      .cursorGap = 6,
      .screenMargin = 6,
      .labelSize = 8,
      .maximumLag = 8,
      .maximumMotion = 8,
      .transitionMilliseconds = 200,
      .opacityPercent = 65,
  };
}

constexpr MacInputIndicatorRenderModel macInputIndicatorRenderModel(
    InputIndicatorCompanion companion
)
{
  switch (companion) {
  case InputIndicatorCompanion::Cat:
    return {
        .companion = InputIndicatorCompanion::Cat,
        .usesSideProfile = true,
        .mirrorsWithMotion = true,
        .sourceFacesRight = true,
        .silhouetteBounds = {
            .x = 1, .y = 2, .width = 28, .height = 16
        },
        .headBounds = {
            .x = 18, .y = 8, .width = 8, .height = 8
        },
        .torsoBounds = {
            .x = 8, .y = 4.5, .width = 16, .height = 10
        },
        .pawBounds = {
            .x = 3.5, .y = 2.5, .width = 23, .height = 5.5
        },
        .tailBounds = {
            .x = 1, .y = 4, .width = 14, .height = 8
        },
        .languageBounds = {
            .x = 12.99847, .y = 4.62789, .width = 9, .height = 8
        },
    };
  case InputIndicatorCompanion::Dog:
    return {
        .companion = InputIndicatorCompanion::Dog,
        .silhouetteBounds = {
            .x = 1, .y = 3, .width = 25, .height = 21
        },
        .languageBounds = {
            .x = 9, .y = 7, .width = 10, .height = 8
        },
    };
  case InputIndicatorCompanion::Turtle:
    return {
        .companion = InputIndicatorCompanion::Turtle,
        .silhouetteBounds = {
            .x = 1, .y = 5, .width = 28, .height = 17
        },
        .languageBounds = {
            .x = 9, .y = 8, .width = 10, .height = 8
        },
    };
  }
}

constexpr InputIndicatorRect inputIndicatorCatLanguageBounds(
    const InputIndicatorCatPose &pose,
    bool facesRight
)
{
  const auto model =
      macInputIndicatorRenderModel(InputIndicatorCompanion::Cat);
  auto bounds = pose.languageBounds;
  if (facesRight != model.sourceFacesRight) {
    bounds.x =
        model.totalWidth - bounds.x - bounds.width;
  }
  return bounds;
}

inline InputIndicatorCompanion inputIndicatorCompanionFromSetting(
    const QString &value
)
{
  if (value == QStringLiteral("dog")) {
    return InputIndicatorCompanion::Dog;
  }
  if (value == QStringLiteral("turtle")) {
    return InputIndicatorCompanion::Turtle;
  }
  return InputIndicatorCompanion::Cat;
}

struct MacInputIndicatorPanelOptions
{
  deskflow::input::InputIndicatorSource source = deskflow::input::InputIndicatorSource::Unknown;
  InputIndicatorCompanion companion = InputIndicatorCompanion::Cat;
  InputIndicatorPosition position = InputIndicatorPosition::CursorBottomRight;
  int opacityPercent = macInputIndicatorPanelStyle().opacityPercent;
  InputIndicatorTone tone = InputIndicatorTone::Automatic;
};

struct MacInputIndicatorPanelMoveResult
{
  bool visible = false;
  bool animating = false;
  bool reducedMotion = false;
  int frameIntervalMilliseconds = 0;
};

constexpr bool macInputIndicatorPresentationNeeded(
    bool activeSpaceChanged, bool visible
)
{
  return activeSpaceChanged || !visible;
}

enum class MacInputIndicatorPanelOrderCommand
{
  FrontRegardless
};

template <typename Dispatch>
void requestMacInputIndicatorPanelPresentation(Dispatch dispatch)
{
  dispatch(MacInputIndicatorPanelOrderCommand::FrontRegardless);
}

class MacInputIndicatorPanel
{
public:
  explicit MacInputIndicatorPanel(
      std::function<void()> presentationInvalidated = {}
  );
  ~MacInputIndicatorPanel();

  MacInputIndicatorPanel(const MacInputIndicatorPanel &) = delete;
  MacInputIndicatorPanel &operator=(const MacInputIndicatorPanel &) = delete;

  bool show(const MacInputIndicatorPanelOptions &options);
  MacInputIndicatorPanelMoveResult move(
      InputIndicatorPosition position
  );
  void hide();

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::gui::input
