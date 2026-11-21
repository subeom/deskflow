/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXPointerHoldSystem.h"

#include "base/Log.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Foundation/Foundation.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace deskflow::platform {

namespace {

// An exception that is caught at the edge of a callback of the system (an observer block, the timer, the work on the
// run loop of the tap).  One line in the log, once for the whole program (a timer that fails four times per second must
// not fill the log): the pointer is not held for that call, which is the worst that may happen.  Call it from a catch
// block
void reportContainedFailure(const char *where) noexcept
{
  static std::atomic<bool> reported{false};
  if (reported.exchange(true)) {
    return;
  }
  try {
    std::string what = "an unknown error";
    try {
      throw;
    } catch (const std::exception &e) {
      what = e.what();
    } catch (...) {
      // the text above
    }
    LOG_ERR("the pointer of this Mac could not be held: %s failed (%s); reported once", where, what.c_str());
  } catch (...) {
    // nothing more can be done
  }
}

// Runs a function that the system calls back.  Nothing leaves it: an exception that leaves a block of a notification
// center, a run loop or a timer aborts the program
void runContained(const std::function<void()> &work, const char *where) noexcept
{
  try {
    work();
  } catch (...) {
    reportContainedFailure(where);
  }
}

// The run loop of the tap thread, retained for as long as the caller needs it
class TapRunLoop
{
public:
  explicit TapRunLoop(const OSXPointerHoldHooks &hooks) : m_runLoop(hooks.tapRunLoop ? hooks.tapRunLoop() : nullptr)
  {
    // do nothing
  }
  ~TapRunLoop()
  {
    if (m_runLoop != nullptr) {
      CFRelease(m_runLoop);
    }
  }
  TapRunLoop(const TapRunLoop &) = delete;
  TapRunLoop &operator=(const TapRunLoop &) = delete;

  CFRunLoopRef get() const
  {
    return m_runLoop;
  }

private:
  CFRunLoopRef m_runLoop;
};

} // namespace

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
  if (event == nullptr) {
    // CFRelease(nullptr) crashes: the caller (the controller) gives this look up
    throw std::runtime_error("the place of the cursor cannot be read");
  }
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
  // only for the line of the log: when the front application cannot be asked, the line does not name it
  try {
    @autoreleasepool {
      NSRunningApplication *front = [[NSWorkspace sharedWorkspace] frontmostApplication];
      NSString *identifier = [front bundleIdentifier];
      return identifier != nil ? std::string([identifier UTF8String]) : std::string();
    }
  } catch (...) {
    return {};
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
  const TapRunLoop runLoop(m_hooks);
  if (runLoop.get() == nullptr) {
    return;
  }
  if (CFRunLoopGetCurrent() == runLoop.get()) {
    work();
    return;
  }
  // `work` is a parameter of this function, so the block copies it: the block can run after this function and its
  // caller are gone.  (A variable of an enclosing lambda would be captured by reference: see the Notifications below.)
  CFRunLoopPerformBlock(runLoop.get(), kCFRunLoopDefaultMode, ^{
    runContained(work, "the work on the thread of the event tap");
  });
  CFRunLoopWakeUp(runLoop.get());
}

void OSXPointerHoldSystem::startTimer(double intervalSeconds, std::function<void()> tick)
{
  std::scoped_lock lock(m_timerMutex);
  stopTimerLocked();
  const TapRunLoop runLoop(m_hooks);
  if (runLoop.get() == nullptr) {
    return;
  }
  // `tick` is a parameter: the block (and the timer) own a copy
  CFRunLoopTimerRef timer = CFRunLoopTimerCreateWithHandler(
      kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + intervalSeconds, intervalSeconds, 0, 0, ^(CFRunLoopTimerRef) {
        runContained(tick, "the timer of the hold");
      }
  );
  if (timer == nullptr) {
    return;
  }
  CFRunLoopAddTimer(runLoop.get(), timer, kCFRunLoopDefaultMode);
  m_timer = timer;
}

