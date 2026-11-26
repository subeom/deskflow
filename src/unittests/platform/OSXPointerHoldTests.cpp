/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXPointerHoldTests.h"

#include "common/Settings.h"
#include "platform/OSXPointerHold.h"

#include <QStringList>
#include <QTest>

#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace deskflow::platform;
using Trigger = PointerHoldTrigger;

namespace {

constexpr PointerHoldPoint kParked{100.0, 200.0};

PointerHoldObservation observation(
    Trigger trigger, PointerHoldPoint cursor, double now, bool tapEnabled = true, bool cursorShown = false
)
{
  PointerHoldObservation o;
  o.trigger = trigger;
  o.cursor = cursor;
  o.now = now;
  o.tapEnabled = tapEnabled;
  o.cursorShown = cursorShown;
  return o;
}

// The system as a list of what was called.  The cursor is a number here; a warp puts it where it was told to
class FakeSystem : public IPointerHoldSystem
{
public:
  PointerHoldPoint cursor = kParked;
  bool tap = true;
  bool shown = false;
  double clock = 0.0;
  std::string front;
  //! false: the warp is lost (the system keeps the cursor where it is)
  bool warpTakesEffect = true;
  //! false: what is posted to the tap thread waits for drain()
  bool runInline = true;

  QStringList calls;
  QStringList lines;
  int cursorReads = 0;
  int timersStarted = 0;
  int timersStopped = 0;
  double timerInterval = 0.0;
  std::function<void()> tick;
  std::vector<std::function<void()>> posted;

  PointerHoldPoint cursorPosition() override
  {
    ++cursorReads;
    return cursor;
  }
  bool tapEnabled() override
  {
    return tap;
  }
  bool cursorShown() override
  {
    return shown;
  }
  void enableTap() override
  {
    calls << QStringLiteral("enableTap");
    tap = true;
  }
  void prepareWarp() override
  {
    calls << QStringLiteral("prepareWarp");
  }
  void dissociate() override
  {
    calls << QStringLiteral("dissociate");
  }
  void warp(PointerHoldPoint where) override
  {
    calls << QStringLiteral("warp %1,%2").arg(where.x).arg(where.y);
    if (warpTakesEffect) {
      cursor = where;
    }
  }
  void hideAgain() override
  {
    calls << QStringLiteral("hideAgain");
    shown = false;
  }
  std::string frontApplication() override
  {
    return front;
  }
  double now() override
  {
    return clock;
  }
  void log(const std::string &line) override
  {
    lines << QString::fromStdString(line);
  }
  void runOnTapThread(std::function<void()> work) override
  {
    if (runInline) {
      work();
    } else {
      posted.push_back(std::move(work));
    }
  }
  void startTimer(double intervalSeconds, std::function<void()> t) override
  {
    ++timersStarted;
    timerInterval = intervalSeconds;
    tick = std::move(t);
  }
  void stopTimer() override
  {
    ++timersStopped;
    tick = nullptr;
  }

  void drain()
  {
    auto work = std::move(posted);
    posted.clear();
    for (auto &w : work) {
      w();
    }
  }
};

class FakeNotifications : public IPointerHoldNotifications
{
public:
  Callback callback;
  int started = 0;
  int stopped = 0;

  void start(Callback c) override
  {
    ++started;
    callback = std::move(c);
  }
  void stop() override
  {
    ++stopped;
    callback = nullptr;
  }
  //! what the system does: it calls whatever was given to start() and has not been stopped
  void fire(Trigger trigger, const std::string &app = {})
  {
    if (callback) {
      callback(trigger, app);
    }
  }
};

// A system whose calls can be made to throw, as a system call that fails in an unforeseen way does
class ThrowingSystem : public FakeSystem
{
public:
  bool throwOnCursor = false;
  bool throwOnWarp = false;
  bool throwOnPost = false;

