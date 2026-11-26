/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorGeometryTests.h"

#include "gui/input/InputIndicatorGeometry.h"
#include "gui/input/MacInputIndicatorCatArtwork.h"
#include "gui/input/MacInputIndicatorCompanionArtwork.h"
#include "gui/input/MacInputIndicatorPanel.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>

#include <QTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

using namespace deskflow::gui::input;

namespace {

std::vector<InputIndicatorRect> oneScreen(double width = 1920, double height = 1080)
{
  return {{.x = 0, .y = 0, .width = width, .height = height}};
}

double linearChannel(double channel)
{
  return channel <= 0.04045
      ? channel / 12.92
      : std::pow((channel + 0.055) / 1.055, 2.4);
}

double relativeLuminance(MacInputIndicatorRgb color)
{
  return 0.2126 * linearChannel(color.red) +
      0.7152 * linearChannel(color.green) +
      0.0722 * linearChannel(color.blue);
}

double contrastRatio(
    MacInputIndicatorRgb foreground,
    MacInputIndicatorRgb background
)
{
  const auto foregroundLuminance =
      relativeLuminance(foreground);
  const auto backgroundLuminance =
      relativeLuminance(background);
  return (
      std::max(foregroundLuminance, backgroundLuminance) +
      0.05
  ) / (
      std::min(foregroundLuminance, backgroundLuminance) +
      0.05
  );
}

NSBezierPath *artworkPath(
    std::span<const MacInputIndicatorCatPathCommand> commands,
    const InputIndicatorCatStartleFrame &startleFrame = {},
    bool facesRight = true,
    double facingScaleX = 1,
    double bodyHeadingRadians = 0
)
{
  NSBezierPath *path = [NSBezierPath bezierPath];
  const auto deformed =
      [&startleFrame, facesRight, facingScaleX,
       bodyHeadingRadians](double x, double y) {
        const auto startlePoint =
            inputIndicatorCatStartlePoint(
                {.x = x, .y = y},
                startleFrame,
                false
            );
        return inputIndicatorCatBodyPoint(
            startlePoint,
            facesRight,
            facingScaleX,
            bodyHeadingRadians
        );
      };
  for (const auto &command : commands) {
    switch (command.type) {
    case MacInputIndicatorCatPathCommandType::Move: {
      const auto point =
          deformed(command.x1, command.y1);
      [path moveToPoint:NSMakePoint(point.x, point.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Line: {
      const auto point =
          deformed(command.x1, command.y1);
      [path lineToPoint:NSMakePoint(point.x, point.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Cubic: {
      const auto control1 =
          deformed(command.x1, command.y1);
      const auto control2 =
          deformed(command.x2, command.y2);
      const auto point =
          deformed(command.x3, command.y3);
      [path curveToPoint:NSMakePoint(point.x, point.y)
          controlPoint1:NSMakePoint(control1.x, control1.y)
          controlPoint2:NSMakePoint(control2.x, control2.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Close:
      [path closePath];
      break;
    }
  }
  return path;
}

NSBezierPath *artworkPath(
    const InputIndicatorCatPose &pose,
    std::span<const MacInputIndicatorCatPathCommand> commands,
    const InputIndicatorCatIdleTailFrame &tailFrame,
    const InputIndicatorCatStartleFrame &startleFrame = {},
    bool facesRight = true,
    double facingScaleX = 1,
    double bodyHeadingRadians = 0
)
{
  NSBezierPath *path = [NSBezierPath bezierPath];
  const auto deformed =
      [&pose, &tailFrame, &startleFrame,
       facesRight, facingScaleX,
       bodyHeadingRadians](
          double x, double y
      ) {
        const auto torsoPoint =
            inputIndicatorCatTorsoPoint(
                pose, {.x = x, .y = y}
            );
        const auto tailPoint =
            inputIndicatorCatIdleTailPoint(
                torsoPoint, tailFrame
            );
        const auto startlePoint =
            inputIndicatorCatStartlePoint(
                tailPoint, startleFrame, true
            );
        return inputIndicatorCatBodyPoint(
            startlePoint,
            facesRight,
            facingScaleX,
            bodyHeadingRadians
        );
      };
  for (const auto &command : commands) {
    switch (command.type) {
    case MacInputIndicatorCatPathCommandType::Move: {
      const auto point =
          deformed(command.x1, command.y1);
      [path moveToPoint:NSMakePoint(point.x, point.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Line: {
      const auto point =
          deformed(command.x1, command.y1);
      [path lineToPoint:NSMakePoint(point.x, point.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Cubic: {
      const auto control1 =
          deformed(command.x1, command.y1);
      const auto control2 =
          deformed(command.x2, command.y2);
      const auto point =
          deformed(command.x3, command.y3);
      [path curveToPoint:NSMakePoint(point.x, point.y)
          controlPoint1:NSMakePoint(control1.x, control1.y)
          controlPoint2:NSMakePoint(control2.x, control2.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Close:
      [path closePath];
      break;
    }
  }
  return path;
}

NSBezierPath *companionArtworkPath(
    InputIndicatorCompanion companion,
    std::span<const MacInputIndicatorCatPathCommand> commands,
    InputIndicatorMotionState motionState,
    double motionPhase,
    const InputIndicatorCompanionIdleFrame &idleFrame,
    const InputIndicatorCompanionStartleFrame &startleFrame,
    bool facesRight = true,
    double facingScaleX = 1,
    double bodyHeadingRadians = 0
)
{
  NSBezierPath *path = [NSBezierPath bezierPath];
  const auto deformed =
      [companion, motionState, motionPhase,
       &idleFrame, &startleFrame,
       facesRight, facingScaleX,
       bodyHeadingRadians](
          double x, double y
      ) {
        const auto animated =
            inputIndicatorCompanionArtworkPoint(
                companion,
                {.x = x, .y = y},
                motionState,
                motionPhase,
                idleFrame,
                startleFrame
            );
        return inputIndicatorCompanionBodyPoint(
            animated,
            facesRight,
            facingScaleX,
            bodyHeadingRadians
        );
      };
  for (const auto &command : commands) {
    switch (command.type) {
    case MacInputIndicatorCatPathCommandType::Move: {
      const auto point =
          deformed(command.x1, command.y1);
      [path moveToPoint:NSMakePoint(point.x, point.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Line: {
      const auto point =
          deformed(command.x1, command.y1);
      [path lineToPoint:NSMakePoint(point.x, point.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Cubic: {
      const auto control1 =
          deformed(command.x1, command.y1);
      const auto control2 =
          deformed(command.x2, command.y2);
      const auto point =
          deformed(command.x3, command.y3);
      [path curveToPoint:NSMakePoint(point.x, point.y)
          controlPoint1:NSMakePoint(control1.x, control1.y)
          controlPoint2:NSMakePoint(control2.x, control2.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Close:
      [path closePath];
      break;
    }
  }
  return path;
}

NSBitmapImageRep *pixelBitmap(
    int scale,
    int paddingPoints
)
{
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  return [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nil
                    pixelsWide:
                        static_cast<NSInteger>(
                            std::lround(
                                style.width +
                                paddingPoints * 2.0
                            )
                        ) * scale
                    pixelsHigh:
                        static_cast<NSInteger>(
                            std::lround(
                                style.height +
                                paddingPoints * 2.0
                            )
                        ) * scale
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSDeviceRGBColorSpace
                   bytesPerRow:0
                  bitsPerPixel:0];
}

NSBitmapImageRep *canvasPixelBitmap(
    int scale,
    int paddingPoints
)
{
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  return [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nil
                    pixelsWide:
                        static_cast<NSInteger>(
                            std::lround(
                                style.canvasWidth +
                                paddingPoints * 2.0
                            )
                        ) * scale
                    pixelsHigh:
                        static_cast<NSInteger>(
                            std::lround(
                                style.canvasHeight +
                                paddingPoints * 2.0
                            )
                        ) * scale
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSDeviceRGBColorSpace
                   bytesPerRow:0
                  bitsPerPixel:0];
}

NSBitmapImageRep *catBodyBitmap(
    const InputIndicatorCatPose &pose,
    const MacInputIndicatorCatArtwork &artwork,
    const InputIndicatorCatIdleTailFrame &tailFrame,
    bool facesRight,
    int scale,
    int paddingPoints,
    bool includeStroke,
    const InputIndicatorCatStartleFrame &startleFrame = {},
    double bodyHeadingRadians =
        std::numeric_limits<double>::quiet_NaN(),
    bool canvas = false,
    double facingScaleX = 1,
    bool includeGlow = false,
    deskflow::input::InputIndicatorSource glowSource =
        deskflow::input::InputIndicatorSource::LatinABC
)
{
  const double effectiveHeading =
      std::isfinite(bodyHeadingRadians)
          ? bodyHeadingRadians
          : facesRight ? 0 : M_PI;
  NSBitmapImageRep *bitmap =
      canvas
          ? canvasPixelBitmap(scale, paddingPoints)
          : pixelBitmap(scale, paddingPoints);
  if (!bitmap) {
    return nil;
  }
  NSGraphicsContext *graphics =
      [NSGraphicsContext
          graphicsContextWithBitmapImageRep:bitmap];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:graphics];
  CGContextRef context = [graphics CGContext];
  CGContextSaveGState(context);
  CGContextScaleCTM(context, scale, scale);
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  CGContextTranslateCTM(
      context,
      paddingPoints +
          (canvas ? style.canvasOffsetX : 0),
      paddingPoints +
          (canvas ? style.canvasOffsetY : 0)
  );
  const auto displayOrigin =
      macInputIndicatorDisplayPoint({});
  CGContextTranslateCTM(
      context, displayOrigin.x, displayOrigin.y
  );
  CGContextScaleCTM(
      context,
      macInputIndicatorPanelStyle().visualScale,
      macInputIndicatorPanelStyle().visualScale
  );
  NSBezierPath *far =
      artworkPath(
          artwork.far,
          startleFrame,
          facesRight,
          facingScaleX,
          effectiveHeading
      );
  NSBezierPath *near =
      artworkPath(
          pose,
          artwork.near,
          tailFrame,
          startleFrame,
          facesRight,
          facingScaleX,
          effectiveHeading
      );
  if (includeGlow &&
      startleFrame.active &&
      startleFrame.haloAlpha > 0) {
    NSBezierPath *halo =
        [NSBezierPath bezierPath];
    [halo appendBezierPath:far];
    [halo appendBezierPath:near];
    NSAffineTransform *haloTransform =
        [NSAffineTransform transform];
    [haloTransform translateXBy:15 yBy:14];
    [haloTransform scaleXBy:startleFrame.haloScale
                        yBy:startleFrame.haloScale];
    [haloTransform translateXBy:-15 yBy:-14];
    [halo transformUsingAffineTransform:haloTransform];
    const auto color =
        macInputIndicatorGlyphColor(
            glowSource,
            InputIndicatorTone::Automatic
        );
    NSColor *haloColor =
        [NSColor colorWithSRGBRed:color.red
                             green:color.green
                              blue:color.blue
                             alpha:startleFrame.haloAlpha];
    NSShadow *haloShadow =
        [[NSShadow alloc] init];
    haloShadow.shadowOffset = NSMakeSize(0, 0);
    haloShadow.shadowBlurRadius = 2.2;
    haloShadow.shadowColor = haloColor;
    [NSGraphicsContext saveGraphicsState];
    [haloShadow set];
    [haloColor setFill];
    [halo fill];
    [NSGraphicsContext restoreGraphicsState];
    [haloShadow release];
  }
  [[NSColor blackColor] setFill];
  if (includeStroke) {
    [[NSColor blackColor] setStroke];
    [near setLineWidth:0.55];
    [far setLineWidth:0.55];
    [near setLineJoinStyle:NSLineJoinStyleRound];
    [far setLineJoinStyle:NSLineJoinStyleRound];
    [near stroke];
    [far stroke];
  }
  [near fill];
  [far fill];
  CGContextRestoreGState(context);
  [NSGraphicsContext restoreGraphicsState];
  return bitmap;
}

NSBitmapImageRep *catGlyphBitmap(
    const InputIndicatorCatPose &pose,
    InputIndicatorMotionState state,
    bool facesRight,
    int scale,
    int paddingPoints,
    deskflow::input::InputIndicatorSource source =
        deskflow::input::InputIndicatorSource::LatinABC,
    double bodyHeadingRadians =
        std::numeric_limits<double>::quiet_NaN(),
    bool canvas = false,
    double facingScaleX = 1
)
{
  const double effectiveHeading =
      std::isfinite(bodyHeadingRadians)
          ? bodyHeadingRadians
          : facesRight ? 0 : M_PI;
  NSString *label =
      source ==
              deskflow::input::InputIndicatorSource::Korean2Set
          ? @"한"
          : @"A";
  auto target =
      macInputIndicatorDisplayRect(
          pose.innerLanguageBounds
      );
  const auto center =
      inputIndicatorCatBodyPoint(
          {
              .x = pose.innerLanguageBounds.x +
                  pose.innerLanguageBounds.width / 2.0,
              .y = pose.innerLanguageBounds.y +
                  pose.innerLanguageBounds.height / 2.0,
          },
          facesRight,
          facingScaleX,
          effectiveHeading
      );
  const auto displayCenter =
      macInputIndicatorDisplayPoint(center);
  target.x = displayCenter.x - target.width / 2.0;
  target.y = displayCenter.y - target.height / 2.0;
  if (canvas) {
    const auto style =
        macInputIndicatorPanelStyle(
            InputIndicatorCompanion::Cat
        );
    target.x += style.canvasOffsetX;
    target.y += style.canvasOffsetY;
  }
  target.x += paddingPoints;
  target.y += paddingPoints;
  const auto preference =
      macInputIndicatorDisplayLabelPreference(
          source, state
      );
  double pointSize = preference.pointSize;
  std::optional<MacInputIndicatorLabelLayout> layout;
  NSFont *font = nil;
  for (int attempt = 0; attempt < 5; ++attempt) {
    font =
        [NSFont systemFontOfSize:pointSize
                          weight:NSFontWeightSemibold];
    const auto measured =
        macInputIndicatorCoreTextLabelMeasurement(
            label, font
        );
    if (!measured) {
      return nil;
    }
    layout = macInputIndicatorFitLabelInkLayout(
        pointSize,
        preference.minimumPointSize,
        target,
        measured->ink
    );
    if (!layout) {
      return nil;
    }
    if (std::abs(
            layout->fontSize - pointSize
        ) < 0.01) {
      font =
          [NSFont systemFontOfSize:layout->fontSize
                            weight:NSFontWeightSemibold];
      break;
    }
    pointSize = layout->fontSize;
  }
  if (!layout || !font) {
    return nil;
  }

  NSBitmapImageRep *bitmap =
      canvas
          ? canvasPixelBitmap(scale, paddingPoints)
          : pixelBitmap(scale, paddingPoints);
  if (!bitmap) {
    return nil;
  }
  NSGraphicsContext *graphics =
      [NSGraphicsContext
          graphicsContextWithBitmapImageRep:bitmap];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:graphics];
  CGContextRef context = [graphics CGContext];
  CGContextSaveGState(context);
  CGContextScaleCTM(context, scale, scale);
  macInputIndicatorDrawCoreTextLabel(
      context,
      label,
      font,
      [NSColor blackColor],
      *layout
  );
  CGContextRestoreGState(context);
  [NSGraphicsContext restoreGraphicsState];
  return bitmap;
}

NSBitmapImageRep *companionBodyBitmap(
    InputIndicatorCompanion companion,
    InputIndicatorMotionState motionState,
    double motionPhase,
    const InputIndicatorCompanionIdleFrame &idleFrame,
    const InputIndicatorCompanionStartleFrame &startleFrame,
    bool facesRight,
    int scale,
    int paddingPoints,
    bool includeStroke,
    double bodyHeadingRadians = 0,
    double facingScaleX = 1,
    bool includeGlow = false,
    deskflow::input::InputIndicatorSource glowSource =
        deskflow::input::InputIndicatorSource::LatinABC
)
{
  NSBitmapImageRep *bitmap =
      canvasPixelBitmap(scale, paddingPoints);
  if (!bitmap) {
    return nil;
  }
  NSGraphicsContext *graphics =
      [NSGraphicsContext
          graphicsContextWithBitmapImageRep:bitmap];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:graphics];
  CGContextRef context = [graphics CGContext];
  CGContextSaveGState(context);
  CGContextScaleCTM(context, scale, scale);
  const auto style =
      macInputIndicatorPanelStyle(companion);
  CGContextTranslateCTM(
      context,
      paddingPoints + style.canvasOffsetX,
      paddingPoints + style.canvasOffsetY
  );
  const auto displayOrigin =
      macInputIndicatorDisplayPoint({});
  CGContextTranslateCTM(
      context, displayOrigin.x, displayOrigin.y
  );
  CGContextScaleCTM(
      context,
      style.visualScale,
      style.visualScale
  );
  const auto artwork =
      companion ==
              InputIndicatorCompanion::Dog
          ? macInputIndicatorDogArtwork()
          : macInputIndicatorTurtleArtwork();
  NSBezierPath *silhouette =
      companionArtworkPath(
          companion,
          artwork.silhouette,
          motionState,
          motionPhase,
          idleFrame,
          startleFrame,
          facesRight,
          facingScaleX,
          bodyHeadingRadians
      );
  if (includeGlow &&
      startleFrame.active &&
      startleFrame.haloAlpha > 0) {
    NSBezierPath *halo =
        [silhouette copy];
    NSAffineTransform *haloTransform =
        [NSAffineTransform transform];
    [haloTransform translateXBy:15 yBy:14];
    [haloTransform scaleXBy:startleFrame.haloScale
                        yBy:startleFrame.haloScale];
    [haloTransform translateXBy:-15 yBy:-14];
    [halo transformUsingAffineTransform:haloTransform];
    const auto color =
        macInputIndicatorGlyphColor(
            glowSource,
            InputIndicatorTone::Automatic
        );
    NSColor *haloColor =
        [NSColor colorWithSRGBRed:color.red
                             green:color.green
                              blue:color.blue
                             alpha:startleFrame.haloAlpha];
    NSShadow *haloShadow =
        [[NSShadow alloc] init];
    haloShadow.shadowOffset = NSMakeSize(0, 0);
    haloShadow.shadowBlurRadius = 2.2;
    haloShadow.shadowColor = haloColor;
    [NSGraphicsContext saveGraphicsState];
    [haloShadow set];
    [haloColor setFill];
    [halo fill];
    [NSGraphicsContext restoreGraphicsState];
    [haloShadow release];
    [halo release];
  }
  [[NSColor blackColor] setFill];
  if (includeStroke) {
    [[NSColor blackColor] setStroke];
    [silhouette setLineWidth:0.55];
    [silhouette setLineJoinStyle:NSLineJoinStyleRound];
    [silhouette stroke];
  }
  [silhouette fill];
  CGContextRestoreGState(context);
  [NSGraphicsContext restoreGraphicsState];
  return bitmap;
}

NSBitmapImageRep *companionGlyphBitmap(
    InputIndicatorCompanion companion,
    InputIndicatorMotionState state,
    deskflow::input::InputIndicatorSource source,
    bool facesRight,
    int scale,
    int paddingPoints,
    double bodyHeadingRadians = 0,
    double facingScaleX = 1
)
{
  NSString *label =
      source ==
              deskflow::input::InputIndicatorSource::Korean2Set
          ? @"한"
          : @"A";
  const auto artwork =
      companion ==
              InputIndicatorCompanion::Dog
          ? macInputIndicatorDogArtwork()
          : macInputIndicatorTurtleArtwork();
  const auto innerBounds =
      artwork.innerLanguageBounds;
  const auto center =
      inputIndicatorCompanionBodyPoint(
          {
              .x = innerBounds.x +
                  innerBounds.width / 2.0,
              .y = innerBounds.y +
                  innerBounds.height / 2.0,
          },
          facesRight,
          facingScaleX,
          bodyHeadingRadians
      );
  auto target =
      macInputIndicatorDisplayRect(innerBounds);
  const auto displayCenter =
      macInputIndicatorDisplayPoint(center);
  target.x =
      displayCenter.x - target.width / 2.0;
  target.y =
      displayCenter.y - target.height / 2.0;
  const auto style =
      macInputIndicatorPanelStyle(companion);
  target.x +=
      style.canvasOffsetX + paddingPoints;
  target.y +=
      style.canvasOffsetY + paddingPoints;
  const auto preference =
      macInputIndicatorDisplayLabelPreference(
          source, state
      );
  double pointSize = preference.pointSize;
  std::optional<MacInputIndicatorLabelLayout> layout;
  NSFont *font = nil;
  for (int attempt = 0;
       attempt < 5;
       ++attempt) {
    font =
        [NSFont systemFontOfSize:pointSize
                          weight:NSFontWeightSemibold];
    const auto measured =
        macInputIndicatorCoreTextLabelMeasurement(
            label, font
        );
    if (!measured) {
      return nil;
    }
    layout =
        macInputIndicatorFitLabelInkLayout(
            pointSize,
            preference.minimumPointSize,
            target,
            measured->ink
        );
    if (!layout) {
      return nil;
    }
    if (std::abs(
            layout->fontSize - pointSize
        ) < 0.01) {
      font =
          [NSFont systemFontOfSize:layout->fontSize
                            weight:NSFontWeightSemibold];
      break;
    }
    pointSize = layout->fontSize;
  }
  if (!layout || !font) {
    return nil;
  }
  NSBitmapImageRep *bitmap =
      canvasPixelBitmap(scale, paddingPoints);
  if (!bitmap) {
    return nil;
  }
  NSGraphicsContext *graphics =
      [NSGraphicsContext
          graphicsContextWithBitmapImageRep:bitmap];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:graphics];
  CGContextRef context = [graphics CGContext];
  CGContextSaveGState(context);
  CGContextScaleCTM(context, scale, scale);
  macInputIndicatorDrawCoreTextLabel(
      context,
      label,
      font,
      [NSColor blackColor],
      *layout
  );
  CGContextRestoreGState(context);
  [NSGraphicsContext restoreGraphicsState];
  return bitmap;
}

struct CatPixelDifference
{
  int tail = 0;
  int nonTail = 0;
};

CatPixelDifference catPixelDifference(
    NSBitmapImageRep *from,
    NSBitmapImageRep *to,
    NSInteger tailBoundaryX
)
{
  CatPixelDifference difference;
  for (NSInteger y = 0; y < from.pixelsHigh; ++y) {
    for (NSInteger x = 0; x < from.pixelsWide; ++x) {
      const double fromAlpha =
          [from colorAtX:x y:y].alphaComponent;
      const double toAlpha =
          [to colorAtX:x y:y].alphaComponent;
      if (std::abs(fromAlpha - toAlpha) <= 0.02) {
        continue;
      }
      if (x < tailBoundaryX) {
        ++difference.tail;
      } else {
        ++difference.nonTail;
      }
    }
  }
  return difference;
}

int bitmapChangedPixels(
    NSBitmapImageRep *from,
    NSBitmapImageRep *to
)
{
  int changed = 0;
  for (NSInteger y = 0; y < from.pixelsHigh; ++y) {
    for (NSInteger x = 0; x < from.pixelsWide; ++x) {
      const double fromAlpha =
          [from colorAtX:x y:y].alphaComponent;
      const double toAlpha =
          [to colorAtX:x y:y].alphaComponent;
      if (std::abs(fromAlpha - toAlpha) > 0.02) {
        ++changed;
      }
    }
  }
  return changed;
}

InputIndicatorRect bitmapAlphaBounds(
    NSBitmapImageRep *bitmap
)
{
  NSInteger left = bitmap.pixelsWide;
  NSInteger right = -1;
  NSInteger bottom = bitmap.pixelsHigh;
  NSInteger top = -1;
  for (NSInteger y = 0; y < bitmap.pixelsHigh; ++y) {
    for (NSInteger x = 0; x < bitmap.pixelsWide; ++x) {
      if ([bitmap colorAtX:x y:y].alphaComponent <=
          0.02) {
        continue;
      }
      left = std::min(left, x);
      right = std::max(right, x);
      bottom = std::min(bottom, y);
      top = std::max(top, y);
    }
  }
  if (right < left || top < bottom) {
    return {};
  }
  return {
      .x = static_cast<double>(left),
      .y = static_cast<double>(bottom),
      .width =
          static_cast<double>(right - left + 1),
      .height =
          static_cast<double>(top - bottom + 1),
  };
}

NSView *catPanelView()
{
  Class panelClass =
      NSClassFromString(@"DeskflowInputIndicatorPanel");
  if (!panelClass) {
    return nil;
  }
  for (NSWindow *candidate in NSApp.windows) {
    if ([candidate isKindOfClass:panelClass] &&
        candidate.visible) {
      return candidate.contentView;
    }
  }
  return nil;
}

// The panel tests never read the live pointer. Deskflow parks the local pointer on the very edge of a screen while another computer is in
// use, and NSPointInRect is half open, so such a pointer is inside no screen. A fractional pointer also makes the whole point window origin
// that AppKit gives the panel differ from the exact target by up to a point. A fixed whole point position inside the first screen keeps the
// tests deterministic and independent of where the real pointer is.
NSPoint fixedPanelTestPointer()
{
  NSScreen *screen = NSScreen.screens.firstObject;
  const NSRect frame =
      screen ? screen.frame : NSMakeRect(0, 0, 1280, 800);
  return NSMakePoint(
      std::floor(NSMidX(frame)), std::floor(NSMidY(frame))
  );
}

} // namespace

void InputIndicatorGeometryTests::bottomRightOffsetsCursor()
{
  const auto result = positionInputIndicator(
      {.x = 400, .y = 500}, {.width = 40, .height = 28}, oneScreen(),
      InputIndicatorPosition::CursorBottomRight, 8, 8
  );

  QVERIFY(result.has_value());
  QCOMPARE(result->x, 408.0);
  QCOMPARE(result->y, 464.0);
}

void InputIndicatorGeometryTests::positionsFlipInsideScreen()
{
  const InputIndicatorSize badge{.width = 40, .height = 28};
  const auto bottom = positionInputIndicator(
      {.x = 1918, .y = 2}, badge, oneScreen(), InputIndicatorPosition::CursorBottomRight, 8, 8
  );
  QVERIFY(bottom.has_value());
  QCOMPARE(bottom->x, 1872.0);
  QCOMPARE(bottom->y, 8.0);

  const auto topLeft = positionInputIndicator(
      {.x = 2, .y = 1078}, badge, oneScreen(), InputIndicatorPosition::CursorTopLeft, 8, 8
  );
  QVERIFY(topLeft.has_value());
  QCOMPARE(topLeft->x, 8.0);
  QCOMPARE(topLeft->y, 1044.0);
}

void InputIndicatorGeometryTests::choosesScreenContainingCursor()
{
  const std::vector<InputIndicatorRect> screens{
      {.x = -1280, .y = 0, .width = 1280, .height = 800},
      {.x = 0, .y = 0, .width = 1920, .height = 1080},
  };
  const auto result = positionInputIndicator(
      {.x = -100, .y = 200}, {.width = 40, .height = 28}, screens,
      InputIndicatorPosition::CursorBottomRight, 8, 8
  );

  QVERIFY(result.has_value());
  QCOMPARE(result->x, -92.0);
  QCOMPARE(result->y, 164.0);
}

void InputIndicatorGeometryTests::everyPositionIsPointerRelative_data()
{
  QTest::addColumn<int>("position");
  QTest::addColumn<double>("x");
  QTest::addColumn<double>("y");

  QTest::newRow("bottom-right")
      << static_cast<int>(InputIndicatorPosition::CursorBottomRight)
      << 408.0 << 464.0;
  QTest::newRow("top-right")
      << static_cast<int>(InputIndicatorPosition::CursorTopRight)
      << 408.0 << 508.0;
  QTest::newRow("bottom-left")
      << static_cast<int>(InputIndicatorPosition::CursorBottomLeft)
      << 352.0 << 464.0;
  QTest::newRow("top-left")
      << static_cast<int>(InputIndicatorPosition::CursorTopLeft)
      << 352.0 << 508.0;
}

void InputIndicatorGeometryTests::everyPositionIsPointerRelative()
{
  QFETCH(int, position);
  QFETCH(double, x);
  QFETCH(double, y);

  const auto result = positionInputIndicator(
      {.x = 400, .y = 500},
      {.width = 40, .height = 28},
      oneScreen(),
      static_cast<InputIndicatorPosition>(position),
      8,
      8
  );

  QVERIFY(result.has_value());
  QCOMPARE(result->x, x);
  QCOMPARE(result->y, y);
}

void InputIndicatorGeometryTests::followApproachUsesExactRectangleSupportAtEightHeadings()
{
  const InputIndicatorPoint cursor{
      .x = 500,
      .y = 500,
  };
  const InputIndicatorSize badge{
      .width = 33,
      .height = 31,
  };
  const double headings[] = {
      0,
      M_PI / 4,
      M_PI / 2,
      3 * M_PI / 4,
      M_PI,
      5 * M_PI / 4,
      3 * M_PI / 2,
      7 * M_PI / 4,
  };
  for (const double heading : headings) {
    const auto result =
        positionInputIndicator(
            cursor,
            badge,
            oneScreen(),
            InputIndicatorPosition::
                CursorFollowApproach,
            7,
            7,
            heading
        );
    QVERIFY(result);
    const InputIndicatorPoint direction{
        .x = std::cos(heading),
        .y = std::sin(heading),
    };
    const InputIndicatorPoint center{
        .x = result->x + badge.width / 2.0,
        .y = result->y + badge.height / 2.0,
    };
    const double support =
        std::abs(direction.x) *
            badge.width / 2.0 +
        std::abs(direction.y) *
            badge.height / 2.0;
    const InputIndicatorPoint supportPoint{
        .x = center.x +
            direction.x * support,
        .y = center.y +
            direction.y * support,
    };
    const double gap =
        std::hypot(
            cursor.x - supportPoint.x,
            cursor.y - supportPoint.y
        );
    QVERIFY(std::abs(gap - 7.0) < 1e-9);
    QVERIFY(
        (center.x - cursor.x) *
                direction.x +
            (center.y - cursor.y) *
                direction.y <
        0
    );
  }
}

void InputIndicatorGeometryTests::followApproachFallbackAndFixedModesStayCompatible()
{
  const InputIndicatorPoint cursor{
      .x = 400,
      .y = 500,
  };
  const InputIndicatorSize badge{
      .width = 33,
      .height = 31,
  };
  const InputIndicatorPosition fixedModes[] = {
      InputIndicatorPosition::CursorBottomRight,
      InputIndicatorPosition::CursorTopRight,
      InputIndicatorPosition::CursorBottomLeft,
      InputIndicatorPosition::CursorTopLeft,
  };
  for (const auto mode : fixedModes) {
    const auto original =
        positionInputIndicator(
            cursor, badge, oneScreen(),
            mode, 7, 7
        );
    const auto withHeading =
        positionInputIndicator(
            cursor, badge, oneScreen(),
            mode, 7, 7, 0.37
        );
    QVERIFY(original);
    QVERIFY(withHeading);
    QCOMPARE(original->x, withHeading->x);
    QCOMPARE(original->y, withHeading->y);
  }

  const auto bottomRight =
      positionInputIndicator(
          cursor,
          badge,
          oneScreen(),
          InputIndicatorPosition::
              CursorBottomRight,
          7,
          7
      );
  QVERIFY(bottomRight);
  const std::optional<double> invalidHeadings[] = {
      std::nullopt,
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity(),
  };
  for (const auto heading : invalidHeadings) {
    const auto fallback =
        positionInputIndicator(
            cursor,
            badge,
            oneScreen(),
            InputIndicatorPosition::
                CursorFollowApproach,
            7,
            7,
            heading
        );
    QVERIFY(fallback);
    QCOMPARE(fallback->x, bottomRight->x);
    QCOMPARE(fallback->y, bottomRight->y);
  }
}

void InputIndicatorGeometryTests::followApproachClampsEveryScreenEdgeAndCorner()
{
  const InputIndicatorSize badge{
      .width = 33,
      .height = 31,
  };
  const std::vector<InputIndicatorRect> screens{{
      .x = 0,
      .y = 0,
      .width = 100,
      .height = 100,
  }};
  const struct
  {
    InputIndicatorPoint cursor;
    double heading;
    double x;
    double y;
  } cases[] = {
      {{.x = 7, .y = 50}, 0, 7, 34.5},
      {{.x = 93, .y = 50},
       M_PI, 60, 34.5},
      {{.x = 50, .y = 7},
       M_PI / 2, 33.5, 7},
      {{.x = 50, .y = 93},
       3 * M_PI / 2, 33.5, 62},
      {{.x = 7, .y = 7},
       M_PI / 4, 7, 7},
      {{.x = 93, .y = 7},
       3 * M_PI / 4, 60, 7},
      {{.x = 93, .y = 93},
       5 * M_PI / 4, 60, 62},
      {{.x = 7, .y = 93},
       7 * M_PI / 4, 7, 62},
  };
  for (const auto &testCase : cases) {
    const auto result =
        positionInputIndicator(
            testCase.cursor,
            badge,
            screens,
            InputIndicatorPosition::
                CursorFollowApproach,
            7,
            7,
            testCase.heading
        );
    QVERIFY(result);
    QCOMPARE(
        result->x,
        testCase.x
    );
    QCOMPARE(
        result->y,
        testCase.y
    );
  }

  const std::vector<InputIndicatorRect> splitScreens{
      {.x = -1280,
       .y = 0,
       .width = 1280,
       .height = 800},
      {.x = 0,
       .y = 0,
       .width = 1920,
       .height = 1080},
  };
  const auto leftScreen =
      positionInputIndicator(
          {.x = -5, .y = 400},
          badge,
          splitScreens,
          InputIndicatorPosition::
              CursorFollowApproach,
          7,
          7,
          0
      );
  QVERIFY(leftScreen);
  QVERIFY(leftScreen->x < 0);
  const auto oppositeSide =
      positionInputIndicator(
          {.x = -5, .y = 400},
          badge,
          splitScreens,
          InputIndicatorPosition::
              CursorFollowApproach,
          7,
          7,
          M_PI
      );
  QVERIFY(oppositeSide);
  QVERIFY(oppositeSide->x + badge.width <= 0);
  QVERIFY(!positionInputIndicator(
      {.x = -1281, .y = 400},
      badge,
      splitScreens,
      InputIndicatorPosition::
          CursorFollowApproach,
      7,
      7,
      0
  ));
}

void InputIndicatorGeometryTests::followApproachUsesLastHeadingWhileIdle()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  NSRect containingScreen;
  bool foundScreen = false;
  for (NSScreen *screen in NSScreen.screens) {
    if (NSPointInRect(fixedCursor, screen.frame)) {
      containingScreen = screen.frame;
      foundScreen = true;
      break;
    }
  }
  QVERIFY(foundScreen);
  const std::vector<InputIndicatorRect> screens{{
      .x = containingScreen.origin.x,
      .y = containingScreen.origin.y,
      .width = containingScreen.size.width,
      .height = containingScreen.size.height,
  }};
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    InputIndicatorTarget target{
        .point = {
            .x = fixedCursor.x,
            .y = fixedCursor.y,
        },
        .kind =
            InputIndicatorTargetKind::Pointer,
    };
    MacInputIndicatorPanel panel(
        {},
        [&target] {
          return target;
        }
    );
    MacInputIndicatorPanelOptions options{
        .source = Source::LatinABC,
        .companion = companion,
        .position =
            InputIndicatorPosition::
                CursorFollowApproach,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    const auto style =
        macInputIndicatorPanelStyle(companion);
    const auto visibleOrigin =
        [view, style] {
          const auto origin =
              view.window.frame.origin;
          return InputIndicatorPoint{
              .x =
                  origin.x +
                  style.canvasOffsetX,
              .y =
                  origin.y +
                  style.canvasOffsetY,
          };
        };
    target.point.x +=
        target.point.x < 1000 ? 300 : -300;
    bool reached = false;
    for (int tick = 0; tick < 400; ++tick) {
      QTest::qWait(8);
      const auto step =
          panel.move(options.position);
      QVERIFY(step.visible);
      const auto current = visibleOrigin();
      const double heading =
          [[view valueForKey:
                @"catBodyHeadingRadians"]
              doubleValue];
      const auto expected =
          positionInputIndicator(
              target.point,
              {
                  .width = style.width,
                  .height = style.height,
              },
              screens,
              options.position,
              style.cursorGap,
              style.screenMargin,
              heading
          );
      QVERIFY(expected);
      if (std::abs(current.x - expected->x) <
              1.0 &&
          std::abs(current.y - expected->y) <
              1.0) {
        reached = true;
        break;
      }
    }
    QVERIFY(reached);
    bool idle = false;
    for (int tick = 0; tick < 10; ++tick) {
      QTest::qWait(100);
      const auto step =
          panel.move(options.position);
      QVERIFY(step.visible);
      if (step.frameIntervalMilliseconds == 75) {
        idle = true;
        break;
      }
    }
    QVERIFY(idle);
    const auto beforeIdle = visibleOrigin();
    for (int tick = 0; tick < 5; ++tick) {
      QTest::qWait(75);
      QVERIFY(panel.move(options.position).visible);
      const auto idle = visibleOrigin();
      QCOMPARE(idle.x, beforeIdle.x);
      QCOMPARE(idle.y, beforeIdle.y);
    }
    panel.hide();
  }
}

void InputIndicatorGeometryTests::followApproachHeadingChangeTraversesIntermediatePositions()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  NSRect containingScreen;
  bool foundScreen = false;
  for (NSScreen *screen in NSScreen.screens) {
    if (NSPointInRect(fixedCursor, screen.frame)) {
      containingScreen = screen.frame;
      foundScreen = true;
      break;
    }
  }
  QVERIFY(foundScreen);
  const std::vector<InputIndicatorRect> screens{{
      .x = containingScreen.origin.x,
      .y = containingScreen.origin.y,
      .width = containingScreen.size.width,
      .height = containingScreen.size.height,
  }};
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    const InputIndicatorPoint initial{
        .x = fixedCursor.x,
        .y = fixedCursor.y,
    };
    InputIndicatorTarget target{
        .point = initial,
        .kind =
            InputIndicatorTargetKind::Pointer,
    };
    MacInputIndicatorPanel panel(
        {},
        [&target] {
          return target;
        }
    );
    const MacInputIndicatorPanelOptions options{
        .source = Source::LatinABC,
        .companion = companion,
        .position =
            InputIndicatorPosition::
                CursorFollowApproach,
        .followSpeedPercent = 50,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    const auto style =
        macInputIndicatorPanelStyle(companion);
    const auto visibleOrigin =
        [view, style] {
          const auto origin =
              view.window.frame.origin;
          return InputIndicatorPoint{
              .x =
                  origin.x +
                  style.canvasOffsetX,
              .y =
                  origin.y +
                  style.canvasOffsetY,
          };
        };
    target.point.x =
        initial.x < 1000
            ? initial.x + 300
            : initial.x - 300;
    for (int tick = 0; tick < 12; ++tick) {
      QTest::qWait(8);
      QVERIFY(panel.move(options.position).visible);
    }
    target.point.x =
        initial.x < 1000
            ? initial.x - 300
            : initial.x + 300;
    std::vector<InputIndicatorPoint> positions{
        visibleOrigin()
    };
    bool reached = false;
    for (int tick = 0; tick < 700; ++tick) {
      QTest::qWait(8);
      const auto step =
          panel.move(options.position);
      QVERIFY(step.visible);
      const auto current = visibleOrigin();
      const auto previous = positions.back();
      const double movement =
          std::hypot(
              current.x - previous.x,
              current.y - previous.y
          );
      QVERIFY(
          movement <=
          2.88 + std::sqrt(2.0)
      );
      positions.push_back(current);
      const double heading =
          [[view valueForKey:
                @"catBodyHeadingRadians"]
              doubleValue];
      const auto expected =
          positionInputIndicator(
              target.point,
              {
                  .width = style.width,
                  .height = style.height,
              },
              screens,
              options.position,
              style.cursorGap,
              style.screenMargin,
              heading
          );
      QVERIFY(expected);
      if (std::abs(current.x - expected->x) <
              1.0 &&
          std::abs(current.y - expected->y) <
              1.0) {
        reached = true;
        break;
      }
    }
    QVERIFY(reached);
    QVERIFY(positions.size() > 20);
    panel.hide();
  }
}

void InputIndicatorGeometryTests::followApproachFirstShowUsesBottomRight()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  NSRect screenFrame =
      NSScreen.screens.firstObject.frame;
  const std::vector<InputIndicatorRect> screens{{
      .x = screenFrame.origin.x,
      .y = screenFrame.origin.y,
      .width = screenFrame.size.width,
      .height = screenFrame.size.height,
  }};
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    MacInputIndicatorPanel panel(
        {},
        [fixedCursor] {
          return InputIndicatorTarget{
              .point = {
                  .x = fixedCursor.x,
                  .y = fixedCursor.y,
              },
              .kind =
                  InputIndicatorTargetKind::Pointer,
          };
        }
    );
    MacInputIndicatorPanelOptions options{
        .source = Source::LatinABC,
        .companion = companion,
        .position =
            InputIndicatorPosition::
                CursorFollowApproach,
    };
    const auto style =
        macInputIndicatorPanelStyle(companion);
    const auto expected =
        positionInputIndicator(
            {
                .x = fixedCursor.x,
                .y = fixedCursor.y,
            },
            {
                .width = style.width,
                .height = style.height,
            },
            screens,
            InputIndicatorPosition::
                CursorBottomRight,
            style.cursorGap,
            style.screenMargin
        );
    QVERIFY(expected);
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    const auto origin =
        view.window.frame.origin;
    QVERIFY(std::abs(
        origin.x + style.canvasOffsetX -
        expected->x
    ) < 1.0);
    QVERIFY(std::abs(
        origin.y + style.canvasOffsetY -
        expected->y
    ) < 1.0);
    panel.hide();
  }
}

void InputIndicatorGeometryTests::followApproachCompanionSpeedRasterMatrix()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  const struct
  {
    int percent;
    double movementCap;
  } speeds[] = {
      {.percent = 50, .movementCap = 2.88},
      {.percent = 100, .movementCap = 5.76},
      {.percent = 200, .movementCap = 11.52},
  };
  int rasterCases = 0;
  int outsidePixels = 0;
  for (const auto companion : companions) {
    for (const auto speed : speeds) {
      InputIndicatorTarget target{
          .point = {
              .x = fixedCursor.x,
              .y = fixedCursor.y,
          },
          .kind =
              InputIndicatorTargetKind::Pointer,
      };
      MacInputIndicatorPanel panel(
          {},
          [&target] {
            return target;
          }
      );
      MacInputIndicatorPanelOptions options{
          .source = Source::LatinABC,
          .companion = companion,
          .position =
              InputIndicatorPosition::
                  CursorFollowApproach,
          .followSpeedPercent = speed.percent,
      };
      QVERIFY(panel.show(options));
      NSView *view = catPanelView();
      QVERIFY(view);
      const auto style =
          macInputIndicatorPanelStyle(companion);
      NSRect containingScreen;
      bool foundScreen = false;
      for (NSScreen *screen in
           NSScreen.screens) {
        if (NSPointInRect(
                fixedCursor,
                screen.frame
            )) {
          containingScreen = screen.frame;
          foundScreen = true;
          break;
        }
      }
      QVERIFY(foundScreen);
      const std::vector<InputIndicatorRect> screens{{
          .x = containingScreen.origin.x,
          .y = containingScreen.origin.y,
          .width = containingScreen.size.width,
          .height = containingScreen.size.height,
      }};
      const auto visibleOrigin =
          [view, style] {
            const auto origin =
                view.window.frame.origin;
            return InputIndicatorPoint{
                .x =
                    origin.x +
                    style.canvasOffsetX,
                .y =
                    origin.y +
                    style.canvasOffsetY,
            };
          };
      const auto rasterOutside =
          [companion, view](
              int scale
          ) -> int {
            constexpr int paddingPoints = 4;
            const auto motionState =
                static_cast<InputIndicatorMotionState>(
                    [[view valueForKey:
                          @"motionState"]
                        integerValue]
                );
            const double motionPhase =
                [[view valueForKey:
                      @"motionPhase"]
                    doubleValue];
            const bool facesRight =
                [[view valueForKey:@"facesRight"]
                    boolValue];
            const double facingScale =
                [[view valueForKey:
                      @"facingScaleX"]
                    doubleValue];
            const double heading =
                [[view valueForKey:
                      @"catBodyHeadingRadians"]
                    doubleValue];
            const InputIndicatorCompanionIdleFrame idle{
                .motion =
                    static_cast<InputIndicatorCompanionIdleMotion>(
                        [[view valueForKey:
                              @"catIdleTailMotion"]
                            integerValue]
                    ),
                .progress =
                    [[view valueForKey:
                          @"catIdleTailProgress"]
                        doubleValue],
                .active =
                    [[view valueForKey:
                          @"catIdleTailActive"]
                        boolValue],
            };
            const InputIndicatorCompanionStartleFrame
                startle{
                    .active =
                        [[view valueForKey:
                              @"catStartleActive"]
                            boolValue],
                    .progress =
                        [[view valueForKey:
                              @"catStartleProgress"]
                            doubleValue],
                    .recoilOffsetX =
                        [[view valueForKey:
                              @"catStartleRecoilOffsetX"]
                            doubleValue],
                    .horizontalScale =
                        [[view valueForKey:
                              @"catStartleHorizontalScale"]
                            doubleValue],
                    .dorsalLift =
                        [[view valueForKey:
                              @"catStartleDorsalLift"]
                            doubleValue],
                    .haloScale =
                        [[view valueForKey:
                              @"catStartleHaloScale"]
                            doubleValue],
                    .haloAlpha =
                        [[view valueForKey:
                              @"catStartleHaloAlpha"]
                            doubleValue],
                };
            NSBitmapImageRep *body = nil;
            NSBitmapImageRep *glyph = nil;
            if (companion ==
                InputIndicatorCompanion::Cat) {
              const auto pose =
                  inputIndicatorCatPose(
                      motionState,
                      motionPhase,
                      false
                  );
              const auto artwork =
                  macInputIndicatorCatArtwork(
                      pose.artworkFrame
                  );
              body = catBodyBitmap(
                  pose,
                  artwork,
                  idle,
                  facesRight,
                  scale,
                  paddingPoints,
                  true,
                  startle,
                  heading,
                  true,
                  facingScale
              );
              glyph = catGlyphBitmap(
                  pose,
                  motionState,
                  facesRight,
                  scale,
                  paddingPoints,
                  Source::LatinABC,
                  heading,
                  true,
                  facingScale
              );
            } else {
              body = companionBodyBitmap(
                  companion,
                  motionState,
                  motionPhase,
                  idle,
                  startle,
                  facesRight,
                  scale,
                  paddingPoints,
                  true,
                  heading,
                  facingScale
              );
              glyph = companionGlyphBitmap(
                  companion,
                  motionState,
                  Source::LatinABC,
                  facesRight,
                  scale,
                  paddingPoints,
                  heading,
                  facingScale
              );
            }
            if (!body || !glyph) {
              if (body) {
                [body release];
              }
              if (glyph) {
                [glyph release];
              }
              return -1;
            }
            const auto rasterStyle =
                macInputIndicatorPanelStyle(
                    companion
                );
            const NSInteger left =
                paddingPoints * scale;
            const NSInteger bottom =
                paddingPoints * scale;
            const NSInteger right =
                left +
                static_cast<NSInteger>(
                    rasterStyle.canvasWidth
                ) *
                    scale;
            const NSInteger top =
                bottom +
                static_cast<NSInteger>(
                    rasterStyle.canvasHeight
                ) *
                    scale;
            int outside = 0;
            int bodyOutside = 0;
            int glyphOutside = 0;
            for (NSInteger y = 0;
                 y < body.pixelsHigh;
                 ++y) {
              for (NSInteger x = 0;
                   x < body.pixelsWide;
                   ++x) {
                const double bodyAlpha =
                    [body colorAtX:x y:y]
                        .alphaComponent;
                const double glyphAlpha =
                    [glyph colorAtX:x y:y]
                        .alphaComponent;
                if (bodyAlpha > 0.02 &&
                    (x < left ||
                     x >= right ||
                     y < bottom ||
                     y >= top)) {
                  ++outside;
                  ++bodyOutside;
                }
                if (glyphAlpha > 0.02 &&
                    bodyAlpha <= 0.02) {
                  ++outside;
                  ++glyphOutside;
                }
              }
            }
            if (outside > 0) {
              qInfo() << "follow-approach-raster-spill"
                      << "companion"
                      << static_cast<int>(companion)
                      << "scale" << scale
                      << "heading" << heading
                      << "bodyOutside" << bodyOutside
                      << "glyphOutside"
                      << glyphOutside;
            }
            [body release];
            [glyph release];
            return outside;
          };

      target.point.x +=
          target.point.x < 1000
              ? 500
              : -500;
      bool reached = false;
      std::vector<InputIndicatorPoint> positions{
          visibleOrigin()
      };
      for (int tick = 0;
           tick < 700;
           ++tick) {
        QTest::qWait(8);
        const auto step =
            panel.move(options.position);
        QVERIFY(step.visible);
        QVERIFY(!step.reducedMotion);
        const auto current = visibleOrigin();
        const double movement =
            std::hypot(
                current.x -
                    positions.back().x,
                current.y -
                    positions.back().y
            );
        QVERIFY(
            movement <=
            speed.movementCap +
                std::sqrt(2.0)
        );
        positions.push_back(current);
        if (tick == 1 || tick == 10) {
          for (const int scale : {1, 2}) {
            const int outside =
                rasterOutside(scale);
            QVERIFY(outside >= 0);
            if (outside > 0) {
              qInfo() << "follow-approach-spill"
                      << static_cast<int>(
                             companion
                         )
                      << speed.percent
                      << tick
                      << scale
                      << [[view valueForKey:
                            @"catBodyHeadingRadians"]
                              doubleValue];
            }
            outsidePixels += outside;
            ++rasterCases;
          }
        }
        const double heading =
            [[view valueForKey:
                  @"catBodyHeadingRadians"]
                doubleValue];
        const auto expected =
            positionInputIndicator(
                target.point,
                {
                    .width = style.width,
                    .height = style.height,
                },
                screens,
                options.position,
                style.cursorGap,
                style.screenMargin,
                heading
            );
        QVERIFY(expected);
        if (std::abs(current.x -
                     expected->x) < 1.0 &&
            std::abs(current.y -
                     expected->y) < 1.0) {
          reached = true;
          for (const int scale : {1, 2}) {
            const int outside =
                rasterOutside(scale);
            QVERIFY(outside >= 0);
            if (outside > 0) {
              qInfo() << "follow-approach-spill-final"
                      << static_cast<int>(
                             companion
                         )
                      << speed.percent
                      << scale
                      << heading;
            }
            outsidePixels += outside;
            ++rasterCases;
          }
          break;
        }
      }
      QVERIFY(reached);
      QVERIFY(positions.size() > 10);
      panel.hide();
    }
  }
  qInfo() << "follow-approach-raster"
          << "cases" << rasterCases
          << "outsidePixels" << outsidePixels;
  QCOMPARE(rasterCases, 54);
  QCOMPARE(outsidePixels, 0);
}

void InputIndicatorGeometryTests::unknownOrInvalidGeometryHides_data()
{
  QTest::addColumn<int>("condition");

  QTest::newRow("no screens") << 0;
  QTest::newRow("cursor outside") << 1;
  QTest::newRow("zero width") << 2;
  QTest::newRow("zero badge height") << 3;
  QTest::newRow("invalid screen") << 4;
}

void InputIndicatorGeometryTests::unknownOrInvalidGeometryHides()
{
  QFETCH(int, condition);
  InputIndicatorPoint cursor{.x = 100, .y = 100};
  InputIndicatorSize badge{.width = 40, .height = 28};
  auto screens = oneScreen();

  switch (condition) {
  case 0:
    screens.clear();
    break;
  case 1:
    cursor = {.x = 3000, .y = 3000};
    break;
  case 2:
    badge.width = 0;
    break;
  case 3:
    badge.height = 0;
    break;
  case 4:
    screens.front().height = 0;
    break;
  default:
    QFAIL("unknown test row");
  }

  QVERIFY(!positionInputIndicator(
               cursor, badge, screens, InputIndicatorPosition::CursorBottomRight, 8, 8
  ).has_value());
}

void InputIndicatorGeometryTests::compactPanelStyleUsesApprovedMetrics()
{
  const auto cat =
      macInputIndicatorPanelStyle(InputIndicatorCompanion::Cat);
  const auto dog =
      macInputIndicatorPanelStyle(InputIndicatorCompanion::Dog);
  const auto turtle =
      macInputIndicatorPanelStyle(InputIndicatorCompanion::Turtle);

  QCOMPARE(cat.width, 33.0);
  QCOMPARE(cat.height, 31.0);
  QCOMPARE(cat.canvasWidth, 49.0);
  QCOMPARE(cat.canvasHeight, 49.0);
  QCOMPARE(cat.canvasOffsetX, 8.0);
  QCOMPARE(cat.canvasOffsetY, 9.0);
  QCOMPARE(dog.width, 33.0);
  QCOMPARE(dog.height, 31.0);
  QCOMPARE(dog.canvasWidth, 49.0);
  QCOMPARE(dog.canvasHeight, 49.0);
  QCOMPARE(dog.canvasOffsetX, 8.0);
  QCOMPARE(dog.canvasOffsetY, 9.0);
  QCOMPARE(turtle.width, 33.0);
  QCOMPARE(turtle.height, 31.0);
  QCOMPARE(turtle.canvasWidth, 49.0);
  QCOMPARE(turtle.canvasHeight, 49.0);
  QCOMPARE(turtle.canvasOffsetX, 8.0);
  QCOMPARE(turtle.canvasOffsetY, 9.0);
  QCOMPARE(cat.visualScale, 1.1);
  QCOMPARE(cat.logicalWidth, 30.0);
  QCOMPARE(cat.logicalHeight, 28.0);
  QCOMPARE(cat.cursorGap, 7.0);
  QCOMPARE(cat.screenMargin, 7.0);
  QCOMPARE(cat.labelSize, 8.0);
  QCOMPARE(cat.maximumLag, 9.0);
  QCOMPARE(cat.maximumMotion, 9.0);
  QCOMPARE(cat.transitionMilliseconds, 200.0);
  QCOMPARE(cat.opacityPercent, 65);

  const MacInputIndicatorPanelOptions options;
  QCOMPARE(options.opacityPercent, 65);
  QCOMPARE(options.companion, InputIndicatorCompanion::Cat);
}

void InputIndicatorGeometryTests::companionDisplayModelScalesAllLogicalGeometry()
{
  const auto point =
      macInputIndicatorDisplayPoint(
          {.x = 15, .y = 14}
      );
  QVERIFY(std::abs(point.x - 16.5) < 1e-9);
  QVERIFY(std::abs(point.y - 15.5) < 1e-9);

  const auto rect =
      macInputIndicatorDisplayRect(
          {.x = 1, .y = 2, .width = 28, .height = 26}
      );
  QVERIFY(std::abs(rect.x - 1.1) < 1e-9);
  QVERIFY(std::abs(rect.y - 2.3) < 1e-9);
  QVERIFY(std::abs(rect.width - 30.8) < 1e-9);
  QVERIFY(std::abs(rect.height - 28.6) < 1e-9);

  const auto preference =
      macInputIndicatorDisplayLabelPreference(
          deskflow::input::InputIndicatorSource::Korean2Set,
          InputIndicatorMotionState::Run
      );
  QVERIFY(std::abs(preference.pointSize - 6.71) < 1e-9);
  QVERIFY(std::abs(
      preference.minimumPointSize - 6.38
  ) < 1e-9);
}

void InputIndicatorGeometryTests::companionFacingTurnCompressesAndSwitchesOnce()
{
  const auto start =
      macInputIndicatorFacingPose(
          true, false, 0, false
      );
  QVERIFY(start.facesRight);
  QVERIFY(start.turning);
  QCOMPARE(start.horizontalScale, 1.0);

  const auto middle =
      macInputIndicatorFacingPose(
          true, false, 140, false
      );
  QVERIFY(!middle.facesRight);
  QVERIFY(middle.turning);
  QCOMPARE(middle.horizontalScale, 0.8);

  const auto end =
      macInputIndicatorFacingPose(
          true, false, 280, false
      );
  QVERIFY(!end.facesRight);
  QVERIFY(!end.turning);
  QCOMPARE(end.horizontalScale, 1.0);

  const auto reduced =
      macInputIndicatorFacingPose(
          true, false, 140, true
      );
  QVERIFY(!reduced.facesRight);
  QVERIFY(!reduced.turning);
  QCOMPARE(reduced.horizontalScale, 1.0);

  QVERIFY(macInputIndicatorFacingTurnStarts(
      true, false, false
  ));
  QVERIFY(!macInputIndicatorFacingTurnStarts(
      true, true, false
  ));
  QVERIFY(!macInputIndicatorFacingTurnStarts(
      true, false, true
  ));
}

void InputIndicatorGeometryTests::companionRenderModelUsesUnifiedSilhouetteAndInternalLabel()
{
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };

  for (const auto companion : companions) {
    const auto model = macInputIndicatorRenderModel(companion);
    QVERIFY(!model.usesEyePrimitives);
    QVERIFY(!model.languageMirrorsWithSilhouette);
    QVERIFY(model.usesSideProfile);
    QVERIFY(model.mirrorsWithMotion);
    QVERIFY(model.sourceFacesRight);
    QCOMPARE(
        model.silhouettePrimitive,
        InputIndicatorRenderPrimitive::SilhouetteUnion
    );
    QCOMPARE(
        model.languagePrimitive,
        InputIndicatorRenderPrimitive::LanguageGlyph
    );
    QVERIFY(model.totalWidth <= 30.0);
    QVERIFY(model.totalHeight <= 28.0);
    QVERIFY(model.languageBounds.x >= model.silhouetteBounds.x);
    QVERIFY(model.languageBounds.y >= model.silhouetteBounds.y);
    QVERIFY(
        model.languageBounds.x + model.languageBounds.width <=
        model.silhouetteBounds.x + model.silhouetteBounds.width
    );
    QVERIFY(
        model.languageBounds.y + model.languageBounds.height <=
        model.silhouetteBounds.y + model.silhouetteBounds.height
    );
  }
}

void InputIndicatorGeometryTests::catRenderModelUsesSideProfileAndPersistentFacing()
{
  const auto model =
      macInputIndicatorRenderModel(InputIndicatorCompanion::Cat);
  QVERIFY(model.usesSideProfile);
  QVERIFY(model.mirrorsWithMotion);
  QVERIFY(model.sourceFacesRight);
  QVERIFY(!model.usesTorsoPad);
  QVERIFY(model.headBounds.x > model.torsoBounds.x);
  QVERIFY(
      model.tailBounds.x <
      model.torsoBounds.x
  );
  QVERIFY(model.pawBounds.y <= model.torsoBounds.y);
  QVERIFY(
      model.languageBounds.x >= model.torsoBounds.x &&
      model.languageBounds.y >= model.torsoBounds.y &&
      model.languageBounds.x + model.languageBounds.width <=
          model.torsoBounds.x + model.torsoBounds.width &&
      model.languageBounds.y + model.languageBounds.height <=
          model.torsoBounds.y + model.torsoBounds.height
  );

  const auto insideSilhouette =
      [silhouette = model.silhouetteBounds](
          const InputIndicatorRect &bounds
      ) {
        return bounds.x >= silhouette.x &&
               bounds.y >= silhouette.y &&
               bounds.x + bounds.width <=
                   silhouette.x + silhouette.width &&
               bounds.y + bounds.height <=
                   silhouette.y + silhouette.height;
      };
  QVERIFY(insideSilhouette(model.headBounds));
  QVERIFY(insideSilhouette(model.torsoBounds));
  QVERIFY(insideSilhouette(model.pawBounds));
  QVERIFY(insideSilhouette(model.tailBounds));

  QVERIFY(inputIndicatorCompanionFacesRight(false, 1.0));
  QVERIFY(!inputIndicatorCompanionFacesRight(true, -1.0));
  QVERIFY(inputIndicatorCompanionFacesRight(true, 0.0));
  QVERIFY(!inputIndicatorCompanionFacesRight(false, 0.0));
  QVERIFY(!inputIndicatorCompanionFacesRight(
      false, std::numeric_limits<double>::quiet_NaN()
  ));
}

void InputIndicatorGeometryTests::catTorsoContourExpandsWithoutPadPrimitive()
{
  const auto idle = inputIndicatorCatPose(
      InputIndicatorMotionState::Idle, 0.0, false
  );
  const auto walk = inputIndicatorCatPose(
      InputIndicatorMotionState::Walk, 0.34, false
  );
  const auto run = inputIndicatorCatPose(
      InputIndicatorMotionState::Run, 0.34, false
  );
  const double idleCenterX =
      idle.innerLanguageBounds.x +
      idle.innerLanguageBounds.width / 2.0;
  const double idleCenterY =
      idle.innerLanguageBounds.y +
      idle.innerLanguageBounds.height / 2.0;

  const auto idleUpper = inputIndicatorCatTorsoPoint(
      idle, {.x = idleCenterX, .y = idleCenterY + 2.5}
  );
  QCOMPARE(idleUpper.x, idleCenterX);
  QVERIFY(idleUpper.y > idleCenterY + 2.5);
  QVERIFY(idleUpper.y <= idleCenterY + 4.0);

  for (const auto *pose : {&walk, &run}) {
    QVERIFY(pose->torsoBounds.height >= 8.0);
    QCOMPARE(
        pose->innerLanguageBounds.width, 8.0
    );
    QCOMPARE(
        pose->innerLanguageBounds.height, 7.0
    );
    QVERIFY(pose->silhouetteBounds.width <=
        macInputIndicatorCatArtwork(pose->artworkFrame)
            .silhouetteBounds.width +
        1.5);

    const double centerX =
        pose->innerLanguageBounds.x +
        pose->innerLanguageBounds.width / 2.0;
    const double centerY =
        pose->innerLanguageBounds.y +
        pose->innerLanguageBounds.height / 2.0;
    for (const double offset : {-3.5, 3.5}) {
      const auto input =
          InputIndicatorPoint{
              .x = centerX,
              .y = centerY + offset,
          };
      const auto output =
          inputIndicatorCatTorsoPoint(*pose, input);
      QCOMPARE(output.x, input.x);
      QVERIFY(std::abs(output.y - input.y) > 0);
      QVERIFY(std::abs(output.y - input.y) <= 1.5);
    }
    const auto outside =
        inputIndicatorCatTorsoPoint(
            *pose,
            {
                .x = centerX + 9,
                .y = centerY + 4,
            }
        );
    QCOMPARE(outside.x, centerX + 9);
    QCOMPARE(outside.y, centerY + 4);
  }
}

void InputIndicatorGeometryTests::catArtworkFramesFollowApprovedPoseSheet()
{
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0.0, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::IdleNeutral
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0.25, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::IdleRise
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0.75, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::IdleFall
  );

  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Walk, 0.0, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::WalkReach
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Walk, 0.34, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::WalkPass
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Walk, 0.67, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::WalkPush
  );

  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.0, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::RunCoil
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.34, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::RunAir
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.67, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::RunLand
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.75, true
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::IdleNeutral
  );
}

void InputIndicatorGeometryTests::catArtworkMetricsMatchApprovedSource()
{
  const auto idle = inputIndicatorCatPose(
      InputIndicatorMotionState::Idle, 0.0, false
  );
  const auto walk = inputIndicatorCatPose(
      InputIndicatorMotionState::Walk, 0.0, false
  );
  const auto run = inputIndicatorCatPose(
      InputIndicatorMotionState::Run, 0.34, false
  );
  const auto model =
      macInputIndicatorRenderModel(InputIndicatorCompanion::Cat);

  QVERIFY(std::abs(
      idle.silhouetteBounds.width - 20.6824
  ) < 0.02);
  QVERIFY(std::abs(
      idle.silhouetteBounds.height - 15.9470
  ) < 0.02);
  QVERIFY(std::abs(
      walk.silhouetteBounds.width - 21.8200
  ) < 0.02);
  QVERIFY(std::abs(
      walk.silhouetteBounds.height - 13.1184
  ) < 0.02);
  QVERIFY(
      walk.silhouetteBounds.height <= 13.1184
  );
  QVERIFY(std::abs(
      run.silhouetteBounds.width - 28.0000
  ) < 0.02);
  QVERIFY(
      run.silhouetteBounds.height >= 12.7445
  );
  QVERIFY(run.silhouetteBounds.height <= 12.745);

  const auto assertGlyph =
      [&model](
          const InputIndicatorCatPose &pose,
          double centerX,
          double centerY
      ) {
        QCOMPARE(pose.languageBounds.width, 9.0);
        QCOMPARE(pose.languageBounds.height, 8.0);
        QVERIFY(std::abs(
            pose.languageBounds.x +
                pose.languageBounds.width / 2.0 -
                centerX
        ) < 0.02);
        QVERIFY(std::abs(
            pose.languageBounds.y +
                pose.languageBounds.height / 2.0 -
                centerY
        ) < 0.02);
        QVERIFY(
            pose.languageBounds.x >= pose.silhouetteBounds.x &&
            pose.languageBounds.y >= pose.silhouetteBounds.y &&
            pose.languageBounds.x + pose.languageBounds.width <=
                pose.silhouetteBounds.x +
                    pose.silhouetteBounds.width &&
            pose.languageBounds.y + pose.languageBounds.height <=
                pose.silhouetteBounds.y +
                    pose.silhouetteBounds.height
        );
        const auto leftBounds =
            inputIndicatorCatLanguageBounds(pose, false);
        const auto mirroredCenterX =
            leftBounds.x + leftBounds.width / 2.0;
        QVERIFY(std::abs(
            mirroredCenterX - (model.totalWidth - centerX)
        ) < 0.02);
      };
  assertGlyph(idle, 17.4985, 9.0907);
  assertGlyph(walk, 16.3336, 10.5258);
  assertGlyph(run, 16.6132, 11.1921);

  const struct
  {
    InputIndicatorMotionState state;
    double phase;
    InputIndicatorCatArtworkFrame frame;
  } keyframes[] = {
      {.state = InputIndicatorMotionState::Idle,
       .phase = 0.0,
       .frame = InputIndicatorCatArtworkFrame::IdleNeutral},
      {.state = InputIndicatorMotionState::Idle,
       .phase = 0.25,
       .frame = InputIndicatorCatArtworkFrame::IdleRise},
      {.state = InputIndicatorMotionState::Idle,
       .phase = 0.75,
       .frame = InputIndicatorCatArtworkFrame::IdleFall},
      {.state = InputIndicatorMotionState::Walk,
       .phase = 0.0,
       .frame = InputIndicatorCatArtworkFrame::WalkReach},
      {.state = InputIndicatorMotionState::Walk,
       .phase = 0.34,
       .frame = InputIndicatorCatArtworkFrame::WalkPass},
      {.state = InputIndicatorMotionState::Walk,
       .phase = 0.67,
       .frame = InputIndicatorCatArtworkFrame::WalkPush},
      {.state = InputIndicatorMotionState::Run,
       .phase = 0.0,
       .frame = InputIndicatorCatArtworkFrame::RunCoil},
      {.state = InputIndicatorMotionState::Run,
       .phase = 0.34,
       .frame = InputIndicatorCatArtworkFrame::RunAir},
      {.state = InputIndicatorMotionState::Run,
       .phase = 0.67,
       .frame = InputIndicatorCatArtworkFrame::RunLand},
  };
  for (const auto &keyframe : keyframes) {
    const auto pose = inputIndicatorCatPose(
        keyframe.state, keyframe.phase, false
    );
    const auto artwork =
        macInputIndicatorCatArtwork(keyframe.frame);
    QCOMPARE(pose.artworkFrame, keyframe.frame);
    QVERIFY(!artwork.near.empty());
    QVERIFY(!artwork.far.empty());
    QCOMPARE(
        pose.silhouetteBounds.x,
        artwork.silhouetteBounds.x
    );
    QCOMPARE(
        pose.languageBounds.x,
        artwork.languageBounds.x
    );
    assertGlyph(
        pose,
        artwork.languageBounds.x +
            artwork.languageBounds.width / 2.0,
        artwork.languageBounds.y +
            artwork.languageBounds.height / 2.0
    );
  }
}

void InputIndicatorGeometryTests::catLabelLayoutFitsEveryArtworkPoseAndSource()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const struct
  {
    Source source;
    InputIndicatorMotionState state;
    double fontSize;
    double minimumFontSize;
  } preferences[] = {
      {.source = Source::LatinABC,
       .state = InputIndicatorMotionState::Idle,
       .fontSize = 8.0,
       .minimumFontSize = 7.2},
      {.source = Source::LatinABC,
       .state = InputIndicatorMotionState::Walk,
       .fontSize = 8.0,
       .minimumFontSize = 7.2},
      {.source = Source::LatinABC,
       .state = InputIndicatorMotionState::Run,
       .fontSize = 7.6,
       .minimumFontSize = 7.0},
      {.source = Source::Korean2Set,
       .state = InputIndicatorMotionState::Idle,
       .fontSize = 6.8,
       .minimumFontSize = 6.3},
      {.source = Source::Korean2Set,
       .state = InputIndicatorMotionState::Walk,
       .fontSize = 6.4,
       .minimumFontSize = 6.0},
      {.source = Source::Korean2Set,
       .state = InputIndicatorMotionState::Run,
       .fontSize = 6.1,
       .minimumFontSize = 5.8},
  };
  for (const auto &expected : preferences) {
    const auto preference =
        macInputIndicatorLabelPreference(
            expected.source, expected.state
        );
    QCOMPARE(preference.pointSize, expected.fontSize);
    QCOMPARE(
        preference.minimumPointSize,
        expected.minimumFontSize
    );
  }

  const struct
  {
    InputIndicatorMotionState state;
    double phase;
  } poses[] = {
      {.state = InputIndicatorMotionState::Idle, .phase = 0.0},
      {.state = InputIndicatorMotionState::Idle, .phase = 0.25},
      {.state = InputIndicatorMotionState::Idle, .phase = 0.75},
      {.state = InputIndicatorMotionState::Walk, .phase = 0.0},
      {.state = InputIndicatorMotionState::Walk, .phase = 0.34},
      {.state = InputIndicatorMotionState::Walk, .phase = 0.67},
      {.state = InputIndicatorMotionState::Run, .phase = 0.0},
      {.state = InputIndicatorMotionState::Run, .phase = 0.34},
      {.state = InputIndicatorMotionState::Run, .phase = 0.67},
  };
  const Source sources[] = {Source::LatinABC, Source::Korean2Set};
  for (const auto &poseInput : poses) {
    const auto pose = inputIndicatorCatPose(
        poseInput.state, poseInput.phase, false
    );
    for (const auto source : sources) {
      const auto preference =
          macInputIndicatorLabelPreference(
              source, poseInput.state
          );
      const MacInputIndicatorLabelMeasurement measurement{
          .origin = {
              .x = preference.pointSize *
                  (source == Source::Korean2Set ? 0.022 : 0.034),
              .y = source == Source::Korean2Set
                  ? -preference.pointSize * 0.055
                  : 0,
          },
          .size = {
              .width = preference.pointSize *
                  (source == Source::Korean2Set ? 0.831 : 0.643),
              .height = preference.pointSize *
                  (source == Source::Korean2Set ? 0.867 : 0.705),
          },
      };
      const auto layout = macInputIndicatorFitLabelInkLayout(
          preference.pointSize,
          preference.minimumPointSize,
          pose.innerLanguageBounds,
          measurement
      );
      QVERIFY(layout);
      QCOMPARE(layout->fontSize, preference.pointSize);
      QVERIFY(layout->inkBounds.x >= pose.innerLanguageBounds.x);
      QVERIFY(layout->inkBounds.y >= pose.innerLanguageBounds.y);
      QVERIFY(
          layout->inkBounds.x + layout->inkBounds.width <=
          pose.innerLanguageBounds.x +
              pose.innerLanguageBounds.width
      );
      QVERIFY(
          layout->inkBounds.y + layout->inkBounds.height <=
          pose.innerLanguageBounds.y +
              pose.innerLanguageBounds.height
      );
      if (source == Source::Korean2Set) {
        QVERIFY(measurement.origin.y < 0);
      }
      QVERIFY(pose.torsoBounds.height >=
          (poseInput.state == InputIndicatorMotionState::Idle
              ? 7.2
              : 8.0));
      QVERIFY(pose.innerLanguageBounds.width >= 8.0 - 1e-9);
      QVERIFY(pose.innerLanguageBounds.height >= 7.0 - 1e-9);
    }
  }

  const auto container =
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.34, false
      ).innerLanguageBounds;
  const auto oversized = macInputIndicatorFitLabelInkLayout(
      6.1,
  5.8,
      container,
      {
          .origin = {.x = 0.25, .y = 0},
      .size = {.width = 8.0, .height = 7.2},
      }
  );
  QVERIFY(oversized);
  QVERIFY(oversized->fontSize < 6.1);
  QVERIFY(oversized->fontSize >= 5.8);
  QVERIFY(oversized->inkBounds.x >= container.x);
  QVERIFY(oversized->inkBounds.y >= container.y);
  QVERIFY(
      oversized->inkBounds.x + oversized->inkBounds.width <=
      container.x + container.width
  );
  QVERIFY(
      oversized->inkBounds.y + oversized->inkBounds.height <=
      container.y + container.height
  );

  QVERIFY(!macInputIndicatorFitLabelInkLayout(
      6.8,
      6.3,
      container,
      {
          .origin = {.x = 0, .y = 0},
          .size = {
              .width = std::numeric_limits<double>::quiet_NaN(),
              .height = 1,
          },
      }
  ));
  QVERIFY(!macInputIndicatorFitLabelInkLayout(
      0,
      6.0,
      container,
      {
          .origin = {.x = 0, .y = 0},
          .size = {.width = 1, .height = 1},
      }
  ));
  QVERIFY(!macInputIndicatorFitLabelInkLayout(
      6.1,
      5.8,
      container,
      {
          .origin = {.x = 0, .y = 0},
          .size = {.width = 12.0, .height = 9.0},
      }
  ));
}

void InputIndicatorGeometryTests::coreTextInkBoundsExposeGlyphOrigin()
{
  @autoreleasepool {
    NSFont *hanFont =
        [NSFont systemFontOfSize:6.8
                          weight:NSFontWeightSemibold];
    const auto han =
        macInputIndicatorCoreTextLabelMeasurement(
            @"한", hanFont
        );
    QVERIFY(han);
    QVERIFY(han->ink.origin.y < -0.30);
    QVERIFY(han->typographicBounds.y < han->ink.origin.y);
    QVERIFY(han->ink.size.height < han->typographicBounds.height);

    NSFont *latinFont =
        [NSFont systemFontOfSize:8
                          weight:NSFontWeightSemibold];
    const auto latin =
        macInputIndicatorCoreTextLabelMeasurement(
            @"A", latinFont
        );
    QVERIFY(latin);
    QVERIFY(std::abs(latin->ink.origin.y) < 0.001);
  }
}

void InputIndicatorGeometryTests::coreTextCatGlyphPixelsStayInsideSilhouette()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  struct PaintPose
  {
    InputIndicatorMotionState state;
    double phase;
    InputIndicatorCatIdleTailFrame tail;
  };
  std::vector<PaintPose> poses;
  const double idlePhases[] = {0.0, 0.25, 0.75};
  const InputIndicatorCatIdleTailMotion idleMotions[] = {
      InputIndicatorCatIdleTailMotion::RelaxedSway,
      InputIndicatorCatIdleTailMotion::TipFlick,
      InputIndicatorCatIdleTailMotion::CurlUnfurl,
      InputIndicatorCatIdleTailMotion::LazySweep,
      InputIndicatorCatIdleTailMotion::QuestionLift,
  };
  const double idleProgress[] = {0.2, 0.4, 0.6, 0.8};
  for (const double phase : idlePhases) {
    for (const auto motion : idleMotions) {
      for (const double progress : idleProgress) {
        poses.push_back({
            .state = InputIndicatorMotionState::Idle,
            .phase = phase,
            .tail = {
                .motion = motion,
                .progress = progress,
                .active = true,
            },
        });
      }
    }
  }
  for (const double phase : {0.0, 0.34, 0.67}) {
    poses.push_back({
        .state = InputIndicatorMotionState::Walk,
        .phase = phase,
    });
    poses.push_back({
        .state = InputIndicatorMotionState::Run,
        .phase = phase,
    });
  }

  const Source sources[] = {Source::LatinABC, Source::Korean2Set};
  const bool directions[] = {true, false};
  constexpr NSInteger scale = 4;
  constexpr int paddingPoints = 4;
  const auto panelStyle =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  int cases = 0;
  int outsidePixels = 0;

  for (const auto &poseInput : poses) {
    const auto pose = inputIndicatorCatPose(
        poseInput.state, poseInput.phase, false
    );
    const auto artwork =
        macInputIndicatorCatArtwork(pose.artworkFrame);
    QVERIFY(pose.torsoBounds.height >=
        (poseInput.state == InputIndicatorMotionState::Idle
            ? 7.2
            : 8.0));
    const double unionLeft =
        std::min(
            artwork.silhouetteBounds.x,
            pose.torsoBounds.x
        );
    const double unionRight =
        std::max(
            artwork.silhouetteBounds.x +
                artwork.silhouetteBounds.width,
            pose.torsoBounds.x + pose.torsoBounds.width
        );
    QVERIFY(
        unionRight - unionLeft <=
        artwork.silhouetteBounds.width + 1.5
    );

    for (const auto source : sources) {
      NSString *label =
          source == Source::LatinABC ? @"A" : @"한";
      for (const bool facesRight : directions) {
        int caseOutsidePixels = 0;
        NSBitmapImageRep *bodyMask =
            catBodyBitmap(
                pose,
                artwork,
                poseInput.tail,
                facesRight,
                scale,
                paddingPoints,
                false
            );
        NSBitmapImageRep *panelMask =
            catBodyBitmap(
                pose,
                artwork,
                poseInput.tail,
                facesRight,
                scale,
                paddingPoints,
                true
            );
        NSBitmapImageRep *glyphMask =
            pixelBitmap(scale, paddingPoints);
        QVERIFY(bodyMask);
        QVERIFY(panelMask);
        QVERIFY(glyphMask);

        auto target =
            macInputIndicatorDisplayRect(
                inputIndicatorCatInnerLanguageBounds(
                    pose, facesRight
                )
            );
        target.x += paddingPoints;
        target.y += paddingPoints;
        const auto displayPreference =
            macInputIndicatorDisplayLabelPreference(
                source, poseInput.state
            );
        double pointSize = displayPreference.pointSize;
        std::optional<MacInputIndicatorLabelLayout> layout;
        NSFont *font = nil;
        for (int attempt = 0; attempt < 5; ++attempt) {
          font =
              [NSFont systemFontOfSize:pointSize
                                weight:NSFontWeightSemibold];
          const auto measured =
              macInputIndicatorCoreTextLabelMeasurement(
                  label, font
              );
          QVERIFY(measured);
          layout = macInputIndicatorFitLabelInkLayout(
              pointSize,
              displayPreference.minimumPointSize, target, measured->ink
          );
          QVERIFY(layout);
          if (std::abs(
                  layout->fontSize - pointSize
              ) < 0.01) {
            break;
          }
          pointSize = layout->fontSize;
        }
        QVERIFY(layout);
        QVERIFY(font);

        NSGraphicsContext *glyphContext =
            [NSGraphicsContext
                graphicsContextWithBitmapImageRep:glyphMask];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:glyphContext];
        CGContextRef glyphCG =
            [glyphContext CGContext];
        CGContextSaveGState(glyphCG);
        CGContextScaleCTM(glyphCG, scale, scale);
        macInputIndicatorDrawCoreTextLabel(
            glyphCG,
            label,
            font,
            [NSColor blackColor],
            *layout
        );
        CGContextRestoreGState(glyphCG);
        [NSGraphicsContext restoreGraphicsState];

        const NSInteger panelLeft =
            paddingPoints * scale;
        const NSInteger panelBottom =
            paddingPoints * scale;
        const NSInteger panelRight =
            panelLeft +
            static_cast<NSInteger>(
                std::lround(panelStyle.width)
            ) *
                scale;
        const NSInteger panelTop =
            panelBottom +
            static_cast<NSInteger>(
                std::lround(panelStyle.height)
            ) *
                scale;
        for (NSInteger y = 0;
             y < glyphMask.pixelsHigh;
             ++y) {
          for (NSInteger x = 0;
               x < glyphMask.pixelsWide;
               ++x) {
            const double glyphAlpha =
                [glyphMask colorAtX:x y:y].alphaComponent;
            const double bodyAlpha =
                [bodyMask colorAtX:x y:y].alphaComponent;
            const double panelAlpha =
                [panelMask colorAtX:x y:y].alphaComponent;
            const bool glyphOutsideBody =
                glyphAlpha > 0.02 &&
                bodyAlpha <= 0.02;
            const bool paintOutsidePanel =
                panelAlpha > 0.02 &&
                (x < panelLeft ||
                 x >= panelRight ||
                 y < panelBottom ||
                 y >= panelTop);
            if (glyphOutsideBody ||
                paintOutsidePanel) {
              ++caseOutsidePixels;
            }
          }
        }
        if (caseOutsidePixels > 0) {
          qInfo() << "contour-spill"
                  << static_cast<int>(poseInput.state)
                  << poseInput.phase
                  << static_cast<int>(source)
                  << facesRight
                  << static_cast<int>(
                         poseInput.tail.motion
                     )
                  << poseInput.tail.progress
                  << caseOutsidePixels;
        }
        outsidePixels += caseOutsidePixels;
        [bodyMask release];
        [panelMask release];
        [glyphMask release];
        ++cases;
      }
    }
  }
  qInfo() << "outsidePixels" << outsidePixels
          << "cases" << cases;
  QCOMPARE(
      cases,
      static_cast<int>(
          poses.size() *
          (sizeof(sources) / sizeof(sources[0])) *
          (sizeof(directions) /
           sizeof(directions[0]))
      )
  );
  QCOMPARE(outsidePixels, 0);
}

void InputIndicatorGeometryTests::catStartleRenderKeepsLabelAndCanvasStable()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto pose =
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0, false
      );
  const auto artwork =
      macInputIndicatorCatArtwork(pose.artworkFrame);
  const InputIndicatorCatIdleTailFrame restTail;
  const InputIndicatorCatStartleFrame restStartle;
  const Source sources[] = {
      Source::LatinABC,
      Source::Korean2Set,
  };
  const bool facings[] = {true, false};
  constexpr int paddingPoints = 4;
  const double ages[] = {70, 110, 160, 230, 299, 300};
  const auto state =
      inputIndicatorCatSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  int cases = 0;
  int outsidePixels = 0;

  for (const int scale : {1, 2}) {
    const NSInteger panelLeft =
        paddingPoints * scale;
    const NSInteger panelBottom =
        paddingPoints * scale;
    const NSInteger panelRight =
        panelLeft + 33 * scale;
    const NSInteger panelTop =
        panelBottom + 31 * scale;
    const NSInteger dorsalLeft =
        static_cast<NSInteger>(
            std::floor(
                (
                    paddingPoints +
                    macInputIndicatorDisplayPoint(
                        {.x = 13.0, .y = 0}
                    ).x
                ) *
                scale
            )
        );
    const NSInteger dorsalRight =
        static_cast<NSInteger>(
            std::ceil(
                (
                    paddingPoints +
                    macInputIndicatorDisplayPoint(
                        {.x = 18.5, .y = 0}
                    ).x
                ) *
                scale
            )
        );
    const NSInteger dorsalBottom =
        static_cast<NSInteger>(
            std::floor(
                (
                    paddingPoints +
                    macInputIndicatorDisplayPoint(
                        {.x = 0, .y = 9.5}
                    ).y
                ) *
                scale
            )
        );
    const NSInteger dorsalTop =
        static_cast<NSInteger>(
            std::ceil(
                (
                    paddingPoints +
                    macInputIndicatorDisplayPoint(
                        {.x = 0, .y = 17.0}
                    ).y
                ) *
                scale
            )
        );

    for (const auto source : sources) {
      for (const bool facesRight : facings) {
        NSBitmapImageRep *baseline =
            catBodyBitmap(
                pose,
                artwork,
                restTail,
                facesRight,
                scale,
                paddingPoints,
                false,
                restStartle
            );
        NSBitmapImageRep *glyph =
            catGlyphBitmap(
                pose,
                InputIndicatorMotionState::Idle,
                facesRight,
                scale,
                paddingPoints,
                source
            );
        QVERIFY(baseline);
        QVERIFY(glyph);

        for (const double age : ages) {
          const auto startleStep =
              inputIndicatorCatSourceStartleAdvance(
                  state, age, false
              );
          NSBitmapImageRep *body =
              catBodyBitmap(
                  pose,
                  artwork,
                  restTail,
                  facesRight,
                  scale,
                  paddingPoints,
                  false,
                  startleStep.frame
              );
          NSBitmapImageRep *paint =
              catBodyBitmap(
                  pose,
                  artwork,
                  restTail,
                  facesRight,
                  scale,
                  paddingPoints,
                  true,
                  startleStep.frame
              );
          QVERIFY(body);
          QVERIFY(paint);
          int changedPixels = 0;
          int dorsalPixels = 0;
          int caseOutsidePixels = 0;
          for (NSInteger y = 0;
               y < body.pixelsHigh;
               ++y) {
            for (NSInteger x = 0;
                 x < body.pixelsWide;
                 ++x) {
              const double baselineAlpha =
                  [baseline colorAtX:x y:y]
                      .alphaComponent;
              const double bodyAlpha =
                  [body colorAtX:x y:y]
                      .alphaComponent;
              const double paintAlpha =
                  [paint colorAtX:x y:y]
                      .alphaComponent;
              const double glyphAlpha =
                  [glyph colorAtX:x y:y]
                      .alphaComponent;
              if (std::abs(
                      baselineAlpha - bodyAlpha
                  ) > 0.02) {
                ++changedPixels;
                if (x >= dorsalLeft &&
                    x < dorsalRight &&
                    y >= dorsalBottom &&
                    y < dorsalTop) {
                  ++dorsalPixels;
                }
              }
              const bool glyphOutside =
                  glyphAlpha > 0.02 &&
                  bodyAlpha <= 0.02;
              const bool paintOutside =
                  paintAlpha > 0.02 &&
                  (x < panelLeft ||
                   x >= panelRight ||
                   y < panelBottom ||
                   y >= panelTop);
              if (glyphOutside ||
                  paintOutside) {
                ++caseOutsidePixels;
              }
            }
          }
          if (age == 160) {
            QVERIFY(dorsalPixels > 0);
          }
          if (age == 300) {
            QCOMPARE(changedPixels, 0);
          }
          outsidePixels += caseOutsidePixels;
          QCOMPARE(caseOutsidePixels, 0);
          [body release];
          [paint release];
          ++cases;
        }
        [baseline release];
        [glyph release];
      }
    }
  }
  qInfo() << "startle-outsidePixels"
          << outsidePixels
          << "cases" << cases;
  QCOMPARE(cases, 48);
  QCOMPARE(outsidePixels, 0);
}

void InputIndicatorGeometryTests::catIdleTailMotionChangesOnlyTailPixels()
{
  const auto pose =
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0, false
      );
  const auto artwork =
      macInputIndicatorCatArtwork(pose.artworkFrame);
  const struct
  {
    InputIndicatorCatIdleTailMotion motion;
    double fromProgress;
    double toProgress;
  } windows[] = {
      {
          InputIndicatorCatIdleTailMotion::RelaxedSway,
          0.25,
          0.75,
      },
      {
          InputIndicatorCatIdleTailMotion::TipFlick,
          0.40,
          0.60,
      },
      {
          InputIndicatorCatIdleTailMotion::CurlUnfurl,
          0.10,
          0.50,
      },
      {
          InputIndicatorCatIdleTailMotion::LazySweep,
          0.25,
          0.75,
      },
      {
          InputIndicatorCatIdleTailMotion::QuestionLift,
          0.10,
          0.55,
      },
  };
  constexpr int paddingPoints = 4;
  for (const int scale : {1, 2}) {
    const NSInteger tailBoundaryX =
        static_cast<NSInteger>(
            std::ceil(
                (
                    paddingPoints +
                    macInputIndicatorDisplayPoint(
                        {.x = 13.2, .y = 0}
                    ).x
                ) *
                scale
            )
        );
    for (const auto &window : windows) {
      const InputIndicatorCatIdleTailFrame fromFrame{
          .motion = window.motion,
          .progress = window.fromProgress,
          .active = true,
      };
      const InputIndicatorCatIdleTailFrame toFrame{
          .motion = window.motion,
          .progress = window.toProgress,
          .active = true,
      };
      NSBitmapImageRep *from =
          catBodyBitmap(
              pose,
              artwork,
              fromFrame,
              true,
              scale,
              paddingPoints,
              false
          );
      NSBitmapImageRep *to =
          catBodyBitmap(
              pose,
              artwork,
              toFrame,
              true,
              scale,
              paddingPoints,
              false
          );
      NSBitmapImageRep *fromGlyph =
          catGlyphBitmap(
              pose,
              InputIndicatorMotionState::Idle,
              true,
              scale,
              paddingPoints
          );
      NSBitmapImageRep *toGlyph =
          catGlyphBitmap(
              pose,
              InputIndicatorMotionState::Idle,
              true,
              scale,
              paddingPoints
          );
      QVERIFY(from);
      QVERIFY(to);
      QVERIFY(fromGlyph);
      QVERIFY(toGlyph);
      const auto difference =
          catPixelDifference(
              from, to, tailBoundaryX
          );
      const int glyphChanged =
          bitmapChangedPixels(fromGlyph, toGlyph);
      qInfo() << "idle-tail-pixels"
              << static_cast<int>(window.motion)
              << "scale" << scale
              << "tail" << difference.tail
              << "nonTail" << difference.nonTail
              << "glyph" << glyphChanged;
      QVERIFY(
          difference.tail >=
          (scale == 1 ? 20 : 65)
      );
      QCOMPARE(difference.nonTail, 0);
      QCOMPARE(glyphChanged, 0);
      [from release];
      [to release];
      [fromGlyph release];
      [toGlyph release];
    }
  }

  for (const int scale : {1, 2}) {
    for (const auto &motion :
         {
             InputIndicatorCatIdleTailMotion::RelaxedSway,
             InputIndicatorCatIdleTailMotion::TipFlick,
             InputIndicatorCatIdleTailMotion::CurlUnfurl,
             InputIndicatorCatIdleTailMotion::LazySweep,
             InputIndicatorCatIdleTailMotion::QuestionLift,
         }) {
      const double duration =
          inputIndicatorCatIdleTailMotionDuration(motion);
      int visibleTicks = 0;
      int ticks = 0;
      int maximumTailPixels = 0;
      const NSInteger tailBoundaryX =
          static_cast<NSInteger>(
              std::ceil(
                  (
                      paddingPoints +
                      macInputIndicatorDisplayPoint(
                          {.x = 13.2, .y = 0}
                      ).x
                  ) *
                  scale
              )
          );
      for (double milliseconds = 0;
           milliseconds + 75.0 < duration;
           milliseconds += 75.0) {
        const InputIndicatorCatIdleTailFrame fromFrame{
            .motion = motion,
            .progress = milliseconds / duration,
            .active = true,
        };
        const InputIndicatorCatIdleTailFrame toFrame{
            .motion = motion,
            .progress =
                (milliseconds + 75.0) / duration,
            .active = true,
        };
        NSBitmapImageRep *from =
            catBodyBitmap(
                pose,
                artwork,
                fromFrame,
                true,
                scale,
                paddingPoints,
                false
            );
        NSBitmapImageRep *to =
            catBodyBitmap(
                pose,
                artwork,
                toFrame,
                true,
                scale,
                paddingPoints,
                false
            );
        QVERIFY(from);
        QVERIFY(to);
        const auto difference =
            catPixelDifference(
                from, to, tailBoundaryX
            );
        if (difference.tail > 0) {
          ++visibleTicks;
        }
        maximumTailPixels =
            std::max(
                maximumTailPixels,
                difference.tail
            );
        QCOMPARE(difference.nonTail, 0);
        [from release];
        [to release];
        ++ticks;
      }
      qInfo() << "idle-tail-cadence"
              << static_cast<int>(motion)
              << "scale" << scale
              << "visibleTicks" << visibleTicks
              << "ticks" << ticks
              << "maxTailPixels"
              << maximumTailPixels;
      QVERIFY(visibleTicks > 0);
    }
  }
}

void InputIndicatorGeometryTests::catPanelIdleTailFrameRepaints()
{
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorPoint fixedPointer{
      .x = fixedCursor.x,
      .y = fixedCursor.y,
  };
  MacInputIndicatorPanel panel(
      {},
      [fixedPointer] {
        return InputIndicatorTarget{
            .point = fixedPointer,
            .kind =
                InputIndicatorTargetKind::Pointer,
        };
      }
  );
  const MacInputIndicatorPanelOptions options{
      .source =
          deskflow::input::InputIndicatorSource::LatinABC,
      .companion = InputIndicatorCompanion::Cat,
  };
  QVERIFY(panel.show(options));

  NSView *view = catPanelView();
  QVERIFY(view);
  QVERIFY(panel.move(options.position).visible);
  [view displayIfNeeded];
  const double beforeProgress =
      [[view valueForKey:@"catIdleTailProgress"]
          doubleValue];
  const NSInteger beforeMotion =
      [[view valueForKey:@"catIdleTailMotion"]
          integerValue];
  const bool beforeActive =
      [[view valueForKey:@"catIdleTailActive"]
          boolValue];
  QVERIFY(beforeActive);
  NSBitmapImageRep *before =
      [view bitmapImageRepForCachingDisplayInRect:view.bounds];
  QVERIFY(before);
  [view cacheDisplayInRect:view.bounds
         toBitmapImageRep:before];

  QTest::qWait(650);
  const auto result =
      panel.move(options.position);
  qInfo() << "cat-panel-state"
          << "interval"
          << result.frameIntervalMilliseconds
          << "motionState"
          << [[view valueForKey:@"motionState"]
                 integerValue]
          << "motionX"
          << [[view valueForKey:@"motionX"]
                 doubleValue]
          << "motionY"
          << [[view valueForKey:@"motionY"]
                 doubleValue]
          << "facesRight"
          << [[view valueForKey:@"facesRight"]
                 boolValue]
          << "facingScaleX"
          << [[view valueForKey:@"facingScaleX"]
                 doubleValue]
          << "transitionProgress"
          << [[view valueForKey:@"transitionProgress"]
                 doubleValue];
  QVERIFY(result.visible);
  QVERIFY(result.animating);
  QVERIFY(!result.reducedMotion);
  QCOMPARE(result.frameIntervalMilliseconds, 75);
  [view displayIfNeeded];
  const double afterProgress =
      [[view valueForKey:@"catIdleTailProgress"]
          doubleValue];
  const bool afterActive =
      [[view valueForKey:@"catIdleTailActive"]
          boolValue];
  const NSInteger afterMotion =
      [[view valueForKey:@"catIdleTailMotion"]
          integerValue];
  QVERIFY(afterActive);
  QVERIFY(
      afterMotion != beforeMotion ||
      std::abs(afterProgress - beforeProgress) >
          1e-9
  );
  NSBitmapImageRep *after =
      [view bitmapImageRepForCachingDisplayInRect:view.bounds];
  QVERIFY(after);
  [view cacheDisplayInRect:view.bounds
         toBitmapImageRep:after];
  const double backingScale =
      static_cast<double>(after.pixelsWide) /
      view.bounds.size.width;
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  const NSInteger tailBoundaryX =
      static_cast<NSInteger>(
          std::ceil(
              (
                  style.canvasOffsetX +
                  macInputIndicatorDisplayPoint(
                      {.x = 13.2, .y = 0}
                  ).x
              ) *
              backingScale
          )
      );
  const auto difference =
      catPixelDifference(
          before, after, tailBoundaryX
      );
  qInfo() << "cat-panel-repaint"
          << beforeProgress << afterProgress
          << "tail" << difference.tail
          << "nonTail" << difference.nonTail
          << "backingScale" << backingScale;
  QVERIFY(difference.tail > 0);
  QCOMPARE(difference.nonTail, 0);
  panel.hide();
}

void InputIndicatorGeometryTests::catPoseUsesDistinctSeatedWalkRunGeometry()
{
  const auto model =
      macInputIndicatorRenderModel(InputIndicatorCompanion::Cat);
  const auto idle = inputIndicatorCatPose(
      InputIndicatorMotionState::Idle, 0.0, false
  );
  const auto walk = inputIndicatorCatPose(
      InputIndicatorMotionState::Walk, 0.0, false
  );
  const auto run = inputIndicatorCatPose(
      InputIndicatorMotionState::Run, 0.34, false
  );

  QVERIFY(idle.torsoBounds.height > walk.torsoBounds.height);
  QVERIFY(walk.torsoBounds.width > idle.torsoBounds.width);
  QVERIFY(run.torsoBounds.width > walk.torsoBounds.width);
  QVERIFY(run.torsoBounds.height <= walk.torsoBounds.height);
  QVERIFY(std::abs(idle.bodyAngleDegrees) < 1e-9);
  QVERIFY(std::abs(walk.bodyAngleDegrees) < 1e-9);
  QVERIFY(std::abs(run.bodyAngleDegrees) > 0);
  QVERIFY(std::abs(run.bodyAngleDegrees) <= 8.0);
  QVERIFY(run.frontPawBounds.x > walk.frontPawBounds.x);
  QVERIFY(run.rearPawBounds.x < walk.rearPawBounds.x);
  QVERIFY(walk.tailStraightness > idle.tailStraightness);
  QVERIFY(run.tailStraightness > walk.tailStraightness);

  const auto insideSilhouette =
      [silhouette = model.silhouetteBounds](
          const InputIndicatorRect &bounds
      ) {
        constexpr double tolerance = 0.0001;
        return bounds.x >= silhouette.x - tolerance &&
               bounds.y >= silhouette.y - tolerance &&
               bounds.x + bounds.width <=
                   silhouette.x + silhouette.width + tolerance &&
               bounds.y + bounds.height <=
                   silhouette.y + silhouette.height + tolerance;
      };
  const InputIndicatorCatPose poses[] = {idle, walk, run};
  for (const auto &pose : poses) {
    QVERIFY(insideSilhouette(pose.headBounds));
    QVERIFY(insideSilhouette(pose.torsoBounds));
    QVERIFY(insideSilhouette(pose.frontPawBounds));
    QVERIFY(insideSilhouette(pose.rearPawBounds));
    QVERIFY(
        pose.languageBounds.x >= pose.torsoBounds.x &&
        pose.languageBounds.y >= pose.torsoBounds.y &&
        pose.languageBounds.x + pose.languageBounds.width <=
            pose.torsoBounds.x + pose.torsoBounds.width &&
        pose.languageBounds.y + pose.languageBounds.height <=
            pose.torsoBounds.y + pose.torsoBounds.height
    );
  }

  const auto reduced = inputIndicatorCatPose(
      InputIndicatorMotionState::Run, 0.75, true
  );
  QCOMPARE(reduced.torsoBounds.x, idle.torsoBounds.x);
  QCOMPARE(reduced.torsoBounds.y, idle.torsoBounds.y);
  QCOMPARE(reduced.torsoBounds.width, idle.torsoBounds.width);
  QCOMPARE(reduced.torsoBounds.height, idle.torsoBounds.height);
  QCOMPARE(reduced.frontPawBounds.x, idle.frontPawBounds.x);
  QCOMPARE(reduced.rearPawBounds.x, idle.rearPawBounds.x);
  QCOMPARE(reduced.bodyAngleDegrees, 0.0);
  QCOMPARE(reduced.tailStraightness, idle.tailStraightness);
}

void InputIndicatorGeometryTests::companionPaletteUsesNeutralBodyAndReadableGlyph()
{
  const auto automatic =
      macInputIndicatorColorModel(InputIndicatorTone::Automatic);
  const auto light =
      macInputIndicatorColorModel(InputIndicatorTone::Light);
  const auto dark =
      macInputIndicatorColorModel(InputIndicatorTone::Dark);

  const MacInputIndicatorColorModel colors[] = {
      automatic, light, dark
  };
  for (const auto &color : colors) {
    QCOMPARE(color.body.red, 242.0 / 255.0);
    QCOMPARE(color.body.green, 243.0 / 255.0);
    QCOMPARE(color.body.blue, 245.0 / 255.0);
    QVERIFY(color.latinGlyph.blue > color.latinGlyph.red);
    QVERIFY(color.koreanGlyph.red > color.koreanGlyph.blue);
    QVERIFY(contrastRatio(color.latinGlyph, color.body) >= 4.5);
    QVERIFY(contrastRatio(color.koreanGlyph, color.body) >= 4.5);
    QVERIFY(color.latinGlyph.red < 0.75);
    QVERIFY(color.koreanGlyph.red < 0.75);
  }
  QCOMPARE(light.body.red, automatic.body.red);
  QCOMPARE(dark.body.red, automatic.body.red);
  QCOMPARE(light.edge.green, automatic.edge.green);
  QCOMPARE(dark.edge.green, automatic.edge.green);

  const InputIndicatorTone tones[] = {
      InputIndicatorTone::Automatic,
      InputIndicatorTone::Light,
      InputIndicatorTone::Dark,
  };
  for (const auto tone : tones) {
    const auto palette = macInputIndicatorColorModel(tone);
    const auto tintedLatinBody = macInputIndicatorBodyColor(
        deskflow::input::InputIndicatorSource::LatinABC, tone
    );
    const auto tintedKoreanBody = macInputIndicatorBodyColor(
        deskflow::input::InputIndicatorSource::Korean2Set, tone
    );
    QVERIFY(contrastRatio(
        palette.latinGlyph, tintedLatinBody
    ) >= 4.5);
    QVERIFY(contrastRatio(
        palette.koreanGlyph, tintedKoreanBody
    ) >= 4.5);
  }

  const auto latinBody = macInputIndicatorBodyColor(
      deskflow::input::InputIndicatorSource::LatinABC,
      InputIndicatorTone::Automatic
  );
  const auto koreanBody = macInputIndicatorBodyColor(
      deskflow::input::InputIndicatorSource::Korean2Set,
      InputIndicatorTone::Automatic
  );
  QVERIFY(std::abs(
      latinBody.red - automatic.body.red
  ) <= std::abs(
      automatic.latinGlyph.red - automatic.body.red
  ) * 0.08 + 1e-12);
  QVERIFY(std::abs(
      koreanBody.blue - automatic.body.blue
  ) <= std::abs(
      automatic.koreanGlyph.blue - automatic.body.blue
  ) * 0.08 + 1e-12);
}

void InputIndicatorGeometryTests::companionRenderAlphaFollowsSettingAndBoundsDetail()
{
  const auto minimum =
      macInputIndicatorRenderAlpha(59);
  QCOMPARE(minimum.body, 0.6);
  QCOMPARE(minimum.detail, 0.7);

  const auto normal =
      macInputIndicatorRenderAlpha(65);
  QCOMPARE(normal.body, 0.65);
  QCOMPARE(normal.detail, 0.75);

  const auto maximum =
      macInputIndicatorRenderAlpha(101);
  QCOMPARE(maximum.body, 1.0);
  QCOMPARE(maximum.detail, 1.0);

  for (const auto &alpha :
       {minimum, normal, maximum}) {
    QVERIFY(alpha.detail >= alpha.body);
    QVERIFY(alpha.detail - alpha.body <= 0.10000001);
  }
}

void InputIndicatorGeometryTests::catTailUsesContinuousPhaseLaggedCurve()
{
  const InputIndicatorPoint root{.x = 3, .y = 8};
  const auto neutral = inputIndicatorCatTailPose(
      root, 10, 0, 0, 0.15, true
  );
  const auto atZero = inputIndicatorCatTailPose(
      root, 10, 0, 0, 0.15, false
  );
  const auto atQuarter = inputIndicatorCatTailPose(
      root, 10, 0.25, 0, 0.15, false
  );
  const auto positiveVelocity = inputIndicatorCatTailPose(
      root, 10, 0.25, 8, 0.15, false
  );
  const auto negativeVelocity = inputIndicatorCatTailPose(
      root, 10, 0.25, -8, 0.15, false
  );
  const auto reducedFast = inputIndicatorCatTailPose(
      root, 10, 0.25, 8, 0.15, true
  );
  const auto runTail = inputIndicatorCatTailPose(
      root, 10, 0.25, 8, 0.85, false
  );

  QCOMPARE(positiveVelocity.root.x, root.x);
  QCOMPARE(positiveVelocity.root.y, root.y);
  QVERIFY(std::abs(
      (positiveVelocity.middle.x -
       positiveVelocity.middleIncomingControl.x) -
      (positiveVelocity.middleOutgoingControl.x -
       positiveVelocity.middle.x)
  ) < 1e-9);
  QVERIFY(std::abs(
      (positiveVelocity.middle.y -
       positiveVelocity.middleIncomingControl.y) -
      (positiveVelocity.middleOutgoingControl.y -
       positiveVelocity.middle.y)
  ) < 1e-9);

  const auto middleDelta =
      atQuarter.middle.y - atZero.middle.y;
  const auto tipDelta =
      atQuarter.tip.y - atZero.tip.y;
  QVERIFY(tipDelta < middleDelta);
  QVERIFY(positiveVelocity.tip.y < atQuarter.tip.y);
  QVERIFY(negativeVelocity.tip.y > atQuarter.tip.y);

  QVERIFY(std::abs(positiveVelocity.tip.y - root.y) <= 6.0);
  QVERIFY(
      std::abs(runTail.tip.y - runTail.middle.y) <
      std::abs(positiveVelocity.tip.y - positiveVelocity.middle.y)
  );
  QVERIFY(positiveVelocity.tip.x >= root.x);
  QVERIFY(positiveVelocity.tip.x <= root.x + 10.5);
  QCOMPARE(reducedFast.middle.x, neutral.middle.x);
  QCOMPARE(reducedFast.middle.y, neutral.middle.y);
  QCOMPARE(reducedFast.tip.x, neutral.tip.x);
  QCOMPARE(reducedFast.tip.y, neutral.tip.y);
}

void InputIndicatorGeometryTests::catIdleTailMotionsCoverFiveFinitePatterns()
{
  auto state =
      inputIndicatorCatIdleTailMotionInitialState(
          0x13579bdfU
      );
  std::vector<InputIndicatorCatIdleTailMotion> motions;
  double now = 0;
  for (int index = 0; index < 10; ++index) {
    const auto step =
        inputIndicatorCatIdleTailMotionAdvance(
            state,
            InputIndicatorMotionState::Idle,
            now,
            false
        );
    QVERIFY(step.frame.active);
    QVERIFY(std::isfinite(step.frame.progress));
    QVERIFY(step.frame.progress >= 0);
    QVERIFY(step.frame.progress < 1);
    if (!motions.empty()) {
      QVERIFY(motions.back() != step.frame.motion);
    }
    motions.push_back(step.frame.motion);
    state = step.state;
    now =
        state.startedMilliseconds +
        inputIndicatorCatIdleTailMotionDuration(
            step.frame.motion
        );
  }

  const std::set<InputIndicatorCatIdleTailMotion> ids(
      motions.begin(),
      motions.begin() + 5
  );
  QCOMPARE(ids.size(), size_t{5});

  const std::array<InputIndicatorCatIdleTailMotion, 5>
      allMotions{{
          InputIndicatorCatIdleTailMotion::RelaxedSway,
          InputIndicatorCatIdleTailMotion::TipFlick,
          InputIndicatorCatIdleTailMotion::CurlUnfurl,
          InputIndicatorCatIdleTailMotion::LazySweep,
          InputIndicatorCatIdleTailMotion::QuestionLift,
      }};
  std::array<std::vector<InputIndicatorPoint>, 5> samples;
  for (size_t motionIndex = 0;
       motionIndex < allMotions.size();
       ++motionIndex) {
    for (int sample = 0; sample <= 100; ++sample) {
      const double progress = sample / 100.0;
      const auto offset =
          inputIndicatorCatIdleTailOffset(
              allMotions[motionIndex], progress
          );
      QVERIFY(std::isfinite(offset.x));
      QVERIFY(std::isfinite(offset.y));
      QVERIFY(std::abs(offset.x) <= 1.60 + 1e-12);
      QVERIFY(std::abs(offset.y) <= 2.60 + 1e-12);
      if (sample % 20 == 0) {
        samples[motionIndex].push_back(offset);
      }
    }
    const auto start =
        inputIndicatorCatIdleTailOffset(
            allMotions[motionIndex], 0
        );
    const auto end =
        inputIndicatorCatIdleTailOffset(
            allMotions[motionIndex], 1
        );
    QCOMPARE(start.x, 0.0);
    QCOMPARE(start.y, 0.0);
    QCOMPARE(end.x, 0.0);
    QCOMPARE(end.y, 0.0);
  }
  for (size_t left = 0; left < samples.size(); ++left) {
    for (size_t right = left + 1;
         right < samples.size();
         ++right) {
      double difference = 0;
      for (size_t index = 0;
           index < samples[left].size();
           ++index) {
        difference = std::max(
            difference,
            std::hypot(
                samples[left][index].x -
                    samples[right][index].x,
                samples[left][index].y -
                    samples[right][index].y
            )
        );
      }
      QVERIFY(difference > 0.1);
    }
  }
}

void InputIndicatorGeometryTests::catIdleTailMotionGatesMovementAndReducedMotion()
{
  const auto fresh =
      inputIndicatorCatIdleTailMotionInitialState(
          0x2468ace0U
      );
  const auto idle =
      inputIndicatorCatIdleTailMotionAdvance(
          fresh,
          InputIndicatorMotionState::Idle,
          1000,
          false
      );
  QVERIFY(idle.frame.active);

  const auto walk =
      inputIndicatorCatIdleTailMotionAdvance(
          idle.state,
          InputIndicatorMotionState::Walk,
          1100,
          false
      );
  QVERIFY(!walk.frame.active);
  QCOMPARE(walk.frame.progress, 0.0);
  const auto run =
      inputIndicatorCatIdleTailMotionAdvance(
          walk.state,
          InputIndicatorMotionState::Run,
          1200,
          false
      );
  QVERIFY(!run.frame.active);
  QCOMPARE(run.frame.progress, 0.0);

  const auto reentered =
      inputIndicatorCatIdleTailMotionAdvance(
          run.state,
          InputIndicatorMotionState::Idle,
          2000,
          false
      );
  QVERIFY(reentered.frame.active);
  QVERIFY(reentered.frame.motion != idle.frame.motion);
  QCOMPARE(reentered.frame.progress, 0.0);
  QCOMPARE(reentered.state.startedMilliseconds, 2000.0);

  const auto reduced =
      inputIndicatorCatIdleTailMotionAdvance(
          fresh,
          InputIndicatorMotionState::Idle,
          1000,
          true
      );
  QVERIFY(!reduced.frame.active);
  QCOMPARE(reduced.state.seed, fresh.seed);
  QCOMPARE(
      reduced.state.remainingMask,
      fresh.remainingMask
  );
  QCOMPARE(
      reduced.state.hasLastMotion,
      fresh.hasLastMotion
  );
  QCOMPARE(reduced.frame.progress, 0.0);
  const InputIndicatorPoint rest{.x = 3.4, .y = 7.0};
  const auto unchanged =
      inputIndicatorCatIdleTailPoint(
          rest, reduced.frame
      );
  QCOMPARE(unchanged.x, rest.x);
  QCOMPARE(unchanged.y, rest.y);

  const auto invalidTime =
      inputIndicatorCatIdleTailMotionAdvance(
          fresh,
          InputIndicatorMotionState::Idle,
          std::numeric_limits<double>::quiet_NaN(),
          false
      );
  QVERIFY(std::isfinite(
      invalidTime.frame.progress
  ));
  QVERIFY(std::isfinite(
      invalidTime.state.startedMilliseconds
  ));

  const auto nan =
      inputIndicatorCatIdleTailOffset(
          InputIndicatorCatIdleTailMotion::LazySweep,
          std::numeric_limits<double>::quiet_NaN()
      );
  const auto negative =
      inputIndicatorCatIdleTailOffset(
          InputIndicatorCatIdleTailMotion::LazySweep,
          -1
      );
  const auto over =
      inputIndicatorCatIdleTailOffset(
          InputIndicatorCatIdleTailMotion::LazySweep,
          2
      );
  QCOMPARE(nan.x, 0.0);
  QCOMPARE(nan.y, 0.0);
  QCOMPARE(negative.x, 0.0);
  QCOMPARE(negative.y, 0.0);
  QCOMPARE(over.x, 0.0);
  QCOMPARE(over.y, 0.0);
}

void InputIndicatorGeometryTests::catIdleTailMotionIsSeededAndFacingDeterministic()
{
  auto leftState =
      inputIndicatorCatIdleTailMotionInitialState(
          0x10203040U
      );
  auto rightState =
      inputIndicatorCatIdleTailMotionInitialState(
          0x10203040U
      );
  for (const double time : {0.0, 300.0, 900.0, 1800.0, 2700.0}) {
    const auto left =
        inputIndicatorCatIdleTailMotionAdvance(
            leftState,
            InputIndicatorMotionState::Idle,
            time,
            false
        );
    const auto right =
        inputIndicatorCatIdleTailMotionAdvance(
            rightState,
            InputIndicatorMotionState::Idle,
            time,
            false
        );
    QCOMPARE(left.state.seed, right.state.seed);
    QCOMPARE(
        left.state.remainingMask,
        right.state.remainingMask
    );
    QCOMPARE(
        left.state.lastMotion,
        right.state.lastMotion
    );
    QCOMPARE(left.state.running, right.state.running);
    QCOMPARE(
        left.state.startedMilliseconds,
        right.state.startedMilliseconds
    );
    QCOMPARE(left.frame.motion, right.frame.motion);
    QCOMPARE(left.frame.progress, right.frame.progress);
    leftState = left.state;
    rightState = right.state;
  }

  bool differentOrder = false;
  auto firstState =
      inputIndicatorCatIdleTailMotionInitialState(1);
  auto secondState =
      inputIndicatorCatIdleTailMotionInitialState(
          0xfeedcafeU
      );
  for (int index = 0; index < 5; ++index) {
    const double time =
        index == 0
            ? 0
            : firstState.startedMilliseconds +
                  inputIndicatorCatIdleTailMotionDuration(
                      firstState.lastMotion
                  );
    const auto first =
        inputIndicatorCatIdleTailMotionAdvance(
            firstState,
            InputIndicatorMotionState::Idle,
            time,
            false
        );
    const double secondTime =
        index == 0
            ? 0
            : secondState.startedMilliseconds +
                  inputIndicatorCatIdleTailMotionDuration(
                      secondState.lastMotion
                  );
    const auto second =
        inputIndicatorCatIdleTailMotionAdvance(
            secondState,
            InputIndicatorMotionState::Idle,
            secondTime,
            false
        );
    differentOrder =
        differentOrder ||
        first.frame.motion != second.frame.motion;
    firstState = first.state;
    secondState = second.state;
  }
  QVERIFY(differentOrder);

  const auto active =
      inputIndicatorCatIdleTailMotionAdvance(
          inputIndicatorCatIdleTailMotionInitialState(
              0xabcdef01U
          ),
          InputIndicatorMotionState::Idle,
          0,
          false
      );
  const auto midpoint =
      inputIndicatorCatIdleTailMotionAdvance(
          active.state,
          InputIndicatorMotionState::Idle,
          inputIndicatorCatIdleTailMotionDuration(
              active.frame.motion
          ) * 0.5,
          false
      );
  QVERIFY(midpoint.frame.active);
  const auto sourcePoint =
      InputIndicatorPoint{.x = 3.4, .y = 7.0};
  const auto right =
      inputIndicatorCatIdleTailPoint(
          sourcePoint, midpoint.frame
      );
  const auto left =
      inputIndicatorCatFacingPoint(right, false);
  const auto restored =
      inputIndicatorCatFacingPoint(left, false);
  QCOMPARE(left.x, 30.0 - right.x);
  QCOMPARE(left.y, right.y);
  QCOMPARE(restored.x, right.x);
  QCOMPARE(restored.y, right.y);
  QVERIFY(right.x != sourcePoint.x ||
          right.y != sourcePoint.y);
}

void InputIndicatorGeometryTests::catSourceStartleTimelineUsesFiniteNeutralEndpoints()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  InputIndicatorCatSourceStartleState state;
  state = inputIndicatorCatSourceStartleTrigger(
      state,
      Source::LatinABC,
      Source::Korean2Set,
      100,
      false
  );

  const auto start =
      inputIndicatorCatSourceStartleAdvance(
          state, 100, false
      );
  const auto recoil =
      inputIndicatorCatSourceStartleAdvance(
          state, 170, false
      );
  const auto fur =
      inputIndicatorCatSourceStartleAdvance(
          state, 260, false
      );
  const auto ended =
      inputIndicatorCatSourceStartleAdvance(
          state, 400, false
      );

  QCOMPARE(
      start.state.latestSource,
      Source::Korean2Set
  );
  QVERIFY(start.frame.active);
  QCOMPARE(start.frame.recoilOffsetX, 0.0);
  QCOMPARE(start.frame.horizontalScale, 1.0);
  QCOMPARE(start.frame.dorsalLift, 0.0);
  QVERIFY(recoil.frame.active);
  QVERIFY(recoil.frame.recoilOffsetX < 0);
  QVERIFY(recoil.frame.horizontalScale >= 0.92);
  QCOMPARE(recoil.frame.dorsalLift, 0.0);
  QVERIFY(fur.frame.active);
  QCOMPARE(fur.frame.recoilOffsetX, 0.0);
  QCOMPARE(fur.frame.horizontalScale, 1.0);
  QVERIFY(std::abs(fur.frame.dorsalLift - 2.4) < 1e-9);
  QVERIFY(!ended.frame.active);
  QCOMPARE(ended.frame.recoilOffsetX, 0.0);
  QCOMPARE(ended.frame.horizontalScale, 1.0);
  QCOMPARE(ended.frame.dorsalLift, 0.0);

  for (double now = 100; now <= 400; now += 1) {
    const auto step =
        inputIndicatorCatSourceStartleAdvance(
            state, now, false
        );
    QVERIFY(std::isfinite(step.frame.progress));
    QVERIFY(std::isfinite(
        step.frame.recoilOffsetX
    ));
    QVERIFY(std::isfinite(
        step.frame.horizontalScale
    ));
    QVERIFY(std::isfinite(step.frame.dorsalLift));
    QVERIFY(step.frame.recoilOffsetX >= -0.45);
    QVERIFY(step.frame.recoilOffsetX <= 0);
    QVERIFY(step.frame.horizontalScale >= 0.92);
    QVERIFY(step.frame.horizontalScale <= 1);
    QVERIFY(step.frame.dorsalLift >= 0);
    QVERIFY(step.frame.dorsalLift <= 2.4);
  }

  const InputIndicatorPoint point{
      .x = 15.5,
      .y = 12.5,
  };
  const auto startPoint =
      inputIndicatorCatStartlePoint(
          point, start.frame, true
      );
  const auto endPoint =
      inputIndicatorCatStartlePoint(
          point, ended.frame, true
      );
  QCOMPARE(startPoint.x, point.x);
  QCOMPARE(startPoint.y, point.y);
  QCOMPARE(endPoint.x, point.x);
  QCOMPARE(endPoint.y, point.y);
}

void InputIndicatorGeometryTests::catSourceStartleCoalescesLatestSourceAtFiftyMilliseconds()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  InputIndicatorCatSourceStartleState state;
  state = inputIndicatorCatSourceStartleTrigger(
      state,
      Source::LatinABC,
      Source::Korean2Set,
      0,
      false
  );
  state = inputIndicatorCatSourceStartleTrigger(
      state,
      Source::Korean2Set,
      Source::LatinABC,
      20,
      false
  );
  state = inputIndicatorCatSourceStartleTrigger(
      state,
      Source::LatinABC,
      Source::Korean2Set,
      35,
      false
  );
  state = inputIndicatorCatSourceStartleTrigger(
      state,
      Source::Korean2Set,
      Source::Korean2Set,
      49,
      false
  );
  const auto throttled =
      inputIndicatorCatSourceStartleAdvance(
          state, 49, false
      );
  QCOMPARE(
      throttled.state.lastStartedMilliseconds,
      0.0
  );
  QVERIFY(throttled.state.pending);
  const auto replacement =
      inputIndicatorCatSourceStartleAdvance(
          state, 50, false
      );
  QCOMPARE(
      replacement.state.lastStartedMilliseconds,
      50.0
  );
  QCOMPARE(
      replacement.state.latestSource,
      Source::Korean2Set
  );
  QVERIFY(!replacement.state.pending);
  QVERIFY(replacement.frame.active);

  double previousStart =
      replacement.state.lastStartedMilliseconds;
  Source current = Source::Korean2Set;
  auto repeated =
      replacement.state;
  for (double now = 60; now <= 1000; now += 10) {
    const auto next =
        current == Source::LatinABC
            ? Source::Korean2Set
            : Source::LatinABC;
    repeated =
        inputIndicatorCatSourceStartleTrigger(
            repeated, current, next, now, false
        );
    current = next;
    const auto step =
        inputIndicatorCatSourceStartleAdvance(
            repeated, now, false
        );
    repeated = step.state;
    if (repeated.lastStartedMilliseconds !=
        previousStart) {
      QVERIFY(
          repeated.lastStartedMilliseconds -
              previousStart >=
          50
      );
      previousStart =
          repeated.lastStartedMilliseconds;
    }
  }
  QVERIFY(repeated.active);
  QCOMPARE(repeated.latestSource, current);
}

void InputIndicatorGeometryTests::catSourceStartleRejectsNonEventsAndReducedMotion()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  InputIndicatorCatSourceStartleState state;
  const auto initial =
      inputIndicatorCatSourceStartleTrigger(
          state,
          Source::Unknown,
          Source::LatinABC,
          10,
          false
      );
  QVERIFY(!initial.active);
  QVERIFY(!initial.pending);
  QVERIFY(!initial.hasStarted);

  const auto duplicate =
      inputIndicatorCatSourceStartleTrigger(
          initial,
          Source::LatinABC,
          Source::LatinABC,
          20,
          false
      );
  QVERIFY(!duplicate.active);
  QVERIFY(!duplicate.pending);
  const auto unknown =
      inputIndicatorCatSourceStartleTrigger(
          duplicate,
          Source::LatinABC,
          Source::Unknown,
          30,
          false
      );
  QVERIFY(!unknown.active);
  QVERIFY(!unknown.pending);

  auto active =
      inputIndicatorCatSourceStartleTrigger(
          unknown,
          Source::LatinABC,
          Source::Korean2Set,
          40,
          false
      );
  const auto reduced =
      inputIndicatorCatSourceStartleAdvance(
          active, 100, true
      );
  QVERIFY(!reduced.frame.active);
  QVERIFY(!reduced.state.active);
  QVERIFY(!reduced.state.pending);
  QCOMPARE(reduced.frame.recoilOffsetX, 0.0);
  QCOMPARE(reduced.frame.horizontalScale, 1.0);
  QCOMPARE(reduced.frame.dorsalLift, 0.0);

  const auto reducedTrigger =
      inputIndicatorCatSourceStartleTrigger(
          reduced.state,
          Source::Korean2Set,
          Source::LatinABC,
          100,
          true
      );
  QVERIFY(!reducedTrigger.active);
  QVERIFY(!reducedTrigger.pending);

  const auto invalid =
      inputIndicatorCatSourceStartleAdvance(
          active,
          std::numeric_limits<double>::quiet_NaN(),
          false
      );
  QVERIFY(std::isfinite(invalid.frame.progress));
  QVERIFY(std::isfinite(
      invalid.frame.recoilOffsetX
  ));
  QVERIFY(std::isfinite(
      invalid.frame.horizontalScale
  ));
  QVERIFY(std::isfinite(invalid.frame.dorsalLift));
}

void InputIndicatorGeometryTests::catSourceStartleWarpsOnlyApprovedDorsalRegion()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto state =
      inputIndicatorCatSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  const auto recoil =
      inputIndicatorCatSourceStartleAdvance(
          state, 70, false
      );
  const auto fur =
      inputIndicatorCatSourceStartleAdvance(
          state, 160, false
      );
  const auto rest =
      inputIndicatorCatSourceStartleAdvance(
          state, 300, false
      );

  const InputIndicatorPoint tail{
      .x = 5,
      .y = 7,
  };
  const InputIndicatorPoint dorsal{
      .x = 15.5,
      .y = 12.5,
  };
  const InputIndicatorPoint head{
      .x = 22,
      .y = 16,
  };
  const auto recoilPoint =
      inputIndicatorCatStartlePoint(
          dorsal, recoil.frame, true
      );
  QVERIFY(recoilPoint.x < dorsal.x);
  const auto furTail =
      inputIndicatorCatStartlePoint(
          tail, fur.frame, true
      );
  const auto furDorsal =
      inputIndicatorCatStartlePoint(
          dorsal, fur.frame, true
      );
  const auto furOnlyDorsal =
      inputIndicatorCatStartlePoint(
          dorsal, fur.frame, false
      );
  const auto furHead =
      inputIndicatorCatStartlePoint(
          head, fur.frame, true
      );
  QCOMPARE(furTail.y, tail.y);
  QVERIFY(furDorsal.y > dorsal.y);
  QCOMPARE(furOnlyDorsal.y, dorsal.y);
  QCOMPARE(furHead.y, head.y);

  for (const auto point :
       {tail, dorsal, head}) {
    const auto ended =
        inputIndicatorCatStartlePoint(
            point, rest.frame, true
        );
    QCOMPARE(ended.x, point.x);
    QCOMPARE(ended.y, point.y);
  }
}

void InputIndicatorGeometryTests::catPanelSourceChangesTriggerOnlyOneStartle()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorPoint fixedPointer{
      .x = fixedCursor.x,
      .y = fixedCursor.y,
  };
  MacInputIndicatorPanel panel(
      {},
      [fixedPointer] {
        return InputIndicatorTarget{
            .point = fixedPointer,
            .kind =
                InputIndicatorTargetKind::Pointer,
        };
      }
  );
  const MacInputIndicatorPanelOptions latin{
      .source = Source::LatinABC,
      .companion = InputIndicatorCompanion::Cat,
  };
  const MacInputIndicatorPanelOptions korean{
      .source = Source::Korean2Set,
      .companion = InputIndicatorCompanion::Cat,
  };
  QVERIFY(panel.show(latin));
  NSView *view = catPanelView();
  QVERIFY(view);
  QVERIFY([view respondsToSelector:
                    NSSelectorFromString(
                        @"catStartleActive"
                    )]);
  QVERIFY(![[view valueForKey:@"catStartleActive"]
      boolValue]);
  QVERIFY(![view respondsToSelector:
                     NSSelectorFromString(
                         @"haloPulseCount"
                     )]);

  QVERIFY(panel.show(latin));
  QVERIFY(![[view valueForKey:@"catStartleActive"]
      boolValue]);
  QVERIFY(panel.show(korean));
  QVERIFY([[view valueForKey:@"catStartleActive"]
      boolValue]);
  QCOMPARE(
      [[view valueForKey:@"catStartleLatestSource"]
          integerValue],
      static_cast<NSInteger>(Source::Korean2Set)
  );
  QCOMPARE(
      panel.move(latin.position)
          .frameIntervalMilliseconds,
      8
  );

  QTest::qWait(20);
  QVERIFY(panel.show(latin));
  QVERIFY([[view valueForKey:@"catStartlePending"]
      boolValue]);
  QTest::qWait(10);
  QVERIFY(panel.show(korean));
  QCOMPARE(
      [[view valueForKey:@"catStartleLatestSource"]
          integerValue],
      static_cast<NSInteger>(Source::Korean2Set)
  );
  QTest::qWait(30);
  const auto replaced =
      panel.move(latin.position);
  QCOMPARE(replaced.frameIntervalMilliseconds, 8);
  QVERIFY(![[view valueForKey:@"catStartlePending"]
      boolValue]);
  QVERIFY([[view valueForKey:@"catStartleActive"]
      boolValue]);

  MacInputIndicatorPanelOptions unknown =
      korean;
  unknown.source = Source::Unknown;
  QVERIFY(!panel.show(unknown));
  QVERIFY(![[view valueForKey:@"catStartleActive"]
      boolValue]);
  QVERIFY(![[view valueForKey:@"catStartlePending"]
      boolValue]);
}

void InputIndicatorGeometryTests::catStartleReplacesHaloAndResumesIdleTail()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorPoint fixedPointer{
      .x = fixedCursor.x,
      .y = fixedCursor.y,
  };
  MacInputIndicatorPanel panel(
      {},
      [fixedPointer] {
        return InputIndicatorTarget{
            .point = fixedPointer,
            .kind =
                InputIndicatorTargetKind::Pointer,
        };
      }
  );
  const MacInputIndicatorPanelOptions latin{
      .source = Source::LatinABC,
      .companion = InputIndicatorCompanion::Cat,
  };
  const MacInputIndicatorPanelOptions korean{
      .source = Source::Korean2Set,
      .companion = InputIndicatorCompanion::Cat,
  };
  QVERIFY(panel.show(latin));
  NSView *view = catPanelView();
  QVERIFY(view);
  QTest::qWait(500);
  const auto before =
      panel.move(latin.position);
  QCOMPARE(before.frameIntervalMilliseconds, 75);
  const auto beforeMotion =
      [[view valueForKey:@"catIdleTailMotion"]
          integerValue];
  const double beforeProgress =
      [[view valueForKey:@"catIdleTailProgress"]
          doubleValue];
  QVERIFY([[view valueForKey:@"catIdleTailActive"]
      boolValue]);

  QVERIFY(panel.show(korean));
  QVERIFY([[view valueForKey:@"catStartleActive"]
      boolValue]);
  QVERIFY(![[view valueForKey:@"catIdleTailActive"]
      boolValue]);
  [view displayIfNeeded];
  NSBitmapImageRep *startleBefore =
      [view bitmapImageRepForCachingDisplayInRect:
                view.bounds];
  QVERIFY(startleBefore);
  [view cacheDisplayInRect:view.bounds
         toBitmapImageRep:startleBefore];
  QTest::qWait(160);
  const auto peak =
      panel.move(korean.position);
  QCOMPARE(peak.frameIntervalMilliseconds, 8);
  [view displayIfNeeded];
  NSBitmapImageRep *startlePeak =
      [view bitmapImageRepForCachingDisplayInRect:
                view.bounds];
  QVERIFY(startlePeak);
  [view cacheDisplayInRect:view.bounds
         toBitmapImageRep:startlePeak];
  const double startleScale =
      static_cast<double>(startlePeak.pixelsWide) /
      view.bounds.size.width;
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  const NSInteger dorsalLeft =
      static_cast<NSInteger>(
          std::floor(
              (
                  style.canvasOffsetX +
                  macInputIndicatorDisplayPoint(
                      {.x = 13, .y = 0}
                  ).x
              ) *
              startleScale
          )
      );
  const NSInteger dorsalRight =
      static_cast<NSInteger>(
          std::ceil(
              (
                  style.canvasOffsetX +
                  macInputIndicatorDisplayPoint(
                      {.x = 18.5, .y = 0}
                  ).x
              ) *
              startleScale
          )
      );
  const NSInteger dorsalBottom =
      static_cast<NSInteger>(
          std::floor(
              (
                  style.canvasOffsetY +
                  macInputIndicatorDisplayPoint(
                      {.x = 0, .y = 9.5}
                  ).y
              ) *
              startleScale
          )
      );
  const NSInteger dorsalTop =
      static_cast<NSInteger>(
          std::ceil(
              (
                  style.canvasOffsetY +
                  macInputIndicatorDisplayPoint(
                      {.x = 0, .y = 17}
                  ).y
              ) *
              startleScale
          )
      );
  int dorsalPixels = 0;
  for (NSInteger y = dorsalBottom;
       y < dorsalTop;
       ++y) {
    for (NSInteger x = dorsalLeft;
         x < dorsalRight;
         ++x) {
      const double beforeAlpha =
          [startleBefore colorAtX:x y:y]
              .alphaComponent;
      const double peakAlpha =
          [startlePeak colorAtX:x y:y]
              .alphaComponent;
      if (std::abs(beforeAlpha - peakAlpha) >
          0.02) {
        ++dorsalPixels;
      }
    }
  }
  qInfo() << "cat-startle-dorsal-pixels"
          << dorsalPixels
          << "scale" << startleScale;
  QVERIFY(dorsalPixels > 0);
  QTest::qWait(160);
  const auto after =
      panel.move(korean.position);
  QCOMPARE(after.frameIntervalMilliseconds, 75);
  QVERIFY(![[view valueForKey:@"catStartleActive"]
      boolValue]);
  QVERIFY([[view valueForKey:@"catIdleTailActive"]
      boolValue]);
  QCOMPARE(
      [[view valueForKey:@"catIdleTailMotion"]
          integerValue],
      beforeMotion
  );
  const double afterProgress =
      [[view valueForKey:@"catIdleTailProgress"]
          doubleValue];
  QVERIFY(std::abs(
      afterProgress - beforeProgress
  ) < 0.08);
}

void InputIndicatorGeometryTests::dogAndTurtleStartleOnSourceChange()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorPoint fixedPointer{
      .x = fixedCursor.x,
      .y = fixedCursor.y,
  };
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    MacInputIndicatorPanel panel(
        {},
        [fixedPointer] {
          return InputIndicatorTarget{
              .point = fixedPointer,
              .kind =
                  InputIndicatorTargetKind::Pointer,
          };
        }
    );
    MacInputIndicatorPanelOptions options{
        .source = Source::LatinABC,
        .companion = companion,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    options.source = Source::Korean2Set;
    QVERIFY(panel.show(options));
    QVERIFY([[view valueForKey:@"catStartleActive"]
        boolValue]);
    QCOMPARE(
        [[view valueForKey:@"catStartleLatestSource"]
            integerValue],
        static_cast<NSInteger>(Source::Korean2Set)
    );
    QVERIFY(
        [[view valueForKey:@"transitionProgress"]
            doubleValue] < 1.0
    );
    const auto result =
        panel.move(options.position);
    QVERIFY(result.animating);
    QCOMPARE(result.frameIntervalMilliseconds, 8);
    panel.hide();
  }
}