void OSXPointerHoldSystem::stopTimer()
{
  std::scoped_lock lock(m_timerMutex);
  stopTimerLocked();
}

void OSXPointerHoldSystem::stopTimerLocked()
{
  if (m_timer != nullptr) {
    CFRunLoopTimerInvalidate(m_timer);
    CFRelease(m_timer);
    m_timer = nullptr;
  }
}

namespace {

// What the observer blocks share with the object that made them.  Every block holds a std::shared_ptr to it BY VALUE,
// so that it outlives the object, and the blocks that are still queued when the object is destroyed find a silenced
// callback and nothing else.  It is the only thing that a block touches
struct Shared
{
  //! The object that owns this (to tell a callback that stops its own source)
  const void *owner = nullptr;
  std::mutex mutex;
  std::condition_variable idle;
  IPointerHoldNotifications::Callback callback;
  //! The deliveries that are inside the callback right now
  int running = 0;
};

// The delivery that this thread is inside of (null outside of a callback)
thread_local Shared *t_delivering = nullptr;

// Hands one notification to the callback.  Holds no lock while the callback runs, counts itself in \c running so that
// stop() can wait for it, and lets nothing escape
void deliver(const std::shared_ptr<Shared> &shared, PointerHoldTrigger trigger, const std::string &app) noexcept
{
  try {
    IPointerHoldNotifications::Callback current;
    {
      std::scoped_lock lock(shared->mutex);
      if (!shared->callback) {
        return;
      }
      current = shared->callback;
      ++shared->running;
    }

    // leaves the delivery on every way out; the callback is released first, so that what it captured is gone when
    // stop() is let through
    struct Leave
    {
      Shared &shared;
      IPointerHoldNotifications::Callback &callback;
      Shared *outer;
      ~Leave()
      {
        t_delivering = outer;
        callback = nullptr;
        try {
          std::scoped_lock lock(shared.mutex);
          --shared.running;
          shared.idle.notify_all();
        } catch (...) {
          // the mutex cannot be locked: nothing more can be done
        }
      }
    } leave{*shared, current, std::exchange(t_delivering, shared.get())};

    current(trigger, app);
  } catch (...) {
    reportContainedFailure("a notification of the system");
  }
}

// The observer of one notification.  The block is made here, in a function whose parameters it copies: `shared` is a
// copy that the block owns.  The block must never name a local of the function that calls this, or of a lambda around
// it: a block captures a C++ lambda that an enclosing lambda holds by reference BY REFERENCE, and that is a stack slot
// that is gone when the function returns (the crash of 2.0.4: every notification read a dead slot and locked a mutex
// that was not there)
void addObserver(
    NSNotificationCenter *center, NSMutableArray *tokens, NSOperationQueue *queue, NSString *name,
    PointerHoldTrigger trigger, bool carriesApplication, std::shared_ptr<Shared> shared
)
{
  id token = [center addObserverForName:name
                                 object:nil
                                  queue:queue
                             usingBlock:^(NSNotification *note) {
                               try {
                                 std::string app;
                                 if (carriesApplication) {
                                   id running = [[note userInfo] objectForKey:NSWorkspaceApplicationKey];
                                   if ([running isKindOfClass:[NSRunningApplication class]]) {
                                     NSString *identifier =
                                         [static_cast<NSRunningApplication *>(running) bundleIdentifier];
                                     if (identifier != nil) {
                                       app = [identifier UTF8String];
                                     }
                                   }
                                 }
                                 deliver(shared, trigger, app);
                               } catch (...) {
                                 reportContainedFailure("an observer of the notifications");
                               }
                             }];
  if (token != nil) {
    [tokens addObject:token];
  }
}

} // namespace

//
// OSXPointerHoldNotifications
//

