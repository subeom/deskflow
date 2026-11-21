/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "platform/OSXPointerHold.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#include <functional>
#include <memory>
#include <mutex>

namespace deskflow::platform {

//! What the system adapter needs from the computer that owns the event tap
struct OSXPointerHoldHooks
{
  //! The event tap (null when there is none)
  std::function<CFMachPortRef()> tapPort;
  //! The run loop of the tap thread (null when the thread does not run)
  std::function<CFRunLoopRef()> tapRunLoop;
  //! Hides the cursor again and keeps the hide count at one
  std::function<void()> hideAgain;
};

//! The calls of the hold into CoreGraphics, AppKit and the run loop of the event tap
/*!
Only used by the program (the tests use a fake).  The calls are the ones that OSXComputer::leave() already makes; none
of them is made until the pointer is parked and the system has given it back.
*/
class OSXPointerHoldSystem final : public IPointerHoldSystem
{
public:
  explicit OSXPointerHoldSystem(OSXPointerHoldHooks hooks);
  ~OSXPointerHoldSystem() override;

  PointerHoldPoint cursorPosition() override;
  bool tapEnabled() override;
  bool cursorShown() override;
  void enableTap() override;
  void prepareWarp() override;
  void dissociate() override;
  void warp(PointerHoldPoint where) override;
  void hideAgain() override;
  std::string frontApplication() override;
  double now() override;
  void log(const std::string &line) override;
  void runOnTapThread(std::function<void()> work) override;
  void startTimer(double intervalSeconds, std::function<void()> tick) override;
  void stopTimer() override;

private:
  OSXPointerHoldHooks m_hooks;
  std::mutex m_timerMutex;
  CFRunLoopTimerRef m_timer = nullptr;
};

//! The notifications of the system that mean that the pointer may have been given back
/*!
Workspace notifications (the application that is active, the space, the displays, the Mac waking) and the distributed
notifications of the lock screen.  They are delivered on a queue of their own and handed to the callback.
*/
class OSXPointerHoldNotifications final : public IPointerHoldNotifications
{
public:
  OSXPointerHoldNotifications();
  ~OSXPointerHoldNotifications() override;

  void start(Callback callback) override;
  void stop() override;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::platform