void InputIndicatorGeometryTests::dogAndTurtleIdlePatternsRepaint()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    MacInputIndicatorPanel panel(
        {},
        [fixedCursor] {
          return InputIndicatorTarget{
              .point = {
                  .x = fixedCursor.x,
                  .y = fixedCursor.y,
              },
              .kind =
                  InputIndicatorTargetKind::Pointer,
          };
        }
    );
    const MacInputIndicatorPanelOptions options{
        .source = Source::LatinABC,
        .companion = companion,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    [view displayIfNeeded];
    NSBitmapImageRep *before =
        [view bitmapImageRepForCachingDisplayInRect:
                  view.bounds];
    QVERIFY(before);
    [view cacheDisplayInRect:view.bounds
           toBitmapImageRep:before];
    const double beforeProgress =
        [[view valueForKey:@"catIdleTailProgress"]
            doubleValue];
    QVERIFY([[view valueForKey:@"catIdleTailActive"]
        boolValue]);

    QTest::qWait(650);
    const auto result =
        panel.move(options.position);
    QVERIFY(result.visible);
    QVERIFY(result.animating);
    QCOMPARE(result.frameIntervalMilliseconds, 75);
    [view displayIfNeeded];
    NSBitmapImageRep *after =
        [view bitmapImageRepForCachingDisplayInRect:
                  view.bounds];
    QVERIFY(after);
    [view cacheDisplayInRect:view.bounds
           toBitmapImageRep:after];
    const int changed =
        bitmapChangedPixels(before, after);
    qInfo() << "dog-turtle-panel-idle"
            << static_cast<int>(companion)
            << beforeProgress
            << [[view valueForKey:
                  @"catIdleTailProgress"]
                   doubleValue]
            << changed;
    QVERIFY(changed > 0);
    QVERIFY(
        [[view valueForKey:@"catIdleTailProgress"]
            doubleValue] > beforeProgress
    );
    panel.hide();
  }
}

