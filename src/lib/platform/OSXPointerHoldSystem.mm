/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXPointerHoldSystem.h"

#include "base/Log.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include <chrono>
#include <utility>

namespace deskflow::platform {

//
// OSXPointerHoldSystem
//

OSXPointerHoldSystem::OSXPointerHoldSystem(OSXPointerHoldHooks hooks) : m_hooks(std::move(hooks))
{
  // do nothing
}

OSXPointerHoldSystem::~OSXPointerHoldSystem()
{
  stopTimer();
}

PointerHoldPoint OSXPointerHoldSystem::cursorPosition()
{
  CGEventRef event = CGEventCreate(nullptr);
  const CGPoint where = CGEventGetLocation(event);
  CFRelease(event);
  return {where.x, where.y};
}

bool OSXPointerHoldSystem::tapEnabled()
{
  const auto port = m_hooks.tapPort ? m_hooks.tapPort() : nullptr;
  // no tap, nothing to switch on
  return port == nullptr || CGEventTapIsEnabled(port);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

bool OSXPointerHoldSystem::cursorShown()
{
  // deprecated, and said to be unreliable, but it is what tells the hidden cursor from the shown one after
  // OSXComputer::hideCursor() (logCursorVisibility() relies on it)
  return CGCursorIsVisible();
}

void OSXPointerHoldSystem::prepareWarp()
{
  // the same interval as OSXComputer::leave(): a warp must not hold the next local events back
  CGSetLocalEventsSuppressionInterval(0.0001);
}

#pragma clang diagnostic pop

void OSXPointerHoldSystem::enableTap()
{
  const auto port = m_hooks.tapPort ? m_hooks.tapPort() : nullptr;
  // OSXComputer::checkAXPermissions() quits the program when the permission is gone: do not switch on a tap without it
  if (port != nullptr && AXIsProcessTrusted()) {
    CGEventTapEnable(port, true);
  }
}

void OSXPointerHoldSystem::dissociate()
{
  CGAssociateMouseAndMouseCursorPosition(false);
}

void OSXPointerHoldSystem::warp(PointerHoldPoint where)
{
  CGWarpMouseCursorPosition(CGPointMake(where.x, where.y));
}

void OSXPointerHoldSystem::hideAgain()
{
  if (m_hooks.hideAgain) {
    m_hooks.hideAgain();
  }
}

std::string OSXPointerHoldSystem::frontApplication()
{
  @autoreleasepool {
    NSRunningApplication *front = [[NSWorkspace sharedWorkspace] frontmostApplication];
    NSString *identifier = [front bundleIdentifier];
    return identifier != nil ? std::string([identifier UTF8String]) : std::string();
  }
}

double OSXPointerHoldSystem::now()
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void OSXPointerHoldSystem::log(const std::string &line)
{
  LOG_WARN("%s", line.c_str());
}

void OSXPointerHoldSystem::runOnTapThread(std::function<void()> work)
{
  const auto runLoop = m_hooks.tapRunLoop ? m_hooks.tapRunLoop() : nullptr;
  if (runLoop == nullptr) {
    return;
  }
  if (CFRunLoopGetCurrent() == runLoop) {
    work();
    return;
  }
  CFRunLoopPerformBlock(runLoop, kCFRunLoopDefaultMode, ^{
    work();
  });
  CFRunLoopWakeUp(runLoop);
}

void OSXPointerHoldSystem::startTimer(double intervalSeconds, std::function<void()> tick)
{
  stopTimer();
  const auto runLoop = m_hooks.tapRunLoop ? m_hooks.tapRunLoop() : nullptr;
  if (runLoop == nullptr) {
    return;
  }
  CFRunLoopTimerRef timer = CFRunLoopTimerCreateWithHandler(
      kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + intervalSeconds, intervalSeconds, 0, 0,
      ^(CFRunLoopTimerRef) {
        tick();
      }
  );
  if (timer == nullptr) {
    return;
  }
  CFRunLoopAddTimer(runLoop, timer, kCFRunLoopDefaultMode);
  std::scoped_lock lock(m_timerMutex);
  m_timer = timer;
}

void OSXPointerHoldSystem::stopTimer()
{
  std::scoped_lock lock(m_timerMutex);
  if (m_timer != nullptr) {
    CFRunLoopTimerInvalidate(m_timer);
    CFRelease(m_timer);
    m_timer = nullptr;
  }
}

//
// OSXPointerHoldNotifications
//

struct OSXPointerHoldNotifications::Impl
{
  std::mutex mutex;
  Callback callback;
  NSOperationQueue *queue = nil;
  NSMutableArray *workspaceTokens = nil;
  NSMutableArray *distributedTokens = nil;
};

OSXPointerHoldNotifications::OSXPointerHoldNotifications() : m_impl(std::make_unique<Impl>())
{
  // do nothing
}

OSXPointerHoldNotifications::~OSXPointerHoldNotifications()
{
  stop();
}

void OSXPointerHoldNotifications::start(Callback callback)
{
  stop();
  @autoreleasepool {
    Impl *impl = m_impl.get();
    {
      std::scoped_lock lock(impl->mutex);
      impl->callback = std::move(callback);
    }
    impl->queue = [[NSOperationQueue alloc] init];
    [impl->queue setMaxConcurrentOperationCount:1];
    impl->workspaceTokens = [[NSMutableArray alloc] init];
    impl->distributedTokens = [[NSMutableArray alloc] init];

    // the callback is looked up when a notification arrives, so that stop() can silence the ones that are in flight
    auto fire = [impl](PointerHoldTrigger trigger, const std::string &app) {
      Callback current;
      {
        std::scoped_lock lock(impl->mutex);
        current = impl->callback;
      }
      if (current) {
        current(trigger, app);
      }
    };

    auto add = [&](NSNotificationCenter *center, NSMutableArray *tokens, NSString *name, PointerHoldTrigger trigger,
                   bool carriesApplication) {
      id token = [center addObserverForName:name
                                     object:nil
                                      queue:impl->queue
                                 usingBlock:^(NSNotification *note) {
                                   std::string app;
                                   if (carriesApplication) {
                                     NSRunningApplication *running =
                                         [[note userInfo] objectForKey:NSWorkspaceApplicationKey];
                                     NSString *identifier = [running bundleIdentifier];
                                     if (identifier != nil) {
                                       app = [identifier UTF8String];
                                     }
                                   }
                                   fire(trigger, app);
                                 }];
      if (token != nil) {
        [tokens addObject:token];
      }
    };

    NSNotificationCenter *workspace = [[NSWorkspace sharedWorkspace] notificationCenter];
    add(workspace, impl->workspaceTokens, NSWorkspaceDidActivateApplicationNotification,
        PointerHoldTrigger::AppActivated, true);
    add(workspace, impl->workspaceTokens, NSWorkspaceActiveSpaceDidChangeNotification,
        PointerHoldTrigger::SpaceChanged, false);
    add(workspace, impl->workspaceTokens, NSWorkspaceScreensDidWakeNotification, PointerHoldTrigger::DisplayWoke,
        false);
    add(workspace, impl->workspaceTokens, NSWorkspaceScreensDidSleepNotification, PointerHoldTrigger::DisplaySlept,
        false);
    add(workspace, impl->workspaceTokens, NSWorkspaceDidWakeNotification, PointerHoldTrigger::SystemWoke, false);

    NSNotificationCenter *distributed = [NSDistributedNotificationCenter defaultCenter];
    add(distributed, impl->distributedTokens, @"com.apple.screenIsLocked", PointerHoldTrigger::ScreenLocked, false);
    add(distributed, impl->distributedTokens, @"com.apple.screenIsUnlocked", PointerHoldTrigger::ScreenUnlocked,
        false);
  }
}

void OSXPointerHoldNotifications::stop()
{
  Impl *impl = m_impl.get();
  {
    std::scoped_lock lock(impl->mutex);
    impl->callback = nullptr;
  }
  @autoreleasepool {
    for (id token in impl->workspaceTokens) {
      [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:token];
    }
    for (id token in impl->distributedTokens) {
      [[NSDistributedNotificationCenter defaultCenter] removeObserver:token];
    }
  }
  [impl->workspaceTokens release];
  impl->workspaceTokens = nil;
  [impl->distributedTokens release];
  impl->distributedTokens = nil;
  [impl->queue release];
  impl->queue = nil;
}

} // namespace deskflow::platform