  PointerHoldPoint cursorPosition() override
  {
    if (throwOnCursor) {
      throw std::runtime_error("the cursor cannot be read");
    }
    return FakeSystem::cursorPosition();
  }
  void warp(PointerHoldPoint where) override
  {
    if (throwOnWarp) {
      throw std::runtime_error("the warp failed");
    }
    FakeSystem::warp(where);
  }
  void runOnTapThread(std::function<void()> work) override
  {
    if (throwOnPost) {
      throw std::runtime_error("the work cannot be posted");
    }
    FakeSystem::runOnTapThread(std::move(work));
  }
};

// A system that holds on to the work that is handed to the tap thread until it is let go
class GatedSystem : public FakeSystem
{
public:
  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};

  void runOnTapThread(std::function<void()> work) override
  {
    entered = true;
    for (int i = 0; i < 4000 && !release.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FakeSystem::runOnTapThread(std::move(work));
  }
};

QStringList warpCall(PointerHoldPoint where)
{
  return {QStringLiteral("prepareWarp"), QStringLiteral("dissociate"),
          QStringLiteral("warp %1,%2").arg(where.x).arg(where.y)};
}

bool almostEqual(double a, double b)
{
  return std::abs(a - b) < 1e-9;
}

} // namespace

//
// the policy
//

void OSXPointerHoldTests::policy_whenNotParked_doesNothing()
{
  PointerHoldPolicy policy;
  QVERIFY(!policy.armed());

  // the pointer is on this Mac: it moves as much as it likes, and nothing is looked at
  const auto d = policy.observe(observation(Trigger::Event, {500.0, 500.0}, 1.0, false, true));
  QVERIFY(!d.moved);
  QVERIFY(!d.warp);
  QVERIFY(!d.reassociate);
  QVERIFY(!d.rehide);
  QVERIFY(!d.reenableTap);
  QVERIFY(!d.log);
  QCOMPARE(policy.detections(), uint64_t(0));

  policy.arm(kParked);
  policy.disarm();
  QVERIFY(!policy.observe(observation(Trigger::Timer, {900.0, 900.0}, 2.0)).warp);
}

void OSXPointerHoldTests::policy_whenUnmoved_doesNothing()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);

  const auto d = policy.observe(observation(Trigger::Timer, kParked, 1.0));
  QVERIFY(!d.moved);
  QVERIFY(!d.warp);
  QVERIFY(!d.reassociate);
  QVERIFY(!d.rehide);
  QVERIFY(!d.reenableTap);
  QVERIFY(!d.log);
  QCOMPARE(policy.detections(), uint64_t(0));
}

void OSXPointerHoldTests::policy_withinTheTolerance_doesNothing()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);
  QVERIFY(policy.isParked({100.5, 200.4}));
  QVERIFY(!policy.isParked({102.0, 200.0}));
  QVERIFY(!policy.observe(observation(Trigger::Event, {100.5, 200.4}, 1.0)).warp);
}

void OSXPointerHoldTests::policy_whenMoved_holdsAtTheParkedPlace()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);

  const auto d = policy.observe(observation(Trigger::Timer, {130.0, 240.0}, 1.0));
  QVERIFY(d.moved);
  QVERIFY(almostEqual(d.distance, 50.0));
  QVERIFY(d.reassociate);
  QVERIFY(d.warp);
  QCOMPARE(d.target.x, 100.0);
  QCOMPARE(d.target.y, 200.0);
  QVERIFY(!d.rehide);
  QVERIFY(!d.reenableTap);
  QVERIFY(d.log);
  QCOMPARE(d.detections, uint64_t(1));
  QCOMPARE(int(d.trigger), int(Trigger::Timer));
}

void OSXPointerHoldTests::policy_aNotification_reassertsTheCaptureEvenWhenUnmoved_withoutALine()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);

  const auto d = policy.observe(observation(Trigger::AppActivated, kParked, 1.0));
  QVERIFY(d.reassociate);
  QVERIFY(!d.warp);
  QVERIFY(!d.moved);
  // nothing was found: nothing is counted and nothing is written
  QVERIFY(!d.log);
  QCOMPARE(policy.detections(), uint64_t(0));
}