void InputIndicatorGeometryTests::dogAndTurtleReactionCoalescesAndEnds()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    MacInputIndicatorPanel panel(
        {},
        [fixedCursor] {
          return InputIndicatorTarget{
              .point = {
                  .x = fixedCursor.x,
                  .y = fixedCursor.y,
              },
              .kind =
                  InputIndicatorTargetKind::Pointer,
          };
        }
    );
    MacInputIndicatorPanelOptions options{
        .source = Source::LatinABC,
        .companion = companion,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    options.source = Source::Korean2Set;
    QVERIFY(panel.show(options));
    QTest::qWait(20);
    options.source = Source::LatinABC;
    QVERIFY(panel.show(options));
    QTest::qWait(140);
    const auto peak =
        panel.move(options.position);
    QVERIFY(peak.visible);
    QCOMPARE(peak.frameIntervalMilliseconds, 8);
    QVERIFY([[view valueForKey:@"catStartleActive"]
        boolValue]);
    QCOMPARE(
        [[view valueForKey:@"catStartleLatestSource"]
            integerValue],
        static_cast<NSInteger>(Source::LatinABC)
    );
    QVERIFY(
        [[view valueForKey:@"catStartleHaloAlpha"]
            doubleValue] > 0
    );
    QVERIFY(
        [[view valueForKey:@"catStartlePending"]
            boolValue] == NO
    );
    const double peakScale =
        [[view valueForKey:@"catStartleHaloScale"]
            doubleValue];
    QVERIFY(peakScale <= 1.12);
    QVERIFY(peakScale > 1.0);

    QTest::qWait(210);
    const auto ended =
        panel.move(options.position);
    QVERIFY(ended.visible);
    QVERIFY(![[view valueForKey:@"catStartleActive"]
        boolValue]);
    QVERIFY(![[view valueForKey:@"catStartlePending"]
        boolValue]);
    QCOMPARE(
        [[view valueForKey:@"catStartleHaloAlpha"]
            doubleValue],
        0.0
    );
    QVERIFY([[view valueForKey:@"catIdleTailActive"]
        boolValue]);
    QCOMPARE(ended.frameIntervalMilliseconds, 75);
    panel.hide();
  }
}

