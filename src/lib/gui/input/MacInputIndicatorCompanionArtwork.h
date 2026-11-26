/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "MacInputIndicatorCatArtwork.h"

namespace deskflow::gui::input {

struct MacInputIndicatorCompanionArtwork
{
  std::span<const MacInputIndicatorCatPathCommand>
      silhouette;
  InputIndicatorRect silhouetteBounds;
  InputIndicatorRect innerLanguageBounds;
};

inline constexpr std::array<
    MacInputIndicatorCatPathCommand,
    44>
    kDogSilhouetteCommands{{
        {MacInputIndicatorCatPathCommandType::Move,
         7.5f, 8.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         10.0f, 5.0f, 15.0f, 4.5f, 20.0f, 6.0f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         21.5f, 6.8f, 22.0f, 8.0f, 22.2f, 10.0f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         23.4f, 10.5f, 26.4f, 11.8f, 27.4f, 13.0f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         28.0f, 14.2f, 27.1f, 15.5f, 25.0f, 15.2f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         23.8f, 15.0f, 22.9f, 14.6f, 22.1f, 14.8f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         22.0f, 17.5f, 20.7f, 19.5f, 18.4f, 19.2f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         17.8f, 17.8f, 17.0f, 16.2f, 17.2f, 15.0f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         15.1f, 17.0f, 11.8f, 16.9f, 9.1f, 14.4f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         7.9f, 13.0f, 7.2f, 10.6f, 7.5f, 8.0f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         8.0f, 9.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         4.6f, 7.8f, 1.1f, 10.0f, 2.5f, 13.2f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         3.5f, 15.4f, 6.5f, 14.0f, 8.0f, 11.4f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         8.8f, 10.0f, 8.7f, 9.6f, 8.0f, 9.0f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         18.3f, 18.3f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         18.0f, 20.5f, 16.5f, 22.2f, 15.2f, 20.7f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         14.7f, 19.6f, 16.0f, 17.2f, 18.3f, 17.0f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         20.0f, 17.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         20.4f, 19.6f, 22.2f, 21.0f, 22.8f, 18.8f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         23.0f, 17.4f, 21.8f, 16.2f, 20.0f, 17.0f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         9.8f, 7.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         9.5f, 5.7f, 8.9f, 3.4f, 10.1f, 3.3f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         11.2f, 3.3f, 11.8f, 5.2f, 11.2f, 7.0f},
        {MacInputIndicatorCatPathCommandType::Line,
         9.8f, 7.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         12.3f, 7.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         12.0f, 5.5f, 11.9f, 4.0f, 13.1f, 3.8f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         14.0f, 3.8f, 14.8f, 5.5f, 14.0f, 7.0f},
        {MacInputIndicatorCatPathCommandType::Line,
         12.3f, 7.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         16.8f, 7.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         16.8f, 5.5f, 16.0f, 3.7f, 17.3f, 3.4f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         18.4f, 3.2f, 19.0f, 5.6f, 18.6f, 7.0f},
        {MacInputIndicatorCatPathCommandType::Line,
         16.8f, 7.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         19.3f, 7.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         19.6f, 5.6f, 19.2f, 3.5f, 20.4f, 3.4f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         21.8f, 3.3f, 22.4f, 5.5f, 21.7f, 7.0f},
        {MacInputIndicatorCatPathCommandType::Line,
         19.3f, 7.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
    }};

inline constexpr std::array<
    MacInputIndicatorCatPathCommand,
    37>
    kTurtleSilhouetteCommands{{
        {MacInputIndicatorCatPathCommandType::Move,
         5.0f, 8.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         6.0f, 4.4f, 10.0f, 3.4f, 15.0f, 3.8f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         20.0f, 4.2f, 23.5f, 7.0f, 23.5f, 11.0f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         23.6f, 15.8f, 20.1f, 18.7f, 15.5f, 18.4f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         10.3f, 18.4f, 5.6f, 15.8f, 5.0f, 11.0f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         4.5f, 10.0f, 4.7f, 9.0f, 5.0f, 8.0f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         22.0f, 12.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         24.5f, 11.0f, 27.5f, 10.8f, 28.6f, 12.3f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         28.9f, 13.5f, 28.3f, 15.5f, 26.3f, 16.0f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         24.8f, 16.3f, 23.4f, 15.1f, 22.1f, 14.5f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         5.5f, 9.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         3.0f, 8.0f, 1.0f, 9.0f, 2.2f, 11.0f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         3.0f, 12.4f, 4.5f, 11.2f, 5.8f, 11.0f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         7.5f, 6.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         6.8f, 4.7f, 7.0f, 3.2f, 8.2f, 3.0f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         9.2f, 3.0f, 10.2f, 4.5f, 9.6f, 6.0f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         10.4f, 6.2f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         10.1f, 4.9f, 10.7f, 3.2f, 11.8f, 3.1f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         13.0f, 3.1f, 13.4f, 4.8f, 12.9f, 6.2f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         15.5f, 6.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         15.2f, 4.7f, 15.6f, 3.1f, 16.9f, 3.1f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         18.2f, 3.2f, 18.6f, 4.9f, 18.1f, 6.0f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         18.8f, 6.2f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         19.0f, 4.8f, 19.6f, 3.2f, 20.8f, 3.2f},
        {MacInputIndicatorCatPathCommandType::Cubic,
         22.1f, 3.2f, 22.2f, 5.0f, 21.8f, 6.2f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Move,
         6.0f, 11.0f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         9.0f, 10.4f, 19.5f, 10.2f, 23.0f, 11.4f},
        {MacInputIndicatorCatPathCommandType::Line,
         22.8f, 11.9f, 0, 0, 0, 0},
        {MacInputIndicatorCatPathCommandType::Cubic,
         18.8f, 11.0f, 9.6f, 11.3f, 6.0f, 11.0f},
        {MacInputIndicatorCatPathCommandType::Close,
         0, 0, 0, 0, 0, 0},
    }};

constexpr MacInputIndicatorCompanionArtwork
macInputIndicatorDogArtwork()
{
  return {
      .silhouette = kDogSilhouetteCommands,
      .silhouetteBounds = {
          .x = 1.0,
          .y = 3.0,
          .width = 27.5,
          .height = 19.2,
      },
      .innerLanguageBounds = {
          .x = 10.0,
          .y = 7.0,
          .width = 8.0,
          .height = 7.0,
      },
  };
}

constexpr MacInputIndicatorCompanionArtwork
macInputIndicatorTurtleArtwork()
{
  return {
      .silhouette = kTurtleSilhouetteCommands,
      .silhouetteBounds = {
          .x = 1.0,
          .y = 3.0,
          .width = 28.0,
          .height = 15.4,
      },
      .innerLanguageBounds = {
          .x = 10.0,
          .y = 7.0,
          .width = 8.0,
          .height = 7.0,
      },
  };
}

} // namespace deskflow::gui::input
