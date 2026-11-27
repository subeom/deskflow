/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacInputIndicatorPanel.h"
#include "MacInputIndicatorCatArtwork.h"
#include "MacInputIndicatorCompanionArtwork.h"

#include "OSXHelpers.h"

#include <AppKit/AppKit.h>
#include <CoreText/CoreText.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

namespace deskflow::gui::input {

std::optional<MacInputIndicatorCoreTextLabelMeasurement>
macInputIndicatorCoreTextLabelMeasurement(
    NSString *label,
    NSFont *font
)
{
  if (!label || !font) {
    return std::nullopt;
  }
  NSAttributedString *attributed =
      [[NSAttributedString alloc]
          initWithString:label
              attributes:@{NSFontAttributeName : font}];
  CTLineRef line =
      CTLineCreateWithAttributedString(
          (__bridge CFAttributedStringRef)attributed
      );
  [attributed release];
  if (!line) {
    return std::nullopt;
  }

  const CGRect ink =
      CTLineGetBoundsWithOptions(
          line, kCTLineBoundsUseGlyphPathBounds
      );
  CGFloat ascent = 0;
  CGFloat descent = 0;
  CGFloat leading = 0;
  const double width =
      CTLineGetTypographicBounds(
          line, &ascent, &descent, &leading
      );
  CFRelease(line);
  if (!std::isfinite(ink.origin.x) ||
      !std::isfinite(ink.origin.y) ||
      !std::isfinite(ink.size.width) ||
      !std::isfinite(ink.size.height) ||
      ink.size.width <= 0 || ink.size.height <= 0 ||
      !std::isfinite(width) || width <= 0 ||
      !std::isfinite(ascent) ||
      !std::isfinite(descent)) {
    return std::nullopt;
  }
  return MacInputIndicatorCoreTextLabelMeasurement{
      .ink = {
          .origin = {
              .x = ink.origin.x,
              .y = ink.origin.y,
          },
          .size = {
              .width = ink.size.width,
              .height = ink.size.height,
          },
      },
      .typographicBounds = {
          .x = 0,
          .y = -descent,
          .width = width,
          .height = ascent + descent,
      },
  };
}

void macInputIndicatorDrawCoreTextLabel(
    CGContextRef context,
    NSString *label,
    NSFont *font,
    NSColor *color,
    const MacInputIndicatorLabelLayout &layout
)
{
  if (!context || !label || !font || !color) {
    return;
  }
  NSAttributedString *attributed =
      [[NSAttributedString alloc]
          initWithString:label
              attributes:@{
                NSFontAttributeName : font,
                NSForegroundColorAttributeName : color,
              }];
  CTLineRef line =
      CTLineCreateWithAttributedString(
          (__bridge CFAttributedStringRef)attributed
      );
  [attributed release];
  if (!line) {
    return;
  }
  CGContextSaveGState(context);
  CGContextSetTextMatrix(
      context, CGAffineTransformIdentity
  );
  CGContextSetTextPosition(
      context,
      layout.drawingOrigin.x,
      layout.drawingOrigin.y
  );
  CTLineDraw(line, context);
  CGContextRestoreGState(context);
  CFRelease(line);
}

namespace {

NSString *indicatorLabel(deskflow::input::InputIndicatorSource source)
{
  switch (source) {
  case deskflow::input::InputIndicatorSource::LatinABC:
    return @"A";
  case deskflow::input::InputIndicatorSource::Korean2Set:
    return @"한";
  case deskflow::input::InputIndicatorSource::Unknown:
    return nil;
  }
}

NSColor *indicatorColor(
    MacInputIndicatorRgb color,
    double opacity
)
{
  return [NSColor colorWithSRGBRed:color.red
                             green:color.green
                              blue:color.blue
                             alpha:opacity];
}

NSColor *indicatorGlyphColor(
    deskflow::input::InputIndicatorSource source,
    InputIndicatorTone tone,
    double opacity
)
{
  return indicatorColor(
      macInputIndicatorGlyphColor(source, tone),
      opacity
  );
}

void fillSilhouette(
    NSBezierPath *path, NSColor *fill, NSColor *edge
)
{
  [edge setStroke];
  [path setLineWidth:0.55];
  [path setLineJoinStyle:NSLineJoinStyleRound];
  [path stroke];
  [fill setFill];
  [path fill];
}

void appendPath(NSBezierPath *silhouette, NSBezierPath *part)
{
  [silhouette appendBezierPath:part];
}

NSBezierPath *companionArtworkPath(
    InputIndicatorCompanion companion,
    std::span<const MacInputIndicatorCatPathCommand> commands,
    InputIndicatorMotionState motionState,
    double motionPhase,
    const InputIndicatorCompanionIdleFrame &idleFrame,
    const InputIndicatorCompanionStartleFrame &startleFrame,
    bool facesRight,
    double facingScaleX,
    double bodyHeadingRadians
)
{
  NSBezierPath *path = [NSBezierPath bezierPath];
  const auto deformed =
      [companion, motionState, motionPhase,
       &idleFrame, &startleFrame, facesRight,
       facingScaleX, bodyHeadingRadians](
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
            companion,
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

void drawCompanionHalo(
    NSBezierPath *silhouette,
    bool reducedMotion,
    const InputIndicatorCompanionStartleFrame &startleFrame,
    deskflow::input::InputIndicatorSource reactionSource,
    InputIndicatorTone tone,
    MacInputIndicatorRenderAlpha alpha
)
{
  if (reducedMotion ||
      !startleFrame.active ||
      startleFrame.haloAlpha <= 0) {
    return;
  }
  NSBezierPath *halo = [silhouette copy];
  NSAffineTransform *haloTransform =
      [NSAffineTransform transform];
  [haloTransform translateXBy:15 yBy:14];
  [haloTransform scaleXBy:startleFrame.haloScale
                    yBy:startleFrame.haloScale];
  [haloTransform translateXBy:-15 yBy:-14];
  [halo transformUsingAffineTransform:haloTransform];
  NSColor *haloColor =
      indicatorGlyphColor(
          reactionSource,
          tone,
          alpha.detail *
              startleFrame.haloAlpha
      );
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

NSBezierPath *catArtworkPath(
    std::span<const MacInputIndicatorCatPathCommand> commands,
    const InputIndicatorCatStartleFrame &startleFrame,
    bool facesRight,
    double facingScaleX,
    double bodyHeadingRadians
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

NSBezierPath *catArtworkPath(
    const InputIndicatorCatPose &pose,
    std::span<const MacInputIndicatorCatPathCommand> commands,
    const InputIndicatorCatIdleTailFrame &tailFrame,
    const InputIndicatorCatStartleFrame &startleFrame,
    bool facesRight,
    double facingScaleX,
    double bodyHeadingRadians
)
{
  NSBezierPath *path = [NSBezierPath bezierPath];
  const auto deformed = [&pose, &tailFrame, &startleFrame, facesRight, facingScaleX,
                         bodyHeadingRadians](double x, double y) {
    const auto facePoint =
        inputIndicatorCatRoundedFacePoint({.x = x, .y = y}, pose.headBounds, pose.innerLanguageBounds);
    const auto torsoPoint = inputIndicatorCatTorsoPoint(pose, facePoint);
    const auto tailPoint = inputIndicatorCatIdleTailPoint(torsoPoint, tailFrame);
    const auto startlePoint = inputIndicatorCatStartlePoint(tailPoint, startleFrame, true);
    return inputIndicatorCatBodyPoint(startlePoint, facesRight, facingScaleX, bodyHeadingRadians);
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

void drawCat(
    NSRect bounds, NSColor *fill, NSColor *edge,
    InputIndicatorMotionState motionState,
    double motionPhase,
    bool reducedMotion,
    bool facesRight,
    double facingScaleX,
    double bodyHeadingRadians,
    const InputIndicatorCatIdleTailFrame &idleTailFrame,
    const InputIndicatorCatStartleFrame &startleFrame,
    deskflow::input::InputIndicatorSource reactionSource,
    InputIndicatorTone tone,
    MacInputIndicatorRenderAlpha alpha
)
{
  (void)bounds;
  const auto pose = inputIndicatorCatPose(
      motionState,
      motionState == InputIndicatorMotionState::Idle
          ? 0
          : motionPhase,
      reducedMotion
  );
  const auto artwork =
      macInputIndicatorCatArtwork(pose.artworkFrame);

  NSBezierPath *far =
      catArtworkPath(
          artwork.far,
          startleFrame,
          facesRight,
          facingScaleX,
          bodyHeadingRadians
      );
  NSBezierPath *near = catArtworkPath(
      pose,
      artwork.near,
      idleTailFrame,
      startleFrame,
      facesRight,
      facingScaleX,
      bodyHeadingRadians
  );
  NSBezierPath *silhouette =
      [NSBezierPath bezierPath];
  appendPath(silhouette, far);
  appendPath(silhouette, near);
  drawCompanionHalo(
      silhouette,
      reducedMotion,
      startleFrame,
      reactionSource,
      tone,
      alpha
  );

  NSColor *opaqueFill = [fill colorWithAlphaComponent:1.0];
  NSColor *opaqueEdge = [edge colorWithAlphaComponent:1.0];
  NSColor *farFill = [opaqueFill blendedColorWithFraction:0.18 ofColor:opaqueEdge];
  CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
  CGContextSaveGState(context);
  CGContextSetAlpha(context, alpha.body);
  CGContextBeginTransparencyLayer(context, nil);
  CGContextSetAlpha(context, 1.0);
  fillSilhouette(far, farFill, opaqueEdge);
  fillSilhouette(near, opaqueFill, opaqueEdge);
  CGContextEndTransparencyLayer(context);
  CGContextRestoreGState(context);
}

void drawDog(
    NSRect bounds, NSColor *fill, NSColor *edge,
    InputIndicatorMotionState motionState,
    double motionPhase,
    bool reducedMotion,
    bool facesRight,
    double facingScaleX,
    double bodyHeadingRadians,
    const InputIndicatorCompanionIdleFrame &idleFrame,
    const InputIndicatorCompanionStartleFrame &startleFrame,
    deskflow::input::InputIndicatorSource reactionSource,
    InputIndicatorTone tone,
    MacInputIndicatorRenderAlpha alpha
)
{
  (void)bounds;
  const auto artwork =
      macInputIndicatorDogArtwork();
  NSBezierPath *silhouette =
      companionArtworkPath(
          InputIndicatorCompanion::Dog,
          artwork.silhouette,
          motionState,
          motionPhase,
          idleFrame,
          startleFrame,
          facesRight,
          facingScaleX,
          bodyHeadingRadians
      );
  drawCompanionHalo(
      silhouette,
      reducedMotion,
      startleFrame,
      reactionSource,
      tone,
      alpha
  );
  fillSilhouette(silhouette, fill, edge);
}

void drawTurtle(
    NSRect bounds, NSColor *fill, NSColor *edge,
    InputIndicatorMotionState motionState,
    double motionPhase,
    bool reducedMotion,
    bool facesRight,
    double facingScaleX,
    double bodyHeadingRadians,
    const InputIndicatorCompanionIdleFrame &idleFrame,
    const InputIndicatorCompanionStartleFrame &startleFrame,
    deskflow::input::InputIndicatorSource reactionSource,
    InputIndicatorTone tone,
    MacInputIndicatorRenderAlpha alpha
)
{
  (void)bounds;
  const auto artwork =
      macInputIndicatorTurtleArtwork();
  NSBezierPath *silhouette =
      companionArtworkPath(
          InputIndicatorCompanion::Turtle,
          artwork.silhouette,
          motionState,
          motionPhase,
          idleFrame,
          startleFrame,
          facesRight,
          facingScaleX,
          bodyHeadingRadians
      );
  drawCompanionHalo(
      silhouette,
      reducedMotion,
      startleFrame,
      reactionSource,
      tone,
      alpha
  );
  fillSilhouette(silhouette, fill, edge);
}

void drawLanguageGlyph(
    InputIndicatorCompanion companion,
    deskflow::input::InputIndicatorSource previousSource,
    deskflow::input::InputIndicatorSource source,
    double transitionProgress,
    InputIndicatorTone tone,
    MacInputIndicatorRenderAlpha alpha,
    InputIndicatorMotionState motionState,
    double motionPhase,
    bool reducedMotion,
    bool facesRight,
    double facingScaleX,
    double bodyHeadingRadians,
    const InputIndicatorCatIdleTailFrame &idleTailFrame,
    const InputIndicatorCatStartleFrame &startleFrame
)
{
  const auto model =
      macInputIndicatorRenderModel(companion);
  auto bounds = model.languageBounds;
  NSBezierPath *silhouette = nil;
  if (companion == InputIndicatorCompanion::Cat) {
    const auto pose = inputIndicatorCatPose(
        motionState,
        motionState == InputIndicatorMotionState::Idle
            ? 0
            : motionPhase,
        reducedMotion
    );
    bounds = pose.innerLanguageBounds;
    const auto artwork =
        macInputIndicatorCatArtwork(pose.artworkFrame);
    silhouette = catArtworkPath(
        pose,
        artwork.near,
        idleTailFrame,
        startleFrame,
        facesRight,
        facingScaleX,
        bodyHeadingRadians
    );
  } else if (
      companion ==
      InputIndicatorCompanion::Dog) {
    const auto artwork =
        macInputIndicatorDogArtwork();
    bounds = artwork.innerLanguageBounds;
    silhouette = companionArtworkPath(
        companion,
        artwork.silhouette,
        motionState,
        motionPhase,
        idleTailFrame,
        startleFrame,
        facesRight,
        facingScaleX,
        bodyHeadingRadians
    );
  } else {
    const auto artwork =
        macInputIndicatorTurtleArtwork();
    bounds = artwork.innerLanguageBounds;
    silhouette = companionArtworkPath(
        companion,
        artwork.silhouette,
        motionState,
        motionPhase,
        idleTailFrame,
        startleFrame,
        facesRight,
        facingScaleX,
        bodyHeadingRadians
    );
  }
  const InputIndicatorPoint labelCenter{
      .x = bounds.x + bounds.width / 2.0,
      .y = bounds.y + bounds.height / 2.0,
  };
  const auto transformedLabelCenter =
      inputIndicatorCompanionBodyPoint(
          companion,
          labelCenter,
          facesRight,
          facingScaleX,
          bodyHeadingRadians
      );
  bounds.x =
      transformedLabelCenter.x -
      bounds.width / 2.0;
  bounds.y =
      transformedLabelCenter.y -
      bounds.height / 2.0;
  const NSRect labelBounds = NSMakeRect(
      bounds.x,
      bounds.y,
      bounds.width,
      bounds.height
  );

  const auto drawLabel =
      [motionState, tone, labelBounds, bounds,
       alpha, silhouette](
          deskflow::input::InputIndicatorSource labelSource,
          double transitionAlpha
      ) {
        NSString *label = indicatorLabel(labelSource);
        if (!label || transitionAlpha <= 0) {
          return;
        }

        const auto preference =
            macInputIndicatorLabelPreference(
                labelSource, motionState
            );
        double pointSize = preference.pointSize;
        std::optional<MacInputIndicatorLabelLayout> layout;
        bool stable = false;
        for (int attempt = 0; attempt < 5; ++attempt) {
          NSFont *font =
              [NSFont systemFontOfSize:pointSize
                                weight:NSFontWeightSemibold];
          const auto measured =
              macInputIndicatorCoreTextLabelMeasurement(
                  label, font
              );
          if (!measured) {
            return;
          }
          layout = macInputIndicatorFitLabelInkLayout(
              pointSize,
              preference.minimumPointSize,
              bounds,
              measured->ink
          );
          if (!layout) {
            return;
          }
          if (std::abs(
                  layout->fontSize - pointSize
              ) < 0.01) {
            stable = true;
            break;
          }
          pointSize = layout->fontSize;
        }
        if (!stable || !layout) {
          return;
        }

        NSFont *font =
            [NSFont systemFontOfSize:layout->fontSize
                              weight:NSFontWeightSemibold];
        NSColor *color =
            indicatorGlyphColor(
                labelSource,
                tone,
                alpha.detail * transitionAlpha
            );
        [NSGraphicsContext saveGraphicsState];
        [silhouette addClip];
        [[NSBezierPath bezierPathWithRect:labelBounds] addClip];
        macInputIndicatorDrawCoreTextLabel(
            [[NSGraphicsContext currentContext] CGContext],
            label,
            font,
            color,
            *layout
        );
        [NSGraphicsContext restoreGraphicsState];
      };
  if (previousSource != source) {
    drawLabel(previousSource, 1.0 - transitionProgress);
  }
  drawLabel(
      source,
      previousSource == source ? 1.0 : transitionProgress
  );
}

} // namespace

} // namespace deskflow::gui::input

@interface DeskflowInputIndicatorPanel : NSPanel
@end

@implementation DeskflowInputIndicatorPanel

- (BOOL)canBecomeKeyWindow
{
  return NO;
}

- (BOOL)canBecomeMainWindow
{
  return NO;
}

@end

@interface DeskflowInputIndicatorView : NSView
@property(nonatomic) NSInteger companion;
@property(nonatomic) NSInteger source;
@property(nonatomic) NSInteger previousSource;
@property(nonatomic) NSInteger tone;
@property(nonatomic) double opacity;
@property(nonatomic) double transitionProgress;
@property(nonatomic) double motionX;
@property(nonatomic) double motionY;
@property(nonatomic) double idleGrowthScale;
@property(nonatomic) NSInteger motionState;
@property(nonatomic) double motionPhase;
@property(nonatomic) BOOL reducedMotion;
@property(nonatomic) BOOL facesRight;
@property(nonatomic) double facingScaleX;
@property(nonatomic) NSInteger catIdleTailMotion;
@property(nonatomic) double catIdleTailProgress;
@property(nonatomic) BOOL catIdleTailActive;
@property(nonatomic) BOOL catStartleActive;
@property(nonatomic) BOOL catStartlePending;
@property(nonatomic) NSInteger catStartleLatestSource;
@property(nonatomic) double catStartleProgress;
@property(nonatomic) double catStartleRecoilOffsetX;
@property(nonatomic) double catStartleHorizontalScale;
@property(nonatomic) double catStartleDorsalLift;
@property(nonatomic) double catStartleHaloScale;
@property(nonatomic) double catStartleHaloAlpha;
@property(nonatomic) double catBodyHeadingRadians;
@end

@implementation DeskflowInputIndicatorView

- (void)drawRect:(NSRect)dirtyRect
{
  (void)dirtyRect;
  const NSRect bounds = self.bounds;
  const auto companion =
      static_cast<deskflow::gui::input::InputIndicatorCompanion>(
          self.companion
      );
  const auto source =
      static_cast<deskflow::input::InputIndicatorSource>(self.source);
  const auto previousSource =
      static_cast<deskflow::input::InputIndicatorSource>(
          self.previousSource
      );
  const auto tone =
      static_cast<deskflow::gui::input::InputIndicatorTone>(self.tone);
  const auto motionState =
      static_cast<deskflow::gui::input::InputIndicatorMotionState>(
          self.motionState
      );
  const deskflow::gui::input::InputIndicatorCatIdleTailFrame
      idleTailFrame{
          .motion = static_cast<
              deskflow::gui::input::
                  InputIndicatorCatIdleTailMotion>(
              self.catIdleTailMotion
          ),
          .progress = self.catIdleTailProgress,
          .active =
              static_cast<bool>(self.catIdleTailActive),
      };
  const deskflow::gui::input::InputIndicatorCatStartleFrame
      startleFrame{
          .active =
              static_cast<bool>(self.catStartleActive),
          .progress = self.catStartleProgress,
          .recoilOffsetX =
              self.catStartleRecoilOffsetX,
          .horizontalScale =
              self.catStartleHorizontalScale,
          .dorsalLift =
              self.catStartleDorsalLift,
          .haloScale =
              self.catStartleHaloScale,
          .haloAlpha =
              self.catStartleHaloAlpha,
      };
  const auto alpha = deskflow::gui::input::
      macInputIndicatorRenderAlpha(
          static_cast<int>(std::lround(self.opacity * 100.0))
      );
  const auto colorModel =
      deskflow::gui::input::macInputIndicatorColorModel(tone);
  NSColor *silhouetteFrom = deskflow::gui::input::indicatorColor(
      deskflow::gui::input::macInputIndicatorBodyColor(
          previousSource, tone
      ),
      alpha.body
  );
  NSColor *silhouetteTo = deskflow::gui::input::indicatorColor(
      deskflow::gui::input::macInputIndicatorBodyColor(
          source, tone
      ),
      alpha.body
  );
  NSColor *silhouetteFill =
      [silhouetteFrom
          blendedColorWithFraction:self.transitionProgress
                           ofColor:silhouetteTo];
  NSColor *silhouetteEdge =
      deskflow::gui::input::indicatorColor(
          colorModel.edge, alpha.body
      );
  [NSGraphicsContext saveGraphicsState];
  CGContextRef context =
      [[NSGraphicsContext currentContext] CGContext];
  CGContextSaveGState(context);
  CGContextClearRect(context, CGRectMake(bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height));
  CGContextRestoreGState(context);
  const double idleScale =
      self.idleGrowthScale >= 1.0
          ? std::min(self.idleGrowthScale, deskflow::gui::input::kInputIndicatorIdleGrowthMaximumScale)
          : 1.0;
  const auto style = deskflow::gui::input::macInputIndicatorPanelStyle(companion, idleScale);
  const auto displayOrigin = deskflow::gui::input::macInputIndicatorDisplayPoint({}, idleScale);
  CGContextTranslateCTM(
      context,
      displayOrigin.x + style.canvasOffsetX,
      displayOrigin.y + style.canvasOffsetY
  );
  CGContextScaleCTM(
      context, style.visualScale, style.visualScale
  );
  switch (companion) {
  case deskflow::gui::input::InputIndicatorCompanion::Cat:
    deskflow::gui::input::drawCat(
        bounds, silhouetteFill, silhouetteEdge,
        motionState,
        self.motionPhase,
        self.reducedMotion,
        self.facesRight,
        self.facingScaleX,
        self.catBodyHeadingRadians,
        idleTailFrame,
        startleFrame,
        static_cast<deskflow::input::InputIndicatorSource>(
            self.catStartleLatestSource
        ),
        tone,
        alpha
    );
    break;
  case deskflow::gui::input::InputIndicatorCompanion::Dog:
    deskflow::gui::input::drawDog(
        bounds, silhouetteFill, silhouetteEdge,
        motionState,
        self.motionPhase,
        self.reducedMotion,
        self.facesRight,
        self.facingScaleX,
        self.catBodyHeadingRadians,
        idleTailFrame,
        startleFrame,
        static_cast<deskflow::input::InputIndicatorSource>(
            self.catStartleLatestSource
        ),
        tone,
        alpha
    );
    break;
  case deskflow::gui::input::InputIndicatorCompanion::Turtle:
    deskflow::gui::input::drawTurtle(
        bounds, silhouetteFill, silhouetteEdge,
        motionState,
        self.motionPhase,
        self.reducedMotion,
        self.facesRight,
        self.facingScaleX,
        self.catBodyHeadingRadians,
        idleTailFrame,
        startleFrame,
        static_cast<deskflow::input::InputIndicatorSource>(
            self.catStartleLatestSource
        ),
        tone,
        alpha
    );
    break;
  }
  deskflow::gui::input::drawLanguageGlyph(
      companion,
      previousSource,
      source,
      self.transitionProgress,
      tone,
      alpha,
      motionState,
      self.motionPhase,
      self.reducedMotion,
      self.facesRight,
      self.facingScaleX,
      self.catBodyHeadingRadians,
      idleTailFrame,
      startleFrame
  );
  [NSGraphicsContext restoreGraphicsState];
}

- (BOOL)isOpaque
{
  return NO;
}

@end

namespace deskflow::gui::input {

namespace {

std::vector<InputIndicatorRect> screenRects()
{
  std::vector<InputIndicatorRect> result;
  for (NSScreen *screen in NSScreen.screens) {
    const auto frame = screen.frame;
    result.push_back({.x = frame.origin.x, .y = frame.origin.y, .width = frame.size.width, .height = frame.size.height});
  }
  return result;
}

constexpr double kSettledDistance = 0.25;

double distance(InputIndicatorPoint point)
{
  return std::hypot(point.x, point.y);
}

} // namespace

struct MacInputIndicatorPanel::Impl
{
  struct Lifetime
  {
    Impl *owner = nullptr;
    bool active = true;
  };

  explicit Impl(
      std::function<void()> invalidated,
      MacInputIndicatorPanel::TargetLocation targetLocation,
      MacInputIndicatorPanel::Clock panelClock
  )
      : presentationInvalidated(std::move(invalidated)),
        targetLocation(
            targetLocation
                ? std::move(targetLocation)
                : [] {
                    const auto cursor =
                        NSEvent.mouseLocation;
                    return InputIndicatorTarget{
                        .point = {
                            .x = cursor.x,
                            .y = cursor.y,
                        },
                        .kind =
                            InputIndicatorTargetKind::Pointer,
                    };
                  }
        ),
        clock(
            panelClock
                ? std::move(panelClock)
                : [] {
                    return std::chrono::steady_clock::now();
                  }
        )
  {
    lifetime = std::make_shared<Lifetime>();
    lifetime->owner = this;
    const auto initialTime = clock();
    idleTailMotionState = inputIndicatorCompanionIdleMotionInitialState(
        static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(initialTime.time_since_epoch()).count()
        )
    );
    animationEpoch = initialTime;
    const auto style =
        macInputIndicatorPanelStyle(InputIndicatorCompanion::Cat);
    panel = [[DeskflowInputIndicatorPanel alloc]
        initWithContentRect:NSMakeRect(
                                0,
                                0,
                                style.canvasWidth,
                                style.canvasHeight
                            )
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:YES];
    [panel setOpaque:NO];
    [panel setReleasedWhenClosed:NO];
    [panel setBackgroundColor:[NSColor clearColor]];
    [panel setIgnoresMouseEvents:YES];
    [panel setHidesOnDeactivate:NO];
    [panel setHasShadow:NO];
    [panel setMovable:NO];
    [panel setBecomesKeyOnlyIfNeeded:NO];
    [panel setLevel:NSStatusWindowLevel + 1];
    [panel setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces |
                                 NSWindowCollectionBehaviorFullScreenAuxiliary |
                                 NSWindowCollectionBehaviorTransient];
    [panel setAnimationBehavior:NSWindowAnimationBehaviorNone];
    [panel setSharingType:NSWindowSharingNone];

    view = [[DeskflowInputIndicatorView alloc]
        initWithFrame:panel.contentView.bounds];
    view.autoresizingMask =
        NSViewWidthSizable | NSViewHeightSizable;
    panel.contentView = view;

    auto *workspace = [NSWorkspace sharedWorkspace];
    std::weak_ptr<Lifetime> weakLifetime = lifetime;
    activeSpaceToken =
        [workspace.notificationCenter
            addObserverForName:NSWorkspaceActiveSpaceDidChangeNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification *) {
                      const auto state = weakLifetime.lock();
                      if (!state || !state->active || !state->owner) {
                        return;
                      }
                      state->owner->activeSpaceChanged = true;
                      state->owner->idleTailPaused = false;
                      const auto now = state->owner->clock();
                      state->owner->motionStateStarted = now;
                      if (state->owner->presentationInvalidated) {
                        state->owner->presentationInvalidated();
                      }
                    }];
  }

  ~Impl()
  {
    lifetime->active = false;
    lifetime->owner = nullptr;
    if (activeSpaceToken) {
      [[[NSWorkspace sharedWorkspace] notificationCenter]
          removeObserver:activeSpaceToken];
      activeSpaceToken = nil;
    }
    [panel orderOut:nil];
    panel.contentView = nil;
    [view release];
    view = nil;
    [panel setReleasedWhenClosed:YES];
    [panel close];
    panel = nil;
  }

  DeskflowInputIndicatorPanel *panel = nil;
  DeskflowInputIndicatorView *view = nil;
  id activeSpaceToken = nil;
  std::shared_ptr<Lifetime> lifetime;
  std::function<void()> presentationInvalidated;
  MacInputIndicatorPanel::TargetLocation targetLocation;
  MacInputIndicatorPanel::Clock clock;
  InputIndicatorCompanion companion =
      InputIndicatorCompanion::Cat;
  deskflow::input::InputIndicatorSource source =
      deskflow::input::InputIndicatorSource::Unknown;
  deskflow::input::InputIndicatorSource previousSource =
      deskflow::input::InputIndicatorSource::Unknown;
  std::chrono::steady_clock::time_point transitionStarted;
  std::chrono::steady_clock::time_point motionStateStarted;
  std::chrono::steady_clock::time_point previousTargetTime;
  std::chrono::steady_clock::time_point lastPointerMovement;
  InputIndicatorIdleGrowthState idleGrowthState;
  bool activeSpaceChanged = true;
  bool followerInitialized = false;
  bool previousTargetInitialized = false;
  InputIndicatorPoint follower;
  InputIndicatorPoint previousTarget;
  InputIndicatorPoint previousCursor;
  bool previousCursorInitialized = false;
  InputIndicatorPoint motion;
  InputIndicatorCompanionPursuitState pursuitState;
  double followSpeedMultiplier = 1;
  bool approachHeadingValid = false;
  double approachHeadingRadians = 0;
  bool facesRight = true;
  bool facingTargetRight = true;
  bool facingTurning = false;
  std::chrono::steady_clock::time_point facingTurnStarted;
  std::chrono::steady_clock::time_point animationEpoch;
  InputIndicatorCompanionSourceStartleState startleState;
  bool idleTailPaused = false;
  double idleTailPauseStartedMilliseconds = 0;
  InputIndicatorMotionState motionState =
      InputIndicatorMotionState::Idle;
  bool previousReducedMotion = false;
  double catLeanHeadingRadians = 0;
  InputIndicatorCompanionIdleMotionState
      idleTailMotionState;
};

MacInputIndicatorPanel::MacInputIndicatorPanel(
    std::function<void()> presentationInvalidated, TargetLocation targetLocation, Clock clock
)
    : m_impl(std::make_unique<Impl>(std::move(presentationInvalidated), std::move(targetLocation), std::move(clock)))
{
}

MacInputIndicatorPanel::~MacInputIndicatorPanel() = default;

bool MacInputIndicatorPanel::show(
    const MacInputIndicatorPanelOptions &options
)
{
  if (!indicatorLabel(options.source)) {
    hide();
    return false;
  }

  const auto previousInputSource =
      m_impl->source;
  const auto now = m_impl->clock();
  const bool companionChanged =
      m_impl->companion != options.companion;
  const bool reducedMotion = isOSXReduceMotion();
  const bool reducedMotionChanged = reducedMotion != m_impl->previousReducedMotion;
  m_impl->previousReducedMotion = reducedMotion;
  const double animationMilliseconds =
      std::chrono::duration<double, std::milli>(
          now - m_impl->animationEpoch
      )
          .count();
  m_impl->startleState = inputIndicatorCompanionSourceStartleTrigger(
      m_impl->startleState, previousInputSource, options.source, animationMilliseconds, reducedMotion
  );
  if (companionChanged) {
    m_impl->idleTailPaused = false;
    m_impl->idleGrowthState = {};
    m_impl->catLeanHeadingRadians = m_impl->facesRight ? 0.0 : M_PI;
  } else if (reducedMotion) {
    m_impl->idleGrowthState = {};
  }
  if (companionChanged &&
      m_impl->followerInitialized &&
      !m_impl->pursuitState.initialized) {
    m_impl->pursuitState = {
        .position = m_impl->follower,
        .velocity = {},
        .headingRadians =
            m_impl->pursuitState.headingRadians,
        .motionState = m_impl->motionState,
        .initialized = true,
    };
  }
  m_impl->companion = options.companion;
  m_impl->previousSource =
      previousInputSource !=
              deskflow::input::InputIndicatorSource::Unknown
          ? previousInputSource
          : options.source;
  m_impl->source = options.source;
  m_impl->transitionStarted = now;
  if (!m_impl->followerInitialized || companionChanged || reducedMotionChanged) {
    m_impl->motionStateStarted = now;
    m_impl->previousTargetTime = now;
    m_impl->lastPointerMovement = now;
  }
  m_impl->activeSpaceChanged = true;
  if (companionChanged) {
    m_impl->motionState =
        InputIndicatorMotionState::Idle;
    m_impl->motion = {};
    m_impl->pursuitState.velocity = {};
    m_impl->pursuitState
        .stoppedMilliseconds = 0;
    m_impl->pursuitState.motionState =
        InputIndicatorMotionState::Idle;
  }
  m_impl->followSpeedMultiplier =
      std::clamp(
          options.followSpeedPercent, 50, 200
      ) /
      100.0;

  const auto style = macInputIndicatorPanelStyle(options.companion, m_impl->idleGrowthState.scale);
  [m_impl->panel setContentSize:NSMakeSize(
                      style.canvasWidth, style.canvasHeight
                  )];
  m_impl->view.companion =
      static_cast<NSInteger>(options.companion);
  m_impl->view.source = static_cast<NSInteger>(options.source);
  m_impl->view.previousSource =
      static_cast<NSInteger>(m_impl->previousSource);
  m_impl->view.tone = static_cast<NSInteger>(options.tone);
  m_impl->view.opacity =
      std::clamp(options.opacityPercent, 60, 100) / 100.0;
  m_impl->view.transitionProgress = 0;
  m_impl->view.motionX = 0;
  m_impl->view.motionY = 0;
  m_impl->view.idleGrowthScale = m_impl->idleGrowthState.scale;
  m_impl->view.facesRight = m_impl->facesRight;
  m_impl->view.facingScaleX = 1;
  m_impl->view.catIdleTailMotion = 0;
  m_impl->view.catIdleTailProgress = 0;
  m_impl->view.catIdleTailActive = NO;
  m_impl->view.catStartleActive = NO;
  m_impl->view.catStartlePending = NO;
  m_impl->view.catStartleLatestSource =
      static_cast<NSInteger>(
          m_impl->startleState.latestSource
      );
  m_impl->view.catStartleProgress = 0;
  m_impl->view.catStartleRecoilOffsetX = 0;
  m_impl->view.catStartleHorizontalScale = 1;
  m_impl->view.catStartleDorsalLift = 0;
  m_impl->view.catStartleHaloScale = 1;
  m_impl->view.catStartleHaloAlpha = 0;
  m_impl->view.catBodyHeadingRadians = 0;
  [m_impl->view setNeedsDisplay:YES];
  return move(options.position).visible;
}

void MacInputIndicatorPanel::setFollowSpeedPercent(
    int percent
)
{
  m_impl->followSpeedMultiplier =
      std::clamp(percent, 50, 200) / 100.0;
}

MacInputIndicatorPanelMoveResult
MacInputIndicatorPanel::move(InputIndicatorPosition position)
{
  const auto now = m_impl->clock();
  const bool reducedMotion = isOSXReduceMotion();
  if (reducedMotion != m_impl->previousReducedMotion) {
    m_impl->previousReducedMotion = reducedMotion;
    m_impl->lastPointerMovement = now;
    if (reducedMotion) {
      m_impl->idleGrowthState = {};
    }
  }
  const auto style = macInputIndicatorPanelStyle(m_impl->companion, m_impl->idleGrowthState.scale);
  const auto targetLocation =
      m_impl->targetLocation();
  const InputIndicatorPoint cursorDelta{
      .x =
          targetLocation.point.x -
          m_impl->previousCursor.x,
      .y =
          targetLocation.point.y -
          m_impl->previousCursor.y,
  };
  const bool cursorMoved =
      m_impl->previousCursorInitialized &&
      distance(cursorDelta) >
          kSettledDistance;
  std::optional<double> approachHeading;
  if (position ==
          InputIndicatorPosition::
              CursorFollowApproach &&
      m_impl->approachHeadingValid &&
      std::isfinite(
          m_impl->approachHeadingRadians
      )) {
    approachHeading =
        m_impl->approachHeadingRadians;
  }
  const auto target = positionInputIndicator(
      targetLocation.point,
      {.width = style.width, .height = style.height},
      screenRects(),
      position,
      style.cursorGap,
      style.screenMargin,
      approachHeading
  );
  if (!target) {
    hide();
    return {};
  }

  const auto transitionElapsed =
      std::chrono::duration<double, std::milli>(
          now - m_impl->transitionStarted
      )
          .count();
  const auto transitionProgress =
      inputIndicatorTransitionProgress(
          transitionElapsed,
          style.transitionMilliseconds,
          reducedMotion ||
              m_impl->previousSource == m_impl->source
      );

  double pointerSpeed = 0;
  double pointerDistance = 0;
  double elapsedMilliseconds = 8;
  if (m_impl->previousTargetInitialized) {
    elapsedMilliseconds =
        std::chrono::duration<double, std::milli>(
            now - m_impl->previousTargetTime
        )
            .count();
    const auto targetDelta = InputIndicatorPoint{
        .x = target->x - m_impl->previousTarget.x,
        .y = target->y - m_impl->previousTarget.y,
    };
    pointerDistance = distance(targetDelta);
    if (elapsedMilliseconds > 0 &&
        std::isfinite(elapsedMilliseconds)) {
      pointerSpeed =
          pointerDistance * 1000.0 / elapsedMilliseconds;
    }
  }
  if (cursorMoved) {
    m_impl->lastPointerMovement = now;
  } else {
    pointerSpeed = 0;
  }
  const double idleMilliseconds = std::chrono::duration<double, std::milli>(now - m_impl->lastPointerMovement).count();
  const double animationMillisecondsAtStart =
      std::chrono::duration<double, std::milli>(now - m_impl->animationEpoch).count();
  const auto idleGrowthStep = inputIndicatorIdleGrowthAdvance(
      m_impl->idleGrowthState, m_impl->companion, idleMilliseconds, animationMillisecondsAtStart, cursorMoved,
      reducedMotion
  );
  const double idleScale = idleGrowthStep.scale;
  const bool idleScaleChanged = std::abs(idleScale - m_impl->idleGrowthState.scale) > 0.0001;
  m_impl->idleGrowthState = idleGrowthStep.state;
  const auto renderedStyle = macInputIndicatorPanelStyle(m_impl->companion, idleScale);
  const auto renderedTarget =
      idleScaleChanged
          ? positionInputIndicator(
                targetLocation.point, {.width = renderedStyle.width, .height = renderedStyle.height}, screenRects(),
                position, renderedStyle.cursorGap, renderedStyle.screenMargin, approachHeading
            )
          : target;
  if (!renderedTarget) {
    hide();
    return {};
  }
  if (!m_impl->pursuitState.initialized &&
      m_impl->followerInitialized) {
    m_impl->pursuitState.position =
        m_impl->follower;
    m_impl->pursuitState.initialized = true;
  }
  const bool hadPursuitPosition =
        m_impl->pursuitState.initialized;
  const auto previousPursuitPosition =
        m_impl->pursuitState.position;
  const bool initializePursuit =
      !m_impl->pursuitState.initialized;
  const auto pursuit =
      inputIndicatorCompanionPursuitAdvance(
          m_impl->pursuitState,
          {
              .point = *target,
              .kind = targetLocation.kind,
          },
          pointerSpeed,
          elapsedMilliseconds,
          initializePursuit,
          reducedMotion,
          m_impl->followSpeedMultiplier
      );
  if (!pursuit) {
    hide();
    return {};
  }
  auto nextPursuitState = pursuit->state;
  const bool scaleOnlyPursuit = !cursorMoved && idleScaleChanged && !initializePursuit;
  if (idleScaleChanged) {
    nextPursuitState.position.x += renderedTarget->x - target->x;
    nextPursuitState.position.y += renderedTarget->y - target->y;
  }
  if (scaleOnlyPursuit) {
    nextPursuitState.velocity = {};
    nextPursuitState.headingRadians = m_impl->pursuitState.headingRadians;
    nextPursuitState.motionState = InputIndicatorMotionState::Idle;
    nextPursuitState.stoppedMilliseconds = 180;
  }
  if (!initializePursuit &&
      !reducedMotion &&
      position ==
          InputIndicatorPosition::
              CursorFollowApproach &&
      !pursuit->reached &&
      pursuit->distanceMoved >
          kSettledDistance &&
      hadPursuitPosition) {
    const InputIndicatorPoint
        previousPursuitCenter{
            .x = previousPursuitPosition.x +
                style.width / 2.0,
            .y = previousPursuitPosition.y +
                style.height / 2.0,
        };
    const InputIndicatorPoint approach{
        .x =
            targetLocation.point.x -
            previousPursuitCenter.x,
        .y =
            targetLocation.point.y -
            previousPursuitCenter.y,
    };
    const double approachDistance =
        distance(approach);
    if (approachDistance >
            kSettledDistance &&
        std::isfinite(approachDistance)) {
      std::optional<double> heading;
      if (cursorMoved) {
        heading = std::atan2(
            cursorDelta.y,
            cursorDelta.x
        );
      } else if (
          !m_impl->approachHeadingValid) {
        heading = std::atan2(
            approach.y,
            approach.x
        );
      }
      if (heading &&
          (!m_impl->approachHeadingValid ||
           std::abs(
               inputIndicatorNormalizedAngle(
                   *heading -
                   m_impl->approachHeadingRadians
               )
           ) > 0.01)) {
        m_impl->approachHeadingRadians =
            *heading;
        m_impl->approachHeadingValid = true;
      }
    }
  }
  m_impl->pursuitState =
      nextPursuitState;
  m_impl->follower =
      m_impl->pursuitState.position;
  m_impl->followerInitialized = true;
  m_impl->motion = reducedMotion || scaleOnlyPursuit ? InputIndicatorPoint{} : pursuit->movement;
  const auto nextMotionState =
      m_impl->pursuitState.motionState;
  if (nextMotionState != m_impl->motionState) {
    m_impl->motionState = nextMotionState;
    m_impl->motionStateStarted = now;
  }
  const auto motionPhase = inputIndicatorMotionPhase(
      m_impl->motionState,
      std::chrono::duration<double, std::milli>(
          now - m_impl->motionStateStarted
      )
          .count()
  );
  const double animationMilliseconds =
      std::chrono::duration<double, std::milli>(
          now - m_impl->animationEpoch
      )
          .count();
  auto startleStep =
      inputIndicatorCompanionSourceStartleAdvance(
          m_impl->startleState,
          animationMilliseconds,
          reducedMotion
      );
  m_impl->startleState =
      startleStep.state;
  const bool startleSuspendsIdle =
      m_impl->motionState ==
          InputIndicatorMotionState::Idle &&
      (startleStep.frame.active ||
       startleStep.state.pending);
  InputIndicatorCatIdleTailFrame idleTailFrame;
  if (startleSuspendsIdle) {
    if (!m_impl->idleTailPaused) {
      m_impl->idleTailPaused = true;
      m_impl->idleTailPauseStartedMilliseconds =
          animationMilliseconds;
    }
  } else {
    if (m_impl->idleTailPaused) {
      if (m_impl->motionState ==
              InputIndicatorMotionState::Idle &&
          !reducedMotion &&
          m_impl->idleTailMotionState.running) {
        m_impl->idleTailMotionState
            .startedMilliseconds +=
            animationMilliseconds -
            m_impl
                ->idleTailPauseStartedMilliseconds;
      }
      m_impl->idleTailPaused = false;
    }
    const auto idleTailStep =
        inputIndicatorCompanionIdleMotionAdvance(
            m_impl->idleTailMotionState,
            m_impl->motionState,
            animationMilliseconds,
            reducedMotion
        );
    m_impl->idleTailMotionState =
        idleTailStep.state;
    idleTailFrame = idleTailStep.frame;
  }

  m_impl->previousTarget = *renderedTarget;
  m_impl->previousTargetTime = now;
  m_impl->previousTargetInitialized = true;
  m_impl->previousCursor =
      targetLocation.point;
  m_impl->previousCursorInitialized = true;

  if (reducedMotion ||
      distance(m_impl->motion) <= kSettledDistance) {
    m_impl->motion = {};
  }

  const auto followerDistance = distance({
      .x = renderedTarget->x - m_impl->follower.x,
      .y = renderedTarget->y - m_impl->follower.y,
  });
  if (followerDistance <= kSettledDistance) {
    m_impl->follower = *renderedTarget;
  }
  if (distance(m_impl->motion) <= kSettledDistance) {
    m_impl->motion = {};
  }
  MacInputIndicatorFacingPose facingPose{
      .facesRight = m_impl->facesRight,
      .turning = false,
      .horizontalScale = 1,
      .progress = 1,
  };
  if (reducedMotion) {
    const bool desired =
        m_impl->facingTurning
            ? m_impl->facingTargetRight
            : inputIndicatorCompanionFacesRight(
                  m_impl->facesRight, m_impl->motion.x
              );
    m_impl->facesRight = desired;
    m_impl->facingTargetRight = desired;
    m_impl->facingTurning = false;
    facingPose = macInputIndicatorFacingPose(
        desired, desired, 0, true
    );
  } else {
    if (!m_impl->facingTurning) {
      const bool desired =
          inputIndicatorCompanionFacesRight(
              m_impl->facesRight, m_impl->motion.x
          );
      if (macInputIndicatorFacingTurnStarts(
              m_impl->facesRight,
              desired,
              m_impl->facingTurning
          )) {
        m_impl->facingTargetRight = desired;
        m_impl->facingTurnStarted = now;
        m_impl->facingTurning = true;
      }
    }
    if (m_impl->facingTurning) {
      const double elapsed =
          std::chrono::duration<double, std::milli>(
              now - m_impl->facingTurnStarted
          )
              .count();
      facingPose = macInputIndicatorFacingPose(
          m_impl->facesRight,
          m_impl->facingTargetRight,
          elapsed,
          false
      );
      if (!facingPose.turning) {
        m_impl->facesRight =
            m_impl->facingTargetRight;
        m_impl->facingTurning = false;
      }
    } else {
      facingPose = macInputIndicatorFacingPose(
          m_impl->facesRight,
          m_impl->facesRight,
          0,
          false
      );
    }
  }
  double bodyHeadingRadians = m_impl->pursuitState.headingRadians;
  if (m_impl->companion == InputIndicatorCompanion::Cat) {
    const InputIndicatorPoint companionCenter{
        .x = m_impl->follower.x + renderedStyle.width / 2.0,
        .y = m_impl->follower.y + renderedStyle.height / 2.0,
    };
    bodyHeadingRadians = inputIndicatorCatCursorLeanHeading(
        m_impl->catLeanHeadingRadians, companionCenter, targetLocation.point, elapsedMilliseconds,
        m_impl->motionState != InputIndicatorMotionState::Idle, reducedMotion, facingPose.facesRight
    );
    m_impl->catLeanHeadingRadians = bodyHeadingRadians;
  }
  const bool animating = transitionProgress < 1.0 || idleGrowthStep.animating || followerDistance > kSettledDistance ||
                         distance(m_impl->motion) > kSettledDistance || startleStep.frame.active ||
                         startleStep.state.pending || facingPose.turning;
  const bool startleAnimating =
      startleStep.frame.active ||
      startleStep.state.pending;
  const bool locomotionAnimating =
      !reducedMotion &&
      (m_impl->motionState == InputIndicatorMotionState::Idle ||
       m_impl->motionState == InputIndicatorMotionState::Walk ||
       m_impl->motionState == InputIndicatorMotionState::Run);
  const auto frameInterval =
      transitionProgress < 1.0 || startleAnimating || facingPose.turning || idleGrowthStep.shrinking
          ? 8
          : inputIndicatorMotionFrameInterval(m_impl->motionState, reducedMotion);

  constexpr double kRedrawTolerance = 0.0001;
  const bool viewNeedsDisplay =
      m_impl->activeSpaceChanged || std::abs(m_impl->view.idleGrowthScale - idleScale) > kRedrawTolerance ||
      std::abs(m_impl->view.transitionProgress - transitionProgress) > kRedrawTolerance ||
      std::abs(m_impl->view.motionX - m_impl->motion.x) > kRedrawTolerance ||
      std::abs(m_impl->view.motionY - m_impl->motion.y) > kRedrawTolerance ||
      m_impl->view.motionState != static_cast<NSInteger>(m_impl->motionState) ||
      std::abs(m_impl->view.motionPhase - motionPhase) > kRedrawTolerance ||
      m_impl->view.catIdleTailMotion != static_cast<NSInteger>(idleTailFrame.motion) ||
      std::abs(m_impl->view.catIdleTailProgress - idleTailFrame.progress) > kRedrawTolerance ||
      static_cast<bool>(m_impl->view.catIdleTailActive) != idleTailFrame.active ||
      m_impl->view.facesRight != facingPose.facesRight ||
      std::abs(m_impl->view.facingScaleX - facingPose.horizontalScale) > kRedrawTolerance ||
      static_cast<bool>(m_impl->view.catStartleActive) != startleStep.frame.active ||
      static_cast<bool>(m_impl->view.catStartlePending) != startleStep.state.pending ||
      m_impl->view.catStartleLatestSource != static_cast<NSInteger>(startleStep.state.latestSource) ||
      std::abs(m_impl->view.catStartleProgress - startleStep.frame.progress) > kRedrawTolerance ||
      std::abs(m_impl->view.catStartleRecoilOffsetX - startleStep.frame.recoilOffsetX) > kRedrawTolerance ||
      std::abs(m_impl->view.catStartleHorizontalScale - startleStep.frame.horizontalScale) > kRedrawTolerance ||
      std::abs(m_impl->view.catStartleDorsalLift - startleStep.frame.dorsalLift) > kRedrawTolerance ||
      std::abs(m_impl->view.catStartleHaloScale - startleStep.frame.haloScale) > kRedrawTolerance ||
      std::abs(m_impl->view.catStartleHaloAlpha - startleStep.frame.haloAlpha) > kRedrawTolerance ||
      std::abs(m_impl->view.catBodyHeadingRadians - bodyHeadingRadians) > kRedrawTolerance;
  m_impl->view.previousSource =
      static_cast<NSInteger>(m_impl->previousSource);
  m_impl->view.source =
      static_cast<NSInteger>(m_impl->source);
  m_impl->view.transitionProgress = transitionProgress;
  m_impl->view.motionX = m_impl->motion.x;
  m_impl->view.motionY = m_impl->motion.y;
  m_impl->view.idleGrowthScale = idleScale;
  m_impl->view.motionState =
      static_cast<NSInteger>(m_impl->motionState);
  m_impl->view.motionPhase = reducedMotion ? 0 : motionPhase;
  m_impl->view.reducedMotion = reducedMotion;
  m_impl->view.catIdleTailMotion =
      static_cast<NSInteger>(idleTailFrame.motion);
  m_impl->view.catIdleTailProgress =
      idleTailFrame.progress;
  m_impl->view.catIdleTailActive =
      idleTailFrame.active ? YES : NO;
  m_impl->view.facesRight = facingPose.facesRight;
  m_impl->view.facingScaleX =
      facingPose.horizontalScale;
  m_impl->view.catStartleActive =
      startleStep.frame.active ? YES : NO;
  m_impl->view.catStartlePending =
      startleStep.state.pending ? YES : NO;
  m_impl->view.catStartleLatestSource =
      static_cast<NSInteger>(
          startleStep.state.latestSource
      );
  m_impl->view.catStartleProgress =
      startleStep.frame.progress;
  m_impl->view.catStartleRecoilOffsetX =
      startleStep.frame.recoilOffsetX;
  m_impl->view.catStartleHorizontalScale =
      startleStep.frame.horizontalScale;
  m_impl->view.catStartleDorsalLift =
      startleStep.frame.dorsalLift;
  m_impl->view.catStartleHaloScale =
      startleStep.frame.haloScale;
  m_impl->view.catStartleHaloAlpha =
      startleStep.frame.haloAlpha;
  m_impl->view.catBodyHeadingRadians =
      bodyHeadingRadians;
  if (viewNeedsDisplay) {
    [m_impl->view setNeedsDisplay:YES];
  }
  const NSSize renderedPanelSize = NSMakeSize(renderedStyle.canvasWidth, renderedStyle.canvasHeight);
  if (!NSEqualSizes(m_impl->panel.contentView.bounds.size, renderedPanelSize)) {
    [m_impl->panel setContentSize:renderedPanelSize];
    [m_impl->view setNeedsDisplay:YES];
  }
  const auto panelOrigin =
      NSMakePoint(m_impl->follower.x - renderedStyle.canvasOffsetX, m_impl->follower.y - renderedStyle.canvasOffsetY);
  [m_impl->panel setFrameOrigin:panelOrigin];
  if (!NSEqualSizes(m_impl->view.bounds.size, renderedPanelSize)) {
    [m_impl->view setFrameSize:renderedPanelSize];
    [m_impl->view setNeedsDisplay:YES];
  }

  const bool visible =
      static_cast<bool>([m_impl->panel isVisible]);
  if (macInputIndicatorPresentationNeeded(
          m_impl->activeSpaceChanged, visible
      )) {
    requestMacInputIndicatorPanelPresentation(
        [this](MacInputIndicatorPanelOrderCommand command) {
          switch (command) {
          case MacInputIndicatorPanelOrderCommand::FrontRegardless:
            [m_impl->panel orderFrontRegardless];
            break;
          }
        }
    );
  }
  const bool presented =
      static_cast<bool>([m_impl->panel isVisible]);
  if (!presented) {
    hide();
    return {};
  }
  m_impl->activeSpaceChanged = false;
  if (transitionProgress >= 1.0) {
    m_impl->previousSource = m_impl->source;
  }
  return {
      .visible = true,
      .animating = animating || locomotionAnimating,
      .reducedMotion = reducedMotion,
      .frameIntervalMilliseconds = frameInterval,
  };
}

void MacInputIndicatorPanel::hide()
{
  [m_impl->panel orderOut:nil];
  m_impl->idleTailMotionState.running = false;
  m_impl->idleTailPaused = false;
  m_impl->catLeanHeadingRadians = m_impl->facesRight ? 0.0 : M_PI;
  m_impl->idleGrowthState = {};
  m_impl->lastPointerMovement = m_impl->clock();
  m_impl->startleState = {};
  m_impl->view.idleGrowthScale = 1;
  m_impl->view.catStartleActive = NO;
  m_impl->view.catStartlePending = NO;
  m_impl->view.catStartleLatestSource =
      static_cast<NSInteger>(
          deskflow::input::InputIndicatorSource::Unknown
      );
  m_impl->view.catStartleProgress = 0;
  m_impl->view.catStartleRecoilOffsetX = 0;
  m_impl->view.catStartleHorizontalScale = 1;
  m_impl->view.catStartleDorsalLift = 0;
  m_impl->view.catStartleHaloScale = 1;
  m_impl->view.catStartleHaloAlpha = 0;
  const auto normalStyle = macInputIndicatorPanelStyle(m_impl->companion);
  [m_impl->panel setContentSize:NSMakeSize(normalStyle.canvasWidth, normalStyle.canvasHeight)];
  [m_impl->view setNeedsDisplay:YES];
}

} // namespace deskflow::gui::input