void OSXPointerHoldTests::policy_justAfterAWarp_doesNotWarpTwiceForTheSameMovement()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);

  QVERIFY(policy.observe(observation(Trigger::Event, {150.0, 200.0}, 1.000)).warp);
  // the place that is read 5 ms after the warp can still be the old one: not a second movement
  const auto again = policy.observe(observation(Trigger::Event, {150.0, 200.0}, 1.005));
  QVERIFY(!again.warp);
  QVERIFY(!again.moved);
  QVERIFY(!again.log);
  QCOMPARE(policy.detections(), uint64_t(1));
  // 20 ms after it, the pointer that is still away is held again
  QVERIFY(policy.observe(observation(Trigger::Event, {150.0, 200.0}, 1.020)).warp);
  QCOMPARE(policy.detections(), uint64_t(2));
}

void OSXPointerHoldTests::policy_aNotificationIsNotHeldBackBySettling()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);

  QVERIFY(policy.observe(observation(Trigger::Timer, {150.0, 200.0}, 1.000)).warp);
  QVERIFY(policy.observe(observation(Trigger::AppActivated, {150.0, 200.0}, 1.005)).warp);
}

void OSXPointerHoldTests::policy_aDisplayReconfiguration_acceptsTheNewPlaceForAMoment()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);

  // the system puts the pointer where it wants after a display changed: that is where it is parked now
  const auto reconfigured = policy.observe(observation(Trigger::DisplayReconfigured, {500.0, 300.0}, 10.0));
  QVERIFY(!reconfigured.warp);
  QVERIFY(reconfigured.reassociate);
  QCOMPARE(policy.parked().x, 500.0);
  QCOMPARE(policy.parked().y, 300.0);

  // it is still settling half a second later
  QVERIFY(!policy.observe(observation(Trigger::Timer, {520.0, 310.0}, 10.5)).warp);
  QCOMPARE(policy.parked().x, 520.0);

  // after the moment, a movement is a movement again
  const auto later = policy.observe(observation(Trigger::Timer, {900.0, 100.0}, 11.2));
  QVERIFY(later.warp);
  QCOMPARE(later.target.x, 520.0);
  QCOMPARE(later.target.y, 310.0);
}

void OSXPointerHoldTests::policy_aTapThatIsOff_isSwitchedOnAtMostOncePerInterval()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);

  const auto first = policy.observe(observation(Trigger::Timer, kParked, 0.0, false));
  QVERIFY(first.reenableTap);
  QVERIFY(first.tapWasOff);
  QVERIFY(first.log);
  QVERIFY(!first.warp);

  QVERIFY(!policy.observe(observation(Trigger::Timer, kParked, 0.5, false)).reenableTap);
  QVERIFY(policy.observe(observation(Trigger::Timer, kParked, 1.0, false)).reenableTap);
  // a tap that is on is left alone
  QVERIFY(!policy.observe(observation(Trigger::Timer, kParked, 5.0, true)).reenableTap);
}

void OSXPointerHoldTests::policy_aTapDisabledByUserInput_isSwitchedOnEvenWhenItLooksOn()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);
  QVERIFY(policy.observe(observation(Trigger::TapDisabledByUser, kParked, 1.0, true)).reenableTap);
}

void OSXPointerHoldTests::policy_aShownCursor_isHiddenAgainAtMostAFewTimesPerStay()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);
  QCOMPARE(policy.tuning().maxRehides, 3);

  for (int i = 0; i < 3; ++i) {
    const auto d = policy.observe(observation(Trigger::Timer, kParked, double(i), true, true));
    QVERIFY(d.rehide);
    QVERIFY(d.wasShown);
  }
  // the system ignores the hide: the hide count is not piled up
  QVERIFY(!policy.observe(observation(Trigger::Timer, kParked, 3.0, true, true)).rehide);

  // a new stay on a client is a new count
  policy.arm(kParked);
  QVERIFY(policy.observe(observation(Trigger::Timer, kParked, 4.0, true, true)).rehide);
}

void OSXPointerHoldTests::policy_theLine_isWrittenAtMostOncePerInterval_andCounts()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);

  struct Step
  {
    double at;
    bool log;
    uint64_t count;
  };
  const Step steps[] = {
      {0.0, true, 1}, {1.0, false, 2}, {4.9, false, 3}, {5.0, true, 4}, {9.9, false, 5}, {10.0, true, 6}
  };
  for (const auto &step : steps) {
    const auto d = policy.observe(observation(Trigger::Timer, {150.0, 200.0}, step.at));
    QVERIFY2(d.warp, qPrintable(QStringLiteral("a warp at %1").arg(step.at)));
    QCOMPARE(d.log, step.log);
    QCOMPARE(d.detections, step.count);
  }
}