void InputIndicatorGeometryTests::allCompanionPanelsHonorHeadingAndGlyphCoverage()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto idle =
      InputIndicatorCompanionIdleFrame{};
  const auto reaction =
      InputIndicatorCompanionStartleFrame{};
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  constexpr int paddingPoints = 4;
  constexpr double heading =
      M_PI / 4;
  for (const auto companion : companions) {
    for (const int scale : {1, 2}) {
      NSBitmapImageRep *straightBody =
          companionBodyBitmap(
              companion,
              InputIndicatorMotionState::Idle,
              0,
              idle,
              reaction,
              true,
              scale,
              paddingPoints,
              true,
              0
          );
      NSBitmapImageRep *diagonalBody =
          companionBodyBitmap(
              companion,
              InputIndicatorMotionState::Idle,
              0,
              idle,
              reaction,
              true,
              scale,
              paddingPoints,
              true,
              heading
          );
      NSBitmapImageRep *straightGlyph =
          companionGlyphBitmap(
              companion,
              InputIndicatorMotionState::Idle,
              Source::LatinABC,
              true,
              scale,
              paddingPoints,
              0
          );
      NSBitmapImageRep *diagonalGlyph =
          companionGlyphBitmap(
              companion,
              InputIndicatorMotionState::Idle,
              Source::LatinABC,
              true,
              scale,
              paddingPoints,
              heading
          );
      QVERIFY(straightBody);
      QVERIFY(diagonalBody);
      QVERIFY(straightGlyph);
      QVERIFY(diagonalGlyph);
      QVERIFY(
          bitmapChangedPixels(
              straightBody, diagonalBody
          ) > 0
      );
      const auto straightBounds =
          bitmapAlphaBounds(straightGlyph);
      const auto diagonalBounds =
          bitmapAlphaBounds(diagonalGlyph);
      QCOMPARE(
          straightBounds.width,
          diagonalBounds.width
      );
      QCOMPARE(
          straightBounds.height,
          diagonalBounds.height
      );
      for (NSInteger y = 0;
           y < diagonalGlyph.pixelsHigh;
           ++y) {
        for (NSInteger x = 0;
             x < diagonalGlyph.pixelsWide;
             ++x) {
          const double glyphAlpha =
              [diagonalGlyph colorAtX:x y:y]
                  .alphaComponent;
          const double bodyAlpha =
              [diagonalBody colorAtX:x y:y]
                  .alphaComponent;
          if (glyphAlpha > 0.02) {
            QVERIFY(bodyAlpha > 0.02);
          }
        }
      }
      [straightBody release];
      [diagonalBody release];
      [straightGlyph release];
      [diagonalGlyph release];
    }

    const auto fixedCursor =
        fixedPanelTestPointer();
    InputIndicatorTarget target{
        .point = {
            .x = fixedCursor.x,
            .y = fixedCursor.y,
        },
        .kind =
            InputIndicatorTargetKind::Pointer,
    };
    MacInputIndicatorPanel panel(
        {},
        [&target] {
          return target;
        }
    );
    const MacInputIndicatorPanelOptions options{
        .source = Source::LatinABC,
        .companion = companion,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    target.point.x += 300;
    target.point.y += 200;
    QTest::qWait(100);
    const auto step =
        panel.move(options.position);
    QVERIFY(step.visible);
    const double renderedHeading =
        [[view valueForKey:
              @"catBodyHeadingRadians"]
            doubleValue];
    QVERIFY(std::isfinite(renderedHeading));
    QVERIFY(std::abs(renderedHeading) > 0.01);
    panel.hide();
  }
}

