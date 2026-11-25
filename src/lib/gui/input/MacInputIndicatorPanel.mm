/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacInputIndicatorPanel.h"
#include "MacInputIndicatorCatArtwork.h"

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

double bodyScale(
    double transitionProgress,
    double motionX,
    double locomotionStretch
)
{
  const double transition =
      std::sin(M_PI * transitionProgress) * 0.04;
  const double movement =
      std::min(std::abs(motionX) / 8.0, 1.0) * 0.05;
  return 1.0 + std::min(
      transition + movement + locomotionStretch, 0.09
  );
}

void appendPath(NSBezierPath *silhouette, NSBezierPath *part)
{
  [silhouette appendBezierPath:part];
}

NSBezierPath *catArtworkPath(
    std::span<const MacInputIndicatorCatPathCommand> commands
)
{
  NSBezierPath *path = [NSBezierPath bezierPath];
  for (const auto &command : commands) {
    switch (command.type) {
    case MacInputIndicatorCatPathCommandType::Move:
      [path moveToPoint:NSMakePoint(
                            command.x1, command.y1
                        )];
      break;
    case MacInputIndicatorCatPathCommandType::Line:
      [path lineToPoint:NSMakePoint(
                            command.x1, command.y1
                        )];
      break;
    case MacInputIndicatorCatPathCommandType::Cubic:
      [path curveToPoint:NSMakePoint(command.x3, command.y3)
          controlPoint1:NSMakePoint(command.x1, command.y1)
          controlPoint2:NSMakePoint(command.x2, command.y2)];
      break;
    case MacInputIndicatorCatPathCommandType::Close:
      [path closePath];
      break;
    }
  }
  return path;
}

