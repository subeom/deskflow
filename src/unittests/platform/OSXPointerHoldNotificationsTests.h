/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

// The notifications of the system that the hold of the pointer of a Mac server listens to
// (OSXPointerHoldNotifications), with the real observers of the real notification center of NSWorkspace of this
// process, and synthetic notifications posted to it.  That center is per process: only this test process sees them. The
// distributed center (the lock screen) is system wide and is never posted to, the real event tap is never created, and
// the cursor is never touched: the system that the controller calls is a fake.
class OSXPointerHoldNotificationsTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  // the log of the program: the observers report a failure that they contain through it
  void initTestCase();
  void cleanupTestCase();

  // the crash of 2.0.4: the observers ran after start() had returned, and read the stack of start()
  void notification_afterStartReturned_reachesTheCallback();
  void notification_everyWorkspaceTrigger_reachesTheCallbackWithItsTrigger();
  void notification_theApplicationThatWasActivated_isPassedOn();

  // stop, restart, throwing and re-entrant callbacks
  void stop_silencesTheNotificationsAndReleasesTheCallback();
  void start_aSecondTime_replacesTheCallback();
  void callback_thatThrows_doesNotTakeTheProcessDown();
  void stop_fromTheCallback_doesNotDeadlockAndSilencesIt();
  void destroying_withoutStarting_isFine();

  // the owner goes away while notifications are in flight
  void destroying_whileNotificationsAreInFlight_neverCallsTheCallbackAfterwards();
  void stopping_waitsForTheCallbackThatIsRunning();

  // the real glue of the system to a run loop (OSXPointerHoldSystem), on a stand-in for the thread of the event tap:
  // work and timers run on that thread, nothing that they throw leaves them, and they do not outlive the system.  Only
  // the calls of the system that touch the cursor or the tap are not made
  void system_work_runsOnTheThreadOfTheRunLoop();
  void system_work_thatThrows_isContainedAndTheLoopGoesOn();
  void system_theTimer_ticksOnTheRunLoopUntilItIsStopped();
  void system_aTick_thatThrows_isContainedAndTheTimerGoesOn();
  void system_destroyedWithWorkQueuedAndATimerDue_runsNoTickAndStillRunsTheWork();
  void system_withoutARunLoop_doesNothing();

  // the controller on top of the real source, with a fake system
  void controller_aNotification_holdsThePointerAgain();
  void controller_destroyedWhileNotificationsAreInFlight_neverTouchesTheSystemAfterwards();
};