void OSXPointerHoldTests::policy_reparking_movesThePlace()
{
  PointerHoldPolicy policy;
  policy.repark({1.0, 2.0});
  // not parked: nothing to move
  QCOMPARE(policy.parked().x, 0.0);

  policy.arm(kParked);
  policy.repark({300.0, 400.0});
  QVERIFY(policy.isParked({300.0, 400.0}));
  QVERIFY(!policy.observe(observation(Trigger::Timer, {300.0, 400.0}, 1.0)).warp);
  const auto d = policy.observe(observation(Trigger::Timer, kParked, 2.0));
  QVERIFY(d.warp);
  QCOMPARE(d.target.x, 300.0);
}

void OSXPointerHoldTests::policy_arming_startsAFreshStay()
{
  PointerHoldPolicy policy;
  policy.arm(kParked);
  QVERIFY(policy.observe(observation(Trigger::Timer, {150.0, 200.0}, 1.000)).warp);
  QVERIFY(policy.observe(observation(Trigger::Timer, kParked, 1.0005, false)).reenableTap);

  // the pointer comes back and leaves again within the settle time and the tap retry time: nothing of the old stay
  // holds
  policy.disarm();
  policy.arm({400.0, 400.0});
  QVERIFY(policy.observe(observation(Trigger::Timer, {450.0, 400.0}, 1.002)).warp);
  QVERIFY(policy.observe(observation(Trigger::Timer, {400.0, 400.0}, 1.003, false)).reenableTap);
}

//
// the line
//

void OSXPointerHoldTests::line_whenMoved_namesTheComputerTheDistanceTheTriggerAndTheCount()
{
  PointerHoldDecision d;
  d.moved = true;
  d.distance = 213.6;
  d.trigger = Trigger::AppActivated;
  d.detections = 7;
  QCOMPARE(
      QString::fromStdString(formatPointerHoldLog(d, "win-pc", "com.example.editor")),
      QStringLiteral("the pointer of this Mac moved while it is on \"win-pc\": held it again "
                     "(moved 214 px, trigger: app activated, times: 7, app: com.example.editor)")
  );

  d.wasShown = true;
  d.tapWasOff = true;
  d.trigger = Trigger::Event;
  d.distance = 5.0;
  d.detections = 3;
  QCOMPARE(
      QString::fromStdString(formatPointerHoldLog(d, "win-pc", {})),
      QStringLiteral("the pointer of this Mac moved while it is on \"win-pc\": held it again "
                     "(moved 5 px, trigger: event, times: 3, the cursor was shown, the event tap was off)")
  );
}

void OSXPointerHoldTests::line_whenShown_saysSo()
{
  PointerHoldDecision d;
  d.wasShown = true;
  d.trigger = Trigger::Timer;
  d.detections = 2;
  QCOMPARE(
      QString::fromStdString(formatPointerHoldLog(d, "win-pc", {})),
      QStringLiteral("the pointer of this Mac was shown while it is on \"win-pc\": hid it again "
                     "(trigger: timer, times: 2)")
  );
}

void OSXPointerHoldTests::line_whenTheTapWasOff_saysSo()
{
  PointerHoldDecision d;
  d.tapWasOff = true;
  d.reenableTap = true;
  d.trigger = Trigger::TapDisabledByUser;
  d.detections = 1;
  QCOMPARE(
      QString::fromStdString(formatPointerHoldLog(d, "win-pc", {})),
      QStringLiteral("the event tap was off while the pointer is on \"win-pc\": switched it on again "
                     "(trigger: tap disabled by user input, times: 1)")
  );
}

void OSXPointerHoldTests::line_withoutAName_hasAQuestionMark()
{
  PointerHoldDecision d;
  d.moved = true;
  d.distance = 1.0;
  d.detections = 1;
  QVERIFY(QString::fromStdString(formatPointerHoldLog(d, {}, {})).contains(QStringLiteral("while it is on \"?\":")));
}

