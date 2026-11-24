/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacInputIndicatorPanel.h"

#include "OSXHelpers.h"

#include <AppKit/AppKit.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

namespace deskflow::gui::input {

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

NSColor *indicatorTint(
    deskflow::input::InputIndicatorSource source,
    InputIndicatorTone tone,
    double opacity
)
{
  NSColor *color =
      source == deskflow::input::InputIndicatorSource::Korean2Set
          ? [NSColor colorWithSRGBRed:0.96
                                green:0.40
                                 blue:0.42
                                alpha:1]
          : [NSColor colorWithSRGBRed:0.38
                                green:0.66
                                 blue:0.98
                                alpha:1];
  switch (tone) {
  case InputIndicatorTone::Light:
    return [[color blendedColorWithFraction:0.45
                                    ofColor:[NSColor whiteColor]]
        colorWithAlphaComponent:opacity];
  case InputIndicatorTone::Dark:
    return [[color blendedColorWithFraction:0.25
                                    ofColor:[NSColor blackColor]]
        colorWithAlphaComponent:opacity];
  case InputIndicatorTone::Automatic:
    return [[color blendedColorWithFraction:0.65
                                    ofColor:[NSColor windowBackgroundColor]]
        colorWithAlphaComponent:opacity];
  }
}

NSColor *indicatorTextColor(InputIndicatorTone tone)
{
  switch (tone) {
  case InputIndicatorTone::Light:
    return [NSColor colorWithWhite:0.08 alpha:1];
  case InputIndicatorTone::Dark:
    return [NSColor colorWithWhite:0.98 alpha:1];
  case InputIndicatorTone::Automatic:
    return [NSColor labelColor];
  }
}

void fillAndHighlight(NSBezierPath *path, NSColor *fill)
{
  [fill setFill];
  [path fill];
  [[NSColor colorWithWhite:1 alpha:0.55] setStroke];
  [path setLineWidth:1];
  [path stroke];
}

double bodyScale(
    double transitionProgress, double motionX
)
{
  const double transition =
      std::sin(M_PI * transitionProgress) * 0.04;
  const double movement =
      std::min(std::abs(motionX) / 8.0, 1.0) * 0.05;
  return 1.0 + std::min(transition + movement, 0.09);
}

void drawCat(
    NSRect bounds, NSColor *fill, double motionX, double motionY,
    double transitionProgress
)
{
  const double scale = bodyScale(transitionProgress, motionX);
  const double center = NSMidX(bounds);
  const double bodyWidth = 16.0 * scale;
  const double bodyX = center - bodyWidth / 2.0;
  const double sway = motionX * 0.12;
  const double lift = std::clamp(motionY * 0.12, -1.0, 1.0);

  NSBezierPath *tail = [NSBezierPath bezierPath];
  [tail moveToPoint:NSMakePoint(bodyX + bodyWidth - 1, 8 + lift)];
  [tail curveToPoint:NSMakePoint(bounds.size.width - 2, 14 + motionY * 0.2)
       controlPoint1:NSMakePoint(bounds.size.width + motionX * 0.35, 5)
       controlPoint2:NSMakePoint(bounds.size.width + motionX * 0.55, 13)];
  [[fill colorWithAlphaComponent:std::max(
      0.25, static_cast<double>(fill.alphaComponent))] setStroke];
  [tail setLineWidth:2.5];
  [tail setLineCapStyle:NSLineCapStyleRound];
  [tail stroke];

  fillAndHighlight(
      [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(
                                                  bodyX, 5, bodyWidth, 15
                                              )
                                      xRadius:7
                                      yRadius:7],
      fill
  );
  fillAndHighlight(
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                   bodyX, 10, bodyWidth, 12
                                               )],
      fill
  );

  NSBezierPath *ears = [NSBezierPath bezierPath];
  [ears moveToPoint:NSMakePoint(bodyX + 1 + sway, 19)];
  [ears lineToPoint:NSMakePoint(bodyX + 4 + sway, 23)];
  [ears lineToPoint:NSMakePoint(bodyX + 7 + sway, 20)];
  [ears lineToPoint:NSMakePoint(bodyX + 9 + sway, 22.5)];
  [ears lineToPoint:NSMakePoint(bodyX + 13 + sway, 22.5)];
  [ears lineToPoint:NSMakePoint(bodyX + 14 + sway, 19)];
  fillAndHighlight(ears, fill);
}