void InputIndicatorGeometryTests::catPursuitBoundsLargeJumpAndNeverTeleports()
{
  const InputIndicatorTarget target{
      .point = {.x = 1000, .y = 0},
      .kind = InputIndicatorTargetKind::Pointer,
  };
  const auto initialized =
      inputIndicatorCatPursuitAdvance(
          {},
          target,
          125000,
          8,
          true,
          false
      );
  QVERIFY(initialized);
  QVERIFY(initialized->state.initialized);
  QCOMPARE(initialized->state.position.x, 1000.0);
  QCOMPARE(initialized->state.position.y, 0.0);
  QVERIFY(initialized->reached);

  InputIndicatorCatPursuitState state{
      .position = {.x = 0, .y = 0},
      .headingRadians = 0,
      .initialized = true,
  };
  std::vector<double> positions;
  positions.push_back(state.position.x);
  int steps = 0;
  while (true) {
    const auto step =
        inputIndicatorCatPursuitAdvance(
            state,
            target,
            125000,
            8,
            false,
            false
        );
    QVERIFY(step);
    QVERIFY(step->state.position.x >= state.position.x);
    QVERIFY(step->state.position.x <= target.point.x);
    QVERIFY(step->distanceMoved <= 5.7600001);
    QVERIFY(
        step->state.motionState ==
        InputIndicatorMotionState::Run
    );
    positions.push_back(step->state.position.x);
    state = step->state;
    ++steps;
    if (step->reached) {
      break;
    }
    QVERIFY(steps < 400);
  }
  QCOMPARE(state.position.x, target.point.x);
  QCOMPARE(state.position.y, target.point.y);
  QVERIFY(steps > 100);
  QVERIFY(positions.size() > 100);
}

void InputIndicatorGeometryTests::companionPursuitScalesCapsWithoutChangingGates()
{
  struct SpeedCase
  {
    double multiplier;
    double walkDistance;
    double runDistance;
  };
  const SpeedCase cases[] = {
      {.multiplier = 0.5,
       .walkDistance = 0.48,
       .runDistance = 2.88},
      {.multiplier = 1.0,
       .walkDistance = 0.96,
       .runDistance = 5.76},
      {.multiplier = 2.0,
       .walkDistance = 1.92,
       .runDistance = 11.52},
  };
  for (const auto speed : cases) {
    const InputIndicatorCatPursuitState origin{
        .initialized = true,
    };
    const InputIndicatorTarget walkTarget{
        .point = {.x = 20, .y = 0},
        .kind = InputIndicatorTargetKind::Pointer,
    };
    const auto walk =
        inputIndicatorCompanionPursuitAdvance(
            origin,
            walkTarget,
            0,
            8,
            false,
            false,
            speed.multiplier
        );
    QVERIFY(walk);
    QVERIFY(std::abs(
        walk->distanceMoved - speed.walkDistance
    ) < 1e-9);
    QCOMPARE(
        walk->state.motionState,
        InputIndicatorMotionState::Walk
    );
    const InputIndicatorTarget runTarget{
        .point = {.x = 100, .y = 0},
        .kind = InputIndicatorTargetKind::Pointer,
    };
    const auto run =
        inputIndicatorCompanionPursuitAdvance(
            origin,
            runTarget,
            0,
            8,
            false,
            false,
            speed.multiplier
        );
    QVERIFY(run);
    QVERIFY(std::abs(
        run->distanceMoved - speed.runDistance
    ) < 1e-9);
    QCOMPARE(
        run->state.motionState,
        InputIndicatorMotionState::Run
    );

    for (const auto target :
         {walkTarget, runTarget}) {
      InputIndicatorCatPursuitState state = origin;
      int steps = 0;
      while (true) {
        const auto step =
            inputIndicatorCompanionPursuitAdvance(
                state,
                target,
                0,
                8,
                false,
                false,
                speed.multiplier
            );
        QVERIFY(step);
        QVERIFY(
            step->state.position.x >=
            state.position.x
        );
        QVERIFY(
            step->state.position.x <=
            target.point.x
        );
        QVERIFY(
            step->state.position.y ==
            state.position.y
        );
        state = step->state;
        if (step->reached) {
          break;
        }
        QVERIFY(++steps < 400);
      }
      QCOMPARE(state.position.x, target.point.x);
      QCOMPARE(state.position.y, target.point.y);
    }
  }

  const InputIndicatorCatPursuitState origin{
      .initialized = true,
  };
  const InputIndicatorTarget fortySeven{
      .point = {.x = 47, .y = 0},
      .kind = InputIndicatorTargetKind::Pointer,
  };
  const auto belowTargetSpeed =
      inputIndicatorCompanionPursuitAdvance(
          origin, fortySeven, 259.999,
          8, false, false, 2.0
      );
  QVERIFY(belowTargetSpeed);
  QCOMPARE(
      belowTargetSpeed->state.motionState,
      InputIndicatorMotionState::Walk
  );
  const auto atTargetSpeed =
      inputIndicatorCompanionPursuitAdvance(
          origin, fortySeven, 260,
          8, false, false, 2.0
      );
  QVERIFY(atTargetSpeed);
  QCOMPARE(
      atTargetSpeed->state.motionState,
      InputIndicatorMotionState::Run
  );
  const auto atDistance =
      inputIndicatorCompanionPursuitAdvance(
          origin,
          {
              .point = {.x = 48, .y = 0},
              .kind = InputIndicatorTargetKind::Pointer,
          },
          0,
          8,
          false,
          false,
          2.0
      );
  QVERIFY(atDistance);
  QCOMPARE(
      atDistance->state.motionState,
      InputIndicatorMotionState::Run
  );
  InputIndicatorCatPursuitState running{
      .motionState = InputIndicatorMotionState::Run,
      .initialized = true,
  };
  const auto hysteresisHolds =
      inputIndicatorCompanionPursuitAdvance(
          running,
          {
              .point = {.x = 13, .y = 0},
              .kind = InputIndicatorTargetKind::Pointer,
          },
          0,
          8,
          false,
          false,
          2.0
      );
  QVERIFY(hysteresisHolds);
  QCOMPARE(
      hysteresisHolds->state.motionState,
      InputIndicatorMotionState::Run
  );
  const auto hysteresisReleases =
      inputIndicatorCompanionPursuitAdvance(
          running,
          {
              .point = {.x = 12, .y = 0},
              .kind = InputIndicatorTargetKind::Pointer,
          },
          0,
          8,
          false,
          false,
          2.0
      );
  QVERIFY(hysteresisReleases);
  QCOMPARE(
      hysteresisReleases->state.motionState,
      InputIndicatorMotionState::Walk
  );

  const auto reduced =
      inputIndicatorCompanionPursuitAdvance(
          origin,
          {
              .point = {.x = 100, .y = 200},
              .kind = InputIndicatorTargetKind::Caret,
          },
          999,
          8,
          false,
          true,
          0.5
      );
  QVERIFY(reduced);
  QCOMPARE(reduced->state.position.x, 100.0);
  QCOMPARE(reduced->state.position.y, 200.0);
  QCOMPARE(
      reduced->state.motionState,
      InputIndicatorMotionState::Idle
  );
  QVERIFY(reduced->reached);

  const double invalidFactors[] = {
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity(),
      0.0,
      0.499,
      2.001,
  };
  for (const double factor : invalidFactors) {
    QVERIFY(!inputIndicatorCompanionPursuitAdvance(
        origin,
        {
            .point = {.x = 20, .y = 0},
            .kind =
                InputIndicatorTargetKind::Pointer,
        },
        0,
        8,
        false,
        false,
        factor
    ));
  }
}

