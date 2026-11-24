/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacInputIndicatorPanel.h"

#include <AppKit/AppKit.h>

#include <algorithm>
#include <vector>

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
@property(copy) NSString *label;
@property(nonatomic) NSInteger tone;
@property(nonatomic) double opacity;
@end

@implementation DeskflowInputIndicatorView

- (void)dealloc
{
  [_label release];
  [super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect
{
  (void)dirtyRect;
  NSRect bounds = self.bounds;
  NSColor *background = nil;
  NSColor *text = nil;
  switch (static_cast<deskflow::gui::input::InputIndicatorTone>(self.tone)) {
  case deskflow::gui::input::InputIndicatorTone::Light:
    background = [NSColor colorWithWhite:0.98 alpha:self.opacity];
    text = [NSColor colorWithWhite:0.08 alpha:1.0];
    break;
  case deskflow::gui::input::InputIndicatorTone::Dark:
    background = [NSColor colorWithWhite:0.08 alpha:self.opacity];
    text = [NSColor colorWithWhite:0.98 alpha:1.0];
    break;
  case deskflow::gui::input::InputIndicatorTone::Automatic:
    background = [[NSColor controlAccentColor]
        colorWithAlphaComponent:self.opacity * 0.55];
    text = [NSColor labelColor];
    break;
  }

  const auto style =
      deskflow::gui::input::macInputIndicatorPanelStyle();
  [background setFill];
  [[NSBezierPath bezierPathWithRoundedRect:bounds
                                  xRadius:style.cornerRadius
                                  yRadius:style.cornerRadius] fill];

  NSMutableParagraphStyle *paragraph = [[NSMutableParagraphStyle alloc] init];
  paragraph.alignment = NSTextAlignmentCenter;
  NSDictionary *attributes = @{
    NSFontAttributeName : [NSFont systemFontOfSize:style.labelSize
                                            weight:NSFontWeightSemibold],
    NSForegroundColorAttributeName : text,
    NSParagraphStyleAttributeName : paragraph
  };
  const auto textRect = NSInsetRect(bounds, 2, 2);
  [self.label drawInRect:textRect withAttributes:attributes];
  [paragraph release];
}

- (BOOL)isOpaque
{
  return NO;
}

@end

namespace deskflow::gui::input {

namespace {

constexpr double kCatchUpFactor = 0.5;
constexpr double kMaximumTrailingLag = 18;

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

std::vector<InputIndicatorRect> screenRects()
{
  std::vector<InputIndicatorRect> result;
  for (NSScreen *screen in NSScreen.screens) {
    const auto frame = screen.frame;
    result.push_back({.x = frame.origin.x, .y = frame.origin.y, .width = frame.size.width, .height = frame.size.height});
  }
  return result;
}

} // namespace

struct MacInputIndicatorPanel::Impl
{
  struct Lifetime
  {
    Impl *owner = nullptr;
    bool active = true;
  };

  Impl()
  {
    lifetime = std::make_shared<Lifetime>();
    lifetime->owner = this;
    const auto style = macInputIndicatorPanelStyle();
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

    view = [[DeskflowInputIndicatorView alloc] initWithFrame:panel.contentView.bounds];
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
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
  bool activeSpaceChanged = true;
  bool followerInitialized = false;
  InputIndicatorPoint follower;
};

MacInputIndicatorPanel::MacInputIndicatorPanel() : m_impl(std::make_unique<Impl>())
{
}

MacInputIndicatorPanel::~MacInputIndicatorPanel() = default;

bool MacInputIndicatorPanel::show(const MacInputIndicatorPanelOptions &options)
{
  NSString *label = indicatorLabel(options.source);
  if (!label) {
    hide();
    return false;
  }

  m_impl->view.label = label;
  m_impl->view.tone = static_cast<NSInteger>(options.tone);
  m_impl->view.opacity = std::clamp(options.opacityPercent, 60, 100) / 100.0;
  m_impl->activeSpaceChanged = true;
  m_impl->followerInitialized = false;
  [m_impl->view setNeedsDisplay:YES];
  return move(options.position);
}

bool MacInputIndicatorPanel::move(InputIndicatorPosition position)
{
  const auto style = macInputIndicatorPanelStyle();
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
    return false;
  }

  if (!m_impl->followerInitialized) {
    m_impl->follower = *target;
    m_impl->followerInitialized = true;
  } else {
    const auto follower = followInputIndicator(
        m_impl->follower,
        *target,
        kCatchUpFactor,
        kMaximumTrailingLag
    );
    if (!follower) {
      hide();
      return false;
    }
    m_impl->follower = *follower;
  }

  [m_impl->panel setFrameOrigin:NSMakePoint(m_impl->follower.x, m_impl->follower.y)];
  const bool visible = static_cast<bool>(
      [m_impl->panel isVisible]
  );
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
  if (![m_impl->panel isVisible]) {
    hide();
    return false;
  }
  m_impl->activeSpaceChanged = false;
  return true;
}

void MacInputIndicatorPanel::hide()
{
  [m_impl->panel orderOut:nil];
}

} // namespace deskflow::gui::input
