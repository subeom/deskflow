/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "InputIndicatorGeometry.h"
#include "MacInputIndicatorCompanionArtwork.h"
#include "deskflow/ipc/InputIndicatorIpc.h"

#include <QString>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#ifdef __OBJC__
#include <CoreGraphics/CoreGraphics.h>
@class NSColor;
@class NSFont;
@class NSString;
#endif

namespace deskflow::gui::input {

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
  bool usesTorsoPad = false;
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
  double minimumPointSize = 0;
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
      return {.pointSize = 6.8, .minimumPointSize = 6.3};
    case InputIndicatorMotionState::Walk:
      return {.pointSize = 6.4, .minimumPointSize = 6.0};
    case InputIndicatorMotionState::Run:
      return {.pointSize = 6.1, .minimumPointSize = 5.8};
    }
  }
  return {
      .pointSize =
          state == InputIndicatorMotionState::Run ? 7.6 : 8.0,
      .minimumPointSize =
          state == InputIndicatorMotionState::Run ? 7.0 : 7.2,
  };
}

inline std::optional<MacInputIndicatorLabelLayout>
macInputIndicatorFitLabelInkLayout(
    double pointSize,
    double minimumPointSize,
    const InputIndicatorRect &container,
    const MacInputIndicatorLabelMeasurement &measurement
)
{
  if (!std::isfinite(pointSize) || pointSize <= 0 ||
      !std::isfinite(minimumPointSize) ||
      minimumPointSize <= 0 ||
      minimumPointSize > pointSize ||
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
  const double fittedPointSize =
      pointSize * scale;
  if (fittedPointSize + 0.001 < minimumPointSize) {
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
  return MacInputIndicatorLabelLayout{
      .fontSize = fittedPointSize,
      .drawingOrigin = {
          .x = inkX - measurement.origin.x * scale,
          .y = centeredY - measurement.origin.y * scale,
      },
      .inkBounds = {
          .x = inkX,
          .y = centeredY,
          .width = inkWidth,
          .height = inkHeight,
      },
  };
}

#ifdef __OBJC__
struct MacInputIndicatorCoreTextLabelMeasurement
{
  MacInputIndicatorLabelMeasurement ink;
  InputIndicatorRect typographicBounds;
};

std::optional<MacInputIndicatorCoreTextLabelMeasurement>
macInputIndicatorCoreTextLabelMeasurement(
    NSString *label,
    NSFont *font
);

void macInputIndicatorDrawCoreTextLabel(
    CGContextRef context,
    NSString *label,
    NSFont *font,
    NSColor *color,
    const MacInputIndicatorLabelLayout &layout
);
#endif

struct MacInputIndicatorRgb
{
  double red = 0;
  double green = 0;
  double blue = 0;
};

struct MacInputIndicatorColorModel
{
  MacInputIndicatorRgb koreanBody;
  MacInputIndicatorRgb latinBody;
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
  constexpr MacInputIndicatorRgb koreanBody{
      .red = 242.0 / 255.0,
      .green = 243.0 / 255.0,
      .blue = 245.0 / 255.0,
  };
  constexpr MacInputIndicatorRgb latinBody{
      .red = 168.0 / 255.0,
      .green = 186.0 / 255.0,
      .blue = 199.0 / 255.0,
  };
  constexpr MacInputIndicatorRgb edge{
      .red = 68.0 / 255.0,
      .green = 80.0 / 255.0,
      .blue = 95.0 / 255.0,
  };
  switch (tone) {
  case InputIndicatorTone::Light:
    return {
        .koreanBody = koreanBody,
        .latinBody = latinBody,
        .edge = edge,
        .latinGlyph = {
            .red = 51.0 / 255.0,
            .green = 74.0 / 255.0,
            .blue = 100.0 / 255.0,
        },
        .koreanGlyph = {
            .red = 122.0 / 255.0,
            .green = 62.0 / 255.0,
            .blue = 82.0 / 255.0,
        },
    };
  case InputIndicatorTone::Dark:
    return {
        .koreanBody = koreanBody,
        .latinBody = latinBody,
        .edge = edge,
        .latinGlyph = {
            .red = 40.0 / 255.0,
            .green = 59.0 / 255.0,
            .blue = 82.0 / 255.0,
        },
        .koreanGlyph = {
            .red = 102.0 / 255.0,
            .green = 49.0 / 255.0,
            .blue = 68.0 / 255.0,
        },
    };
  case InputIndicatorTone::Automatic:
    return {
        .koreanBody = koreanBody,
        .latinBody = latinBody,
        .edge = edge,
        .latinGlyph = {
            .red = 48.0 / 255.0,
            .green = 66.0 / 255.0,
            .blue = 90.0 / 255.0,
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
  return source ==
              deskflow::input::InputIndicatorSource::Korean2Set
      ? model.koreanBody
      : model.latinBody;
}

struct MacInputIndicatorPanelStyle
{
  double width = 0;
  double height = 0;
  double canvasWidth = 0;
  double canvasHeight = 0;
  double canvasOffsetX = 0;
  double canvasOffsetY = 0;
  double logicalWidth = 0;
  double logicalHeight = 0;
  double visualScale = 0;
  double cursorGap = 0;
  double screenMargin = 0;
  double labelSize = 0;
  double maximumLag = 0;
  double maximumMotion = 0;
  double transitionMilliseconds = 0;
  int opacityPercent = 0;
};

struct MacInputIndicatorFacingPose
{
  bool facesRight = true;
  bool turning = false;
  double horizontalScale = 1;
  double progress = 1;
};

inline constexpr double kMacInputIndicatorFacingTurnMilliseconds =
    280;

inline MacInputIndicatorFacingPose macInputIndicatorFacingPose(
    bool fromFacesRight,
    bool targetFacesRight,
    double elapsedMilliseconds,
    bool reducedMotion
)
{
  if (reducedMotion ||
      fromFacesRight == targetFacesRight) {
    return {
        .facesRight = targetFacesRight,
        .turning = false,
        .horizontalScale = 1,
        .progress = 1,
    };
  }
  if (!std::isfinite(elapsedMilliseconds)) {
    elapsedMilliseconds = 0;
  }
  const double progress =
      std::clamp(
          elapsedMilliseconds /
              kMacInputIndicatorFacingTurnMilliseconds,
          0.0,
          1.0
      );
  constexpr double kPi = 3.14159265358979323846;
  return {
      .facesRight =
          progress >= 0.5
              ? targetFacesRight
              : fromFacesRight,
      .turning = progress < 1.0,
      .horizontalScale =
          1.0 -
          0.2 * std::sin(progress * kPi),
      .progress = progress,
  };
}

constexpr bool macInputIndicatorFacingTurnStarts(
    bool facesRight,
    bool requestedFacesRight,
    bool turning
)
{
  return !turning &&
      facesRight != requestedFacesRight;
}

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

constexpr MacInputIndicatorPanelStyle
macInputIndicatorPanelStyle(InputIndicatorCompanion companion = InputIndicatorCompanion::Cat, double idleScale = 1)
{
  (void)companion;
  const double boundedScale =
      idleScale >= 1.0
          ? idleScale <= kInputIndicatorIdleGrowthMaximumScale ? idleScale : kInputIndicatorIdleGrowthMaximumScale
          : 1.0;
  return {
      .width = 33 * boundedScale,
      .height = 31 * boundedScale,
      .canvasWidth = 49 * boundedScale,
      .canvasHeight = 49 * boundedScale,
      .canvasOffsetX = 8 * boundedScale,
      .canvasOffsetY = 9 * boundedScale,
      .logicalWidth = 30,
      .logicalHeight = 28,
      .visualScale = 1.1 * boundedScale,
      .cursorGap = 7,
      .screenMargin = 7,
      .labelSize = 8,
      .maximumLag = 9,
      .maximumMotion = 9,
      .transitionMilliseconds = 200,
      .opacityPercent = 65,
  };
}

constexpr InputIndicatorPoint macInputIndicatorDisplayPoint(InputIndicatorPoint point, double idleScale = 1)
{
  const auto style = macInputIndicatorPanelStyle(InputIndicatorCompanion::Cat, idleScale);
  return {
      .x = point.x * style.visualScale +
          (style.width -
           style.logicalWidth * style.visualScale) /
              2.0,
      .y = point.y * style.visualScale +
          (style.height -
           style.logicalHeight * style.visualScale) /
              2.0,
  };
}

constexpr InputIndicatorPoint macInputIndicatorCanvasPoint(
    InputIndicatorPoint point, InputIndicatorCompanion companion = InputIndicatorCompanion::Cat, double idleScale = 1
)
{
  const auto display = macInputIndicatorDisplayPoint(point, idleScale);
  const auto style = macInputIndicatorPanelStyle(companion, idleScale);
  return {
      .x = display.x + style.canvasOffsetX,
      .y = display.y + style.canvasOffsetY,
  };
}

constexpr InputIndicatorRect macInputIndicatorDisplayRect(const InputIndicatorRect &rect, double idleScale = 1)
{
  const auto origin = macInputIndicatorDisplayPoint(
      {
          .x = rect.x,
          .y = rect.y,
      },
      idleScale
  );
  const auto style = macInputIndicatorPanelStyle(InputIndicatorCompanion::Cat, idleScale);
  return {
      .x = origin.x,
      .y = origin.y,
      .width = rect.width * style.visualScale,
      .height = rect.height * style.visualScale,
  };
}

constexpr InputIndicatorRect macInputIndicatorCanvasRect(
    const InputIndicatorRect &rect, InputIndicatorCompanion companion = InputIndicatorCompanion::Cat,
    double idleScale = 1
)
{
  const auto origin = macInputIndicatorCanvasPoint({.x = rect.x, .y = rect.y}, companion, idleScale);
  const auto style = macInputIndicatorPanelStyle(companion, idleScale);
  return {
      .x = origin.x,
      .y = origin.y,
      .width = rect.width * style.visualScale,
      .height = rect.height * style.visualScale,
  };
}

constexpr MacInputIndicatorLabelPreference macInputIndicatorDisplayLabelPreference(
    deskflow::input::InputIndicatorSource source, InputIndicatorMotionState state, double idleScale = 1
)
{
  const auto logical =
      macInputIndicatorLabelPreference(source, state);
  const auto style = macInputIndicatorPanelStyle(InputIndicatorCompanion::Cat, idleScale);
  return {
      .pointSize =
          logical.pointSize * style.visualScale,
      .minimumPointSize =
          logical.minimumPointSize * style.visualScale,
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
            .x = 1, .y = 1.5, .width = 28, .height = 17.5
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
    {
      const auto artwork =
          macInputIndicatorDogArtwork();
      return {
        .companion = InputIndicatorCompanion::Dog,
        .usesSideProfile = true,
        .mirrorsWithMotion = true,
        .sourceFacesRight = true,
        .silhouetteBounds =
            artwork.silhouetteBounds,
        .headBounds = {
            .x = 18, .y = 9,
            .width = 10, .height = 11
        },
        .torsoBounds = {
            .x = 8, .y = 5,
            .width = 14, .height = 12
        },
        .pawBounds = {
            .x = 9, .y = 3,
            .width = 13, .height = 5
        },
        .tailBounds = {
            .x = 2, .y = 8,
            .width = 7, .height = 7
        },
        .languageBounds =
            artwork.innerLanguageBounds,
      };
    }
  case InputIndicatorCompanion::Turtle:
    {
      const auto artwork =
          macInputIndicatorTurtleArtwork();
      return {
        .companion = InputIndicatorCompanion::Turtle,
        .usesSideProfile = true,
        .mirrorsWithMotion = true,
        .sourceFacesRight = true,
        .silhouetteBounds =
            artwork.silhouetteBounds,
        .headBounds = {
            .x = 22, .y = 10.5,
            .width = 7, .height = 6.5
        },
        .torsoBounds = {
            .x = 5, .y = 4,
            .width = 19, .height = 15
        },
        .pawBounds = {
            .x = 7, .y = 3,
            .width = 15, .height = 4
        },
        .tailBounds = {
            .x = 1, .y = 8,
            .width = 6, .height = 4
        },
        .languageBounds =
            artwork.innerLanguageBounds,
      };
    }
  }
}

constexpr InputIndicatorRect
inputIndicatorCatInnerLanguageBounds(
    const InputIndicatorCatPose &pose,
    bool facesRight
)
{
  const auto model =
      macInputIndicatorRenderModel(InputIndicatorCompanion::Cat);
  auto bounds = pose.innerLanguageBounds;
  if (facesRight != model.sourceFacesRight) {
    bounds.x = inputIndicatorCatFacingPoint(
        {
            .x = bounds.x + bounds.width,
            .y = bounds.y,
        },
        false
    ).x;
  }
  return bounds;
}

constexpr InputIndicatorRect inputIndicatorCatLanguageBounds(
    const InputIndicatorCatPose &pose,
    bool facesRight
)
{
  return inputIndicatorCatInnerLanguageBounds(
      pose, facesRight
  );
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
  int followSpeedPercent = 100;
  double idleGrowthMaxScale = kInputIndicatorIdleGrowthMaximumScale;
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
  using TargetLocation =
      std::function<InputIndicatorTarget()>;
  using Clock = std::function<std::chrono::steady_clock::time_point()>;

  explicit MacInputIndicatorPanel(
      std::function<void()> presentationInvalidated = {}, TargetLocation targetLocation = {}, Clock clock = {}
  );
  ~MacInputIndicatorPanel();

  MacInputIndicatorPanel(const MacInputIndicatorPanel &) = delete;
  MacInputIndicatorPanel &operator=(const MacInputIndicatorPanel &) = delete;

  bool show(const MacInputIndicatorPanelOptions &options);
  MacInputIndicatorPanelMoveResult move(
      InputIndicatorPosition position
  );
  void setFollowSpeedPercent(int percent);
  void setIdleGrowthMaxScale(double maximumScale);
  void hide();

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::gui::input