void InputIndicatorGeometryTests::companionModelUsesFiveSharedIdlePatterns()
{
  auto state =
      inputIndicatorCompanionIdleMotionInitialState(
          0x12345678
      );
  std::set<int> patterns;
  std::optional<int> previous;
  double now = 0;
  for (int advancement = 0;
       advancement < 40;
       ++advancement) {
    const auto step =
        inputIndicatorCompanionIdleMotionAdvance(
            state,
            InputIndicatorMotionState::Idle,
            now,
            false
        );
    QVERIFY(step.frame.active);
    QVERIFY(step.frame.progress >= 0);
    QVERIFY(step.frame.progress < 1);
    const int pattern =
        static_cast<int>(step.frame.motion);
    patterns.insert(pattern);
    if (previous) {
      QVERIFY(pattern != *previous);
    }
    previous = pattern;
    state = step.state;
    now +=
        inputIndicatorCompanionIdleMotionDuration(
            step.frame.motion
        );
  }
  QCOMPARE(patterns.size(), size_t{5});

  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    (void)companion;
    const auto walking =
        inputIndicatorCompanionIdleMotionAdvance(
            state,
            InputIndicatorMotionState::Walk,
            now,
            false
        );
    QVERIFY(!walking.frame.active);
    QVERIFY(!walking.state.running);
    const auto running =
        inputIndicatorCompanionIdleMotionAdvance(
            state,
            InputIndicatorMotionState::Run,
            now,
            false
        );
    QVERIFY(!running.frame.active);
    QVERIFY(!running.state.running);
    const auto reduced =
        inputIndicatorCompanionIdleMotionAdvance(
            state,
            InputIndicatorMotionState::Idle,
            now,
            true
        );
    QVERIFY(!reduced.frame.active);
    QVERIFY(!reduced.state.running);
  }
}

void InputIndicatorGeometryTests::companionMotionPoseDistinguishesDogAndTurtleGaits()
{
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    const auto idle =
        inputIndicatorCompanionMotionPose(
            companion,
            InputIndicatorMotionState::Idle,
            0.25,
            false
        );
    const auto walkQuarter =
        inputIndicatorCompanionMotionPose(
            companion,
            InputIndicatorMotionState::Walk,
            0.25,
            false
        );
    const auto walkThreeQuarters =
        inputIndicatorCompanionMotionPose(
            companion,
            InputIndicatorMotionState::Walk,
            0.75,
            false
        );
    const auto run =
        inputIndicatorCompanionMotionPose(
            companion,
            InputIndicatorMotionState::Run,
            0.25,
            false
        );
    for (const double value :
         {
             idle.bodyBob,
             idle.bodyStretch,
             idle.tailPhase,
             idle.limbStride,
             idle.bodyLead,
             walkQuarter.bodyBob,
             walkQuarter.bodyStretch,
             walkQuarter.tailPhase,
             walkQuarter.limbStride,
             walkQuarter.bodyLead,
             run.bodyBob,
             run.bodyStretch,
             run.tailPhase,
             run.limbStride,
             run.bodyLead,
         }) {
      QVERIFY(std::isfinite(value));
    }
    QVERIFY(
        walkQuarter.limbStride *
            walkThreeQuarters.limbStride <
        0
    );
    QVERIFY(
        std::abs(run.limbStride) >
        std::abs(walkQuarter.limbStride)
    );
    if (companion ==
        InputIndicatorCompanion::Dog) {
      QVERIFY(std::abs(walkQuarter.bodyBob) > 0.2);
      QVERIFY(run.bodyLead > walkQuarter.bodyLead);
    }
    if (companion ==
        InputIndicatorCompanion::Turtle) {
      QVERIFY(std::abs(walkQuarter.bodyBob) > 0.1);
      QVERIFY(run.bodyLead > walkQuarter.bodyLead);
    }
    const auto reduced =
        inputIndicatorCompanionMotionPose(
            companion,
            InputIndicatorMotionState::Run,
            0.25,
            true
        );
    QCOMPARE(reduced.bodyBob, 0.0);
    QCOMPARE(reduced.bodyStretch, 0.0);
    QCOMPARE(reduced.tailPhase, 0.0);
    QCOMPARE(reduced.limbStride, 0.0);
    QCOMPARE(reduced.bodyLead, 0.0);
  }
}

void InputIndicatorGeometryTests::dogAndTurtleReactionPointsReturnToNeutral()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  auto reactionState =
      inputIndicatorCompanionSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  const auto peak =
      inputIndicatorCompanionSourceStartleAdvance(
          reactionState, 160, false
      );
  const auto ended =
      inputIndicatorCompanionSourceStartleAdvance(
          reactionState, 300, false
      );
  reactionState = peak.state;
  QVERIFY(peak.frame.active);
  QVERIFY(!ended.frame.active);

  const InputIndicatorCompanionIdleFrame idle;
  const auto dogEar =
      InputIndicatorPoint{.x = 20, .y = 19};
  const auto dogTail =
      InputIndicatorPoint{.x = 3, .y = 12};
  const auto turtleHead =
      InputIndicatorPoint{.x = 27, .y = 13};
  const auto turtleShell =
      InputIndicatorPoint{.x = 12, .y = 17};
  const struct
  {
    InputIndicatorCompanion companion;
    InputIndicatorPoint point;
  } cases[] = {
      {InputIndicatorCompanion::Dog, dogEar},
      {InputIndicatorCompanion::Dog, dogTail},
      {InputIndicatorCompanion::Turtle, turtleHead},
      {InputIndicatorCompanion::Turtle, turtleShell},
  };
  for (const auto &testCase : cases) {
    const auto atPeak =
        inputIndicatorCompanionArtworkPoint(
            testCase.companion,
            testCase.point,
            InputIndicatorMotionState::Idle,
            0,
            idle,
            peak.frame
        );
    QVERIFY(
        std::abs(atPeak.x - testCase.point.x) >
            1e-9 ||
        std::abs(atPeak.y - testCase.point.y) >
            1e-9
    );
    const auto atEnd =
        inputIndicatorCompanionArtworkPoint(
            testCase.companion,
            testCase.point,
            InputIndicatorMotionState::Idle,
            0,
            idle,
            ended.frame
        );
    QCOMPARE(atEnd.x, testCase.point.x);
    QCOMPARE(atEnd.y, testCase.point.y);
  }
  QVERIFY(
      peak.frame.horizontalScale >= 0.92
  );
  QVERIFY(peak.frame.haloAlpha > 0);
}

void InputIndicatorGeometryTests::companionHeadingTransformsBodyButNotGlyphBounds()
{
  constexpr double heading =
      3.14159265358979323846 / 4.0;
  const InputIndicatorPoint bodyPoint{
      .x = 17,
      .y = 11,
  };
  InputIndicatorPoint expected{};
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    (void)companion;
    const auto transformed =
        inputIndicatorCompanionBodyPoint(
            bodyPoint,
            true,
            1,
            heading
        );
    if (companion ==
        InputIndicatorCompanion::Cat) {
      expected = transformed;
    } else {
      QCOMPARE(transformed.x, expected.x);
      QCOMPARE(transformed.y, expected.y);
    }
    const InputIndicatorRect glyph{
        .x = transformed.x - 4,
        .y = transformed.y - 3.5,
        .width = 8,
        .height = 7,
    };
    QCOMPARE(glyph.width, 8.0);
    QCOMPARE(glyph.height, 7.0);
  }
}

void InputIndicatorGeometryTests::companionArtworkUsesContinuousSpeciesSilhouettes()
{
  const auto dog =
      macInputIndicatorDogArtwork();
  const auto turtle =
      macInputIndicatorTurtleArtwork();
  QVERIFY(dog.silhouette.size() >= 40);
  QVERIFY(turtle.silhouette.size() >= 32);
  const InputIndicatorRect logicalBounds{
      .x = 1,
      .y = 3,
      .width = 28,
      .height = 20,
  };
  const MacInputIndicatorCompanionArtwork artworks[] = {
      dog, turtle
  };
  for (const auto &artwork : artworks) {
    QVERIFY(
        artwork.silhouetteBounds.x >=
        logicalBounds.x
    );
    QVERIFY(
        artwork.silhouetteBounds.y >=
        logicalBounds.y
    );
    QVERIFY(
        artwork.silhouetteBounds.x +
            artwork.silhouetteBounds.width <=
        logicalBounds.x + logicalBounds.width
    );
    QVERIFY(
        artwork.silhouetteBounds.y +
            artwork.silhouetteBounds.height <=
        logicalBounds.y + logicalBounds.height
    );
    QVERIFY(
        artwork.innerLanguageBounds.x >=
        artwork.silhouetteBounds.x
    );
    QVERIFY(
        artwork.innerLanguageBounds.y >=
        artwork.silhouetteBounds.y
    );
    QVERIFY(
        artwork.innerLanguageBounds.x +
            artwork.innerLanguageBounds.width <=
        artwork.silhouetteBounds.x +
            artwork.silhouetteBounds.width
    );
    QVERIFY(
        artwork.innerLanguageBounds.y +
            artwork.innerLanguageBounds.height <=
        artwork.silhouetteBounds.y +
            artwork.silhouetteBounds.height
    );
    QVERIFY(!artwork.silhouette.empty());
    QCOMPARE(
        static_cast<int>(
            artwork.silhouette.front().type
        ),
        static_cast<int>(
            MacInputIndicatorCatPathCommandType::Move
        )
    );
    QCOMPARE(
        static_cast<int>(
            artwork.silhouette.back().type
        ),
        static_cast<int>(
            MacInputIndicatorCatPathCommandType::Close
        )
    );
  }
}

void InputIndicatorGeometryTests::dogAndTurtleIdleAndGaitPixelsChangeAtOneAndTwoX()
{
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  constexpr int paddingPoints = 4;
  const InputIndicatorCompanionIdleFrame restIdle;
  const InputIndicatorCompanionStartleFrame restStartle;
  const InputIndicatorCompanionIdleMotion motions[] = {
      InputIndicatorCompanionIdleMotion::RelaxedSway,
      InputIndicatorCompanionIdleMotion::TipFlick,
      InputIndicatorCompanionIdleMotion::CurlUnfurl,
      InputIndicatorCompanionIdleMotion::LazySweep,
      InputIndicatorCompanionIdleMotion::QuestionLift,
  };
  for (const auto companion : companions) {
    for (const int scale : {1, 2}) {
      NSBitmapImageRep *rest =
          companionBodyBitmap(
              companion,
              InputIndicatorMotionState::Idle,
              0,
              restIdle,
              restStartle,
              true,
              scale,
              paddingPoints,
              true
          );
      QVERIFY(rest);
      int minimumIdlePixels =
          std::numeric_limits<int>::max();
      for (const auto motion : motions) {
        const InputIndicatorCompanionIdleFrame frame{
            .motion = motion,
            .progress = 0.38,
            .active = true,
        };
        NSBitmapImageRep *animated =
            companionBodyBitmap(
                companion,
                InputIndicatorMotionState::Idle,
                0,
                frame,
                restStartle,
                true,
                scale,
                paddingPoints,
                true
            );
        QVERIFY(animated);
        const int changed =
            bitmapChangedPixels(rest, animated);
        minimumIdlePixels =
            std::min(
                minimumIdlePixels, changed
            );
        qInfo() << "companion-idle-pixels"
                << static_cast<int>(companion)
                << static_cast<int>(motion)
                << scale
                << changed;
        QVERIFY(changed > 0);
        [animated release];
      }

      const struct
      {
        InputIndicatorMotionState state;
        double firstPhase;
        double secondPhase;
      } gaits[] = {
          {
              .state =
                  InputIndicatorMotionState::Walk,
              .firstPhase = 0.1,
              .secondPhase = 0.6,
          },
          {
              .state =
                  InputIndicatorMotionState::Run,
              .firstPhase = 0.1,
              .secondPhase = 0.6,
          },
      };
      for (const auto gait : gaits) {
        NSBitmapImageRep *first =
            companionBodyBitmap(
                companion,
                gait.state,
                gait.firstPhase,
                restIdle,
                restStartle,
                true,
                scale,
                paddingPoints,
                true
            );
        NSBitmapImageRep *second =
            companionBodyBitmap(
                companion,
                gait.state,
                gait.secondPhase,
                restIdle,
                restStartle,
                true,
                scale,
                paddingPoints,
                true
            );
        QVERIFY(first);
        QVERIFY(second);
        const int changed =
            bitmapChangedPixels(first, second);
        qInfo() << "companion-gait-pixels"
                << static_cast<int>(companion)
                << static_cast<int>(gait.state)
                << scale
                << changed;
        QVERIFY(changed > 0);
        [first release];
        [second release];
      }
      QVERIFY(minimumIdlePixels > 0);
      [rest release];
    }
  }
}

void InputIndicatorGeometryTests::dogAndTurtleReactionAndHaloPixelsUseOneLayer()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  const Source sources[] = {
      Source::LatinABC,
      Source::Korean2Set,
  };
  constexpr int paddingPoints = 4;
  const InputIndicatorCompanionIdleFrame idle;
  const InputIndicatorCompanionStartleFrame rest;
  const auto trigger =
      inputIndicatorCompanionSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  const auto peak =
      inputIndicatorCompanionSourceStartleAdvance(
          trigger, 160, false
      );
  const auto ended =
      inputIndicatorCompanionSourceStartleAdvance(
          trigger, 300, false
      );
  int totalHaloPixels = 0;
  int totalColorPixels = 0;
  int outsidePixels = 0;

  for (const auto companion : companions) {
    for (const auto source : sources) {
      const auto expectedColor =
          macInputIndicatorGlyphColor(
              source,
              InputIndicatorTone::Automatic
          );
      const double expectedSum =
          expectedColor.red +
          expectedColor.green +
          expectedColor.blue;
      for (const int scale : {1, 2}) {
        const auto style =
            macInputIndicatorPanelStyle(
                companion
            );
        const NSInteger canvasLeft =
            paddingPoints * scale;
        const NSInteger canvasBottom =
            paddingPoints * scale;
        const NSInteger canvasRight =
            canvasLeft +
            static_cast<NSInteger>(
                style.canvasWidth
            ) *
                scale;
        const NSInteger canvasTop =
            canvasBottom +
            static_cast<NSInteger>(
                style.canvasHeight
            ) *
                scale;
        NSBitmapImageRep *baseline =
            companionBodyBitmap(
                companion,
                InputIndicatorMotionState::Idle,
                0,
                idle,
                rest,
                true,
                scale,
                paddingPoints,
                true
            );
        NSBitmapImageRep *body =
            companionBodyBitmap(
                companion,
                InputIndicatorMotionState::Idle,
                0,
                idle,
                peak.frame,
                true,
                scale,
                paddingPoints,
                true
            );
        NSBitmapImageRep *paint =
            companionBodyBitmap(
                companion,
                InputIndicatorMotionState::Idle,
                0,
                idle,
                peak.frame,
                true,
                scale,
                paddingPoints,
                true,
                0,
                1,
                true,
                source
            );
        NSBitmapImageRep *endBody =
            companionBodyBitmap(
                companion,
                InputIndicatorMotionState::Idle,
                0,
                idle,
                ended.frame,
                true,
                scale,
                paddingPoints,
                true
            );
        NSBitmapImageRep *endPaint =
            companionBodyBitmap(
                companion,
                InputIndicatorMotionState::Idle,
                0,
                idle,
                ended.frame,
                true,
                scale,
                paddingPoints,
                true,
                0,
                1,
                true,
                source
            );
        QVERIFY(baseline);
        QVERIFY(body);
        QVERIFY(paint);
        QVERIFY(endBody);
        QVERIFY(endPaint);
        QVERIFY(
            bitmapChangedPixels(baseline, body) > 0
        );
        QCOMPARE(
            bitmapChangedPixels(
                baseline, endBody
            ),
            0
        );
        QCOMPARE(
            bitmapChangedPixels(
                endBody, endPaint
            ),
            0
        );
        int haloPixels = 0;
        int colorPixels = 0;
        for (NSInteger y = 0;
             y < paint.pixelsHigh;
             ++y) {
          for (NSInteger x = 0;
               x < paint.pixelsWide;
               ++x) {
            const NSColor *color =
                [paint colorAtX:x y:y];
            const double alpha =
                color.alphaComponent;
            const double bodyAlpha =
                [body colorAtX:x y:y]
                    .alphaComponent;
            if (alpha > 0.02 &&
                (x < canvasLeft ||
                 x >= canvasRight ||
                 y < canvasBottom ||
                 y >= canvasTop)) {
              ++outsidePixels;
            }
            if (alpha <= 0.12 ||
                bodyAlpha > 0) {
              continue;
            }
            ++haloPixels;
            const double sum =
                color.redComponent +
                color.greenComponent +
                color.blueComponent;
            if (sum > 0 &&
                std::abs(
                    color.redComponent /
                        sum -
                    expectedColor.red /
                        expectedSum
                ) <= 0.03 &&
                std::abs(
                    color.greenComponent /
                        sum -
                    expectedColor.green /
                        expectedSum
                ) <= 0.03 &&
                std::abs(
                    color.blueComponent /
                        sum -
                    expectedColor.blue /
                        expectedSum
                ) <= 0.03) {
              ++colorPixels;
            }
          }
        }
        qInfo() << "companion-reaction-pixels"
                << static_cast<int>(companion)
                << static_cast<int>(source)
                << scale
                << "body"
                << bitmapChangedPixels(
                       baseline, body
                   )
                << "halo" << haloPixels
                << "color" << colorPixels;
        QVERIFY(haloPixels > 0);
        QCOMPARE(colorPixels, haloPixels);
        totalHaloPixels += haloPixels;
        totalColorPixels += colorPixels;
        [baseline release];
        [body release];
        [paint release];
        [endBody release];
        [endPaint release];
      }
    }
  }
  QCOMPARE(outsidePixels, 0);
  QVERIFY(totalHaloPixels > 0);
  QCOMPARE(totalColorPixels, totalHaloPixels);
}

void InputIndicatorGeometryTests::allCompanionCanvasContainsEightHeadings()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  const Source sources[] = {
      Source::LatinABC,
      Source::Korean2Set,
  };
  const bool facings[] = {true, false};
  const double headings[] = {
      0,
      M_PI / 4,
      M_PI / 2,
      3 * M_PI / 4,
      M_PI,
      5 * M_PI / 4,
      3 * M_PI / 2,
      7 * M_PI / 4,
  };
  constexpr int paddingPoints = 4;
  const InputIndicatorCompanionIdleFrame idle{
      .motion =
          InputIndicatorCompanionIdleMotion::LazySweep,
      .progress = 0.42,
      .active = true,
  };
  const auto reactionState =
      inputIndicatorCompanionSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  const auto peak =
      inputIndicatorCompanionSourceStartleAdvance(
          reactionState, 160, false
      );
  int cases = 0;
  int outsidePixels = 0;
  int paintOutsidePixels = 0;
  int glyphOutsidePixels = 0;
  int haloPixels = 0;
  for (const int scale : {1, 2}) {
    for (const auto companion : companions) {
      const auto style =
          macInputIndicatorPanelStyle(companion);
      const NSInteger canvasLeft =
          paddingPoints * scale;
      const NSInteger canvasBottom =
          paddingPoints * scale;
      const NSInteger canvasRight =
          canvasLeft +
          static_cast<NSInteger>(
              style.canvasWidth
          ) *
              scale;
      const NSInteger canvasTop =
          canvasBottom +
          static_cast<NSInteger>(
              style.canvasHeight
          ) *
              scale;
      for (const auto source : sources) {
        for (const bool facesRight : facings) {
          for (const double heading : headings) {
            NSBitmapImageRep *body =
                companionBodyBitmap(
                    companion,
                    InputIndicatorMotionState::Idle,
                    0,
                    idle,
                    peak.frame,
                    facesRight,
                    scale,
                    paddingPoints,
                    true,
                    heading
                );
            NSBitmapImageRep *paint =
                companionBodyBitmap(
                    companion,
                    InputIndicatorMotionState::Idle,
                    0,
                    idle,
                    peak.frame,
                    facesRight,
                    scale,
                    paddingPoints,
                    true,
                    heading,
                    1,
                    true,
                    source
                );
            NSBitmapImageRep *glyph =
                companionGlyphBitmap(
                    companion,
                    InputIndicatorMotionState::Idle,
                    source,
                    facesRight,
                    scale,
                    paddingPoints,
                    heading
                );
            QVERIFY(body);
            QVERIFY(paint);
            QVERIFY(glyph);
            int caseOutsidePixels = 0;
            int caseHaloPixels = 0;
            for (NSInteger y = 0;
                 y < paint.pixelsHigh;
                 ++y) {
              for (NSInteger x = 0;
                   x < paint.pixelsWide;
                   ++x) {
                const double paintAlpha =
                    [paint colorAtX:x y:y]
                        .alphaComponent;
                const double bodyAlpha =
                    [body colorAtX:x y:y]
                        .alphaComponent;
                const double glyphAlpha =
                    [glyph colorAtX:x y:y]
                        .alphaComponent;
                if (paintAlpha > 0.02 &&
                    (x < canvasLeft ||
                     x >= canvasRight ||
                     y < canvasBottom ||
                     y >= canvasTop)) {
                  ++caseOutsidePixels;
                  ++paintOutsidePixels;
                }
                if (glyphAlpha > 0.02 &&
                    bodyAlpha <= 0.02) {
                  ++caseOutsidePixels;
                  ++glyphOutsidePixels;
                }
                if (paintAlpha > 0.02 &&
                    bodyAlpha == 0) {
                  ++caseHaloPixels;
                  ++haloPixels;
                }
              }
            }
            QCOMPARE(caseOutsidePixels, 0);
            QVERIFY(caseHaloPixels > 0);
            outsidePixels += caseOutsidePixels;
            [body release];
            [paint release];
            [glyph release];
            ++cases;
          }
        }
      }
    }
  }
  qInfo() << "dog-turtle-rotating-canvas"
          << "outsidePixels" << outsidePixels
          << "paintOutside" << paintOutsidePixels
          << "glyphOutside" << glyphOutsidePixels
          << "haloPixels" << haloPixels
          << "cases" << cases;
  QCOMPARE(cases, 128);
  QCOMPARE(paintOutsidePixels, 0);
  QCOMPARE(glyphOutsidePixels, 0);
  QCOMPARE(outsidePixels, 0);
  QVERIFY(haloPixels > 0);
}

void InputIndicatorGeometryTests::catPursuitWalksNearAndSettlesWithoutOvershoot()
{
  const InputIndicatorTarget target{
      .point = {.x = 10, .y = 0},
      .kind = InputIndicatorTargetKind::Pointer,
  };
  InputIndicatorCatPursuitState state{
      .position = {.x = 0, .y = 0},
      .initialized = true,
  };
  const auto first =
      inputIndicatorCatPursuitAdvance(
          state, target, 40, 8, false, false
      );
  QVERIFY(first);
  QCOMPARE(
      first->state.motionState,
      InputIndicatorMotionState::Walk
  );
  QVERIFY(first->distanceMoved <= 0.9600001);
  state = first->state;

  while (!first->reached) {
    const auto step =
        inputIndicatorCatPursuitAdvance(
            state, target, 40, 8, false, false
        );
    QVERIFY(step);
    QVERIFY(step->state.position.x >= state.position.x);
    QVERIFY(step->state.position.x <= target.point.x);
    state = step->state;
    if (step->reached) {
      break;
    }
  }
  QCOMPARE(state.position.x, target.point.x);
  QCOMPARE(state.position.y, target.point.y);
  state.stoppedMilliseconds = 0;
  state.motionState = InputIndicatorMotionState::Walk;
  const auto at179 =
      inputIndicatorCatPursuitAdvance(
          state, target, 0, 179, false, false
      );
  QVERIFY(at179);
  QCOMPARE(
      at179->state.motionState,
      InputIndicatorMotionState::Walk
  );
  const auto at180 =
      inputIndicatorCatPursuitAdvance(
          at179->state, target, 0, 1, false, false
      );
  QVERIFY(at180);
  QCOMPARE(
      at180->state.motionState,
      InputIndicatorMotionState::Idle
  );

  const InputIndicatorPoint approaches[] = {
      {.x = 10, .y = 0},
      {.x = -10, .y = 0},
      {.x = 0, .y = 10},
      {.x = 0, .y = -10},
      {.x = 7, .y = 7},
      {.x = -7, .y = -7},
  };
  for (const auto point : approaches) {
    InputIndicatorCatPursuitState current{
        .position = {},
        .initialized = true,
    };
    const InputIndicatorTarget directional{
        .point = point,
        .kind = InputIndicatorTargetKind::Pointer,
    };
    double previousDistance =
        std::hypot(point.x, point.y);
    for (int attempt = 0; attempt < 100; ++attempt) {
      const auto step =
          inputIndicatorCatPursuitAdvance(
              current,
              directional,
              20,
              8,
              false,
              false
          );
      QVERIFY(step);
      const double distance =
          std::hypot(
              directional.point.x -
                  step->state.position.x,
              directional.point.y -
                  step->state.position.y
          );
      QVERIFY(distance <= previousDistance + 1e-9);
      previousDistance = distance;
      current = step->state;
      if (step->reached) {
        break;
      }
    }
    QCOMPARE(current.position.x, point.x);
    QCOMPARE(current.position.y, point.y);
  }
}

void InputIndicatorGeometryTests::catPursuitHandlesEightHeadingsDeterministically()
{
  constexpr double kPi = 3.14159265358979323846;
  const double angles[] = {
      0,
      kPi / 4,
      kPi / 2,
      3 * kPi / 4,
      kPi,
      5 * kPi / 4,
      3 * kPi / 2,
      7 * kPi / 4,
  };
  for (const double angle : angles) {
    InputIndicatorCatPursuitState state{
        .position = {},
        .headingRadians = angle,
        .initialized = true,
    };
    const InputIndicatorTarget target{
        .point = {
            .x = std::cos(angle) * 100,
            .y = std::sin(angle) * 100,
        },
    };
    const auto step =
        inputIndicatorCatPursuitAdvance(
            state, target, 800, 8, false, false
        );
    QVERIFY(step);
    QVERIFY(std::abs(
        inputIndicatorNormalizedAngle(
            step->state.headingRadians - angle
        )
    ) < 1e-9);
  }

  InputIndicatorCatPursuitState turning{
      .position = {},
      .headingRadians = 3.1,
      .initialized = true,
  };
  const InputIndicatorTarget wrapTarget{
      .point = {
          .x = std::cos(-3.1) * 100,
          .y = std::sin(-3.1) * 100,
      },
  };
  const auto turned =
      inputIndicatorCatPursuitAdvance(
          turning, wrapTarget, 800, 16, false, false
      );
  QVERIFY(turned);
  QVERIFY(std::abs(
      inputIndicatorNormalizedAngle(
          turned->state.headingRadians - 3.1
      )
  ) <= 0.1600001);

  const auto straight =
      inputIndicatorCatBodyPoint(
          {.x = 20, .y = 10},
          true,
          1,
          0
      );
  QCOMPARE(straight.x, 20.0);
  QCOMPARE(straight.y, 10.0);

  const auto diagonal =
      inputIndicatorCatBodyPoint(
          {.x = 20, .y = 10},
          true,
          1,
          kPi / 4
      );
  QVERIFY(std::abs(
      diagonal.x -
      (15 + (5 + 4) / std::sqrt(2.0))
  ) < 1e-9);
  QVERIFY(std::abs(
      diagonal.y -
      (14 + (5 - 4) / std::sqrt(2.0))
  ) < 1e-9);

  const auto left =
      inputIndicatorCatBodyPoint(
          {.x = 20, .y = 10},
          false,
          1,
          kPi
      );
  QCOMPARE(left.x, 10.0);
  QCOMPARE(left.y, 10.0);
}