void OSXPointerHoldTests::line_theApplication_isOnlyNamedWhenItIsToldToBe()
{
  const std::string own = "org.deskflow.deskflow";
  // an activation names the application that took the foreground
  QCOMPARE(
      pointerHoldLogApp(Trigger::AppActivated, "com.example.game", "com.example.game", own),
      std::string("com.example.game")
  );
  // any other trigger names the front application only when it is this program
  QCOMPARE(pointerHoldLogApp(Trigger::Timer, {}, own, own), own);
  QCOMPARE(pointerHoldLogApp(Trigger::Timer, {}, "com.example.game", own), std::string());
  QCOMPARE(pointerHoldLogApp(Trigger::Event, {}, {}, own), std::string());
  // without an own id nothing is the own program
  QCOMPARE(pointerHoldLogApp(Trigger::Timer, {}, {}, {}), std::string());
}

//
// the controller
//

void OSXPointerHoldTests::controller_onThisMac_neverTouchesTheSystem()
{
  FakeSystem system;
  FakeNotifications notifications;
  PointerHoldController controller(system);
  controller.attach(notifications);
  system.cursor = {900.0, 900.0};
  system.shown = true;
  system.tap = false;

  controller.onPointerEvent({10.0, 10.0});
  controller.onNotification(Trigger::AppActivated, "com.example.app");
  notifications.fire(Trigger::ScreenUnlocked);
  controller.repark({1.0, 1.0});

  QVERIFY(!controller.armed());
  QVERIFY(system.calls.isEmpty());
  QVERIFY(system.lines.isEmpty());
  QCOMPARE(system.cursorReads, 0);
  QCOMPARE(system.timersStarted, 0);
}

void OSXPointerHoldTests::controller_anEventAtTheParkedPlace_costsNoSystemCall()
{
  FakeSystem system;
  PointerHoldController controller(system);
  controller.arm();
  QCOMPARE(system.cursorReads, 1);

  for (int i = 0; i < 100; ++i) {
    controller.onPointerEvent(kParked);
  }
  // the hot path: the place of the event is the parked place, nothing is asked of the system
  QCOMPARE(system.cursorReads, 1);
  QVERIFY(system.calls.isEmpty());
}

void OSXPointerHoldTests::controller_anOldEvent_isNotAMovement()
{
  FakeSystem system;
  PointerHoldController controller(system);
  controller.arm();

  // an event that was queued before the pointer was held again: its place is old, the live place is the parked one
  controller.onPointerEvent({180.0, 260.0});
  QVERIFY(system.calls.isEmpty());
  QVERIFY(system.lines.isEmpty());
  QCOMPARE(controller.detections(), uint64_t(0));
}

void OSXPointerHoldTests::controller_aMovement_isHeldInTheOrderOfTheCalls()
{
  FakeSystem system;
  system.front = "com.example.other";
  PointerHoldController controller(system);
  controller.setOwnBundleId("org.deskflow.deskflow");
  controller.setRemoteName("win-pc");
  controller.arm();

  system.cursor = {180.0, 260.0}; // the system gave the pointer back and it moved
  controller.onPointerEvent({180.0, 260.0});

  // the suppression interval first, then the capture, then the warp back; the cursor was not shown: no hide
  QCOMPARE(system.calls, warpCall(kParked));
  QCOMPARE(system.cursor.x, 100.0);
  QCOMPARE(system.cursor.y, 200.0);
  QCOMPARE(system.lines.size(), 1);
  QCOMPARE(
      system.lines.first(),
      QStringLiteral("the pointer of this Mac moved while it is on \"win-pc\": held it again "
                     "(moved 100 px, trigger: event, times: 1)")
  );

  // it stays where it was parked: the next events of the same stream are not movements
  system.calls.clear();
  controller.onPointerEvent(kParked);
  QVERIFY(system.calls.isEmpty());
  QCOMPARE(controller.detections(), uint64_t(1));
}