NSBezierPath *catArtworkPath(
    const InputIndicatorCatPose &pose,
    std::span<const MacInputIndicatorCatPathCommand> commands
)
{
  NSBezierPath *path = [NSBezierPath bezierPath];
  const auto deformed =
      [&pose](double x, double y) {
        return inputIndicatorCatTorsoPoint(
            pose, {.x = x, .y = y}
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

void drawCat(
    NSRect bounds, NSColor *fill, NSColor *edge,
    InputIndicatorMotionState motionState,
    double motionPhase,
    bool reducedMotion,
    bool facesRight,
    const std::vector<MacInputIndicatorHaloFrame> &haloFrames,
    deskflow::input::InputIndicatorSource source,
    InputIndicatorTone tone,
    MacInputIndicatorRenderAlpha alpha
)
{
  const auto pose = inputIndicatorCatPose(
      motionState, motionPhase, reducedMotion
  );
  const auto artwork =
      macInputIndicatorCatArtwork(pose.artworkFrame);
  const auto model =
      macInputIndicatorRenderModel(InputIndicatorCompanion::Cat);

  [NSGraphicsContext saveGraphicsState];
  if (facesRight != model.sourceFacesRight) {
    NSAffineTransform *mirror = [NSAffineTransform transform];
    [mirror translateXBy:bounds.size.width yBy:0];
    [mirror scaleXBy:-1 yBy:1];
    [mirror concat];
  }

  NSBezierPath *far = catArtworkPath(artwork.far);
  NSBezierPath *near = catArtworkPath(
      pose, artwork.near
  );
  NSBezierPath *silhouette = [NSBezierPath bezierPath];
  appendPath(silhouette, far);
  appendPath(silhouette, near);

  if (!reducedMotion) {
    const auto drawHalo =
        [silhouette, tone, alpha](
            deskflow::input::InputIndicatorSource haloSource,
            double haloScale,
            double haloAlpha
        ) {
          NSBezierPath *halo = [silhouette copy];
          NSAffineTransform *transform =
              [NSAffineTransform transform];
          [transform translateXBy:15 yBy:14];
          [transform scaleXBy:haloScale yBy:haloScale];
          [transform translateXBy:-15 yBy:-14];
          [halo transformUsingAffineTransform:transform];
          [indicatorGlyphColor(
               haloSource, tone, alpha.body * haloAlpha
           ) setFill];
          [halo fill];
          [halo release];
        };
    drawHalo(source, 1.05, 0.08);
    for (const auto &frame : haloFrames) {
      drawHalo(
          frame.source,
          1.0 + (frame.scale - 1.0) * 0.12,
          frame.alpha * 0.28
      );
      drawHalo(
          frame.source,
          1.0 + (frame.scale - 1.0) * 0.21,
          frame.alpha * 0.22
      );
      drawHalo(
          frame.source,
          1.0 + (frame.scale - 1.0) * 0.30,
          frame.alpha * 0.16
      );
    }
  }

  NSColor *farFill =
      [fill blendedColorWithFraction:0.18 ofColor:edge];
  fillSilhouette(far, farFill, edge);
  fillSilhouette(near, fill, edge);
  [NSGraphicsContext restoreGraphicsState];
}

void drawDog(
    NSRect bounds, NSColor *fill, NSColor *edge,
    double motionX, double motionY,
    double transitionProgress,
    InputIndicatorMotionState motionState,
    double motionPhase,
    bool reducedMotion
)
{
  (void)bounds;
  const auto pose = inputIndicatorMotionPose(
      motionState, motionPhase, reducedMotion
  );
  const double scale = bodyScale(
      transitionProgress, motionX, pose.bodyStretch
  );
  const double bodyWidth = 17.0 * scale;
  const double bodyX = 12.0 - bodyWidth / 2.0;
  const double lift =
      pose.bodyBob +
      std::clamp(motionY * 0.12, -1.0, 1.0);
  const double tailSwayAmount =
      pose.tailSway +
      motionX * 0.5;
  const double earSway = motionX * 0.12;

  NSBezierPath *silhouette = [NSBezierPath bezierPath];
  NSBezierPath *tail = [NSBezierPath bezierPath];
  [tail moveToPoint:NSMakePoint(bodyX + bodyWidth - 1, 8 + lift)];
  [tail curveToPoint:NSMakePoint(
                         25, 18 + tailSwayAmount
                     )
       controlPoint1:NSMakePoint(
                          29 + tailSwayAmount, 7
                      )
       controlPoint2:NSMakePoint(
                          29 + tailSwayAmount, 16
                      )];
  [tail curveToPoint:NSMakePoint(bodyX + bodyWidth - 1, 12 + lift)
       controlPoint1:NSMakePoint(27 + tailSwayAmount, 15)
       controlPoint2:NSMakePoint(bodyX + bodyWidth + 2, 13 + lift)];
  [tail closePath];
  appendPath(silhouette, tail);
  appendPath(
      silhouette,
      [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(
                                                  bodyX,
                                                  5 + lift,
                                                  bodyWidth,
                                                  15
                                              )
                                      xRadius:7
                                      yRadius:7]
  );
  appendPath(
      silhouette,
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                   bodyX,
                                                   10 + lift,
                                                   bodyWidth,
                                                   12
                                               )]
  );
  appendPath(
      silhouette,
      [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(
                                                  bodyX - 1 + earSway,
                                                  11 + lift,
                                                  5,
                                                  10
                                              )
                                      xRadius:2.5
                                      yRadius:2.5]
  );
  appendPath(
      silhouette,
      [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(
                                                  bodyX + bodyWidth - 4 +
                                                      earSway,
                                                  11 + lift,
                                                  5,
                                                  10
                                              )
                                      xRadius:2.5
                                      yRadius:2.5]
  );

  if (motionState != InputIndicatorMotionState::Idle) {
    const double paw =
        std::sin(motionPhase * 2 * M_PI) * 0.8;
    appendPath(
        silhouette,
        [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                     bodyX + 3,
                                                     4 + lift + paw,
                                                     4,
                                                     2.5
                                                 )]
    );
    appendPath(
        silhouette,
        [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                     bodyX + 10,
                                                     4 + lift - paw,
                                                     4,
                                                     2.5
                                                 )]
    );
  }
  fillSilhouette(silhouette, fill, edge);
}

