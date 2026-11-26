/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "InputIndicatorGeometry.h"

#include <array>
#include <span>

namespace deskflow::gui::input {

enum class MacInputIndicatorCatPathCommandType : unsigned char
{
  Move,
  Line,
  Cubic,
  Close,
};

struct MacInputIndicatorCatPathCommand
{
  MacInputIndicatorCatPathCommandType type =
      MacInputIndicatorCatPathCommandType::Move;
  float x1 = 0;
  float y1 = 0;
  float x2 = 0;
  float y2 = 0;
  float x3 = 0;
  float y3 = 0;
};

struct MacInputIndicatorCatArtwork
{
  std::span<const MacInputIndicatorCatPathCommand> near;
  std::span<const MacInputIndicatorCatPathCommand> far;
  InputIndicatorRect silhouetteBounds;
  InputIndicatorRect languageBounds;
};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 43> kCatIdleRiseNearCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 18.84970f, 14.93356f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.31564f, 15.13858f, 19.59520f, 15.39485f, 19.96796f, 15.65111f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.01455f, 16.11240f, 20.10774f, 16.57367f, 20.24752f, 16.93245f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.30965f, 17.10329f, 20.40283f, 17.13746f, 20.52708f, 17.03496f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.80665f, 16.82994f, 21.08621f, 16.36866f, 21.36578f, 16.00989f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.59875f, 16.06114f, 21.87831f, 16.00989f, 22.15787f, 15.85613f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.39084f, 16.06114f, 22.62381f, 16.41991f, 22.81019f, 16.52242f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.90338f, 16.59075f, 22.96550f, 16.53950f, 22.99657f, 16.36866f},
    {MacInputIndicatorCatPathCommandType::Line, 22.99657f, 15.39485f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.18294f, 15.03607f, 23.27613f, 14.52354f, 23.32272f, 14.21602f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.36932f, 13.95975f, 23.78866f, 14.01100f, 23.92845f, 13.70349f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.99057f, 13.56681f, 23.95951f, 13.44722f, 23.83526f, 13.34471f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.74207f, 13.24220f, 23.55569f, 13.19095f, 23.41591f, 13.08844f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.18294f, 12.78093f, 22.90338f, 12.57591f, 22.57722f, 12.57591f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.15787f, 12.62716f, 21.78512f, 12.42215f, 21.55215f, 12.01212f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.31918f, 11.29457f, 21.41237f, 10.47452f, 21.73853f, 9.50070f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.01809f, 8.62939f, 22.11128f, 7.86059f, 22.11128f, 7.09179f},
    {MacInputIndicatorCatPathCommandType::Line, 22.11128f, 5.40043f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.51510f, 5.36627f, 22.79466f, 5.24667f, 22.94997f, 5.04166f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.16741f, 4.59746f, 23.02763f, 4.35828f, 22.53063f, 4.32410f},
    {MacInputIndicatorCatPathCommandType::Line, 21.27259f, 4.32410f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.99302f, 4.32410f, 20.82218f, 4.47787f, 20.76005f, 4.78539f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.62027f, 5.60544f, 20.52708f, 6.32299f, 20.34071f, 7.09179f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.21646f, 7.46765f, 20.07668f, 7.67267f, 19.92136f, 7.70683f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.68839f, 7.70683f, 19.45542f, 6.98928f, 19.22245f, 6.47675f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.16033f, 6.34007f, 19.22245f, 6.20340f, 19.40883f, 6.06672f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.01455f, 5.70795f, 20.34071f, 5.24667f, 20.06114f, 4.78539f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.93689f, 4.51204f, 19.54861f, 4.35828f, 18.89629f, 4.32410f},
    {MacInputIndicatorCatPathCommandType::Line, 14.88921f, 4.32410f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.05052f, 4.32410f, 13.49139f, 4.58037f, 13.21182f, 4.83664f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.11864f, 4.93915f, 12.97885f, 4.95623f, 12.79248f, 4.88790f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.44125f, 4.37536f, 8.97177f, 4.11910f, 6.96823f, 4.68288f},
    {MacInputIndicatorCatPathCommandType::Cubic, 5.19765f, 5.19541f, 4.03280f, 6.47675f, 3.75324f, 7.80934f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.61346f, 8.47564f, 3.79983f, 8.98817f, 4.03280f, 9.09068f},
    {MacInputIndicatorCatPathCommandType::Cubic, 4.21918f, 9.15901f, 4.31237f, 9.00525f, 4.31237f, 8.62939f},
    {MacInputIndicatorCatPathCommandType::Cubic, 4.35896f, 7.45057f, 5.38403f, 6.11797f, 7.20120f, 5.60544f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.06496f, 5.09291f, 11.16169f, 5.40043f, 12.32654f, 5.91297f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.48185f, 5.98130f, 12.52845f, 6.11797f, 12.46632f, 6.32299f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.04697f, 7.55308f, 12.23335f, 8.83441f, 13.07204f, 9.80822f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.81755f, 10.73079f, 14.88921f, 10.88455f, 15.58812f, 11.70460f},
    {MacInputIndicatorCatPathCommandType::Cubic, 16.33362f, 12.57591f, 16.75297f, 12.83217f, 17.73144f, 13.60098f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.19738f, 14.01100f, 18.52354f, 14.47229f, 18.84970f, 14.93356f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 9> kCatIdleRiseFarCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 20.89984f, 9.19319f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.06469f, 9.29569f, 22.15787f, 8.27062f, 22.34425f, 7.04054f},
    {MacInputIndicatorCatPathCommandType::Line, 22.57722f, 5.60544f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.01210f, 5.60544f, 23.27613f, 5.46876f, 23.36932f, 5.19541f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.43144f, 4.95623f, 23.24507f, 4.83664f, 22.81019f, 4.83664f},
    {MacInputIndicatorCatPathCommandType::Line, 21.83172f, 4.83664f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.58322f, 4.83664f, 21.42790f, 4.99041f, 21.36578f, 5.29792f},
    {MacInputIndicatorCatPathCommandType::Line, 21.13281f, 6.83553f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 43> kCatIdleNeutralNearCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 18.84970f, 14.93356f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.31564f, 15.13858f, 19.59520f, 15.39485f, 19.96796f, 15.65111f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.01455f, 16.11240f, 20.10774f, 16.57367f, 20.24752f, 16.93245f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.30965f, 17.10329f, 20.40283f, 17.13746f, 20.52708f, 17.03496f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.80665f, 16.82994f, 21.08621f, 16.36866f, 21.36578f, 16.00989f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.59875f, 16.06114f, 21.87831f, 16.00989f, 22.15787f, 15.85613f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.39084f, 16.06114f, 22.62381f, 16.41991f, 22.81019f, 16.52242f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.90338f, 16.59075f, 22.96550f, 16.53950f, 22.99657f, 16.36866f},
    {MacInputIndicatorCatPathCommandType::Line, 22.99657f, 15.39485f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.18294f, 15.03607f, 23.27613f, 14.52354f, 23.32272f, 14.21602f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.36932f, 13.95975f, 23.78866f, 14.01100f, 23.92845f, 13.70349f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.99057f, 13.56681f, 23.95951f, 13.44722f, 23.83526f, 13.34471f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.74207f, 13.24220f, 23.55569f, 13.19095f, 23.41591f, 13.08844f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.18294f, 12.78093f, 22.90338f, 12.57591f, 22.57722f, 12.57591f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.15787f, 12.62716f, 21.78512f, 12.42215f, 21.55215f, 12.01212f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.31918f, 11.29457f, 21.41237f, 10.47452f, 21.73853f, 9.50070f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.01809f, 8.62939f, 22.11128f, 7.86059f, 22.11128f, 7.09179f},
    {MacInputIndicatorCatPathCommandType::Line, 22.11128f, 5.40043f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.51510f, 5.36627f, 22.79466f, 5.24667f, 22.94997f, 5.04166f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.16741f, 4.59746f, 23.02763f, 4.35828f, 22.53063f, 4.32410f},
    {MacInputIndicatorCatPathCommandType::Line, 21.27259f, 4.32410f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.99302f, 4.32410f, 20.82218f, 4.47787f, 20.76005f, 4.78539f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.62027f, 5.60544f, 20.52708f, 6.32299f, 20.34071f, 7.09179f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.21646f, 7.46765f, 20.07668f, 7.67267f, 19.92136f, 7.70683f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.68839f, 7.70683f, 19.45542f, 6.98928f, 19.22245f, 6.47675f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.16033f, 6.34007f, 19.22245f, 6.20340f, 19.40883f, 6.06672f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.01455f, 5.70795f, 20.34071f, 5.24667f, 20.06114f, 4.78539f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.93689f, 4.51204f, 19.54861f, 4.35828f, 18.89629f, 4.32410f},
    {MacInputIndicatorCatPathCommandType::Line, 14.88921f, 4.32410f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.05052f, 4.32410f, 13.49139f, 4.58037f, 13.21182f, 4.83664f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.11864f, 4.93915f, 12.97885f, 4.95623f, 12.79248f, 4.88790f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.44125f, 4.37536f, 9.11155f, 3.91408f, 7.10801f, 4.27285f},
    {MacInputIndicatorCatPathCommandType::Cubic, 5.10447f, 4.63163f, 3.66005f, 5.75921f, 3.33389f, 7.14304f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.19411f, 7.75808f, 3.33389f, 8.06561f, 3.52027f, 8.06561f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.70665f, 8.06561f, 3.81537f, 7.91184f, 3.84643f, 7.60433f},
    {MacInputIndicatorCatPathCommandType::Cubic, 4.03280f, 6.47675f, 5.38403f, 5.55419f, 7.20120f, 5.24667f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.29793f, 4.88790f, 11.16169f, 5.34917f, 12.32654f, 5.91297f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.48185f, 5.98130f, 12.52845f, 6.11797f, 12.46632f, 6.32299f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.04697f, 7.55308f, 12.23335f, 8.83441f, 13.07204f, 9.80822f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.81755f, 10.73079f, 14.88921f, 10.88455f, 15.58812f, 11.70460f},
    {MacInputIndicatorCatPathCommandType::Cubic, 16.33362f, 12.57591f, 16.75297f, 12.83217f, 17.73144f, 13.60098f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.19738f, 14.01100f, 18.52354f, 14.47229f, 18.84970f, 14.93356f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 9> kCatIdleNeutralFarCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 20.89984f, 9.19319f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.06469f, 9.29569f, 22.15787f, 8.27062f, 22.34425f, 7.04054f},
    {MacInputIndicatorCatPathCommandType::Line, 22.57722f, 5.60544f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.01210f, 5.60544f, 23.27613f, 5.46876f, 23.36932f, 5.19541f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.43144f, 4.95623f, 23.24507f, 4.83664f, 22.81019f, 4.83664f},
    {MacInputIndicatorCatPathCommandType::Line, 21.83172f, 4.83664f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.58322f, 4.83664f, 21.42790f, 4.99041f, 21.36578f, 5.29792f},
    {MacInputIndicatorCatPathCommandType::Line, 21.13281f, 6.83553f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 43> kCatIdleFallNearCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 18.84970f, 14.93356f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.31564f, 15.13858f, 19.59520f, 15.39485f, 19.96796f, 15.65111f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.01455f, 16.11240f, 20.10774f, 16.57367f, 20.24752f, 16.93245f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.30965f, 17.10329f, 20.40283f, 17.13746f, 20.52708f, 17.03496f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.80665f, 16.82994f, 21.08621f, 16.36866f, 21.36578f, 16.00989f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.59875f, 16.06114f, 21.87831f, 16.00989f, 22.15787f, 15.85613f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.39084f, 16.06114f, 22.62381f, 16.41991f, 22.81019f, 16.52242f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.90338f, 16.59075f, 22.96550f, 16.53950f, 22.99657f, 16.36866f},
    {MacInputIndicatorCatPathCommandType::Line, 22.99657f, 15.39485f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.18294f, 15.03607f, 23.27613f, 14.52354f, 23.32272f, 14.21602f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.36932f, 13.95975f, 23.78866f, 14.01100f, 23.92845f, 13.70349f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.99057f, 13.56681f, 23.95951f, 13.44722f, 23.83526f, 13.34471f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.74207f, 13.24220f, 23.55569f, 13.19095f, 23.41591f, 13.08844f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.18294f, 12.78093f, 22.90338f, 12.57591f, 22.57722f, 12.57591f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.15787f, 12.62716f, 21.78512f, 12.42215f, 21.55215f, 12.01212f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.31918f, 11.29457f, 21.41237f, 10.47452f, 21.73853f, 9.50070f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.01809f, 8.62939f, 22.11128f, 7.86059f, 22.11128f, 7.09179f},
    {MacInputIndicatorCatPathCommandType::Line, 22.11128f, 5.40043f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.51510f, 5.36627f, 22.79466f, 5.24667f, 22.94997f, 5.04166f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.16741f, 4.59746f, 23.02763f, 4.35828f, 22.53063f, 4.32410f},
    {MacInputIndicatorCatPathCommandType::Line, 21.27259f, 4.32410f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.99302f, 4.32410f, 20.82218f, 4.47787f, 20.76005f, 4.78539f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.62027f, 5.60544f, 20.52708f, 6.32299f, 20.34071f, 7.09179f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.21646f, 7.46765f, 20.07668f, 7.67267f, 19.92136f, 7.70683f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.68839f, 7.70683f, 19.45542f, 6.98928f, 19.22245f, 6.47675f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.16033f, 6.34007f, 19.22245f, 6.20340f, 19.40883f, 6.06672f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.01455f, 5.70795f, 20.34071f, 5.24667f, 20.06114f, 4.78539f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.93689f, 4.51204f, 19.54861f, 4.35828f, 18.89629f, 4.32410f},
    {MacInputIndicatorCatPathCommandType::Line, 14.88921f, 4.32410f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.05052f, 4.32410f, 13.49139f, 4.58037f, 13.21182f, 4.83664f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.11864f, 4.93915f, 12.97885f, 4.95623f, 12.79248f, 4.88790f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.44125f, 4.32410f, 8.97177f, 3.81157f, 6.82844f, 4.06784f},
    {MacInputIndicatorCatPathCommandType::Cubic, 4.87150f, 4.32410f, 3.56686f, 5.09291f, 3.10092f, 6.32299f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.86795f, 6.93803f, 2.96114f, 7.29681f, 3.14752f, 7.34806f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.33389f, 7.38223f, 3.47368f, 7.21138f, 3.56686f, 6.83553f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.98621f, 5.81046f, 5.24425f, 5.19541f, 6.96823f, 5.04166f},
    {MacInputIndicatorCatPathCommandType::Cubic, 8.83199f, 4.88790f, 11.16169f, 5.34917f, 12.32654f, 5.91297f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.48185f, 5.98130f, 12.52845f, 6.11797f, 12.46632f, 6.32299f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.04697f, 7.55308f, 12.23335f, 8.83441f, 13.07204f, 9.80822f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.81755f, 10.73079f, 14.88921f, 10.88455f, 15.58812f, 11.70460f},
    {MacInputIndicatorCatPathCommandType::Cubic, 16.33362f, 12.57591f, 16.75297f, 12.83217f, 17.73144f, 13.60098f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.19738f, 14.01100f, 18.52354f, 14.47229f, 18.84970f, 14.93356f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 9> kCatIdleFallFarCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 20.89984f, 9.19319f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.06469f, 9.29569f, 22.15787f, 8.27062f, 22.34425f, 7.04054f},
    {MacInputIndicatorCatPathCommandType::Line, 22.57722f, 5.60544f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.01210f, 5.60544f, 23.27613f, 5.46876f, 23.36932f, 5.19541f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.43144f, 4.95623f, 23.24507f, 4.83664f, 22.81019f, 4.83664f},
    {MacInputIndicatorCatPathCommandType::Line, 21.83172f, 4.83664f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.58322f, 4.83664f, 21.42790f, 4.99041f, 21.36578f, 5.29792f},
    {MacInputIndicatorCatPathCommandType::Line, 21.13281f, 6.83553f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 48> kCatWalkReachNearCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 19.36223f, 12.83217f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.82817f, 13.03719f, 20.10774f, 13.29346f, 20.48049f, 13.54973f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.52708f, 14.01100f, 20.62027f, 14.47229f, 20.76005f, 14.83106f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.82218f, 15.00190f, 20.91537f, 15.03607f, 21.03962f, 14.93356f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.31918f, 14.72855f, 21.59875f, 14.26727f, 21.87831f, 13.90849f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.11128f, 13.95975f, 22.39084f, 13.90849f, 22.67041f, 13.75473f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.90338f, 13.95975f, 23.13635f, 14.31853f, 23.32272f, 14.42103f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.41591f, 14.48937f, 23.47804f, 14.43812f, 23.50910f, 14.26727f},
    {MacInputIndicatorCatPathCommandType::Line, 23.50910f, 13.29346f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.69548f, 12.93468f, 23.78866f, 12.42215f, 23.83526f, 12.11462f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.88185f, 11.85836f, 24.30120f, 11.90962f, 24.44098f, 11.60209f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.50311f, 11.46541f, 24.47204f, 11.34582f, 24.34779f, 11.24332f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.25460f, 11.14081f, 24.06823f, 11.08956f, 23.92845f, 10.98706f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.69548f, 10.67953f, 23.41591f, 10.47452f, 23.08975f, 10.47452f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.67041f, 10.52577f, 22.29766f, 10.32075f, 22.06469f, 9.91073f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.11128f, 9.44946f, 22.29766f, 9.03942f, 22.29766f, 8.37313f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.25106f, 7.80934f, 22.39084f, 7.45057f, 22.53063f, 6.88677f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.76360f, 6.27174f, 22.90338f, 5.60544f, 22.99657f, 5.04166f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.24507f, 5.04166f, 23.44698f, 4.95623f, 23.60229f, 4.78539f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.75760f, 4.51204f, 23.66441f, 4.35828f, 23.32272f, 4.32410f},
    {MacInputIndicatorCatPathCommandType::Line, 22.67041f, 4.32410f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.45297f, 4.32410f, 22.31319f, 4.46078f, 22.25106f, 4.73414f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.01809f, 5.70795f, 21.78512f, 6.37424f, 21.41237f, 6.83553f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.08621f, 7.29681f, 20.89984f, 7.34806f, 20.62027f, 7.39931f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.37177f, 7.46765f, 20.12327f, 7.55308f, 19.87477f, 7.65558f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.15079f, 8.16811f, 16.47341f, 8.37313f, 14.88921f, 7.96310f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.42327f, 7.86059f, 14.19030f, 7.39931f, 14.00392f, 6.88677f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.77095f, 6.27174f, 13.25842f, 6.06672f, 13.07204f, 5.55419f},
    {MacInputIndicatorCatPathCommandType::Line, 12.88567f, 4.99041f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.16523f, 5.05874f, 13.39820f, 4.99041f, 13.58458f, 4.78539f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.80202f, 4.51204f, 13.75542f, 4.34120f, 13.44479f, 4.27285f},
    {MacInputIndicatorCatPathCommandType::Line, 12.65270f, 4.27285f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.40419f, 4.27285f, 12.24888f, 4.40953f, 12.18676f, 4.68288f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.09357f, 5.19541f, 12.37313f, 5.60544f, 12.37313f, 6.16923f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.34207f, 6.54509f, 12.14016f, 6.86969f, 11.76741f, 7.14304f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.20828f, 7.80934f, 10.55597f, 8.21937f, 10.22981f, 8.62939f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.13662f, 8.80024f, 9.98131f, 8.86858f, 9.76387f, 8.83441f},
    {MacInputIndicatorCatPathCommandType::Cubic, 8.41264f, 8.78315f, 6.54888f, 9.09068f, 5.19765f, 9.91073f},
    {MacInputIndicatorCatPathCommandType::Cubic, 4.12599f, 10.57702f, 3.28730f, 11.55084f, 2.86795f, 12.57591f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.58839f, 13.19095f, 2.58839f, 13.80599f, 2.82136f, 13.85724f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.97667f, 13.92559f, 3.11646f, 13.68640f, 3.24071f, 13.13969f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.66005f, 12.11462f, 4.54534f, 11.29457f, 5.61700f, 10.73079f},
    {MacInputIndicatorCatPathCommandType::Cubic, 7.01482f, 9.96199f, 8.64561f, 9.70572f, 9.71727f, 9.85948f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.90365f, 9.89365f, 10.02790f, 10.01324f, 10.09003f, 10.21826f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.32300f, 11.24332f, 11.11509f, 12.16588f, 12.41973f, 12.57591f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.63117f, 12.93468f, 15.07558f, 12.62716f, 16.24044f, 12.62716f},
    {MacInputIndicatorCatPathCommandType::Cubic, 17.45188f, 12.57591f, 18.47695f, 12.93468f, 19.36223f, 12.83217f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 21> kCatWalkReachFarCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 20.34071f, 9.29569f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.08621f, 9.50070f, 21.50556f, 8.78315f, 21.27259f, 8.06561f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.03962f, 7.14304f, 20.57368f, 6.06672f, 20.34071f, 5.34917f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.55815f, 5.34917f, 20.71346f, 5.26376f, 20.80665f, 5.09291f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.89984f, 4.85373f, 20.79112f, 4.71705f, 20.48049f, 4.68288f},
    {MacInputIndicatorCatPathCommandType::Line, 19.82817f, 4.68288f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.61074f, 4.68288f, 19.51755f, 4.83664f, 19.54861f, 5.14416f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.59520f, 6.01548f, 19.73499f, 6.68177f, 19.82817f, 7.29681f},
    {MacInputIndicatorCatPathCommandType::Line, 19.78158f, 8.47564f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Move, 11.06850f, 9.39820f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.86060f, 9.60321f, 12.60610f, 9.03942f, 12.37313f, 8.16811f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.18676f, 7.45057f, 11.62763f, 6.93803f, 11.20828f, 6.52801f},
    {MacInputIndicatorCatPathCommandType::Line, 10.88212f, 5.70795f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.09956f, 5.77629f, 11.27041f, 5.74213f, 11.39466f, 5.60544f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.54997f, 5.40043f, 11.48785f, 5.24667f, 11.20828f, 5.14416f},
    {MacInputIndicatorCatPathCommandType::Line, 10.60256f, 4.99041f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.35406f, 4.92206f, 10.19874f, 5.02457f, 10.13662f, 5.29792f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.09003f, 5.65670f, 10.27640f, 6.11797f, 10.32300f, 6.68177f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.36959f, 7.04054f, 10.60256f, 7.45057f, 10.74234f, 7.96310f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 49> kCatWalkPassNearCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 19.17586f, 13.08844f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.64180f, 13.29346f, 19.92136f, 13.54973f, 20.29411f, 13.80599f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.34071f, 14.26727f, 20.43390f, 14.72855f, 20.57368f, 15.08733f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.63580f, 15.25817f, 20.72899f, 15.29234f, 20.85324f, 15.18983f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.13281f, 14.98482f, 21.41237f, 14.52354f, 21.69193f, 14.16476f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.92490f, 14.21602f, 22.20447f, 14.16476f, 22.48403f, 14.01100f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.71700f, 14.21602f, 22.94997f, 14.57480f, 23.13635f, 14.67729f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.22954f, 14.74564f, 23.29166f, 14.69439f, 23.32272f, 14.52354f},
    {MacInputIndicatorCatPathCommandType::Line, 23.32272f, 13.54973f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.50910f, 13.19095f, 23.60229f, 12.67842f, 23.64888f, 12.37089f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.69548f, 12.11462f, 24.11482f, 12.16588f, 24.25460f, 11.85836f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.31673f, 11.72168f, 24.28567f, 11.60209f, 24.16142f, 11.49959f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.06823f, 11.39708f, 23.88185f, 11.34582f, 23.74207f, 11.24332f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.50910f, 10.93580f, 23.22954f, 10.73079f, 22.90338f, 10.73079f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.48403f, 10.78204f, 22.11128f, 10.57702f, 21.87831f, 10.16700f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.69193f, 9.80822f, 21.83172f, 9.09068f, 21.83172f, 8.52689f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.83172f, 8.01435f, 21.64534f, 7.65558f, 21.50556f, 7.24555f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.31918f, 6.68177f, 21.31918f, 5.91297f, 21.41237f, 5.09291f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.66087f, 5.09291f, 21.86278f, 5.00749f, 22.01809f, 4.83664f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.17341f, 4.56329f, 22.08022f, 4.39245f, 21.73853f, 4.32410f},
    {MacInputIndicatorCatPathCommandType::Line, 20.99302f, 4.32410f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.77559f, 4.32410f, 20.66687f, 4.46078f, 20.66687f, 4.73414f},
    {MacInputIndicatorCatPathCommandType::Line, 20.66687f, 6.01548f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.66687f, 6.57926f, 20.62027f, 6.88677f, 20.43390f, 7.19430f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.30965f, 7.43348f, 20.07668f, 7.60433f, 19.73499f, 7.70683f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.61673f, 8.27062f, 17.07913f, 8.27062f, 15.72790f, 8.01435f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.93580f, 7.86059f, 14.51646f, 7.60433f, 14.23689f, 7.14304f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.81755f, 6.42550f, 13.02545f, 6.11797f, 12.46632f, 5.65670f},
    {MacInputIndicatorCatPathCommandType::Line, 12.00038f, 4.88790f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.24888f, 4.85373f, 12.40419f, 4.73414f, 12.46632f, 4.52912f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.55951f, 4.28994f, 12.45079f, 4.15326f, 12.14016f, 4.11910f},
    {MacInputIndicatorCatPathCommandType::Line, 11.44125f, 4.11910f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.19275f, 4.11910f, 11.08403f, 4.25577f, 11.11509f, 4.52912f},
    {MacInputIndicatorCatPathCommandType::Line, 11.30147f, 5.40043f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.16169f, 5.96422f, 11.48785f, 6.47675f, 11.62763f, 6.98928f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.65869f, 7.33098f, 11.47231f, 7.62141f, 11.06850f, 7.86059f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.60256f, 8.13394f, 10.29193f, 8.45855f, 10.13662f, 8.83441f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.04343f, 9.00525f, 9.90365f, 9.12485f, 9.71727f, 9.19319f},
    {MacInputIndicatorCatPathCommandType::Cubic, 8.03989f, 9.34695f, 6.36250f, 10.01324f, 4.87150f, 11.19206f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.89302f, 11.96087f, 3.33389f, 12.88342f, 3.10092f, 13.65222f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.96114f, 14.21602f, 3.00774f, 14.57480f, 3.19411f, 14.57480f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.34943f, 14.57480f, 3.47368f, 14.38686f, 3.56686f, 14.01100f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.84643f, 13.19095f, 4.59193f, 12.37089f, 5.47722f, 11.75586f},
    {MacInputIndicatorCatPathCommandType::Cubic, 6.96823f, 10.73079f, 8.55242f, 10.26951f, 9.71727f, 10.21826f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.90365f, 10.21826f, 10.04343f, 10.33784f, 10.13662f, 10.57702f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.41618f, 11.55084f, 11.20828f, 12.37089f, 12.51291f, 12.78093f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.81755f, 13.19095f, 14.98240f, 12.83217f, 16.10065f, 12.83217f},
    {MacInputIndicatorCatPathCommandType::Cubic, 17.40529f, 12.83217f, 18.24398f, 13.13969f, 19.17586f, 13.08844f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 22> kCatWalkPassFarCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 20.80665f, 9.39820f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.45896f, 9.70572f, 22.11128f, 9.19319f, 22.43744f, 8.47564f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.94997f, 7.65558f, 23.32272f, 7.04054f, 23.41591f, 6.42550f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.44698f, 6.22048f, 23.38485f, 6.03256f, 23.22954f, 5.86171f},
    {MacInputIndicatorCatPathCommandType::Line, 22.76360f, 5.40043f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.57722f, 5.22958f, 22.39084f, 5.21250f, 22.20447f, 5.34917f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.98703f, 5.48586f, 21.92490f, 5.67378f, 22.01809f, 5.91297f},
    {MacInputIndicatorCatPathCommandType::Line, 22.43744f, 6.52801f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.15787f, 6.83553f, 21.69193f, 7.09179f, 21.27259f, 7.45057f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.05515f, 7.62141f, 20.80665f, 7.75808f, 20.52708f, 7.86059f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Move, 11.53444f, 9.75697f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.41973f, 10.16700f, 13.16523f, 9.55195f, 13.49139f, 8.62939f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.91073f, 7.70683f, 13.77095f, 6.93803f, 13.81755f, 5.86171f},
    {MacInputIndicatorCatPathCommandType::Line, 13.86414f, 5.40043f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.11264f, 5.36627f, 14.29902f, 5.26376f, 14.42327f, 5.09291f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.54752f, 4.85373f, 14.45433f, 4.69996f, 14.14370f, 4.63163f},
    {MacInputIndicatorCatPathCommandType::Line, 13.39820f, 4.58037f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.14970f, 4.58037f, 12.99439f, 4.71705f, 12.93226f, 4.99041f},
    {MacInputIndicatorCatPathCommandType::Line, 12.69929f, 6.52801f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.26441f, 6.76719f, 11.84507f, 7.12596f, 11.44125f, 7.60433f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 46> kCatWalkPushNearCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 19.31564f, 12.93468f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.78158f, 13.13969f, 20.06114f, 13.39596f, 20.43390f, 13.65222f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.48049f, 14.11351f, 20.57368f, 14.57480f, 20.71346f, 14.93356f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.77559f, 15.10441f, 20.86877f, 15.13858f, 20.99302f, 15.03607f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.27259f, 14.83106f, 21.55215f, 14.36978f, 21.83172f, 14.01100f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.06469f, 14.06226f, 22.34425f, 14.01100f, 22.62381f, 13.85724f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.85678f, 14.06226f, 23.08975f, 14.42103f, 23.27613f, 14.52354f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.36932f, 14.59188f, 23.43144f, 14.54062f, 23.46251f, 14.36978f},
    {MacInputIndicatorCatPathCommandType::Line, 23.46251f, 13.39596f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.64888f, 13.03719f, 23.74207f, 12.52466f, 23.78866f, 12.21713f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.83526f, 11.96087f, 24.25460f, 12.01212f, 24.39439f, 11.70460f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.45651f, 11.56792f, 24.42545f, 11.44833f, 24.30120f, 11.34582f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.20801f, 11.24332f, 24.02163f, 11.19206f, 23.88185f, 11.08956f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.64888f, 10.78204f, 23.36932f, 10.57702f, 23.04316f, 10.57702f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.62381f, 10.62828f, 22.25106f, 10.42326f, 22.01809f, 10.01324f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.78512f, 9.50070f, 22.06469f, 8.88566f, 21.92490f, 8.27062f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.73853f, 7.55308f, 21.45896f, 6.63051f, 21.36578f, 5.70795f},
    {MacInputIndicatorCatPathCommandType::Line, 21.36578f, 5.04166f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.58322f, 5.04166f, 21.75406f, 4.93915f, 21.87831f, 4.73414f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.97150f, 4.46078f, 21.84725f, 4.32410f, 21.50556f, 4.32410f},
    {MacInputIndicatorCatPathCommandType::Line, 20.85324f, 4.32410f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.60474f, 4.32410f, 20.48049f, 4.47787f, 20.48049f, 4.78539f},
    {MacInputIndicatorCatPathCommandType::Line, 20.52708f, 6.32299f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.55815f, 6.93803f, 20.30965f, 7.38223f, 19.78158f, 7.65558f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.38376f, 8.27062f, 16.98594f, 8.42439f, 15.49493f, 8.21937f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.96687f, 8.11686f, 14.68730f, 7.84351f, 14.65624f, 7.39931f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.60964f, 6.83553f, 14.84261f, 6.52801f, 14.93580f, 6.16923f},
    {MacInputIndicatorCatPathCommandType::Cubic, 15.18430f, 6.20340f, 15.38621f, 6.13507f, 15.54152f, 5.96422f},
    {MacInputIndicatorCatPathCommandType::Cubic, 15.72790f, 5.72503f, 15.69684f, 5.50294f, 15.44834f, 5.29792f},
    {MacInputIndicatorCatPathCommandType::Line, 14.88921f, 4.99041f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.64071f, 4.85373f, 14.42327f, 4.93915f, 14.23689f, 5.24667f},
    {MacInputIndicatorCatPathCommandType::Line, 13.53798f, 6.22048f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.74588f, 6.68177f, 11.95379f, 6.93803f, 11.25488f, 7.50182f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.82000f, 7.84351f, 10.49384f, 8.18520f, 10.27640f, 8.52689f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.18321f, 8.69774f, 10.02790f, 8.76607f, 9.81046f, 8.73190f},
    {MacInputIndicatorCatPathCommandType::Cubic, 8.59902f, 8.57814f, 6.82844f, 8.88566f, 5.43062f, 9.55195f},
    {MacInputIndicatorCatPathCommandType::Cubic, 4.21918f, 10.11575f, 3.19411f, 10.98706f, 2.63498f, 11.90962f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.26223f, 12.52466f, 2.12245f, 12.93468f, 2.30883f, 13.08844f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.46414f, 13.19095f, 2.65052f, 13.03719f, 2.86795f, 12.62716f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.47368f, 11.70460f, 4.45215f, 11.03831f, 5.75678f, 10.47452f},
    {MacInputIndicatorCatPathCommandType::Cubic, 7.15460f, 9.85948f, 8.73880f, 9.70572f, 9.71727f, 9.75697f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.93471f, 9.75697f, 10.07449f, 9.87656f, 10.13662f, 10.11575f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.36959f, 11.24332f, 11.30147f, 12.37089f, 12.65270f, 12.62716f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.81755f, 12.83217f, 15.07558f, 12.67842f, 16.28703f, 12.67842f},
    {MacInputIndicatorCatPathCommandType::Cubic, 17.49847f, 12.67842f, 18.38376f, 12.93468f, 19.31564f, 12.93468f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 21> kCatWalkPushFarCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 20.43390f, 9.34695f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.17940f, 9.50070f, 22.01809f, 9.03942f, 22.43744f, 8.32188f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.99657f, 7.55308f, 23.22954f, 6.83553f, 23.50910f, 6.32299f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.75760f, 6.28883f, 23.95951f, 6.16923f, 24.11482f, 5.96422f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.30120f, 5.69086f, 24.20801f, 5.52002f, 23.83526f, 5.45168f},
    {MacInputIndicatorCatPathCommandType::Line, 23.32272f, 5.45168f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.04316f, 5.45168f, 22.84125f, 5.58836f, 22.71700f, 5.86171f},
    {MacInputIndicatorCatPathCommandType::Line, 22.11128f, 6.78428f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.58322f, 7.19430f, 21.08621f, 7.51890f, 20.62027f, 7.75808f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Move, 11.39466f, 9.75697f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.18676f, 9.70572f, 12.93226f, 8.93692f, 12.79248f, 8.01435f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.65270f, 7.24555f, 12.00038f, 6.78428f, 11.62763f, 6.11797f},
    {MacInputIndicatorCatPathCommandType::Line, 11.16169f, 5.24667f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.44125f, 5.24667f, 11.61210f, 5.14416f, 11.67422f, 4.93915f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.76741f, 4.69996f, 11.64316f, 4.56329f, 11.30147f, 4.52912f},
    {MacInputIndicatorCatPathCommandType::Line, 10.60256f, 4.52912f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.32300f, 4.52912f, 10.21428f, 4.68288f, 10.27640f, 4.99041f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.36959f, 5.96422f, 10.74234f, 6.63051f, 10.83553f, 7.50182f},
    {MacInputIndicatorCatPathCommandType::Line, 10.60256f, 8.62939f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 45> kCatRunCoilNearCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 18.89629f, 11.80711f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.36223f, 12.01212f, 19.64180f, 12.26839f, 20.01455f, 12.52466f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.06114f, 12.98593f, 20.15433f, 13.44722f, 20.29411f, 13.80599f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.35624f, 13.97684f, 20.44943f, 14.01100f, 20.57368f, 13.90849f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.85324f, 13.70349f, 21.13281f, 13.24220f, 21.41237f, 12.88342f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.64534f, 12.93468f, 21.92490f, 12.88342f, 22.20447f, 12.72967f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.43744f, 12.93468f, 22.67041f, 13.29346f, 22.85678f, 13.39596f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.94997f, 13.46430f, 23.01210f, 13.41305f, 23.04316f, 13.24220f},
    {MacInputIndicatorCatPathCommandType::Line, 23.04316f, 12.26839f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.22954f, 11.90962f, 23.32272f, 11.39708f, 23.36932f, 11.08956f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.41591f, 10.83329f, 23.83526f, 10.88455f, 23.97504f, 10.57702f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.03717f, 10.44035f, 24.00610f, 10.32075f, 23.88185f, 10.21826f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.78866f, 10.11575f, 23.60229f, 10.06449f, 23.46251f, 9.96199f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.22954f, 9.65446f, 22.94997f, 9.44946f, 22.62381f, 9.44946f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.20447f, 9.50070f, 21.83172f, 9.29569f, 21.59875f, 8.88566f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.36578f, 8.37313f, 21.50556f, 7.65558f, 21.22599f, 7.09179f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.94643f, 6.47675f, 20.20093f, 5.91297f, 19.96796f, 5.19541f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.34071f, 5.16125f, 20.58921f, 5.00749f, 20.71346f, 4.73414f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.83771f, 4.42661f, 20.66687f, 4.27285f, 20.20093f, 4.27285f},
    {MacInputIndicatorCatPathCommandType::Line, 18.98948f, 4.27285f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.70992f, 4.27285f, 18.58567f, 4.42661f, 18.61673f, 4.73414f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.61673f, 5.34917f, 18.98948f, 6.22048f, 19.17586f, 6.68177f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.30011f, 7.02346f, 19.22245f, 7.22847f, 18.94289f, 7.29681f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.24398f, 7.50182f, 17.26550f, 7.45057f, 16.56659f, 7.09179f},
    {MacInputIndicatorCatPathCommandType::Cubic, 16.38022f, 6.95512f, 16.27150f, 6.75010f, 16.24044f, 6.47675f},
    {MacInputIndicatorCatPathCommandType::Cubic, 16.14725f, 5.86171f, 15.54152f, 5.60544f, 14.93580f, 5.45168f},
    {MacInputIndicatorCatPathCommandType::Line, 14.46986f, 4.99041f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.87368f, 4.99041f, 15.13771f, 4.87081f, 15.26196f, 4.63163f},
    {MacInputIndicatorCatPathCommandType::Cubic, 15.38621f, 4.35828f, 15.19984f, 4.22161f, 14.70283f, 4.22161f},
    {MacInputIndicatorCatPathCommandType::Line, 13.35161f, 4.22161f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.00992f, 4.22161f, 12.83907f, 4.39245f, 12.83907f, 4.73414f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.93226f, 5.50294f, 13.67776f, 6.16923f, 13.81755f, 6.68177f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.83907f, 6.83553f, 11.25488f, 7.29681f, 10.27640f, 8.01435f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.15215f, 8.15103f, 9.96577f, 8.18520f, 9.71727f, 8.11686f},
    {MacInputIndicatorCatPathCommandType::Cubic, 8.22627f, 7.96310f, 6.26932f, 8.27062f, 4.77831f, 8.83441f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.66005f, 9.24444f, 2.54180f, 9.85948f, 2.02926f, 10.88455f},
    {MacInputIndicatorCatPathCommandType::Cubic, 1.79629f, 11.34582f, 1.84289f, 11.80711f, 2.02926f, 11.80711f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.21564f, 11.80711f, 2.37095f, 11.65335f, 2.49520f, 11.34582f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.28730f, 10.26951f, 4.40556f, 9.91073f, 5.61700f, 9.55195f},
    {MacInputIndicatorCatPathCommandType::Cubic, 7.10801f, 9.09068f, 8.69221f, 9.03942f, 9.43771f, 9.19319f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.62409f, 9.22735f, 9.73280f, 9.34695f, 9.76387f, 9.55195f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.99684f, 10.78204f, 10.74234f, 11.90962f, 11.95379f, 12.62716f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.63117f, 13.65222f, 15.26196f, 13.24220f, 16.75297f, 12.52466f},
    {MacInputIndicatorCatPathCommandType::Cubic, 17.77804f, 12.11462f, 18.29057f, 11.85836f, 18.89629f, 11.80711f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 19> kCatRunCoilFarCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 19.87477f, 8.47564f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.89984f, 8.88566f, 21.73853f, 8.47564f, 21.83172f, 7.80934f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.83172f, 6.98928f, 21.13281f, 6.11797f, 20.89984f, 5.29792f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.30365f, 5.26376f, 21.55215f, 5.12708f, 21.64534f, 4.88790f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.70747f, 4.68288f, 21.52109f, 4.56329f, 21.08621f, 4.52912f},
    {MacInputIndicatorCatPathCommandType::Line, 20.20093f, 4.52912f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.92136f, 4.52912f, 19.79711f, 4.68288f, 19.82817f, 4.99041f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.92136f, 5.70795f, 20.01455f, 6.32299f, 20.10774f, 7.04054f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Move, 11.06850f, 9.75697f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.32654f, 10.11575f, 13.21182f, 9.34695f, 13.35161f, 8.27062f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.58458f, 7.39931f, 12.97885f, 6.63051f, 12.55951f, 5.70795f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.05651f, 5.70795f, 13.35161f, 5.55419f, 13.44479f, 5.24667f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.53798f, 4.97331f, 13.32054f, 4.83664f, 12.79248f, 4.83664f},
    {MacInputIndicatorCatPathCommandType::Line, 11.48785f, 4.83664f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.23934f, 4.87081f, 11.13063f, 5.02457f, 11.16169f, 5.29792f},
    {MacInputIndicatorCatPathCommandType::Line, 11.39466f, 6.78428f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.95978f, 7.33098f, 10.74234f, 7.84351f, 10.74234f, 8.32188f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 46> kCatRunAirNearCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 21.13281f, 13.54973f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.59875f, 13.75473f, 21.87831f, 14.01100f, 22.25106f, 14.26727f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.29766f, 14.72855f, 22.39084f, 15.18983f, 22.53063f, 15.54860f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.59275f, 15.71945f, 22.68594f, 15.75362f, 22.81019f, 15.65111f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.08975f, 15.44609f, 23.36932f, 14.98482f, 23.64888f, 14.62604f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.88185f, 14.67729f, 24.16142f, 14.62604f, 24.44098f, 14.47229f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.67395f, 14.67729f, 24.90692f, 15.03607f, 25.09330f, 15.13858f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.18649f, 15.20692f, 25.24861f, 15.15566f, 25.27967f, 14.98482f},
    {MacInputIndicatorCatPathCommandType::Line, 25.27967f, 14.01100f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.46605f, 13.65222f, 25.55924f, 13.13969f, 25.60583f, 12.83217f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.65243f, 12.57591f, 26.07177f, 12.62716f, 26.21155f, 12.31964f},
    {MacInputIndicatorCatPathCommandType::Cubic, 26.27368f, 12.18297f, 26.24262f, 12.06337f, 26.11837f, 11.96087f},
    {MacInputIndicatorCatPathCommandType::Cubic, 26.02518f, 11.85836f, 25.83880f, 11.80711f, 25.69902f, 11.70460f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.46605f, 11.39708f, 25.18649f, 11.19206f, 24.86033f, 11.19206f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.44098f, 11.24332f, 24.06823f, 11.03831f, 23.83526f, 10.62828f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.89738f, 10.28659f, 24.11482f, 10.01324f, 24.48758f, 9.80822f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.27967f, 9.39820f, 26.11837f, 9.09068f, 27.05025f, 8.83441f},
    {MacInputIndicatorCatPathCommandType::Line, 27.84234f, 8.62939f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 28.24616f, 8.73190f, 28.57232f, 8.66356f, 28.82082f, 8.42439f},
    {MacInputIndicatorCatPathCommandType::Cubic, 29.13144f, 8.08269f, 29.03826f, 7.82643f, 28.54125f, 7.65558f},
    {MacInputIndicatorCatPathCommandType::Line, 27.60937f, 7.55308f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 27.26768f, 7.51890f, 26.92599f, 7.55308f, 26.58431f, 7.65558f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.65243f, 7.91184f, 24.48758f, 8.06561f, 23.46251f, 8.01435f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.81019f, 8.01435f, 22.34425f, 8.16811f, 21.64534f, 8.47564f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.73499f, 9.24444f, 18.29057f, 9.50070f, 16.65978f, 9.50070f},
    {MacInputIndicatorCatPathCommandType::Cubic, 15.30855f, 9.50070f, 13.77095f, 9.19319f, 12.79248f, 8.68065f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.76741f, 8.11686f, 10.74234f, 8.01435f, 9.95024f, 7.70683f},
    {MacInputIndicatorCatPathCommandType::Line, 8.13308f, 6.93803f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 7.94670f, 6.63051f, 7.69820f, 6.47675f, 7.38757f, 6.47675f},
    {MacInputIndicatorCatPathCommandType::Line, 6.45569f, 6.47675f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 6.08294f, 6.54509f, 5.98975f, 6.71593f, 6.17613f, 6.98928f},
    {MacInputIndicatorCatPathCommandType::Cubic, 7.10801f, 7.96310f, 8.17967f, 8.11686f, 9.15815f, 8.78315f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.65515f, 9.09068f, 9.96577f, 9.46654f, 10.09003f, 9.91073f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.84152f, 10.21826f, 9.65515f, 10.49161f, 9.53090f, 10.73079f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.43771f, 10.86746f, 9.28240f, 10.93580f, 9.06496f, 10.93580f},
    {MacInputIndicatorCatPathCommandType::Cubic, 7.15460f, 11.08956f, 5.47722f, 11.49959f, 3.93962f, 12.31964f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.91455f, 12.83217f, 2.07586f, 13.29346f, 1.42354f, 13.54973f},
    {MacInputIndicatorCatPathCommandType::Cubic, 1.11291f, 13.61806f, 0.97313f, 13.73765f, 1.00419f, 13.90849f},
    {MacInputIndicatorCatPathCommandType::Cubic, 1.03526f, 14.07934f, 1.25269f, 14.13059f, 1.65651f, 14.06226f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.86795f, 13.80599f, 3.52027f, 13.60098f, 4.40556f, 13.24220f},
    {MacInputIndicatorCatPathCommandType::Cubic, 5.84997f, 12.62716f, 7.76033f, 12.21713f, 9.01836f, 12.16588f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.20474f, 12.16588f, 9.37558f, 12.23422f, 9.53090f, 12.37089f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.27640f, 12.98593f, 11.20828f, 13.34471f, 12.46632f, 13.34471f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.81755f, 13.34471f, 15.21537f, 12.93468f, 16.89275f, 12.88342f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.61673f, 12.88342f, 20.06114f, 13.34471f, 21.13281f, 13.54973f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 20> kCatRunAirFarCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 21.59875f, 10.11575f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.90338f, 10.11575f, 24.06823f, 9.60321f, 24.90692f, 8.57814f},
    {MacInputIndicatorCatPathCommandType::Line, 26.16496f, 7.39931f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 26.59984f, 7.36514f, 26.91046f, 7.22847f, 27.09684f, 6.98928f},
    {MacInputIndicatorCatPathCommandType::Cubic, 27.25215f, 6.64760f, 27.06578f, 6.45967f, 26.53771f, 6.42550f},
    {MacInputIndicatorCatPathCommandType::Line, 25.69902f, 6.42550f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.45052f, 6.45967f, 25.18649f, 6.57926f, 24.90692f, 6.78428f},
    {MacInputIndicatorCatPathCommandType::Line, 23.27613f, 7.96310f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.68594f, 8.13394f, 22.14234f, 8.28770f, 21.64534f, 8.42439f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Move, 11.76741f, 10.57702f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.74588f, 10.47452f, 13.07204f, 9.34695f, 12.37313f, 8.47564f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.90719f, 7.86059f, 11.11509f, 7.75808f, 10.41618f, 7.45057f},
    {MacInputIndicatorCatPathCommandType::Line, 8.83199f, 6.47675f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 8.92518f, 6.13507f, 8.75433f, 5.96422f, 8.31945f, 5.96422f},
    {MacInputIndicatorCatPathCommandType::Line, 7.29438f, 5.96422f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 7.07695f, 6.03256f, 7.04588f, 6.18632f, 7.20120f, 6.42550f},
    {MacInputIndicatorCatPathCommandType::Cubic, 7.66714f, 7.29681f, 8.69221f, 7.70683f, 9.29793f, 8.27062f},
    {MacInputIndicatorCatPathCommandType::Line, 10.32300f, 9.55195f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 45> kCatRunLandNearCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 20.20093f, 12.42215f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 20.66687f, 12.62716f, 20.94643f, 12.88342f, 21.31918f, 13.13969f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.36578f, 13.60098f, 21.45896f, 14.06226f, 21.59875f, 14.42103f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.66087f, 14.59188f, 21.75406f, 14.62604f, 21.87831f, 14.52354f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.15787f, 14.31853f, 22.43744f, 13.85724f, 22.71700f, 13.49847f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.94997f, 13.54973f, 23.22954f, 13.49847f, 23.50910f, 13.34471f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.74207f, 13.54973f, 23.97504f, 13.90849f, 24.16142f, 14.01100f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.25460f, 14.07934f, 24.31673f, 14.02808f, 24.34779f, 13.85724f},
    {MacInputIndicatorCatPathCommandType::Line, 24.34779f, 12.88342f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.53417f, 12.52466f, 24.62736f, 12.01212f, 24.67395f, 11.70460f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.72055f, 11.44833f, 25.13989f, 11.49959f, 25.27967f, 11.19206f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.34180f, 11.05539f, 25.31074f, 10.93580f, 25.18649f, 10.83329f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.09330f, 10.73079f, 24.90692f, 10.67953f, 24.76714f, 10.57702f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.53417f, 10.26951f, 24.25460f, 10.06449f, 23.92845f, 10.06449f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.50910f, 10.11575f, 23.13635f, 9.91073f, 22.90338f, 9.50070f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.94997f, 8.98817f, 23.46251f, 8.57814f, 23.88185f, 8.06561f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.34779f, 7.45057f, 24.58076f, 6.16923f, 24.90692f, 5.29792f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.43499f, 5.26376f, 25.76114f, 5.11000f, 25.88540f, 4.83664f},
    {MacInputIndicatorCatPathCommandType::Cubic, 25.97858f, 4.49496f, 25.77668f, 4.30702f, 25.27967f, 4.27285f},
    {MacInputIndicatorCatPathCommandType::Line, 24.25460f, 4.27285f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.94398f, 4.27285f, 23.74207f, 4.40953f, 23.64888f, 4.68288f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.41591f, 5.40043f, 23.08975f, 6.11797f, 22.71700f, 6.73302f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.39084f, 7.24555f, 22.01809f, 7.45057f, 21.45896f, 7.70683f},
    {MacInputIndicatorCatPathCommandType::Cubic, 19.59520f, 8.32188f, 18.57014f, 8.47564f, 17.07913f, 8.42439f},
    {MacInputIndicatorCatPathCommandType::Cubic, 16.05406f, 8.37313f, 15.26196f, 8.16811f, 15.02899f, 7.80934f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.84261f, 7.50182f, 14.92027f, 7.21138f, 15.26196f, 6.93803f},
    {MacInputIndicatorCatPathCommandType::Cubic, 15.69684f, 7.00637f, 16.03853f, 6.90387f, 16.28703f, 6.63051f},
    {MacInputIndicatorCatPathCommandType::Cubic, 16.50447f, 6.25466f, 16.36469f, 5.99839f, 15.86768f, 5.86171f},
    {MacInputIndicatorCatPathCommandType::Line, 14.74943f, 5.55419f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 14.43880f, 5.48586f, 14.19030f, 5.62253f, 14.00392f, 5.96422f},
    {MacInputIndicatorCatPathCommandType::Line, 13.25842f, 7.14304f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.00992f, 7.45057f, 12.63716f, 7.68975f, 12.14016f, 7.86059f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.25488f, 8.21937f, 10.46278f, 8.83441f, 10.04343f, 9.29569f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.88812f, 9.43236f, 9.71727f, 9.51779f, 9.53090f, 9.55195f},
    {MacInputIndicatorCatPathCommandType::Cubic, 8.27286f, 9.70572f, 6.26932f, 10.11575f, 4.73171f, 11.08956f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.52027f, 11.85836f, 2.58839f, 12.88342f, 2.12245f, 13.70349f},
    {MacInputIndicatorCatPathCommandType::Cubic, 1.88948f, 14.21602f, 1.84289f, 14.57480f, 2.07586f, 14.62604f},
    {MacInputIndicatorCatPathCommandType::Cubic, 2.26223f, 14.66021f, 2.44861f, 14.48937f, 2.63498f, 14.11351f},
    {MacInputIndicatorCatPathCommandType::Cubic, 3.28730f, 13.29346f, 4.21918f, 12.47340f, 5.29084f, 11.90962f},
    {MacInputIndicatorCatPathCommandType::Cubic, 6.73526f, 11.03831f, 8.41264f, 10.73079f, 9.53090f, 10.62828f},
    {MacInputIndicatorCatPathCommandType::Cubic, 9.71727f, 10.59410f, 9.85706f, 10.67953f, 9.95024f, 10.88455f},
    {MacInputIndicatorCatPathCommandType::Cubic, 10.36959f, 11.65335f, 11.20828f, 12.57591f, 12.60610f, 12.88342f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.91073f, 13.24220f, 15.68131f, 12.88342f, 17.12572f, 12.72967f},
    {MacInputIndicatorCatPathCommandType::Cubic, 18.38376f, 12.52466f, 19.40883f, 12.57591f, 20.20093f, 12.42215f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

inline constexpr std::array<MacInputIndicatorCatPathCommand, 20> kCatRunLandFarCommands{{
    {MacInputIndicatorCatPathCommandType::Move, 21.69193f, 8.93692f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.34425f, 8.98817f, 22.85678f, 8.52689f, 22.94997f, 7.75808f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.08975f, 6.93803f, 22.94997f, 6.11797f, 23.04316f, 5.40043f},
    {MacInputIndicatorCatPathCommandType::Cubic, 23.54016f, 5.40043f, 23.85079f, 5.26376f, 23.97504f, 4.99041f},
    {MacInputIndicatorCatPathCommandType::Cubic, 24.03717f, 4.75122f, 23.86632f, 4.59746f, 23.46251f, 4.52912f},
    {MacInputIndicatorCatPathCommandType::Line, 22.25106f, 4.52912f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 22.00256f, 4.56329f, 21.86278f, 4.76831f, 21.83172f, 5.14416f},
    {MacInputIndicatorCatPathCommandType::Line, 21.83172f, 6.42550f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 21.52109f, 7.00637f, 21.25706f, 7.48473f, 21.03962f, 7.86059f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Move, 11.25488f, 10.37201f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 12.41973f, 10.26951f, 13.30501f, 9.44946f, 13.30501f, 8.68065f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.30501f, 7.96310f, 12.83907f, 7.34806f, 12.74588f, 6.93803f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.21182f, 6.93803f, 13.52245f, 6.83553f, 13.67776f, 6.63051f},
    {MacInputIndicatorCatPathCommandType::Cubic, 13.86414f, 6.32299f, 13.70883f, 6.13507f, 13.21182f, 6.06672f},
    {MacInputIndicatorCatPathCommandType::Line, 12.09357f, 6.06672f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.81400f, 6.06672f, 11.64316f, 6.20340f, 11.58103f, 6.47675f},
    {MacInputIndicatorCatPathCommandType::Line, 11.58103f, 7.65558f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
    {MacInputIndicatorCatPathCommandType::Cubic, 11.23934f, 7.92894f, 10.97531f, 8.23645f, 10.78894f, 8.57814f},
    {MacInputIndicatorCatPathCommandType::Close, 0.00000f, -0.40000f, 0.00000f, 0.00000f, 0.00000f, 0.00000f},
}};

constexpr MacInputIndicatorCatArtwork macInputIndicatorCatArtwork(
    InputIndicatorCatArtworkFrame frame
)
{
  switch (frame) {
  case InputIndicatorCatArtworkFrame::IdleRise:
    return {
        .near = kCatIdleRiseNearCommands,
        .far = kCatIdleRiseFarCommands,
        .silhouetteBounds = {
            .x = 3.70509, .y = 4.32410, .width = 20.25440, .height = 12.76849
        },
        .languageBounds = {
            .x = 12.99847, .y = 5.09068, .width = 9.00000, .height = 8.00000
        },
    };
  case InputIndicatorCatArtworkFrame::IdleNeutral:
    return {
        .near = kCatIdleNeutralNearCommands,
        .far = kCatIdleNeutralFarCommands,
        .silhouetteBounds = {
            .x = 3.27712, .y = 4.14562, .width = 20.68238, .height = 12.94699
        },
        .languageBounds = {
            .x = 12.99847, .y = 5.09068, .width = 9.00000, .height = 8.00000
        },
    };
  case InputIndicatorCatArtworkFrame::IdleFall:
    return {
        .near = kCatIdleFallNearCommands,
        .far = kCatIdleFallFarCommands,
        .silhouetteBounds = {
            .x = 2.96272, .y = 4.00000, .width = 20.99677, .height = 13.09261
        },
        .languageBounds = {
            .x = 12.99847, .y = 5.09068, .width = 9.00000, .height = 8.00000
        },
    };
  case InputIndicatorCatArtworkFrame::WalkReach:
    return {
        .near = kCatWalkReachNearCommands,
        .far = kCatWalkReachFarCommands,
        .silhouetteBounds = {
            .x = 2.65206, .y = 4.27285, .width = 21.81997, .height = 10.71836
        },
        .languageBounds = {
            .x = 11.83362, .y = 6.52577, .width = 9.00000, .height = 8.00000
        },
    };
  case InputIndicatorCatArtworkFrame::WalkPass:
    return {
        .near = kCatWalkPassNearCommands,
        .far = kCatWalkPassFarCommands,
        .silhouetteBounds = {
            .x = 3.01966, .y = 4.11910, .width = 21.26599, .height = 11.12839
        },
        .languageBounds = {
            .x = 11.74044, .y = 6.73079, .width = 9.00000, .height = 8.00000
        },
    };
  case InputIndicatorCatArtworkFrame::WalkPush:
    return {
        .near = kCatWalkPushNearCommands,
        .far = kCatWalkPushFarCommands,
        .silhouetteBounds = {
            .x = 2.22663, .y = 4.32410, .width = 22.19880, .height = 10.76962
        },
        .languageBounds = {
            .x = 11.83362, .y = 6.57702, .width = 9.00000, .height = 8.00000
        },
    };
  case InputIndicatorCatArtworkFrame::RunCoil:
    return {
        .near = kCatRunCoilNearCommands,
        .far = kCatRunCoilFarCommands,
        .silhouetteBounds = {
            .x = 1.87156, .y = 4.22161, .width = 22.13453, .height = 9.74455
        },
        .languageBounds = {
            .x = 11.78703, .y = 5.91073, .width = 9.00000, .height = 8.00000
        },
    };
  case InputIndicatorCatArtworkFrame::RunAir:
    return {
        .near = kCatRunAirNearCommands,
        .far = kCatRunAirFarCommands,
        .silhouetteBounds = {
            .x = 1.00000, .y = 5.96422, .width = 28.00000, .height = 9.74455
        },
        .languageBounds = {
            .x = 12.11319, .y = 7.19206, .width = 9.00000, .height = 8.00000
        },
    };
  case InputIndicatorCatArtworkFrame::RunLand:
    return {
        .near = kCatRunLandNearCommands,
        .far = kCatRunLandFarCommands,
        .silhouetteBounds = {
            .x = 1.92275, .y = 4.27285, .width = 23.98471, .height = 10.35746
        },
        .languageBounds = {
            .x = 12.34616, .y = 6.57702, .width = 9.00000, .height = 8.00000
        },
    };
  }
}

} // namespace deskflow::gui::input