struct OSXPointerHoldNotifications::Impl
{
  //! start() and stop() one at a time
  std::mutex control;
  std::shared_ptr<Shared> shared;
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
  Impl *impl = m_impl.get();
  try {
    std::scoped_lock control(impl->control);
    stopLocked(*impl);

    auto shared = std::make_shared<Shared>();
    shared->owner = impl;
    shared->callback = std::move(callback);
    impl->shared = shared;

    @autoreleasepool {
      impl->queue = [[NSOperationQueue alloc] init];
      [impl->queue setMaxConcurrentOperationCount:1];
      impl->workspaceTokens = [[NSMutableArray alloc] init];
      impl->distributedTokens = [[NSMutableArray alloc] init];

      NSNotificationCenter *workspace = [[NSWorkspace sharedWorkspace] notificationCenter];
      NSMutableArray *workspaceTokens = impl->workspaceTokens;
      NSOperationQueue *queue = impl->queue;
      addObserver(
          workspace, workspaceTokens, queue, NSWorkspaceDidActivateApplicationNotification,
          PointerHoldTrigger::AppActivated, true, shared
      );
      addObserver(
          workspace, workspaceTokens, queue, NSWorkspaceActiveSpaceDidChangeNotification,
          PointerHoldTrigger::SpaceChanged, false, shared
      );
      addObserver(
          workspace, workspaceTokens, queue, NSWorkspaceScreensDidWakeNotification, PointerHoldTrigger::DisplayWoke,
          false, shared
      );
      addObserver(
          workspace, workspaceTokens, queue, NSWorkspaceScreensDidSleepNotification, PointerHoldTrigger::DisplaySlept,
          false, shared
      );
      addObserver(
          workspace, workspaceTokens, queue, NSWorkspaceDidWakeNotification, PointerHoldTrigger::SystemWoke, false,
          shared
      );

      NSNotificationCenter *distributed = [NSDistributedNotificationCenter defaultCenter];
      NSMutableArray *distributedTokens = impl->distributedTokens;
      addObserver(
          distributed, distributedTokens, queue, @"com.apple.screenIsLocked", PointerHoldTrigger::ScreenLocked, false,
          shared
      );
      addObserver(
          distributed, distributedTokens, queue, @"com.apple.screenIsUnlocked", PointerHoldTrigger::ScreenUnlocked,
          false, shared
      );
    }
  } catch (...) {
    // what was made so far is taken down again: a source that cannot listen is a source that is silent
    try {
      stop();
    } catch (...) {
      // nothing more can be done
    }
    throw;
  }
}

void OSXPointerHoldNotifications::stop()
{
  Impl *impl = m_impl.get();

  // a callback that stops its own source cannot wait for itself, nor take the lock of a start() that waits for it: it
  // is silenced, and the observers go with the next stop() or the destructor
  if (Shared *delivering = t_delivering; delivering != nullptr && delivering->owner == impl) {
    std::scoped_lock lock(delivering->mutex);
    delivering->callback = nullptr;
    return;
  }

  std::scoped_lock control(impl->control);
  stopLocked(*impl);
}

void OSXPointerHoldNotifications::stopLocked(Impl &impl)
{
  // 1. silence: a block that runs from now on finds nothing to call.  The lock is let go before anything of AppKit is
  //    called: the thread that posts a notification waits for the observers, and it must never wait for this lock
  const auto shared = std::move(impl.shared);
  impl.shared.reset();
  if (shared) {
    std::scoped_lock lock(shared->mutex);
    shared->callback = nullptr;
  }

  // 2. no new delivery
  @autoreleasepool {
    for (id token in impl.workspaceTokens) {
      [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:token];
    }
    for (id token in impl.distributedTokens) {
      [[NSDistributedNotificationCenter defaultCenter] removeObserver:token];
    }
  }

  // 3. the ones that are inside the callback are done (the callback may use what its owner is about to destroy)
  if (shared) {
    std::unique_lock lock(shared->mutex);
    shared->idle.wait(lock, [&shared] { return shared->running == 0; });
  }

  // 4. what is left of the blocks that are queued finds no callback; the queue and the tokens can go
  [impl.workspaceTokens release];
  impl.workspaceTokens = nil;
  [impl.distributedTokens release];
  impl.distributedTokens = nil;
  [impl.queue release];
  impl.queue = nil;
}

} // namespace deskflow::platform