void drawTurtle(
    NSRect bounds, NSColor *fill, NSColor *edge,
    double motionX, double motionY,
    double transitionProgress,
    InputIndicatorMotionState motionState,
    double motionPhase,
    bool reducedMotion
)
{
  (void)bounds;
  (void)motionY;
  const auto pose = inputIndicatorMotionPose(
      motionState, motionPhase, reducedMotion
  );
  const double scale = bodyScale(
      transitionProgress, motionX, pose.bodyStretch
  );
  const double bob = pose.bodyBob;
  const double shellWidth = 21.0 * scale;
  const double shellX = 3.0 + (21.0 - shellWidth) / 2.0;

  NSBezierPath *tail = [NSBezierPath bezierPath];
  const double tailSwayAmount =
      pose.tailSway +
      motionX * 0.2;
  NSBezierPath *silhouette = [NSBezierPath bezierPath];
  [tail moveToPoint:NSMakePoint(shellX, 10 + bob)];
  [tail lineToPoint:NSMakePoint(
                          1 + tailSwayAmount, 11 + bob
                      )];
  [tail lineToPoint:NSMakePoint(shellX + 2, 8 + bob)];
  [tail closePath];
  appendPath(silhouette, tail);
  appendPath(
      silhouette,
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                   shellX,
                                                   5 + bob,
                                                   shellWidth,
                                                   13
                                               )]
  );
  appendPath(
      silhouette,
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                   shellX + shellWidth - 2,
                                                   10 + bob,
                                                   6,
                                                   6
                                               )]
  );
  for (const auto x : {shellX + 4, shellX + shellWidth - 7}) {
    appendPath(
        silhouette,
        [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                     x,
                                                     3 + bob,
                                                     4,
                                                     5
                                                 )]
    );
  }
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
    bool facesRight
)
{
  const auto model =
      macInputIndicatorRenderModel(companion);
  auto bounds = model.languageBounds;
  NSBezierPath *catSilhouette = nil;
  if (companion == InputIndicatorCompanion::Cat) {
    const auto pose = inputIndicatorCatPose(
        motionState, motionPhase, reducedMotion
    );
    bounds = inputIndicatorCatInnerLanguageBounds(
        pose, facesRight
    );
    const auto artwork =
        macInputIndicatorCatArtwork(pose.artworkFrame);
    catSilhouette = catArtworkPath(
        pose, artwork.near
    );
    if (facesRight != model.sourceFacesRight) {
      NSAffineTransform *mirror =
          [NSAffineTransform transform];
      [mirror translateXBy:model.totalWidth yBy:0];
      [mirror scaleXBy:-1 yBy:1];
      [catSilhouette transformUsingAffineTransform:mirror];
    }
  }
  const NSRect labelBounds = NSMakeRect(
      bounds.x,
      bounds.y,
      bounds.width,
      bounds.height
  );

  NSMutableParagraphStyle *paragraph =
      [[NSMutableParagraphStyle alloc] init];
  paragraph.alignment =
      companion == InputIndicatorCompanion::Cat
          ? NSTextAlignmentLeft
          : NSTextAlignmentCenter;
  const auto drawLabel =
      [paragraph, companion, motionState, tone, labelBounds, bounds, alpha, catSilhouette](
          deskflow::input::InputIndicatorSource labelSource,
          double transitionAlpha
      ) {
        NSString *label = indicatorLabel(labelSource);
        if (!label || transitionAlpha <= 0) {
          return;
        }

        const double fallbackPointSize =
            macInputIndicatorPanelStyle(companion).labelSize;
        if (companion != InputIndicatorCompanion::Cat) {
          NSFont *font =
              [NSFont systemFontOfSize:fallbackPointSize
                                weight:NSFontWeightSemibold];
          NSDictionary *attributes = @{
            NSFontAttributeName : font,
            NSForegroundColorAttributeName :
                indicatorGlyphColor(
                    labelSource,
                    tone,
                    alpha.detail * transitionAlpha
                ),
            NSParagraphStyleAttributeName : paragraph
          };
          [label drawInRect:labelBounds withAttributes:attributes];
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
        [catSilhouette addClip];
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
  [paragraph release];
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
@property(nonatomic) NSInteger motionState;
@property(nonatomic) double motionPhase;
@property(nonatomic) BOOL reducedMotion;
@property(nonatomic) BOOL facesRight;
@property(nonatomic) NSInteger haloPulseCount;
@property(nonatomic) NSInteger haloSource0;
@property(nonatomic) double haloScale0;
@property(nonatomic) double haloAlpha0;
@property(nonatomic) NSInteger haloSource1;
@property(nonatomic) double haloScale1;
@property(nonatomic) double haloAlpha1;
@property(nonatomic) NSInteger haloSource2;
@property(nonatomic) double haloScale2;
@property(nonatomic) double haloAlpha2;
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
  std::vector<deskflow::gui::input::MacInputIndicatorHaloFrame>
      haloFrames;
  const auto addHaloFrame =
      [&haloFrames](NSInteger haloSource, double scale, double haloAlpha) {
        if (haloAlpha > 0) {
          haloFrames.push_back({
              .source =
                  static_cast<deskflow::input::InputIndicatorSource>(
                      haloSource
                  ),
              .scale = scale,
              .alpha = haloAlpha,
              .active = true,
          });
        }
      };
  if (self.haloPulseCount > 0) {
    addHaloFrame(
        self.haloSource0, self.haloScale0, self.haloAlpha0
    );
  }
  if (self.haloPulseCount > 1) {
    addHaloFrame(
        self.haloSource1, self.haloScale1, self.haloAlpha1
    );
  }
  if (self.haloPulseCount > 2) {
    addHaloFrame(
        self.haloSource2, self.haloScale2, self.haloAlpha2
    );
  }

  switch (companion) {
  case deskflow::gui::input::InputIndicatorCompanion::Cat:
    deskflow::gui::input::drawCat(
        bounds, silhouetteFill, silhouetteEdge,
        motionState,
        self.motionPhase,
        self.reducedMotion,
        self.facesRight,
        haloFrames,
        source,
        tone,
        alpha
    );
    break;
  case deskflow::gui::input::InputIndicatorCompanion::Dog:
    deskflow::gui::input::drawDog(
        bounds, silhouetteFill, silhouetteEdge,
        self.motionX, self.motionY,
        self.transitionProgress,
        motionState,
        self.motionPhase,
        self.reducedMotion
    );
    break;
  case deskflow::gui::input::InputIndicatorCompanion::Turtle:
    deskflow::gui::input::drawTurtle(
        bounds, silhouetteFill, silhouetteEdge,
        self.motionX, self.motionY,
        self.transitionProgress,
        motionState,
        self.motionPhase,
        self.reducedMotion
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
      self.facesRight
  );
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

constexpr double kCatchUpFactor = 0.5;
constexpr double kMotionEaseFactor = 0.5;
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

  explicit Impl(std::function<void()> invalidated)
      : presentationInvalidated(std::move(invalidated))
  {
    lifetime = std::make_shared<Lifetime>();
    lifetime->owner = this;
    haloEpoch = std::chrono::steady_clock::now();
    const auto style =
        macInputIndicatorPanelStyle(InputIndicatorCompanion::Cat);
    panel = [[DeskflowInputIndicatorPanel alloc]
        initWithContentRect:NSMakeRect(
                                0, 0, style.width, style.height
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
                      state->owner->followerInitialized = false;
                      state->owner->previousTargetInitialized = false;
                      state->owner->motion = {};
                      state->owner->motionState =
                          InputIndicatorMotionState::Idle;
                      const auto now =
                          std::chrono::steady_clock::now();
                      state->owner->motionStateStarted = now;
                      state->owner->lastPointerMovement = now;
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
    [panel close];
    [panel release];
    [view release];
  }

  DeskflowInputIndicatorPanel *panel = nil;
  DeskflowInputIndicatorView *view = nil;
  id activeSpaceToken = nil;
  std::shared_ptr<Lifetime> lifetime;
  std::function<void()> presentationInvalidated;
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
  bool activeSpaceChanged = true;
  bool followerInitialized = false;
  bool previousTargetInitialized = false;
  InputIndicatorPoint follower;
  InputIndicatorPoint previousTarget;
  InputIndicatorPoint motion;
  bool facesRight = true;
  std::chrono::steady_clock::time_point haloEpoch;
  std::vector<MacInputIndicatorHaloPulse> haloPulses;
  deskflow::input::InputIndicatorSource pendingHaloSource =
      deskflow::input::InputIndicatorSource::Unknown;
  bool haloPending = false;
  InputIndicatorMotionState motionState =
      InputIndicatorMotionState::Idle;
};

MacInputIndicatorPanel::MacInputIndicatorPanel(
    std::function<void()> presentationInvalidated
)
    : m_impl(std::make_unique<Impl>(
          std::move(presentationInvalidated)
      ))
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

  const bool sourceChanged =
      m_impl->source !=
          deskflow::input::InputIndicatorSource::Unknown &&
      m_impl->source != options.source &&
      options.companion == InputIndicatorCompanion::Cat;
  if (options.companion != InputIndicatorCompanion::Cat) {
    m_impl->haloPulses.clear();
    m_impl->haloPending = false;
    m_impl->pendingHaloSource =
        deskflow::input::InputIndicatorSource::Unknown;
  }
  m_impl->companion = options.companion;
  m_impl->previousSource =
      m_impl->source != deskflow::input::InputIndicatorSource::Unknown
          ? m_impl->source
          : options.source;
  m_impl->source = options.source;
  const auto now = std::chrono::steady_clock::now();
  if (sourceChanged) {
    const double elapsedMilliseconds =
        std::chrono::duration<double, std::milli>(
            now - m_impl->haloEpoch
        )
            .count();
    if (macInputIndicatorAppendHaloPulse(
            m_impl->haloPulses,
            options.source,
            elapsedMilliseconds
        )) {
      m_impl->haloPending = false;
    } else {
      m_impl->pendingHaloSource = options.source;
      m_impl->haloPending = true;
    }
  }
  m_impl->transitionStarted = now;
  m_impl->motionStateStarted = now;
  m_impl->previousTargetTime = now;
  m_impl->lastPointerMovement = now;
  m_impl->motionState = InputIndicatorMotionState::Idle;
  m_impl->activeSpaceChanged = true;
  m_impl->followerInitialized = false;
  m_impl->previousTargetInitialized = false;
  m_impl->motion = {};

  const auto style = macInputIndicatorPanelStyle(options.companion);
  [m_impl->panel setContentSize:NSMakeSize(
                      style.width, style.height
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
  m_impl->view.facesRight = m_impl->facesRight;
  m_impl->view.haloPulseCount = 0;
  [m_impl->view setNeedsDisplay:YES];
  return move(options.position).visible;
}

MacInputIndicatorPanelMoveResult
MacInputIndicatorPanel::move(InputIndicatorPosition position)
{
  const auto style =
      macInputIndicatorPanelStyle(m_impl->companion);
  const auto cursor = NSEvent.mouseLocation;
  const auto target = positionInputIndicator(
      {.x = cursor.x, .y = cursor.y},
      {.width = style.width, .height = style.height},
      screenRects(),
      position,
      style.cursorGap,
      style.screenMargin
  );
  if (!target) {
    hide();
    return {};
  }

  const bool reducedMotion = isOSXReduceMotion();
  const auto now = std::chrono::steady_clock::now();
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
  if (m_impl->previousTargetInitialized) {
    const auto elapsedMilliseconds =
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
    if (pointerDistance > kSettledDistance) {
      m_impl->lastPointerMovement = now;
    }
  }
  const auto stoppedMilliseconds =
      std::chrono::duration<double, std::milli>(
          now - m_impl->lastPointerMovement
      )
          .count();
  const auto nextMotionState = inputIndicatorMotionState(
      m_impl->motionState,
      pointerSpeed,
      pointerDistance,
      m_impl->followerInitialized
          ? distance({
                .x = target->x - m_impl->follower.x,
                .y = target->y - m_impl->follower.y,
            })
          : 0,
      stoppedMilliseconds,
      reducedMotion
  );
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

  if (reducedMotion || !m_impl->followerInitialized) {
    m_impl->follower = *target;
    m_impl->followerInitialized = true;
  } else {
    const auto follower = followInputIndicator(
        m_impl->follower,
        *target,
        kCatchUpFactor,
        style.maximumLag
    );
    if (!follower) {
      hide();
      return {};
    }
    m_impl->follower = *follower;
  }

  InputIndicatorPoint desiredMotion{};
  if (m_impl->previousTargetInitialized) {
    const auto motion = inputIndicatorMotionOffset(
        m_impl->previousTarget,
        *target,
        style.maximumMotion,
        reducedMotion
    );
    if (!motion) {
      hide();
      return {};
    }
    desiredMotion = *motion;
  }
  m_impl->previousTarget = *target;
  m_impl->previousTargetTime = now;
  m_impl->previousTargetInitialized = true;

  if (reducedMotion) {
    m_impl->motion = {};
  } else {
    const auto motion = followInputIndicator(
        m_impl->motion,
        desiredMotion,
        kMotionEaseFactor,
        style.maximumMotion
    );
    if (!motion) {
      hide();
      return {};
    }
    m_impl->motion = *motion;
  }

  const auto followerDistance = distance({
      .x = target->x - m_impl->follower.x,
      .y = target->y - m_impl->follower.y,
  });
  if (followerDistance <= kSettledDistance) {
    m_impl->follower = *target;
  }
  if (distance(m_impl->motion) <= kSettledDistance) {
    m_impl->motion = {};
  }
  const double haloMilliseconds =
      std::chrono::duration<double, std::milli>(
          now - m_impl->haloEpoch
      )
          .count();
  if (reducedMotion) {
    m_impl->haloPulses.clear();
    m_impl->haloPending = false;
    m_impl->pendingHaloSource =
        deskflow::input::InputIndicatorSource::Unknown;
  } else if (
      m_impl->haloPending &&
      macInputIndicatorAppendHaloPulse(
          m_impl->haloPulses,
          m_impl->pendingHaloSource,
          haloMilliseconds
      )) {
    m_impl->haloPending = false;
    m_impl->pendingHaloSource =
        deskflow::input::InputIndicatorSource::Unknown;
  }
  const auto haloFrames =
      macInputIndicatorActiveHaloFrames(
          m_impl->haloPulses,
          haloMilliseconds,
          reducedMotion
      );
  const bool haloAnimating =
      !haloFrames.empty() || m_impl->haloPending;
  m_impl->facesRight =
      inputIndicatorCompanionFacesRight(
          m_impl->facesRight, m_impl->motion.x
      );
  const bool animating =
      transitionProgress < 1.0 ||
      followerDistance > kSettledDistance ||
      distance(m_impl->motion) > kSettledDistance ||
      haloAnimating;
  const bool locomotionAnimating =
      !reducedMotion &&
      (m_impl->motionState == InputIndicatorMotionState::Idle ||
       m_impl->motionState == InputIndicatorMotionState::Walk ||
       m_impl->motionState == InputIndicatorMotionState::Run);
  const auto frameInterval =
      transitionProgress < 1.0 || haloAnimating
          ? 8
          : inputIndicatorMotionFrameInterval(
                m_impl->motionState, reducedMotion
            );

  constexpr double kRedrawTolerance = 0.0001;
  const bool viewNeedsDisplay =
      m_impl->activeSpaceChanged ||
      std::abs(
          m_impl->view.transitionProgress - transitionProgress
      ) > kRedrawTolerance ||
      std::abs(m_impl->view.motionX - m_impl->motion.x) >
          kRedrawTolerance ||
      std::abs(m_impl->view.motionY - m_impl->motion.y) >
          kRedrawTolerance ||
      m_impl->view.motionState !=
          static_cast<NSInteger>(m_impl->motionState) ||
      std::abs(m_impl->view.motionPhase - motionPhase) >
          kRedrawTolerance ||
      m_impl->view.facesRight != m_impl->facesRight ||
      haloAnimating ||
      m_impl->view.haloPulseCount !=
          static_cast<NSInteger>(haloFrames.size());
  m_impl->view.previousSource =
      static_cast<NSInteger>(m_impl->previousSource);
  m_impl->view.source =
      static_cast<NSInteger>(m_impl->source);
  m_impl->view.transitionProgress = transitionProgress;
  m_impl->view.motionX = m_impl->motion.x;
  m_impl->view.motionY = m_impl->motion.y;
  m_impl->view.motionState =
      static_cast<NSInteger>(m_impl->motionState);
  m_impl->view.motionPhase = reducedMotion ? 0 : motionPhase;
  m_impl->view.reducedMotion = reducedMotion;
  m_impl->view.facesRight = m_impl->facesRight;
  m_impl->view.haloPulseCount =
      static_cast<NSInteger>(haloFrames.size());
  m_impl->view.haloAlpha0 = 0;
  m_impl->view.haloAlpha1 = 0;
  m_impl->view.haloAlpha2 = 0;
  for (size_t i = 0; i < haloFrames.size(); ++i) {
    switch (i) {
    case 0:
      m_impl->view.haloSource0 =
          static_cast<NSInteger>(haloFrames[i].source);
      m_impl->view.haloScale0 = haloFrames[i].scale;
      m_impl->view.haloAlpha0 = haloFrames[i].alpha;
      break;
    case 1:
      m_impl->view.haloSource1 =
          static_cast<NSInteger>(haloFrames[i].source);
      m_impl->view.haloScale1 = haloFrames[i].scale;
      m_impl->view.haloAlpha1 = haloFrames[i].alpha;
      break;
    case 2:
      m_impl->view.haloSource2 =
          static_cast<NSInteger>(haloFrames[i].source);
      m_impl->view.haloScale2 = haloFrames[i].scale;
      m_impl->view.haloAlpha2 = haloFrames[i].alpha;
      break;
    default:
      break;
    }
  }
  if (viewNeedsDisplay) {
    [m_impl->view setNeedsDisplay:YES];
  }
  [m_impl->panel setFrameOrigin:NSMakePoint(
                      m_impl->follower.x, m_impl->follower.y
                  )];

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
  m_impl->haloPulses.clear();
  m_impl->haloPending = false;
  m_impl->pendingHaloSource =
      deskflow::input::InputIndicatorSource::Unknown;
  m_impl->view.haloPulseCount = 0;
  [m_impl->panel orderOut:nil];
}

} // namespace deskflow::gui::input
