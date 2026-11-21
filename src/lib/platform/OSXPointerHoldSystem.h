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
  //! The run loop of the tap thread, with a retain that the caller releases (null when the thread does not run): the
  //! thread can end at any moment, and a run loop that is not retained is gone with it
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
  void stopTimerLocked();

  OSXPointerHoldHooks m_hooks;
  std::mutex m_timerMutex;
  CFRunLoopTimerRef m_timer = nullptr;
};

//! The notifications of the system that mean that the pointer may have been given back
/*!
Workspace notifications (the application that is active, the space, the displays, the Mac waking) and the distributed
notifications of the lock screen.  They are delivered on a queue of their own and handed to the callback.

The center posts a notification on the thread of the poster, and the poster waits until the observer of a queue has run
(the main thread waits for the observers of the activation of an application), so an observer is short, takes no lock
that a caller of the program can hold, and never throws.  An observer block holds the state that it needs by value
(\c Shared), never a pointer to this object or to a local of the function that made it: it can run after both are gone.
\c stop() silences the callback first, then removes the observers, then waits until a callback that is running is done.
*/
class OSXPointerHoldNotifications final : public IPointerHoldNotifications
{
public:
  OSXPointerHoldNotifications();
  ~OSXPointerHoldNotifications() override;

  //! Listens again with \p callback (what was started before is stopped); it is called on a queue of its own
  void start(Callback callback) override;
  //! From any thread, one call at a time; from the callback it only silences the callback (see
  //! IPointerHoldNotifications)
  void stop() override;

private:
  struct Impl;
  static void stopLocked(Impl &impl);
  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::platform