void drawDog(
    NSRect bounds, NSColor *fill, double motionX, double motionY,
    double transitionProgress
)
{
  const double scale = bodyScale(transitionProgress, motionX);
  const double bodyWidth = 17.0 * scale;
  const double bodyX = NSMidX(bounds) - bodyWidth / 2.0;
  const double lift = std::clamp(motionY * 0.12, -1.0, 1.0);
  const double earSway = motionX * 0.12;

  NSBezierPath *tail = [NSBezierPath bezierPath];
  [tail moveToPoint:NSMakePoint(bodyX + bodyWidth - 1, 8 + lift)];
  [tail curveToPoint:NSMakePoint(bounds.size.width - 2, 18)
       controlPoint1:NSMakePoint(bounds.size.width + motionX * 0.5, 7)
       controlPoint2:NSMakePoint(bounds.size.width + motionX * 0.5, 16)];
  [[fill colorWithAlphaComponent:std::max(
      0.25, static_cast<double>(fill.alphaComponent))] setStroke];
  [tail setLineWidth:2.5];
  [tail setLineCapStyle:NSLineCapStyleRound];
  [tail stroke];

  fillAndHighlight(
      [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(
                                                  bodyX, 5, bodyWidth, 15
                                              )
                                      xRadius:7
                                      yRadius:7],
      fill
  );
  fillAndHighlight(
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                   bodyX, 10, bodyWidth, 12
                                               )],
      fill
  );
  fillAndHighlight(
      [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(
                                                  bodyX - 1 + earSway,
                                                  11,
                                                  5,
                                                  10
                                              )
                                      xRadius:2.5
                                      yRadius:2.5],
      fill
  );
  fillAndHighlight(
      [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(
                                                  bodyX + bodyWidth - 4 +
                                                      earSway,
                                                  11,
                                                  5,
                                                  10
                                              )
                                      xRadius:2.5
                                      yRadius:2.5],
      fill
  );
}

void drawTurtle(
    NSRect bounds, NSColor *fill, double motionX, double motionY,
    double transitionProgress
)
{
  (void)motionY;
  const double scale = bodyScale(transitionProgress, motionX);
  const double shellWidth = 21.0 * scale;
  const double shellX = 3.0 + (21.0 - shellWidth) / 2.0;

  fillAndHighlight(
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                   shellX, 5, shellWidth, 13
                                               )],
      fill
  );
  fillAndHighlight(
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                   shellX + shellWidth - 2,
                                                   10,
                                                   6,
                                                   6
                                               )],
      fill
  );
  for (const auto x : {shellX + 4, shellX + shellWidth - 7}) {
    fillAndHighlight(
        [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(
                                                     x, 3, 4, 5
                                                 )],
        fill
    );
  }
  NSBezierPath *tail = [NSBezierPath bezierPath];
  [tail moveToPoint:NSMakePoint(shellX, 10)];
  [tail lineToPoint:NSMakePoint(1 + motionX * 0.2, 11)];
  [tail lineToPoint:NSMakePoint(shellX + 2, 8)];
  fillAndHighlight(tail, fill);
}

