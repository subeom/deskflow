/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "InputIndicatorGeometry.h"
#include "deskflow/ipc/InputIndicatorIpc.h"

#include <QString>

#include <functional>
#include <memory>

namespace deskflow::gui::input {

enum class InputIndicatorCompanion
{
  Cat,
  Dog,
  Turtle
};

enum class InputIndicatorRenderPrimitive
{
  Tile,
  SilhouetteUnion,
  LanguageGlyph
};

struct MacInputIndicatorRenderModel
{
  InputIndicatorCompanion companion =
      InputIndicatorCompanion::Cat;
  InputIndicatorRenderPrimitive silhouettePrimitive =
      InputIndicatorRenderPrimitive::SilhouetteUnion;
  bool usesEyePrimitives = false;
  double tileWidth = 24;
  double tileHeight = 24;
  double totalWidth = 30;
  double totalHeight = 28;
  InputIndicatorRect silhouetteBounds;
  InputIndicatorRect languageBounds;
};

enum class InputIndicatorTone
{
  Automatic,
  Light,
  Dark
};

struct MacInputIndicatorPanelStyle
{
  double width = 0;
  double height = 0;
  double cursorGap = 0;
  double screenMargin = 0;
  double labelSize = 0;
  double cornerRadius = 0;
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
      .cornerRadius = 7,
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
        .silhouetteBounds = {
            .x = 1, .y = 3, .width = 25, .height = 21
        },
        .languageBounds = {
            .x = 9, .y = 7, .width = 10, .height = 8
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
