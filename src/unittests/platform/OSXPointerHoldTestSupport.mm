/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXPointerHoldTestSupport.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <chrono>
namespace OSXPointerHoldTestSupport {

using deskflow::platform::PointerHoldTrigger;

void postWorkspaceNotification(PointerHoldTrigger trigger)
{
  @autoreleasepool {
    NSString *name = nil;
    NSDictionary *info = nil;
    switch (trigger) {
    case PointerHoldTrigger::AppActivated:
      name = NSWorkspaceDidActivateApplicationNotification;
      info = [NSDictionary dictionaryWithObject:[NSRunningApplication currentApplication]
                                         forKey:NSWorkspaceApplicationKey];
      break;
    case PointerHoldTrigger::SpaceChanged:
      name = NSWorkspaceActiveSpaceDidChangeNotification;
      break;
    case PointerHoldTrigger::DisplayWoke:
      name = NSWorkspaceScreensDidWakeNotification;
      break;
    case PointerHoldTrigger::DisplaySlept:
      name = NSWorkspaceScreensDidSleepNotification;
      break;
    case PointerHoldTrigger::SystemWoke:
      name = NSWorkspaceDidWakeNotification;
      break;
    default:
      // the others are not workspace notifications; the distributed ones are system wide and are never posted
      return;
    }
    [[[NSWorkspace sharedWorkspace] notificationCenter] postNotificationName:name object:nil userInfo:info];
  }
}

__attribute__((noinline)) void churnStack()
{
  volatile char buffer[16384];
  for (unsigned i = 0; i < sizeof(buffer); ++i) {
    buffer[i] = static_cast<char>(0xA5);
  }
}

RunLoopThread::RunLoopThread()
{
  m_thread = std::thread([this] {
    CFRunLoopRef runLoop = CFRunLoopGetCurrent();
    // a run loop without a source or a timer returns at once: this one is kept alive by a timer that never fires
    CFRunLoopTimerRef keepAlive = CFRunLoopTimerCreateWithHandler(
        kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 1.0e9, 1.0e9, 0, 0, ^(CFRunLoopTimerRef){}
    );
    CFRunLoopAddTimer(runLoop, keepAlive, kCFRunLoopDefaultMode);
    {
      std::scoped_lock lock(m_mutex);
      m_runLoop = static_cast<CFRunLoopRef>(const_cast<void *>(CFRetain(runLoop)));
      m_id = std::this_thread::get_id();
    }
    m_ready = true;
    CFRunLoopRun();
    {
      std::scoped_lock lock(m_mutex);
      CFRelease(m_runLoop);
      m_runLoop = nullptr;
    }
    CFRunLoopTimerInvalidate(keepAlive);
    CFRelease(keepAlive);
  });
  while (!m_ready.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

RunLoopThread::~RunLoopThread()
{
  stop();
}

CFRunLoopRef RunLoopThread::retainRunLoop()
{
  std::scoped_lock lock(m_mutex);
  return m_runLoop != nullptr ? static_cast<CFRunLoopRef>(const_cast<void *>(CFRetain(m_runLoop))) : nullptr;
}

std::thread::id RunLoopThread::threadId() const
{
  return m_id;
}

void RunLoopThread::stop()
{
  if (!m_thread.joinable()) {
    return;
  }
  // stopped from inside, so that it also works before the loop has started to run
  if (CFRunLoopRef runLoop = retainRunLoop()) {
    CFRunLoopPerformBlock(runLoop, kCFRunLoopDefaultMode, ^{
      CFRunLoopStop(CFRunLoopGetCurrent());
    });
    CFRunLoopWakeUp(runLoop);
    CFRelease(runLoop);
  }
  m_thread.join();
}

} // namespace OSXPointerHoldTestSupport
