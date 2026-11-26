/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "platform/OSXPointerHold.h"

#include <CoreFoundation/CoreFoundation.h>

#include <atomic>
#include <mutex>
#include <thread>
namespace OSXPointerHoldTestSupport {

//! Posts what the system posts for \p trigger on the notification center of NSWorkspace of THIS process.
/*!
That center is per process, so only this test process sees the notification.  The posting thread waits until the
observers that asked for a queue have run (that is how the center works).  Only the workspace triggers can be posted
(AppActivated, SpaceChanged, DisplayWoke, DisplaySlept, SystemWoke).  The lock screen triggers come from the distributed
center, which is system wide: they are never posted, and asking for one does nothing.
*/
void postWorkspaceNotification(deskflow::platform::PointerHoldTrigger trigger);

//! Writes over the stack below the caller, as any later use of the thread does
void churnStack();

//! A thread with a run loop of its own, in the place of the thread of the event tap
/*!
It has no event tap and no source of events, and nothing of the cursor: it only runs what is posted to its run loop and
its timers.  The run loop is handed out retained, as OSXComputer hands out the one of the tap thread.
*/
class RunLoopThread
{
public:
  RunLoopThread();
  ~RunLoopThread();
  RunLoopThread(const RunLoopThread &) = delete;
  RunLoopThread &operator=(const RunLoopThread &) = delete;

  //! The run loop with a retain that the caller releases, or null once the thread has ended
  CFRunLoopRef retainRunLoop();
  std::thread::id threadId() const;
  //! Ends the run loop and joins the thread
  void stop();

private:
  std::mutex m_mutex;
  CFRunLoopRef m_runLoop = nullptr;
  std::thread::id m_id;
  std::atomic<bool> m_ready{false};
  std::thread m_thread;
};

} // namespace OSXPointerHoldTestSupport