void OSXPointerHoldTests::controller_theTimer_runsOnlyWhileTheStayLasts_andHoldsTheMovedPointer()
{
  FakeSystem system;
  PointerHoldController controller(system);
  QVERIFY(!system.tick);

  controller.arm();
  QCOMPARE(system.timersStarted, 1);
  QVERIFY(almostEqual(system.timerInterval, 0.25));
  QVERIFY(bool(system.tick));
  const auto tick = system.tick;

  // nothing moved: nothing is done
  tick();
  QVERIFY(system.calls.isEmpty());

  system.cursor = {300.0, 200.0};
  system.clock = 1.0;
  tick();
  QCOMPARE(system.calls, warpCall(kParked));

  // the pointer is on this Mac again
  controller.disarm();
  QVERIFY(!system.tick);
  QVERIFY(!controller.armed());
  system.calls.clear();
  system.cursor = {900.0, 900.0};
  tick(); // a tick that was already on its way is harmless
  QVERIFY(system.calls.isEmpty());
}

void OSXPointerHoldTests::controller_theLine_followsTheClockAndTheCounter()
{
  FakeSystem system;
  PointerHoldController controller(system);
  controller.arm();
  const auto tick = system.tick;

  // a pointer that is taken again and again: one line per 5 seconds, and the counter says how many looks found
  // something
  const double times[] = {0.0, 1.0, 2.0, 6.0};
  for (const auto at : times) {
    system.clock = at;
    system.cursor = {250.0, 200.0};
    tick();
  }
  QCOMPARE(controller.detections(), uint64_t(4));
  QCOMPARE(system.lines.size(), 2);
  QVERIFY(system.lines.at(0).contains(QStringLiteral("times: 1")));
  QVERIFY(system.lines.at(1).contains(QStringLiteral("times: 4")));
  QVERIFY(system.lines.at(1).contains(QStringLiteral("trigger: timer")));
}

void OSXPointerHoldTests::controller_aNotificationFromTheSource_looksAtThePointerOnTheTapThread()
{
  FakeSystem system;
  system.runInline = false;
  system.front = "com.example.other";
  FakeNotifications notifications;
  PointerHoldController controller(system);
  controller.setRemoteName("win-pc");
  controller.attach(notifications);
  QCOMPARE(notifications.started, 1);
  controller.arm();

  system.cursor = {140.0, 200.0};
  notifications.fire(Trigger::AppActivated, "com.example.game");
  // it was handed to the tap thread: nothing was done on the thread of the notification
  QVERIFY(system.calls.isEmpty());
  QCOMPARE(int(system.posted.size()), 1);

  system.drain();
  QCOMPARE(system.calls, warpCall(kParked));
  QCOMPARE(system.lines.size(), 1);
  QVERIFY(system.lines.first().contains(QStringLiteral("trigger: app activated")));
  QVERIFY(system.lines.first().contains(QStringLiteral("app: com.example.game")));
  QVERIFY(system.lines.first().contains(QStringLiteral("\"win-pc\"")));
}

void OSXPointerHoldTests::controller_aSilentNotification_holdsAgainWithoutALine()
{
  FakeSystem system;
  FakeNotifications notifications;
  PointerHoldController controller(system);
  controller.attach(notifications);
  controller.arm();

  // the screen is locked and the pointer did not move: the capture is made again, nothing is written
  notifications.fire(Trigger::ScreenLocked);
  QCOMPARE(system.calls, QStringList({QStringLiteral("prepareWarp"), QStringLiteral("dissociate")}));
  QVERIFY(system.lines.isEmpty());
  QCOMPARE(controller.detections(), uint64_t(0));
}

void OSXPointerHoldTests::controller_aTapDisabledByUserInput_switchesTheTapOn()
{
  FakeSystem system;
  system.tap = false;
  FakeNotifications notifications;
  PointerHoldController controller(system);
  controller.attach(notifications);
  controller.arm();

  controller.onNotification(Trigger::TapDisabledByUser);
  QVERIFY(system.calls.contains(QStringLiteral("enableTap")));
  QVERIFY(system.tap);
  QCOMPARE(system.lines.size(), 1);
  QVERIFY(system.lines.first().contains(QStringLiteral("the event tap was off")));
}