void InputIndicatorGeometryTests::catPursuitRejectsInvalidAndHonorsReducedMotion()
{
  InputIndicatorCatPursuitState valid{
      .position = {.x = 10, .y = 20},
      .initialized = true,
  };
  const auto reduced =
      inputIndicatorCatPursuitAdvance(
          valid,
          {
              .point = {.x = 100, .y = 200},
              .kind = InputIndicatorTargetKind::Caret,
          },
          999,
          8,
          false,
          true
      );
  QVERIFY(reduced);
  QCOMPARE(reduced->state.position.x, 100.0);
  QCOMPARE(reduced->state.position.y, 200.0);
  QCOMPARE(
      reduced->targetKind,
      InputIndicatorTargetKind::Caret
  );
  QCOMPARE(
      reduced->state.motionState,
      InputIndicatorMotionState::Idle
  );
  QVERIFY(reduced->reached);

  const double nan =
      std::numeric_limits<double>::quiet_NaN();
  const double infinity =
      std::numeric_limits<double>::infinity();
  QVERIFY(!inputIndicatorCatPursuitAdvance(
      valid,
      {.point = {.x = nan, .y = 0}},
      10,
      8,
      false,
      false
  ));
  QVERIFY(!inputIndicatorCatPursuitAdvance(
      valid,
      {.point = {.x = 10, .y = 0}},
      -1,
      8,
      false,
      false
  ));
  QVERIFY(!inputIndicatorCatPursuitAdvance(
      valid,
      {.point = {.x = 10, .y = 0}},
      10,
      -1,
      false,
      false
  ));
  QVERIFY(!inputIndicatorCatPursuitAdvance(
      {
          .position = {.x = infinity, .y = 0},
          .initialized = true,
      },
      {.point = {.x = 10, .y = 0}},
      10,
      8,
      false,
      false
  ));
  QVERIFY(!inputIndicatorCatPursuitAdvance(
      {
          .position = {.x = infinity, .y = 0},
          .initialized = true,
      },
      {.point = {.x = 10, .y = 0}},
      10,
      8,
      false,
      true
  ));
}

void InputIndicatorGeometryTests::catPursuitTreatsPointerAndCaretTargetsIdentically()
{
  const InputIndicatorCatPursuitState state{
      .position = {.x = 10, .y = 20},
      .velocity = {.x = 5, .y = 3},
      .headingRadians = 0.3,
      .stoppedMilliseconds = 0,
      .motionState = InputIndicatorMotionState::Walk,
      .initialized = true,
  };
  const auto pointer =
      inputIndicatorCatPursuitAdvance(
          state,
          {
              .point = {.x = 200, .y = 130},
              .kind = InputIndicatorTargetKind::Pointer,
          },
          800,
          8,
          false,
          false
      );
  const auto caret =
      inputIndicatorCatPursuitAdvance(
          state,
          {
              .point = {.x = 200, .y = 130},
              .kind = InputIndicatorTargetKind::Caret,
          },
          800,
          8,
          false,
          false
      );
  QVERIFY(pointer);
  QVERIFY(caret);
  QCOMPARE(pointer->state.position.x,
           caret->state.position.x);
  QCOMPARE(pointer->state.position.y,
           caret->state.position.y);
  QCOMPARE(pointer->state.velocity.x,
           caret->state.velocity.x);
  QCOMPARE(pointer->state.velocity.y,
           caret->state.velocity.y);
  QCOMPARE(pointer->state.headingRadians,
           caret->state.headingRadians);
  QCOMPARE(pointer->state.motionState,
           caret->state.motionState);
  QCOMPARE(pointer->distanceMoved,
           caret->distanceMoved);
  QCOMPARE(
      pointer->targetKind,
      InputIndicatorTargetKind::Pointer
  );
  QCOMPARE(
      caret->targetKind,
      InputIndicatorTargetKind::Caret
  );
}

void InputIndicatorGeometryTests::catPursuitPreservesInitializedStateAcrossRepresentationChanges()
{
  const InputIndicatorCatPursuitState initialized{
      .position = {.x = 123, .y = 456},
      .velocity = {.x = 7, .y = 2},
      .headingRadians = 0.7,
      .stoppedMilliseconds = 15,
      .motionState = InputIndicatorMotionState::Run,
      .initialized = true,
  };
  const auto sourceChange =
      inputIndicatorCatPursuitAdvance(
          initialized,
          {
              .point = {.x = 223, .y = 456},
              .kind = InputIndicatorTargetKind::Pointer,
          },
          0,
          0,
          false,
          false
      );
  QVERIFY(sourceChange);
  QCOMPARE(sourceChange->state.position.x,
           initialized.position.x);
  QCOMPARE(sourceChange->state.position.y,
           initialized.position.y);
  QCOMPARE(sourceChange->state.headingRadians,
           initialized.headingRadians);
  QCOMPARE(sourceChange->distanceMoved, 0.0);
  QVERIFY(sourceChange->state.initialized);

  const auto firstVisible =
      inputIndicatorCatPursuitAdvance(
          initialized,
          {
              .point = {.x = 223, .y = 456},
              .kind = InputIndicatorTargetKind::Pointer,
          },
          0,
          8,
          true,
          false
      );
  QVERIFY(firstVisible);
  QCOMPARE(firstVisible->state.position.x, 223.0);
  QCOMPARE(firstVisible->state.position.y, 456.0);
  QVERIFY(firstVisible->reached);
}

void InputIndicatorGeometryTests::catPursuitPanelTraversesLargeTargetJump()
{
  const auto fixedCursor =
      fixedPanelTestPointer();
  InputIndicatorTarget target{
      .point = {
          .x = fixedCursor.x,
          .y = fixedCursor.y,
      },
      .kind = InputIndicatorTargetKind::Pointer,
  };
  MacInputIndicatorPanel panel(
      {},
      [&target] {
        return target;
      }
  );
  const MacInputIndicatorPanelOptions options{
      .source =
          deskflow::input::InputIndicatorSource::LatinABC,
      .companion = InputIndicatorCompanion::Cat,
  };
  QVERIFY(panel.show(options));
  NSView *view = catPanelView();
  QVERIFY(view);
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  QCOMPARE(style.width, 33.0);
  QCOMPARE(style.height, 31.0);
  QCOMPARE(style.canvasWidth, 49.0);
  QCOMPARE(style.canvasHeight, 49.0);
  QCOMPARE(style.canvasOffsetX, 8.0);
  QCOMPARE(style.canvasOffsetY, 9.0);

  NSRect containingScreen;
  bool foundScreen = false;
  for (NSScreen *screen in NSScreen.screens) {
    if (NSPointInRect(fixedCursor, screen.frame)) {
      containingScreen = screen.frame;
      foundScreen = true;
      break;
    }
  }
  QVERIFY(foundScreen);
  target.point.x =
      fixedCursor.x <
              NSMidX(containingScreen)
          ? std::max(
                fixedCursor.x + 700,
                NSMinX(containingScreen) + 700
            )
          : std::min(
                fixedCursor.x - 700,
                NSMaxX(containingScreen) - 700
            );
  target.point.y =
      fixedCursor.y <
              NSMidY(containingScreen)
          ? std::min(
                fixedCursor.y + 500,
                NSMaxY(containingScreen) - 80
            )
          : std::max(
                fixedCursor.y - 500,
                NSMinY(containingScreen) + 80
            );
  QVERIFY(NSPointInRect(
      NSMakePoint(target.point.x, target.point.y),
      containingScreen
  ));

  const std::vector<InputIndicatorRect> screens{{
      .x = containingScreen.origin.x,
      .y = containingScreen.origin.y,
      .width = containingScreen.size.width,
      .height = containingScreen.size.height,
  }};
  const auto expected =
      positionInputIndicator(
          target.point,
          {.width = style.width, .height = style.height},
          screens,
          options.position,
          style.cursorGap,
          style.screenMargin
      );
  QVERIFY(expected);

  const auto visibleOrigin =
      [view, style] {
        const auto origin =
            view.window.frame.origin;
        return InputIndicatorPoint{
            .x = origin.x + style.canvasOffsetX,
            .y = origin.y + style.canvasOffsetY,
        };
      };
  std::vector<InputIndicatorPoint> positions;
  positions.push_back(visibleOrigin());
  const double expectedHeading =
      std::atan2(
          expected->y - positions.front().y,
          expected->x - positions.front().x
      );
  bool reached = false;
  for (int tick = 0; tick < 400; ++tick) {
    QTest::qWait(8);
    const auto result =
        panel.move(options.position);
    QVERIFY(result.visible);
    QVERIFY(!result.reducedMotion);
    QCOMPARE(result.frameIntervalMilliseconds, 8);
    const auto current = visibleOrigin();
    const auto previous = positions.back();
    const double movement =
        std::hypot(
            current.x - previous.x,
            current.y - previous.y
        );
    const double kPanelCoordinateTolerance =
        0.5 * std::sqrt(2.0);
    QVERIFY(
        movement <=
        5.760001 + kPanelCoordinateTolerance
    );
    const double previousDistance =
        std::hypot(
            expected->x - previous.x,
            expected->y - previous.y
        );
    const double currentDistance =
        std::hypot(
            expected->x - current.x,
            expected->y - current.y
        );
    QVERIFY(
        currentDistance <=
        previousDistance + kPanelCoordinateTolerance
    );
    if (tick == 20) {
      const double renderedHeading =
          [[view valueForKey:@"catBodyHeadingRadians"]
              doubleValue];
      QVERIFY(std::isfinite(renderedHeading));
      QVERIFY(std::abs(
          inputIndicatorNormalizedAngle(
              renderedHeading - expectedHeading
          )
      ) < 1.0);
    }
    positions.push_back(current);
    if (currentDistance <=
        kPanelCoordinateTolerance) {
      reached = true;
      break;
    }
  }
  QVERIFY(reached);
  QVERIFY(positions.size() > 20);
  const auto final = positions.back();
  QVERIFY(std::abs(final.x - expected->x) <= 0.5);
  QVERIFY(std::abs(final.y - expected->y) <= 0.5);
}

void InputIndicatorGeometryTests::companionPursuitPanelHonorsFollowSpeed()
{
  const auto fixedCursor =
      fixedPanelTestPointer();
  NSRect containingScreen;
  bool foundScreen = false;
  for (NSScreen *screen in NSScreen.screens) {
    if (NSPointInRect(fixedCursor, screen.frame)) {
      containingScreen = screen.frame;
      foundScreen = true;
      break;
    }
  }
  QVERIFY(foundScreen);
  const std::vector<InputIndicatorRect> screens{{
      .x = containingScreen.origin.x,
      .y = containingScreen.origin.y,
      .width = containingScreen.size.width,
      .height = containingScreen.size.height,
  }};
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  const struct
  {
    int percent;
    double movementCap;
    int minimumIntermediatePositions;
  } speeds[] = {
      {.percent = 50,
       .movementCap = 2.88,
       .minimumIntermediatePositions = 100},
      {.percent = 100,
       .movementCap = 5.76,
       .minimumIntermediatePositions = 40},
      {.percent = 200,
       .movementCap = 11.52,
       .minimumIntermediatePositions = 10},
  };

  for (const auto companion : companions) {
    const auto style =
        macInputIndicatorPanelStyle(companion);
    for (const auto speed : speeds) {
      InputIndicatorTarget target{
          .point = {
              .x = fixedCursor.x,
              .y = fixedCursor.y,
          },
          .kind =
              InputIndicatorTargetKind::Pointer,
      };
      MacInputIndicatorPanel panel(
          {},
          [&target] {
            return target;
          }
      );
      const MacInputIndicatorPanelOptions options{
          .source =
              deskflow::input::InputIndicatorSource::LatinABC,
          .companion = companion,
          .followSpeedPercent = speed.percent,
      };
      QVERIFY(panel.show(options));
      NSView *view = catPanelView();
      QVERIFY(view);
      const auto visibleOrigin =
          [view, style] {
            const auto origin =
                view.window.frame.origin;
            return InputIndicatorPoint{
                .x =
                    origin.x +
                    style.canvasOffsetX,
                .y =
                    origin.y +
                    style.canvasOffsetY,
            };
          };

      target.point.x =
          fixedCursor.x <
                  NSMidX(containingScreen)
              ? std::max(
                    fixedCursor.x + 700,
                    NSMinX(containingScreen) +
                        700
                )
              : std::min(
                    fixedCursor.x - 700,
                    NSMaxX(containingScreen) -
                        700
                );
      target.point.y =
          fixedCursor.y <
                  NSMidY(containingScreen)
              ? std::min(
                    fixedCursor.y + 500,
                    NSMaxY(containingScreen) -
                        80
                )
              : std::max(
                    fixedCursor.y - 500,
                    NSMinY(containingScreen) +
                        80
                );
      const auto expected =
          positionInputIndicator(
              target.point,
              {
                  .width = style.width,
                  .height = style.height,
              },
              screens,
              options.position,
              style.cursorGap,
              style.screenMargin
          );
      QVERIFY(expected);

      std::vector<InputIndicatorPoint> positions{
          visibleOrigin()
      };
      const double quantizationTolerance =
          std::sqrt(2.0);
      bool reached = false;
      for (int tick = 0;
           tick < 600;
           ++tick) {
        QTest::qWait(8);
        const auto result =
            panel.move(options.position);
        QVERIFY(result.visible);
        QVERIFY(!result.reducedMotion);
        QCOMPARE(
            result.frameIntervalMilliseconds,
            8
        );
        const auto current = visibleOrigin();
        const auto previous = positions.back();
        const double movement =
            std::hypot(
                current.x - previous.x,
                current.y - previous.y
            );
        if (movement >
            speed.movementCap +
                quantizationTolerance) {
          qInfo() << "companion-speed-step"
                  << static_cast<int>(companion)
                  << speed.percent
                  << tick
                  << movement
                  << speed.movementCap;
        }
        QVERIFY(
            movement <=
            speed.movementCap +
                quantizationTolerance
        );
        const double previousDistance =
            std::hypot(
                expected->x - previous.x,
                expected->y - previous.y
            );
        const double currentDistance =
            std::hypot(
                expected->x - current.x,
                expected->y - current.y
            );
        QVERIFY(
            currentDistance <=
            previousDistance +
                quantizationTolerance
        );
        positions.push_back(current);
        if (std::abs(current.x - expected->x) < 1.0 &&
            std::abs(current.y - expected->y) < 1.0) {
          reached = true;
          break;
        }
      }
      QVERIFY(reached);
      QVERIFY(
          static_cast<int>(positions.size()) >
          speed.minimumIntermediatePositions
      );
      const auto final = positions.back();
      QVERIFY(
          std::abs(final.x - expected->x) < 1.0
      );
      QVERIFY(
          std::abs(final.y - expected->y) < 1.0
      );
      panel.hide();
    }
  }
}

void InputIndicatorGeometryTests::companionPursuitPanelSpeedChangePreservesPosition()
{
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  NSRect containingScreen;
  bool foundScreen = false;
  for (NSScreen *screen in NSScreen.screens) {
    if (NSPointInRect(fixedCursor, screen.frame)) {
      containingScreen = screen.frame;
      foundScreen = true;
      break;
    }
  }
  QVERIFY(foundScreen);

  for (const auto companion : companions) {
    InputIndicatorTarget target{
        .point = {
            .x = fixedCursor.x,
            .y = fixedCursor.y,
        },
        .kind = InputIndicatorTargetKind::Pointer,
    };
    MacInputIndicatorPanel panel(
        {},
        [&target] {
          return target;
        }
    );
    const MacInputIndicatorPanelOptions options{
        .source =
            deskflow::input::InputIndicatorSource::LatinABC,
        .companion = companion,
        .followSpeedPercent = 50,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    const auto style =
        macInputIndicatorPanelStyle(companion);
    const auto visibleOrigin =
        [view, style] {
          const auto origin =
              view.window.frame.origin;
          return InputIndicatorPoint{
              .x =
                  origin.x +
                  style.canvasOffsetX,
              .y =
                  origin.y +
                  style.canvasOffsetY,
          };
        };
    target.point.x =
        fixedCursor.x < NSMidX(containingScreen)
            ? std::min(
                  fixedCursor.x + 700,
                  NSMaxX(containingScreen) - 80
              )
            : std::max(
                  fixedCursor.x - 700,
                  NSMinX(containingScreen) + 80
              );
    QTest::qWait(8);
    QVERIFY(panel.move(options.position).visible);
    const auto beforeChange = visibleOrigin();
    panel.setFollowSpeedPercent(200);
    const auto afterChange = visibleOrigin();
    QCOMPARE(afterChange.x, beforeChange.x);
    QCOMPARE(afterChange.y, beforeChange.y);

    QTest::qWait(8);
    QVERIFY(panel.move(options.position).visible);
    const auto afterMove = visibleOrigin();
    const double movement =
        std::hypot(
            afterMove.x - beforeChange.x,
            afterMove.y - beforeChange.y
        );
    QVERIFY(movement > 2.88);
    QVERIFY(
        movement <=
        11.52 + std::sqrt(2.0)
    );
    panel.hide();
  }
}

void InputIndicatorGeometryTests::companionPursuitPanelPreservesPositionOnSourceAndSpaceChange()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };
  for (const auto companion : companions) {
    InputIndicatorTarget target{
        .point = {
            .x = fixedCursor.x,
            .y = fixedCursor.y,
        },
        .kind =
            InputIndicatorTargetKind::Pointer,
    };
    MacInputIndicatorPanel panel(
        {},
        [&target] {
          return target;
        }
    );
    MacInputIndicatorPanelOptions options{
        .source = Source::LatinABC,
        .companion = companion,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    const auto style =
        macInputIndicatorPanelStyle(companion);
    const auto visibleOrigin =
        [view, style] {
          const auto origin =
              view.window.frame.origin;
          return InputIndicatorPoint{
              .x =
                  origin.x +
                  style.canvasOffsetX,
              .y =
                  origin.y +
                  style.canvasOffsetY,
          };
        };

    target.point.x +=
        target.point.x < 1000 ? 500 : -500;
    QTest::qWait(8);
    QVERIFY(panel.move(options.position).visible);
    const auto beforeSource = visibleOrigin();
    options.source = Source::Korean2Set;
    QVERIFY(panel.show(options));
    const auto afterSource = visibleOrigin();
    QVERIFY(std::hypot(
        afterSource.x - beforeSource.x,
        afterSource.y - beforeSource.y
    ) <= 5.760001 + std::sqrt(2.0));

    QTest::qWait(8);
    QVERIFY(panel.move(options.position).visible);
    const auto beforeSpace = visibleOrigin();
    [[NSWorkspace sharedWorkspace].notificationCenter
        postNotificationName:
            NSWorkspaceActiveSpaceDidChangeNotification
                  object:nil];
    QCoreApplication::processEvents();
    QVERIFY(panel.move(options.position).visible);
    const auto afterSpace = visibleOrigin();
    QVERIFY(std::hypot(
        afterSpace.x - beforeSpace.x,
        afterSpace.y - beforeSpace.y
    ) <= 5.760001 + std::sqrt(2.0));
    panel.hide();
  }
}

void InputIndicatorGeometryTests::catPursuitPanelSettlesWithinWholePointOfFractionalTargets()
{
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  NSScreen *screen = NSScreen.screens.firstObject;
  QVERIFY(screen);
  const NSRect screenFrame = screen.frame;
  const std::vector<InputIndicatorRect> screens{{
      .x = screenFrame.origin.x,
      .y = screenFrame.origin.y,
      .width = screenFrame.size.width,
      .height = screenFrame.size.height,
  }};
  const NSPoint start = fixedPanelTestPointer();
  // AppKit gives the panel window a whole point origin, so a target whose
  // exact position has a fractional part can be reproduced only to within
  // a point per axis. These are fractions that failed with a live pointer.
  const std::array<InputIndicatorPoint, 5> fractions{{
      {.x = 0.25, .y = 0.25},
      {.x = 0.5, .y = 0.5},
      {.x = 0.75, .y = 0.75},
      {.x = 0.98, .y = 0.719},
      {.x = 0.3, .y = 0.7},
  }};
  for (const auto &fraction : fractions) {
    InputIndicatorTarget target{
        .point = {
            .x = start.x + fraction.x,
            .y = start.y + fraction.y,
        },
        .kind = InputIndicatorTargetKind::Pointer,
    };
    MacInputIndicatorPanel panel(
        {},
        [&target] {
          return target;
        }
    );
    const MacInputIndicatorPanelOptions options{
        .source =
            deskflow::input::InputIndicatorSource::LatinABC,
        .companion = InputIndicatorCompanion::Cat,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    const auto visibleOrigin =
        [view, style] {
          const auto origin =
              view.window.frame.origin;
          return InputIndicatorPoint{
              .x = origin.x + style.canvasOffsetX,
              .y = origin.y + style.canvasOffsetY,
          };
        };

    target.point.x -= 300;
    target.point.y -= 200;
    const auto expected =
        positionInputIndicator(
            target.point,
            {.width = style.width, .height = style.height},
            screens,
            options.position,
            style.cursorGap,
            style.screenMargin
        );
    QVERIFY(expected);
    bool settled = false;
    InputIndicatorPoint settledOrigin{};
    for (int tick = 0; tick < 400; ++tick) {
      QTest::qWait(8);
      const auto result =
          panel.move(options.position);
      QVERIFY(result.visible);
      if (result.frameIntervalMilliseconds != 8) {
        settled = true;
        settledOrigin = visibleOrigin();
        break;
      }
    }
    QVERIFY(settled);
    QVERIFY(
        std::abs(settledOrigin.x - expected->x) < 1.0
    );
    QVERIFY(
        std::abs(settledOrigin.y - expected->y) < 1.0
    );
    for (int tick = 0; tick < 5; ++tick) {
      QTest::qWait(8);
      const auto idle =
          panel.move(options.position);
      QVERIFY(idle.frameIntervalMilliseconds != 8);
      const auto current = visibleOrigin();
      QCOMPARE(current.x, settledOrigin.x);
      QCOMPARE(current.y, settledOrigin.y);
    }
  }
}

void InputIndicatorGeometryTests::catPursuitPanelPreservesPositionOnSourceAndSpaceChange()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  InputIndicatorTarget target{
      .point = {
          .x = fixedCursor.x,
          .y = fixedCursor.y,
      },
      .kind = InputIndicatorTargetKind::Pointer,
  };
  MacInputIndicatorPanel panel(
      {},
      [&target] {
        return target;
      }
  );
  MacInputIndicatorPanelOptions options{
      .source = Source::LatinABC,
      .companion = InputIndicatorCompanion::Cat,
  };
  QVERIFY(panel.show(options));
  NSView *view = catPanelView();
  QVERIFY(view);
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  const auto visibleOrigin =
      [view, style] {
        const auto origin =
            view.window.frame.origin;
        return InputIndicatorPoint{
            .x = origin.x + style.canvasOffsetX,
            .y = origin.y + style.canvasOffsetY,
        };
      };

  target.point.x +=
      target.point.x < 1000 ? 500 : -500;
  QTest::qWait(8);
  QVERIFY(panel.move(options.position).visible);
  const auto beforeSource = visibleOrigin();
  options.source = Source::Korean2Set;
  QVERIFY(panel.show(options));
  const auto afterSource = visibleOrigin();
  QVERIFY(std::hypot(
      afterSource.x - beforeSource.x,
      afterSource.y - beforeSource.y
  ) <= 5.760001 + 0.5 * std::sqrt(2.0));

  QTest::qWait(8);
  QVERIFY(panel.move(options.position).visible);
  const auto beforeSpace = visibleOrigin();
  [[NSWorkspace sharedWorkspace].notificationCenter
      postNotificationName:
          NSWorkspaceActiveSpaceDidChangeNotification
                    object:nil];
  QCoreApplication::processEvents();
  QVERIFY(panel.move(options.position).visible);
  const auto afterSpace = visibleOrigin();
  QVERIFY(std::hypot(
      afterSpace.x - beforeSpace.x,
      afterSpace.y - beforeSpace.y
  ) <= 5.760001 + 0.5 * std::sqrt(2.0));
}

void InputIndicatorGeometryTests::catPursuitRotatesSilhouetteButKeepsGlyphUpright()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto pose =
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0, false
      );
  const auto artwork =
      macInputIndicatorCatArtwork(pose.artworkFrame);
  const InputIndicatorCatIdleTailFrame restTail;
  const InputIndicatorCatStartleFrame restStartle;
  constexpr double heading =
      M_PI / 4.0;
  constexpr int scale = 2;
  constexpr int paddingPoints = 4;

  NSBitmapImageRep *straightBody =
      catBodyBitmap(
          pose,
          artwork,
          restTail,
          true,
          scale,
          paddingPoints,
          false,
          restStartle,
          0,
          true
      );
  NSBitmapImageRep *diagonalBody =
      catBodyBitmap(
          pose,
          artwork,
          restTail,
          true,
          scale,
          paddingPoints,
          false,
          restStartle,
          heading,
          true
      );
  NSBitmapImageRep *straightGlyph =
      catGlyphBitmap(
          pose,
          InputIndicatorMotionState::Idle,
          true,
          scale,
          paddingPoints,
          Source::LatinABC,
          0,
          true
      );
  NSBitmapImageRep *diagonalGlyph =
      catGlyphBitmap(
          pose,
          InputIndicatorMotionState::Idle,
          true,
          scale,
          paddingPoints,
          Source::LatinABC,
          heading,
          true
      );
  QVERIFY(straightBody);
  QVERIFY(diagonalBody);
  QVERIFY(straightGlyph);
  QVERIFY(diagonalGlyph);
  QVERIFY(bitmapChangedPixels(
      straightBody, diagonalBody
  ) > 0);
  const auto straightGlyphBounds =
      bitmapAlphaBounds(straightGlyph);
  const auto diagonalGlyphBounds =
      bitmapAlphaBounds(diagonalGlyph);
  QVERIFY(straightGlyphBounds.width > 0);
  QVERIFY(diagonalGlyphBounds.width > 0);
  QVERIFY(std::abs(
      straightGlyphBounds.width -
      diagonalGlyphBounds.width
  ) <= 1);
  QVERIFY(std::abs(
      straightGlyphBounds.height -
      diagonalGlyphBounds.height
  ) <= 1);
  QVERIFY(std::abs(
      straightGlyphBounds.x -
      diagonalGlyphBounds.x
  ) > 2 ||
      std::abs(
          straightGlyphBounds.y -
          diagonalGlyphBounds.y
      ) > 2);

  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  const NSInteger panelLeft =
      (paddingPoints + style.canvasOffsetX) * scale;
  const NSInteger panelBottom =
      (paddingPoints + style.canvasOffsetY) * scale;
  const NSInteger visibleRight =
      panelLeft +
      static_cast<NSInteger>(
          std::lround(style.width)
      ) *
          scale;
  const NSInteger visibleTop =
      panelBottom +
      static_cast<NSInteger>(
          std::lround(style.height)
      ) *
          scale;
  QCOMPARE(panelLeft, 24);
  QCOMPARE(panelBottom, 26);
  QCOMPARE(visibleRight - panelLeft, 66);
  QCOMPARE(visibleTop - panelBottom, 62);

  for (NSInteger y = 0;
       y < diagonalBody.pixelsHigh;
       ++y) {
    for (NSInteger x = 0;
         x < diagonalBody.pixelsWide;
         ++x) {
      const double bodyAlpha =
          [diagonalBody colorAtX:x y:y]
              .alphaComponent;
      const double glyphAlpha =
          [diagonalGlyph colorAtX:x y:y]
              .alphaComponent;
      if (glyphAlpha > 0.02) {
        QVERIFY(bodyAlpha > 0.02);
      }
    }
  }
  [straightBody release];
  [diagonalBody release];
  [straightGlyph release];
  [diagonalGlyph release];

  const auto fixedCursor =
      fixedPanelTestPointer();
  InputIndicatorTarget target{
      .point = {
          .x = fixedCursor.x,
          .y = fixedCursor.y,
      },
      .kind = InputIndicatorTargetKind::Pointer,
  };
  MacInputIndicatorPanel panel(
      {},
      [&target] {
        return target;
      }
  );
  const MacInputIndicatorPanelOptions options{
      .source = Source::LatinABC,
      .companion = InputIndicatorCompanion::Cat,
  };
  QVERIFY(panel.show(options));
  NSView *view = catPanelView();
  QVERIFY(view);
  [view displayIfNeeded];
  NSBitmapImageRep *beforePanel =
      [view bitmapImageRepForCachingDisplayInRect:
                view.bounds];
  QVERIFY(beforePanel);
  [view cacheDisplayInRect:view.bounds
         toBitmapImageRep:beforePanel];
  target.point.x +=
      target.point.x < 1000 ? 300 : -300;
  target.point.y +=
      target.point.y < 700 ? 300 : -300;
  QTest::qWait(100);
  const auto diagonalMove =
      panel.move(options.position);
  QVERIFY(diagonalMove.visible);
  QCOMPARE(
      diagonalMove.frameIntervalMilliseconds,
      8
  );
  [view displayIfNeeded];
  const double renderedHeading =
      [[view valueForKey:@"catBodyHeadingRadians"]
          doubleValue];
  QVERIFY(std::isfinite(renderedHeading));
  QVERIFY(std::abs(renderedHeading) > 0.01);
  NSBitmapImageRep *afterPanel =
      [view bitmapImageRepForCachingDisplayInRect:
                view.bounds];
  QVERIFY(afterPanel);
  [view cacheDisplayInRect:view.bounds
         toBitmapImageRep:afterPanel];
  QVERIFY(bitmapChangedPixels(
      beforePanel, afterPanel
  ) > 0);
}