void drawFaceAndTag(
    NSRect bounds,
    InputIndicatorCompanion companion,
    deskflow::input::InputIndicatorSource previousSource,
    deskflow::input::InputIndicatorSource source,
    double transitionProgress,
    InputIndicatorTone tone
)
{
  const auto textColor = indicatorTextColor(tone);
  const double center = NSMidX(bounds);
  if (companion != InputIndicatorCompanion::Turtle) {
    [[textColor colorWithAlphaComponent:0.72] setFill];
    NSRectFill(NSMakeRect(center - 5.5, 15, 1.5, 1.5));
    NSRectFill(NSMakeRect(center + 4, 15, 1.5, 1.5));
  }

  const NSRect tagRect = NSMakeRect(center - 7, 0.5, 14, 10);
  NSBezierPath *tag =
      [NSBezierPath bezierPathWithRoundedRect:tagRect
                                     xRadius:4.5
                                     yRadius:4.5];
  [[[NSColor windowBackgroundColor]
      colorWithAlphaComponent:0.78] setFill];
  [tag fill];
  [[NSColor separatorColor] setStroke];
  [tag setLineWidth:0.5];
  [tag stroke];

  NSMutableParagraphStyle *paragraph =
      [[NSMutableParagraphStyle alloc] init];
  paragraph.alignment = NSTextAlignmentCenter;
  const auto font =
      [NSFont systemFontOfSize:macInputIndicatorPanelStyle(companion).labelSize
                        weight:NSFontWeightSemibold];
  const auto drawLabel =
      [paragraph, font, textColor, tagRect](
          deskflow::input::InputIndicatorSource labelSource,
          double alpha
      ) {
        NSString *label = indicatorLabel(labelSource);
        if (!label || alpha <= 0) {
          return;
        }
        NSDictionary *attributes = @{
          NSFontAttributeName : font,
          NSForegroundColorAttributeName :
              [textColor colorWithAlphaComponent:alpha],
          NSParagraphStyleAttributeName : paragraph
        };
        [label drawInRect:tagRect withAttributes:attributes];
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
@property(nonatomic) BOOL reducedMotion;
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
  NSColor *from = deskflow::gui::input::indicatorTint(
      previousSource, tone, self.opacity
  );
  NSColor *to = deskflow::gui::input::indicatorTint(
      source, tone, self.opacity
  );
  NSColor *fill =
      [from blendedColorWithFraction:self.transitionProgress
                            ofColor:to];

  switch (companion) {
  case deskflow::gui::input::InputIndicatorCompanion::Cat:
    deskflow::gui::input::drawCat(
        bounds, fill, self.motionX, self.motionY,
        self.transitionProgress
    );
    break;
  case deskflow::gui::input::InputIndicatorCompanion::Dog:
    deskflow::gui::input::drawDog(
        bounds, fill, self.motionX, self.motionY,
        self.transitionProgress
    );
    break;
  case deskflow::gui::input::InputIndicatorCompanion::Turtle:
    deskflow::gui::input::drawTurtle(
        bounds, fill, self.motionX, self.motionY,
        self.transitionProgress
    );
    break;
  }
  deskflow::gui::input::drawFaceAndTag(
      bounds,
      companion,
      previousSource,
      source,
      self.transitionProgress,
      tone
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
  bool activeSpaceChanged = true;
  bool followerInitialized = false;
  bool previousTargetInitialized = false;
  InputIndicatorPoint follower;
  InputIndicatorPoint previousTarget;
  InputIndicatorPoint motion;
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

  m_impl->companion = options.companion;
  m_impl->previousSource =
      m_impl->source != deskflow::input::InputIndicatorSource::Unknown
          ? m_impl->source
          : options.source;
  m_impl->source = options.source;
  m_impl->transitionStarted = std::chrono::steady_clock::now();
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
  const bool animating =
      transitionProgress < 1.0 ||
      followerDistance > kSettledDistance ||
      distance(m_impl->motion) > kSettledDistance;

  constexpr double kRedrawTolerance = 0.0001;
  const bool viewNeedsDisplay =
      m_impl->activeSpaceChanged ||
      std::abs(
          m_impl->view.transitionProgress - transitionProgress
      ) > kRedrawTolerance ||
      std::abs(m_impl->view.motionX - m_impl->motion.x) >
          kRedrawTolerance ||
      std::abs(m_impl->view.motionY - m_impl->motion.y) >
          kRedrawTolerance;
  m_impl->view.previousSource =
      static_cast<NSInteger>(m_impl->previousSource);
  m_impl->view.source =
      static_cast<NSInteger>(m_impl->source);
  m_impl->view.transitionProgress = transitionProgress;
  m_impl->view.motionX = m_impl->motion.x;
  m_impl->view.motionY = m_impl->motion.y;
  m_impl->view.reducedMotion = reducedMotion;
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
  return {.visible = true, .animating = animating};
}

void MacInputIndicatorPanel::hide()
{
  [m_impl->panel orderOut:nil];
}

} // namespace deskflow::gui::input