void OSXPointerHoldTests::controller_aShownCursor_isHiddenAgain()
{
  FakeSystem system;
  PointerHoldController controller(system);
  controller.arm();
  const auto tick = system.tick;

  system.shown = true;
  tick();
  QCOMPARE(system.calls, QStringList{QStringLiteral("hideAgain")});
  QVERIFY(!system.shown);
  QCOMPARE(system.lines.size(), 1);
  QVERIFY(system.lines.first().contains(QStringLiteral("was shown")));

  // hidden: nothing more is done
  system.calls.clear();
  tick();
  QVERIFY(system.calls.isEmpty());
}

void OSXPointerHoldTests::controller_theNameOfTheClient_isInTheLine()
{
  FakeSystem system;
  system.front = "org.deskflow.deskflow";
  PointerHoldController controller(system);
  controller.setOwnBundleId("org.deskflow.deskflow");
  controller.arm();
  const auto tick = system.tick;

  system.cursor = {111.0, 200.0};
  tick();
  QCOMPARE(system.lines.size(), 1);
  // not told: a question mark; the front application is named because it is this program
  QVERIFY(system.lines.first().contains(QStringLiteral("while it is on \"?\":")));
  QVERIFY(system.lines.first().contains(QStringLiteral("app: org.deskflow.deskflow")));

  controller.setRemoteName("mini");
  system.clock = 10.0;
  system.cursor = {111.0, 200.0};
  tick();
  QCOMPARE(system.lines.size(), 2);
  QVERIFY(system.lines.last().contains(QStringLiteral("while it is on \"mini\":")));
}

void OSXPointerHoldTests::controller_theServerMovingThePointer_isNotAMovement()
{
  FakeSystem system;
  PointerHoldController controller(system);
  controller.arm();
  const auto tick = system.tick;

  // the server warps the pointer on purpose (it comes back to this Mac at another place): that is the parked place
  controller.repark({700.0, 300.0});
  system.cursor = {700.0, 300.0};
  controller.onPointerEvent({700.0, 300.0});
  tick();
  QVERIFY(system.calls.isEmpty());
  QVERIFY(system.lines.isEmpty());
}

void OSXPointerHoldTests::controller_detaching_stopsTheNotifications()
{
  FakeSystem system;
  FakeNotifications notifications;
  PointerHoldController controller(system);
  controller.attach(notifications);
  controller.arm();
  system.cursor = {300.0, 300.0};

  controller.detach();
  QCOMPARE(notifications.stopped, 1);
  notifications.fire(Trigger::AppActivated, "com.example.app");
  QVERIFY(system.calls.isEmpty());

  // attached again, it is listened to again, and a second attach does not leave the first one running
  controller.attach(notifications);
  controller.attach(notifications);
  QCOMPARE(notifications.started, 3);
  QCOMPARE(notifications.stopped, 2);
  notifications.fire(Trigger::AppActivated, "com.example.app");
  QCOMPARE(system.calls, warpCall(kParked));
}

void OSXPointerHoldTests::controller_destroyingIt_stopsTheTimerAndTheSource()
{
  FakeSystem system;
  FakeNotifications notifications;
  {
    PointerHoldController controller(system);
    controller.attach(notifications);
    controller.arm();
    QVERIFY(bool(system.tick));
  }
  QCOMPARE(notifications.stopped, 1);
  QVERIFY(!system.tick);
  QVERIFY(system.timersStopped >= 1);
}

//
// lifetime
//
void OSXPointerHoldTests::controller_workThatOutlivesTheController_doesNotTouchTheSystem()
{
  // what was handed to the tap thread (a notification, a tick of the timer) can run after the controller is gone: the
  // system that is still there must not be called, and nothing of the controller may be read
  FakeSystem system;
  system.runInline = false;
  FakeNotifications notifications;
  std::function<void()> tick;
  {
    PointerHoldController controller(system);
    controller.attach(notifications);
    controller.arm();
    tick = system.tick;
    QVERIFY(bool(tick));
    system.cursor = {300.0, 300.0};
    notifications.fire(Trigger::AppActivated, "com.example.app");
    controller.onNotification(Trigger::SpaceChanged);
    QCOMPARE(int(system.posted.size()), 2);
  }
  const auto calls = system.calls;
  const auto lines = system.lines;
  const int reads = system.cursorReads;

  system.drain();
  tick();
  tick();

  QCOMPARE(system.calls, calls);
  QCOMPARE(system.lines, lines);
  QCOMPARE(system.cursorReads, reads);
}