void InputIndicatorGeometryTests::catPursuitCanvasContainsEightHeadings()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto pose =
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0, false
      );
  const auto artwork =
      macInputIndicatorCatArtwork(pose.artworkFrame);
  const Source sources[] = {
      Source::LatinABC,
      Source::Korean2Set,
  };
  const bool facings[] = {true, false};
  const double headings[] = {
      0,
      M_PI / 4,
      M_PI / 2,
      3 * M_PI / 4,
      M_PI,
      5 * M_PI / 4,
      3 * M_PI / 2,
      7 * M_PI / 4,
  };
  constexpr int paddingPoints = 4;
  int cases = 0;
  int outsidePixels = 0;
  for (const int scale : {1, 2}) {
    const auto style =
        macInputIndicatorPanelStyle(
            InputIndicatorCompanion::Cat
        );
    const NSInteger canvasLeft =
        paddingPoints * scale;
    const NSInteger canvasBottom =
        paddingPoints * scale;
    const NSInteger canvasRight =
        canvasLeft +
        static_cast<NSInteger>(
            std::lround(style.canvasWidth)
        ) *
            scale;
    const NSInteger canvasTop =
        canvasBottom +
        static_cast<NSInteger>(
            std::lround(style.canvasHeight)
        ) *
            scale;
    for (const auto source : sources) {
      for (const bool facesRight : facings) {
        for (const double heading : headings) {
          const InputIndicatorCatIdleTailFrame tail{
              .motion =
                  InputIndicatorCatIdleTailMotion::QuestionLift,
              .progress = 0.5,
              .active = true,
          };
          const InputIndicatorCatStartleFrame startle{
              .active = true,
              .progress = 160.0 / 300.0,
              .horizontalScale = 1,
              .dorsalLift = 1.8,
          };
          NSBitmapImageRep *body =
              catBodyBitmap(
                  pose,
                  artwork,
                  tail,
                  facesRight,
                  scale,
                  paddingPoints,
                  true,
                  startle,
                  heading,
                  true
              );
          NSBitmapImageRep *glyph =
              catGlyphBitmap(
                  pose,
                  InputIndicatorMotionState::Idle,
                  facesRight,
                  scale,
                  paddingPoints,
                  source,
                  heading,
                  true
              );
          QVERIFY(body);
          QVERIFY(glyph);
          int caseOutsidePixels = 0;
          for (NSInteger y = 0;
               y < body.pixelsHigh;
               ++y) {
            for (NSInteger x = 0;
                 x < body.pixelsWide;
                 ++x) {
              const double bodyAlpha =
                  [body colorAtX:x y:y]
                      .alphaComponent;
              const double glyphAlpha =
                  [glyph colorAtX:x y:y]
                      .alphaComponent;
              if (bodyAlpha > 0.02 &&
                  (x < canvasLeft ||
                   x >= canvasRight ||
                   y < canvasBottom ||
                   y >= canvasTop)) {
                ++caseOutsidePixels;
              }
              if (glyphAlpha > 0.02 &&
                  bodyAlpha <= 0.02) {
                ++caseOutsidePixels;
              }
            }
          }
          outsidePixels += caseOutsidePixels;
          QCOMPARE(caseOutsidePixels, 0);
          [body release];
          [glyph release];
          ++cases;
        }
      }
    }
  }
  qInfo() << "pursuit-canvas-outsidePixels"
          << outsidePixels
          << "cases" << cases;
  QCOMPARE(cases, 64);
  QCOMPARE(outsidePixels, 0);
}

void InputIndicatorGeometryTests::catPursuitCaretTargetFollowsSamePath()
{
  const auto fixedCursor =
      fixedPanelTestPointer();
  InputIndicatorTarget target{
      .point = {
          .x = fixedCursor.x,
          .y = fixedCursor.y,
      },
      .kind = InputIndicatorTargetKind::Pointer,
  };
  MacInputIndicatorPanel panel(
      {},
      [&target] {
        return target;
      }
  );
  const MacInputIndicatorPanelOptions options{
      .source =
          deskflow::input::InputIndicatorSource::LatinABC,
      .companion = InputIndicatorCompanion::Cat,
  };
  QVERIFY(panel.show(options));
  target.kind =
      InputIndicatorTargetKind::Caret;
  target.point.x +=
      target.point.x < 1000 ? 300 : -300;
  QTest::qWait(8);
  const auto caretStep =
      panel.move(options.position);
  QVERIFY(caretStep.visible);
  QCOMPARE(caretStep.frameIntervalMilliseconds, 8);
  QVERIFY(std::isfinite(
      [[catPanelView()
          valueForKey:@"catBodyHeadingRadians"]
          doubleValue]
  ));

  target.kind =
      InputIndicatorTargetKind::Pointer;
  target.point = {
      .x = fixedCursor.x,
      .y = fixedCursor.y,
  };
  QTest::qWait(8);
  const auto pointerStep =
      panel.move(options.position);
  QVERIFY(pointerStep.visible);
  QCOMPARE(pointerStep.frameIntervalMilliseconds, 8);
}

void InputIndicatorGeometryTests::catStartleFurReachesTwoPointFourWithoutChangingTiming()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto state =
      inputIndicatorCatSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  const auto recoil =
      inputIndicatorCatSourceStartleAdvance(
          state, 70, false
      );
  const auto peak =
      inputIndicatorCatSourceStartleAdvance(
          state, 160, false
      );
  const auto ended =
      inputIndicatorCatSourceStartleAdvance(
          state, 300, false
      );

  QCOMPARE(recoil.frame.dorsalLift, 0.0);
  QVERIFY(std::abs(
      peak.frame.dorsalLift - 2.4
  ) < 1e-9);
  QVERIFY(!ended.frame.active);
  QCOMPARE(ended.frame.dorsalLift, 0.0);

  const InputIndicatorPoint dorsal{
      .x = 15.5,
      .y = 12.5,
  };
  const auto peakPoint =
      inputIndicatorCatStartlePoint(
          dorsal, peak.frame, true
      );
  QVERIFY(std::abs(
      peakPoint.y - dorsal.y - 2.4
  ) < 1e-9);
}

void InputIndicatorGeometryTests::catStartleGlowUsesOneBoundedEnvelope()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto state =
      inputIndicatorCatSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  const auto start =
      inputIndicatorCatSourceStartleAdvance(
          state, 0, false
      );
  const auto peak =
      inputIndicatorCatSourceStartleAdvance(
          state, 150, false
      );
  const auto ended =
      inputIndicatorCatSourceStartleAdvance(
          state, 300, false
      );
  QCOMPARE(start.frame.haloScale, 1.0);
  QCOMPARE(start.frame.haloAlpha, 0.0);
  QVERIFY(std::abs(
      peak.frame.haloScale - 1.12
  ) < 1e-9);
  QVERIFY(std::abs(
      peak.frame.haloAlpha - 0.16
  ) < 1e-9);
  QCOMPARE(ended.frame.haloScale, 1.0);
  QCOMPARE(ended.frame.haloAlpha, 0.0);

  for (double age = 0; age <= 300; age += 1) {
    const auto step =
        inputIndicatorCatSourceStartleAdvance(
            state, age, false
        );
    QVERIFY(std::isfinite(step.frame.haloScale));
    QVERIFY(std::isfinite(step.frame.haloAlpha));
    QVERIFY(step.frame.haloScale >= 1.0);
    QVERIFY(step.frame.haloScale <= 1.12);
    QVERIFY(step.frame.haloAlpha >= 0.0);
    QVERIFY(step.frame.haloAlpha <= 0.16);
  }
}

void InputIndicatorGeometryTests::catStartleLatestSourceSelectsGlowColor()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  auto state =
      inputIndicatorCatSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  state =
      inputIndicatorCatSourceStartleTrigger(
          state,
          Source::Korean2Set,
          Source::LatinABC,
          20,
          false
      );
  const auto step =
      inputIndicatorCatSourceStartleAdvance(
          state, 50, false
      );
  const auto colored =
      inputIndicatorCatSourceStartleAdvance(
          step.state, 100, false
      );
  QCOMPARE(
      step.state.latestSource,
      Source::LatinABC
  );
  const auto selected =
      macInputIndicatorGlyphColor(
          step.state.latestSource,
          InputIndicatorTone::Automatic
      );
  const auto latin =
      macInputIndicatorGlyphColor(
          Source::LatinABC,
          InputIndicatorTone::Automatic
      );
  const auto korean =
      macInputIndicatorGlyphColor(
          Source::Korean2Set,
          InputIndicatorTone::Automatic
      );
  QCOMPARE(selected.red, latin.red);
  QCOMPARE(selected.green, latin.green);
  QCOMPARE(selected.blue, latin.blue);
  QVERIFY(selected.blue > selected.red);
  QVERIFY(korean.red > korean.blue);
  QVERIFY(colored.frame.haloAlpha > 0.0);
}

void InputIndicatorGeometryTests::catStartleFurAndGlowRespectReducedMotion()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto state =
      inputIndicatorCatSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  const auto reduced =
      inputIndicatorCatSourceStartleAdvance(
          state, 160, true
      );
  QVERIFY(!reduced.frame.active);
  QCOMPARE(reduced.frame.dorsalLift, 0.0);
  QCOMPARE(reduced.frame.haloScale, 1.0);
  QCOMPARE(reduced.frame.haloAlpha, 0.0);
  QVERIFY(!reduced.state.active);
  QVERIFY(!reduced.state.pending);
}

void InputIndicatorGeometryTests::catStartleFurIsVisiblyStrongerThanOldPeak()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto pose =
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0, false
      );
  const auto artwork =
      macInputIndicatorCatArtwork(pose.artworkFrame);
  const auto state =
      inputIndicatorCatSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  const auto peak =
      inputIndicatorCatSourceStartleAdvance(
          state, 160, false
      );
  auto oldPeak = peak.frame;
  oldPeak.dorsalLift = 1.8;
  const InputIndicatorCatIdleTailFrame restTail;

  for (const int scale : {1, 2}) {
    NSBitmapImageRep *baseline =
        catBodyBitmap(
            pose,
            artwork,
            restTail,
            true,
            scale,
            4,
            false,
            {},
            0,
            true
        );
    NSBitmapImageRep *oldFur =
        catBodyBitmap(
            pose,
            artwork,
            restTail,
            true,
            scale,
            4,
            false,
            oldPeak,
            0,
            true
        );
    NSBitmapImageRep *newFur =
        catBodyBitmap(
            pose,
            artwork,
            restTail,
            true,
            scale,
            4,
            false,
            peak.frame,
            0,
            true
        );
    QVERIFY(baseline);
    QVERIFY(oldFur);
    QVERIFY(newFur);
    const int oldChanged =
        bitmapChangedPixels(baseline, oldFur);
    const int newChanged =
        bitmapChangedPixels(baseline, newFur);
    const int additional =
        bitmapChangedPixels(oldFur, newFur);
    qInfo() << "cat-fur-pixels"
            << "scale" << scale
            << "old" << oldChanged
            << "new" << newChanged
            << "additional" << additional;
    QVERIFY(oldChanged > 0);
    QVERIFY(newChanged > oldChanged);
    QVERIFY(additional > 0);
    [baseline release];
    [oldFur release];
    [newFur release];
  }
}

void InputIndicatorGeometryTests::catStartleGlowIsSingleColoredAndBehindBody()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto fixedCursor =
      fixedPanelTestPointer();
  const InputIndicatorPoint fixedPointer{
      .x = fixedCursor.x,
      .y = fixedCursor.y,
  };
  const struct
  {
    Source from;
    Source to;
    bool redDominant;
  } transitions[] = {
      {Source::LatinABC, Source::Korean2Set, true},
      {Source::Korean2Set, Source::LatinABC, false},
  };

  for (const auto transition : transitions) {
    InputIndicatorTarget target{
        .point = fixedPointer,
        .kind = InputIndicatorTargetKind::Pointer,
    };
    MacInputIndicatorPanel panel(
        {},
        [&target] {
          return target;
        }
    );
    MacInputIndicatorPanelOptions options{
        .source = transition.from,
        .companion = InputIndicatorCompanion::Cat,
    };
    QVERIFY(panel.show(options));
    NSView *view = catPanelView();
    QVERIFY(view);
    options.source = transition.to;
    QVERIFY(panel.show(options));
    QVERIFY([view respondsToSelector:
                     NSSelectorFromString(
                         @"catStartleHaloScale"
                     )]);
    QVERIFY([view respondsToSelector:
                     NSSelectorFromString(
                         @"catStartleHaloAlpha"
                     )]);

    const auto countHaloPixels =
        [&](double waitMilliseconds) -> std::pair<int, int> {
          QTest::qWait(
              static_cast<int>(waitMilliseconds)
          );
          const auto result =
              panel.move(options.position);
          if (!result.visible) {
            return {-1, -1};
          }
          [view displayIfNeeded];
          const InputIndicatorCatStartleFrame frame{
              .active =
                  [[view valueForKey:@"catStartleActive"]
                      boolValue],
              .progress =
                  [[view valueForKey:@"catStartleProgress"]
                      doubleValue],
              .recoilOffsetX =
                  [[view valueForKey:@"catStartleRecoilOffsetX"]
                      doubleValue],
              .horizontalScale =
                  [[view valueForKey:@"catStartleHorizontalScale"]
                      doubleValue],
              .dorsalLift =
                  [[view valueForKey:@"catStartleDorsalLift"]
                      doubleValue],
              .haloScale =
                  [[view valueForKey:@"catStartleHaloScale"]
                      doubleValue],
              .haloAlpha =
                  [[view valueForKey:@"catStartleHaloAlpha"]
                      doubleValue],
          };
          const InputIndicatorCatIdleTailFrame tail{
              .motion = static_cast<
                  InputIndicatorCatIdleTailMotion>(
                  [[view valueForKey:@"catIdleTailMotion"]
                      integerValue]
              ),
              .progress =
                  [[view valueForKey:@"catIdleTailProgress"]
                      doubleValue],
              .active =
                  [[view valueForKey:@"catIdleTailActive"]
                      boolValue],
          };
          const bool facesRight =
              [[view valueForKey:@"facesRight"]
                  boolValue];
          const double facingScale =
              [[view valueForKey:@"facingScaleX"]
                  doubleValue];
          const double heading =
              [[view valueForKey:@"catBodyHeadingRadians"]
                  doubleValue];
          const auto pose =
              inputIndicatorCatPose(
                  InputIndicatorMotionState::Idle,
                  0,
                  false
              );
          const auto artwork =
              macInputIndicatorCatArtwork(
                  pose.artworkFrame
              );
          constexpr int scale = 2;
          NSBitmapImageRep *body =
              catBodyBitmap(
                  pose,
                  artwork,
                  tail,
                  facesRight,
                  scale,
                  0,
                  true,
                  frame,
                  heading,
                  true,
                  facingScale
              );
          NSBitmapImageRep *actual =
              [view bitmapImageRepForCachingDisplayInRect:
                        view.bounds];
          if (!body || !actual) {
            if (body) {
              [body release];
            }
            return {-1, -1};
          }
          [view cacheDisplayInRect:view.bounds
                 toBitmapImageRep:actual];
          int pixels = 0;
          int colorMatches = 0;
          for (NSInteger y = 0;
               y < actual.pixelsHigh;
               ++y) {
            for (NSInteger x = 0;
                 x < actual.pixelsWide;
                 ++x) {
              const NSColor *color =
                  [actual colorAtX:x y:y];
              const double actualAlpha =
                  color.alphaComponent;
              const double bodyAlpha =
                  [body colorAtX:x y:y]
                      .alphaComponent;
              if (actualAlpha <= 0.02 ||
                  bodyAlpha > 0.02) {
                continue;
              }
              ++pixels;
              if ((transition.redDominant &&
                   color.redComponent >
                       color.blueComponent) ||
                  (!transition.redDominant &&
                   color.blueComponent >
                       color.redComponent)) {
                ++colorMatches;
              }
            }
          }
          [body release];
          return std::pair<int, int>{
              pixels, colorMatches
          };
        };

    const auto atStart =
        countHaloPixels(0);
    QVERIFY(atStart.first >= 0);
    const auto atPeak =
        countHaloPixels(160);
    QVERIFY(atPeak.first >= 0);
    QVERIFY(atPeak.first > 0);
    QVERIFY(atPeak.second >= atPeak.first / 2);
    QVERIFY(
        [[view valueForKey:@"catStartleHaloScale"]
            doubleValue] <= 1.12
    );
    QVERIFY(
        [[view valueForKey:@"catStartleHaloAlpha"]
            doubleValue] <= 0.16
    );
    const auto atEnd =
        countHaloPixels(160);
    QVERIFY(atEnd.first >= 0);
    QCOMPARE(atEnd.first, 0);
    panel.hide();
  }
}

void InputIndicatorGeometryTests::catStartleFurAndGlowStayInsideRotatingCanvas()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const auto pose =
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0, false
      );
  const auto artwork =
      macInputIndicatorCatArtwork(pose.artworkFrame);
  const InputIndicatorCatIdleTailFrame tail{
      .motion =
          InputIndicatorCatIdleTailMotion::QuestionLift,
      .progress = 0.5,
      .active = true,
  };
  const auto state =
      inputIndicatorCatSourceStartleTrigger(
          {},
          Source::LatinABC,
          Source::Korean2Set,
          0,
          false
      );
  const auto peak =
      inputIndicatorCatSourceStartleAdvance(
          state, 160, false
      );
  const Source sources[] = {
      Source::LatinABC,
      Source::Korean2Set,
  };
  const bool facings[] = {true, false};
  const double headings[] = {
      0,
      M_PI / 4,
      M_PI / 2,
      3 * M_PI / 4,
      M_PI,
      5 * M_PI / 4,
      3 * M_PI / 2,
      7 * M_PI / 4,
  };
  constexpr int paddingPoints = 4;
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  int cases = 0;
  int outsidePixels = 0;
  int paintOutsidePixels = 0;
  int glyphOutsidePixels = 0;
  int haloOnlyPixels = 0;
  int haloCorePixels = 0;
  int haloColorPixels = 0;
  double haloMinimumAlpha = 1;
  double haloMaximumAlpha = 0;

  QVERIFY(std::abs(
      peak.frame.dorsalLift - 2.4
  ) < 1e-9);
  QVERIFY(peak.frame.haloScale > 1.0);
  QVERIFY(peak.frame.haloScale <= 1.12);
  QVERIFY(peak.frame.haloAlpha > 0.0);
  QVERIFY(peak.frame.haloAlpha <= 0.16);

  for (const int scale : {1, 2}) {
    const NSInteger canvasLeft =
        paddingPoints * scale;
    const NSInteger canvasBottom =
        paddingPoints * scale;
    const NSInteger canvasRight =
        canvasLeft +
        static_cast<NSInteger>(
            std::lround(style.canvasWidth)
        ) *
            scale;
    const NSInteger canvasTop =
        canvasBottom +
        static_cast<NSInteger>(
            std::lround(style.canvasHeight)
        ) *
            scale;

    for (const auto source : sources) {
      const auto sourceColor =
          macInputIndicatorGlyphColor(
              source,
              InputIndicatorTone::Automatic
          );
      const double sourceColorSum =
          sourceColor.red +
          sourceColor.green +
          sourceColor.blue;
      for (const bool facesRight : facings) {
        for (const double heading : headings) {
          NSBitmapImageRep *body =
              catBodyBitmap(
                  pose,
                  artwork,
                  tail,
                  facesRight,
                  scale,
                  paddingPoints,
                  true,
                  peak.frame,
                  heading,
                  true,
                  1,
                  false,
                  source
              );
          NSBitmapImageRep *paint =
              catBodyBitmap(
                  pose,
                  artwork,
                  tail,
                  facesRight,
                  scale,
                  paddingPoints,
                  true,
                  peak.frame,
                  heading,
                  true,
                  1,
                  true,
                  source
              );
          NSBitmapImageRep *glyph =
              catGlyphBitmap(
                  pose,
                  InputIndicatorMotionState::Idle,
                  facesRight,
                  scale,
                  paddingPoints,
                  source,
                  heading,
                  true
              );
          QVERIFY(body);
          QVERIFY(paint);
          QVERIFY(glyph);
          int caseOutsidePixels = 0;
          int caseHaloOnlyPixels = 0;

          for (NSInteger y = 0;
               y < paint.pixelsHigh;
               ++y) {
            for (NSInteger x = 0;
                 x < paint.pixelsWide;
                 ++x) {
              const NSColor *color =
                  [paint colorAtX:x y:y];
              const double paintAlpha =
                  color.alphaComponent;
              const double bodyAlpha =
                  [body colorAtX:x y:y]
                      .alphaComponent;
              const double glyphAlpha =
                  [glyph colorAtX:x y:y]
                      .alphaComponent;
              if (paintAlpha > 0.02 &&
                  (x < canvasLeft ||
                   x >= canvasRight ||
                   y < canvasBottom ||
                   y >= canvasTop)) {
                ++caseOutsidePixels;
                ++paintOutsidePixels;
              }
              if (glyphAlpha > 0.02 &&
                  bodyAlpha <= 0.02) {
                ++caseOutsidePixels;
                ++glyphOutsidePixels;
              }
              if (paintAlpha <= 0.02 ||
                  bodyAlpha > 0) {
                continue;
              }
              ++caseHaloOnlyPixels;
              ++haloOnlyPixels;
              haloMinimumAlpha =
                  std::min(
                      haloMinimumAlpha,
                      paintAlpha
                  );
              haloMaximumAlpha =
                  std::max(
                      haloMaximumAlpha,
                      paintAlpha
                  );
              const double paintColorSum =
                  color.redComponent +
                  color.greenComponent +
                  color.blueComponent;
              if (paintAlpha <= 0.1) {
                continue;
              }
              ++haloCorePixels;
              if (paintColorSum > 0 &&
                  std::abs(
                      color.redComponent /
                          paintColorSum -
                      sourceColor.red /
                          sourceColorSum
                  ) <= 0.03 &&
                  std::abs(
                      color.greenComponent /
                          paintColorSum -
                      sourceColor.green /
                          sourceColorSum
                  ) <= 0.03 &&
                  std::abs(
                      color.blueComponent /
                          paintColorSum -
                      sourceColor.blue /
                          sourceColorSum
                  ) <= 0.03) {
                ++haloColorPixels;
              }
            }
          }
          outsidePixels += caseOutsidePixels;
          QCOMPARE(caseOutsidePixels, 0);
          QVERIFY(caseHaloOnlyPixels > 0);
          [body release];
          [paint release];
          [glyph release];
          ++cases;
        }
      }
    }
  }

  qInfo() << "startle-rotating-canvas"
          << "outsidePixels" << outsidePixels
          << "paintOutside" << paintOutsidePixels
          << "glyphOutside" << glyphOutsidePixels
          << "haloOnlyPixels" << haloOnlyPixels
          << "haloCorePixels" << haloCorePixels
          << "haloColorPixels" << haloColorPixels
          << "haloFrameAlpha"
          << peak.frame.haloAlpha
          << "haloEdgeAlphaRange"
          << haloMinimumAlpha
          << haloMaximumAlpha
          << "cases" << cases;
  QCOMPARE(cases, 64);
  QCOMPARE(paintOutsidePixels, 0);
  QCOMPARE(glyphOutsidePixels, 0);
  QCOMPARE(outsidePixels, 0);
  QVERIFY(haloCorePixels > 0);
  QCOMPARE(haloColorPixels, haloCorePixels);
  QVERIFY(haloMinimumAlpha < 0.05);
  const double maximumCompositedAlpha =
      1 - std::pow(
          1 - peak.frame.haloAlpha, 2
      );
  QVERIFY(
      haloMaximumAlpha <=
      maximumCompositedAlpha + 0.002
  );
}

void InputIndicatorGeometryTests::companionSettingDefaultsToCat()
{
  QCOMPARE(
      inputIndicatorCompanionFromSetting(QStringLiteral("cat")),
      InputIndicatorCompanion::Cat
  );
  QCOMPARE(
      inputIndicatorCompanionFromSetting(QStringLiteral("dog")),
      InputIndicatorCompanion::Dog
  );
  QCOMPARE(
      inputIndicatorCompanionFromSetting(QStringLiteral("turtle")),
      InputIndicatorCompanion::Turtle
  );
  QCOMPARE(
      inputIndicatorCompanionFromSetting(QStringLiteral("other")),
      InputIndicatorCompanion::Cat
  );
}

void InputIndicatorGeometryTests::motionOffsetClampsAndRespectsReducedMotion()
{
  const auto shortMotion = inputIndicatorMotionOffset(
      {.x = 10, .y = 10}, {.x = 13, .y = 10}, 8, false
  );
  QVERIFY(shortMotion);
  QCOMPARE(shortMotion->x, 3.0);
  QCOMPARE(shortMotion->y, 0.0);

  const auto longMotion = inputIndicatorMotionOffset(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 8, false
  );
  QVERIFY(longMotion);
  QCOMPARE(longMotion->x, 8.0);
  QCOMPARE(longMotion->y, 0.0);

  const auto reduced = inputIndicatorMotionOffset(
      {.x = 0, .y = 0}, {.x = 100, .y = 100}, 8, true
  );
  QVERIFY(reduced);
  QCOMPARE(reduced->x, 0.0);
  QCOMPARE(reduced->y, 0.0);
}

void InputIndicatorGeometryTests::transitionProgressEasesAndRespectsReducedMotion()
{
  QCOMPARE(inputIndicatorTransitionProgress(0, 200, false), 0.0);
  QCOMPARE(inputIndicatorTransitionProgress(100, 200, false), 0.5);
  QCOMPARE(inputIndicatorTransitionProgress(200, 200, false), 1.0);
  QCOMPARE(inputIndicatorTransitionProgress(100, 200, true), 1.0);
}

void InputIndicatorGeometryTests::companionMotionStateClassifiesIdleWalkRun()
{
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Idle, 0, 0, 0, 180, false
      ),
      InputIndicatorMotionState::Idle
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Idle, 30, 0, 0, 0, false
      ),
      InputIndicatorMotionState::Walk
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Idle, 0, 3, 0, 0, false
      ),
      InputIndicatorMotionState::Walk
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 650, 0, 0, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 0, 48, 0, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 0, 0, 24, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 900, 0, 0, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 900, 48, 24, 0, true
      ),
      InputIndicatorMotionState::Idle
  );
}

void InputIndicatorGeometryTests::companionMotionStateKeepsMovingForSettleDelay()
{
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 0, 0, 0, 179, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 0, 0, 0, 180, false
      ),
      InputIndicatorMotionState::Idle
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 0, 0, 0, 179, false
      ),
      InputIndicatorMotionState::Walk
  );
}

void InputIndicatorGeometryTests::companionMotionPhaseWrapsPerState()
{
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Idle, 0
      ),
      0.0
  );
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Idle, 600
      ),
      0.5
  );
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Idle, 1200
      ),
      0.0
  );
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Walk, 125
      ),
      0.5
  );
  QVERIFY(std::abs(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Run,
          1000.0 / 14.0
      ) - 0.5
  ) < 1e-9);
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Run,
          std::numeric_limits<double>::quiet_NaN()
      ),
      0.0
  );
}

void InputIndicatorGeometryTests::companionMotionPoseHonorsBounds()
{
  const auto idle = inputIndicatorMotionPose(
      InputIndicatorMotionState::Idle, 0.25, false
  );
  QCOMPARE(idle.bodyBob, 0.0);
  QCOMPARE(idle.bodyStretch, 0.0);
  QCOMPARE(idle.tailSway, 2.0);

  const auto walk = inputIndicatorMotionPose(
      InputIndicatorMotionState::Walk, 0.25, false
  );
  QCOMPARE(walk.bodyBob, 0.45);
  QCOMPARE(walk.bodyStretch, 0.025);
  QCOMPARE(walk.tailSway, 0.0);

  const auto run = inputIndicatorMotionPose(
      InputIndicatorMotionState::Run, 0.25, false
  );
  QCOMPARE(run.bodyBob, 0.9);
  QCOMPARE(run.bodyStretch, 0.07);
  QCOMPARE(run.tailSway, 0.0);

  const auto reduced = inputIndicatorMotionPose(
      InputIndicatorMotionState::Run, 0.25, true
  );
  QCOMPARE(reduced.bodyBob, 0.0);
  QCOMPARE(reduced.bodyStretch, 0.0);
  QCOMPARE(reduced.tailSway, 0.0);
}

void InputIndicatorGeometryTests::companionMotionFrameIntervalsStayLowDuty()
{
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Idle, false
      ),
      75
  );
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Walk, false
      ),
      8
  );
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Run, false
      ),
      8
  );
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Idle, true
      ),
      0
  );
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Run, true
      ),
      0
  );
}

void InputIndicatorGeometryTests::followerCatchesUpWithoutOvershoot()
{
  const auto first = followInputIndicator(
      {.x = 0, .y = 0}, {.x = 10, .y = 0}, 0.5, 18
  );
  QVERIFY(first);
  QCOMPARE(first->x, 5.0);
  QCOMPARE(first->y, 0.0);

  auto current = *first;
  current = *followInputIndicator(
      current, {.x = 10, .y = 0}, 0.5, 18
  );
  QCOMPARE(current.x, 7.5);
  current = *followInputIndicator(
      current, {.x = 10, .y = 0}, 0.5, 18
  );
  QCOMPARE(current.x, 8.75);
  QVERIFY(current.x <= 10.0);

  const auto approachingFromAhead = followInputIndicator(
      {.x = 12, .y = 0}, {.x = 10, .y = 0}, 0.5, 18
  );
  QVERIFY(approachingFromAhead);
  QCOMPARE(approachingFromAhead->x, 11.0);
  QVERIFY(approachingFromAhead->x >= 10.0);
}

void InputIndicatorGeometryTests::followerCapsTrailingLag()
{
  const auto result = followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 0.5, 18
  );

  QVERIFY(result);
  QCOMPARE(result->x, 82.0);
  QCOMPARE(result->y, 0.0);
}

void InputIndicatorGeometryTests::followerRejectsInvalidInput()
{
  const auto nan = std::numeric_limits<double>::quiet_NaN();

  QVERIFY(!followInputIndicator(
      {.x = nan, .y = 0}, {.x = 10, .y = 0}, 0.5, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 10, .y = nan}, 0.5, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 0, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, -0.1, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 1.1, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 0.5, -1
  ));
}

QTEST_MAIN(InputIndicatorGeometryTests)