void OSXPointerHoldTests::controller_destroyingIt_waitsForANotificationThatIsBeingHandedOver()
{
  // a notification is being handed to the tap thread (the system is in use) on one thread while the controller is
  // destroyed on another: the controller must not be gone before the system is let go of
  GatedSystem system;
  auto controller = std::make_unique<PointerHoldController>(system);
  controller->arm();
  std::atomic<bool> destroyed{false};

  std::thread notifier([&] { controller->onNotification(Trigger::SpaceChanged); });
  QTRY_VERIFY_WITH_TIMEOUT(system.entered.load(), 5000);

  std::thread destroyer([&] {
    controller.reset();
    destroyed = true;
  });
  QTest::qWait(150);
  const bool destroyedTooEarly = destroyed.load();
  system.release = true;
  destroyer.join();
  notifier.join();
  QVERIFY(!destroyedTooEarly);
  QVERIFY(destroyed.load());
}

void OSXPointerHoldTests::controller_aSystemThatThrows_isContainedAndReportedOnce()
{
  // an exception that leaves a callback of the system (the tap, the timer, a notification) aborts the process: the
  // worst that may happen is that the pointer is not held, and one line says so
  ThrowingSystem system;
  system.throwOnCursor = true;
  PointerHoldController controller(system);

  // the cursor cannot be read: nothing is parked, nothing throws
  controller.arm();
  controller.onPointerEvent({1.0, 1.0});
  controller.onNotification(Trigger::AppActivated);
  controller.disarm();
  controller.repark({2.0, 2.0});
  controller.setRemoteName("client");
  QVERIFY(!controller.armed());
  QCOMPARE(int(system.lines.size()), 1);
  QVERIFY(system.lines.first().contains(QStringLiteral("could not be held")));

  // the next stay works when the system does
  system.throwOnCursor = false;
  controller.arm();
  QVERIFY(controller.armed());
  QVERIFY(bool(system.tick));

  // a warp that fails: the tick, the event and the notification all come back, and the line is not written again
  system.throwOnWarp = true;
  system.cursor = {300.0, 300.0};
  for (int i = 0; i < 3; ++i) {
    system.tick();
    controller.onPointerEvent({300.0, 300.0});
    controller.onNotification(Trigger::SpaceChanged);
  }
  QVERIFY(controller.armed());
  QCOMPARE(int(system.lines.size()), 1);

  // a post that fails
  system.throwOnPost = true;
  controller.onNotification(Trigger::DisplayWoke);
  QCOMPARE(int(system.lines.size()), 1);

  // and when the system works again the hold works again
  system.throwOnWarp = false;
  system.throwOnPost = false;
  system.cursor = {300.0, 300.0};
  controller.onNotification(Trigger::SpaceChanged);
  QCOMPARE(system.cursor.x, kParked.x);
  QCOMPARE(system.cursor.y, kParked.y);
}

//
// on and off
//
void OSXPointerHoldTests::decision_theHoldIsMadeOnlyForAServerThatTurnedItOn()
{
  // OSXComputer makes the system object, the controller and (when the tap runs) the source of notifications only when
  // this says yes, and every call that it makes into the hold is behind the absence of the controller
  QVERIFY(pointerHoldWanted(true, true));
  QVERIFY(!pointerHoldWanted(true, false));
  QVERIFY(!pointerHoldWanted(false, true));
  QVERIFY(!pointerHoldWanted(false, false));
}

void OSXPointerHoldTests::decision_aServerThatWasNotTold_makesNothing()
{
  // the setting that nobody wrote: off, so a server that was not told to hold the pointer makes nothing of the hold
  const auto key = Settings::Core::MacHoldPointer;
  QVERIFY(Settings::validKeys().contains(key));
  const bool notTold = Settings::defaultValue(key).toBool();
  QVERIFY(!notTold);
  QVERIFY(!pointerHoldWanted(true, notTold));
  QVERIFY(!pointerHoldWanted(false, notTold));
}

QTEST_MAIN(OSXPointerHoldTests)
