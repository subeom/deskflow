/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXPointerHoldNotificationsTests.h"

#include "OSXPointerHoldTestSupport.h"
#include "base/ILogOutputter.h"
#include "base/Log.h"
#include "platform/OSXPointerHold.h"
#include "platform/OSXPointerHoldSystem.h"

#include <QTest>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace deskflow::platform;
using Trigger = PointerHoldTrigger;
using OSXPointerHoldTestSupport::churnStack;
using OSXPointerHoldTestSupport::postWorkspaceNotification;
using OSXPointerHoldTestSupport::RunLoopThread;

namespace {

// Counts the lines of the log that say that something was contained.  Gets every line, writes none
class FailureLines : public ILogOutputter
{
public:
  void open(const QString &) override
  {
    // do nothing
  }
  void close() override
  {
    // do nothing
  }
  bool write(LogLevel::Level, const QString &message) override
  {
    if (message.contains(QStringLiteral("could not be held"))) {
      ++count;
    }
    // the next outputter writes it to the console as well
    return false;
  }

  std::atomic<int> count{0};
};

Log *g_log = nullptr;
FailureLines *g_failureLines = nullptr;

// What a callback saw.  The workspace may also post real notifications to this process while a test runs (another
// application is activated, the space changes), so a test only ever expects "at least" what it posted itself
struct Probe
{
  std::mutex mutex;
  std::vector<Trigger> seen;
  std::vector<std::string> apps;
  std::atomic<int> total{0};

  void note(Trigger trigger, const std::string &app)
  {
    {
      std::scoped_lock lock(mutex);
      seen.push_back(trigger);
      apps.push_back(app);
    }
    ++total;
  }
  int count(Trigger trigger)
  {
    std::scoped_lock lock(mutex);
    int n = 0;
    for (const auto seenTrigger : seen) {
      n += seenTrigger == trigger ? 1 : 0;
    }
    return n;
  }
};

IPointerHoldNotifications::Callback callbackOf(const std::shared_ptr<Probe> &probe)
{
  return [probe](Trigger trigger, const std::string &app) { probe->note(trigger, app); };
}

// start() is its own frame, as it is in the program, and it has returned when the notification arrives
__attribute__((noinline)) void startListening(IPointerHoldNotifications &notifications, std::shared_ptr<Probe> probe)
{
  notifications.start(callbackOf(probe));
}

// Posts from other threads until it is told to stop, like the system does from whichever thread an application is
// activated on
class Posters
{
public:
  explicit Posters(int threads = 2)
  {
    for (int i = 0; i < threads; ++i) {
      m_threads.emplace_back([this, i] {
        const Trigger triggers[] = {Trigger::SpaceChanged, Trigger::AppActivated, Trigger::DisplayWoke};
        unsigned n = static_cast<unsigned>(i);
        while (!m_stop.load()) {
          postWorkspaceNotification(triggers[n++ % 3]);
          ++m_posted;
          std::this_thread::yield();
        }
      });
    }
  }
  ~Posters()
  {
    stop();
  }
  void stop()
  {
    m_stop = true;
    for (auto &thread : m_threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }
  int posted() const
  {
    return m_posted.load();
  }

private:
  std::atomic<bool> m_stop{false};
  std::atomic<int> m_posted{0};
  std::vector<std::thread> m_threads;
};

// The system as the controller sees it, thread safe, with a switch that says "the owner of the system is gone": every
// call after that is counted.  Nothing here touches the real cursor, the real tap or a real event.  The work that is
// posted to the "tap thread" runs at once, on the thread that posts it
class RecordingSystem : public IPointerHoldSystem
{
public:
  PointerHoldPoint cursorPosition() override
  {
    note("cursorPosition");
    std::scoped_lock lock(m_mutex);
    return m_cursor;
  }
  bool tapEnabled() override
  {
    note("tapEnabled");
    return true;
  }
  bool cursorShown() override
  {
    note("cursorShown");
    return false;
  }
  void enableTap() override
  {
    note("enableTap");
  }
  void prepareWarp() override
  {
    note("prepareWarp");
  }
  void dissociate() override
  {
    note("dissociate");
  }
  void warp(PointerHoldPoint where) override
  {
    note("warp");
    std::scoped_lock lock(m_mutex);
    m_cursor = where;
    m_warps.push_back(where);
  }
  void hideAgain() override
  {
    note("hideAgain");
  }
  std::string frontApplication() override
  {
    note("frontApplication");
    return {};
  }
  double now() override
  {
    note("now");
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
  }
  void log(const std::string &) override
  {
    note("log");
  }
  void runOnTapThread(std::function<void()> work) override
  {
    note("runOnTapThread");
    work();
  }
  void startTimer(double, std::function<void()>) override
  {
    note("startTimer");
  }
  void stopTimer() override
  {
    note("stopTimer");
  }

  void moveCursor(PointerHoldPoint where)
  {
    std::scoped_lock lock(m_mutex);
    m_cursor = where;
  }
  int warps()
  {
    std::scoped_lock lock(m_mutex);
    return static_cast<int>(m_warps.size());
  }
  PointerHoldPoint lastWarp()
  {
    std::scoped_lock lock(m_mutex);
    return m_warps.empty() ? PointerHoldPoint{} : m_warps.back();
  }
  //! The owner of the system is gone: it must not be called any more
  void close()
  {
    std::scoped_lock lock(m_mutex);
    m_closed = true;
  }
  int callsAfterClose()
  {
    std::scoped_lock lock(m_mutex);
    return m_callsAfterClose;
  }

private:
  void note(const char *)
  {
    std::scoped_lock lock(m_mutex);
    if (m_closed) {
      ++m_callsAfterClose;
    }
  }

  std::mutex m_mutex;
  PointerHoldPoint m_cursor{100.0, 200.0};
  std::vector<PointerHoldPoint> m_warps;
  bool m_closed = false;
  int m_callsAfterClose = 0;
};

OSXPointerHoldHooks hooksOf(OSXPointerHoldTestSupport::RunLoopThread &loop)
{
  OSXPointerHoldHooks hooks;
  hooks.tapRunLoop = [&loop] { return loop.retainRunLoop(); };
  return hooks;
}

} // namespace

//
// the log
//
void OSXPointerHoldNotificationsTests::initTestCase()
{
  // the program always has a log; a log line without one dereferences a null pointer
  g_log = new Log;
  g_failureLines = new FailureLines;
  g_log->insert(g_failureLines, true);
}

void OSXPointerHoldNotificationsTests::cleanupTestCase()
{
  g_failureLines = nullptr;
  delete g_log; // deletes the outputter that it adopted
  g_log = nullptr;
}

//
// the real observers
//
void OSXPointerHoldNotificationsTests::notification_afterStartReturned_reachesTheCallback()
{
  auto probe = std::make_shared<Probe>();
  OSXPointerHoldNotifications notifications;
  startListening(notifications, probe);

  // the thread has done other things since start() returned, as the main thread of the program has
  churnStack();
  postWorkspaceNotification(Trigger::SpaceChanged);

  QTRY_VERIFY_WITH_TIMEOUT(probe->count(Trigger::SpaceChanged) >= 1, 5000);
}

void OSXPointerHoldNotificationsTests::notification_everyWorkspaceTrigger_reachesTheCallbackWithItsTrigger()
{
  auto probe = std::make_shared<Probe>();
  OSXPointerHoldNotifications notifications;
  notifications.start(callbackOf(probe));

  const Trigger workspaceTriggers[] = {
      Trigger::AppActivated, Trigger::SpaceChanged, Trigger::DisplayWoke, Trigger::DisplaySlept, Trigger::SystemWoke
  };
  for (const auto trigger : workspaceTriggers) {
    postWorkspaceNotification(trigger);
  }
  for (const auto trigger : workspaceTriggers) {
    QTRY_VERIFY_WITH_TIMEOUT(probe->count(trigger) >= 1, 5000);
  }
}

void OSXPointerHoldNotificationsTests::notification_theApplicationThatWasActivated_isPassedOn()
{
  // the test program is not a bundle and may have no identifier: what matters is that the userInfo of the notification
  // is read without a crash and that a notification without a userInfo gives an empty name
  auto probe = std::make_shared<Probe>();
  OSXPointerHoldNotifications notifications;
  notifications.start(callbackOf(probe));
  postWorkspaceNotification(Trigger::AppActivated);
  QTRY_VERIFY_WITH_TIMEOUT(probe->count(Trigger::AppActivated) >= 1, 5000);
  postWorkspaceNotification(Trigger::SpaceChanged);
  QTRY_VERIFY_WITH_TIMEOUT(probe->count(Trigger::SpaceChanged) >= 1, 5000);
  std::scoped_lock lock(probe->mutex);
  for (size_t i = 0; i < probe->seen.size(); ++i) {
    if (probe->seen[i] == Trigger::SpaceChanged) {
      QVERIFY(probe->apps[i].empty());
    }
  }
}

//
// stop, restart, throwing and re-entrant callbacks
//
void OSXPointerHoldNotificationsTests::stop_silencesTheNotificationsAndReleasesTheCallback()
{
  auto probe = std::make_shared<Probe>();
  OSXPointerHoldNotifications notifications;
  notifications.start(callbackOf(probe));
  postWorkspaceNotification(Trigger::SpaceChanged);
  QTRY_VERIFY_WITH_TIMEOUT(probe->count(Trigger::SpaceChanged) >= 1, 5000);

  notifications.stop();
  // nothing of the callback is kept: the object that it captured is the test's alone again
  QCOMPARE(static_cast<long>(probe.use_count()), 1L);
  const int before = probe->total.load();
  for (int i = 0; i < 20; ++i) {
    postWorkspaceNotification(Trigger::SpaceChanged);
    postWorkspaceNotification(Trigger::AppActivated);
  }
  QTest::qWait(30);
  QCOMPARE(probe->total.load(), before);

  // stopping a source that is stopped is harmless
  notifications.stop();
  notifications.stop();
}

void OSXPointerHoldNotificationsTests::start_aSecondTime_replacesTheCallback()
{
  auto first = std::make_shared<Probe>();
  auto second = std::make_shared<Probe>();
  OSXPointerHoldNotifications notifications;
  notifications.start(callbackOf(first));
  notifications.start(callbackOf(second));
  QCOMPARE(static_cast<long>(first.use_count()), 1L);

  const int firstBefore = first->total.load();
  postWorkspaceNotification(Trigger::SpaceChanged);
  QTRY_VERIFY_WITH_TIMEOUT(second->count(Trigger::SpaceChanged) >= 1, 5000);
  QCOMPARE(first->total.load(), firstBefore);

  // a stop and a start again listen again
  notifications.stop();
  notifications.start(callbackOf(first));
  postWorkspaceNotification(Trigger::DisplayWoke);
  QTRY_VERIFY_WITH_TIMEOUT(first->count(Trigger::DisplayWoke) >= 1, 5000);
}

void OSXPointerHoldNotificationsTests::callback_thatThrows_doesNotTakeTheProcessDown()
{
  // an exception that leaves an observer block aborts the process (std::terminate): it must stay inside
  auto thrown = std::make_shared<std::atomic<int>>(0);
  OSXPointerHoldNotifications notifications;
  notifications.start([thrown](Trigger, const std::string &) {
    ++*thrown;
    throw std::runtime_error("the callback failed");
  });

  postWorkspaceNotification(Trigger::SpaceChanged);
  postWorkspaceNotification(Trigger::SpaceChanged);
  QTRY_VERIFY_WITH_TIMEOUT(thrown->load() >= 2, 5000);
  // the failure is reported, once, however often it happens
  postWorkspaceNotification(Trigger::SpaceChanged);
  QTRY_VERIFY_WITH_TIMEOUT(thrown->load() >= 3, 5000);
  QCOMPARE(g_failureLines->count.load(), 1);

  // and the source still works afterwards
  auto probe = std::make_shared<Probe>();
  notifications.start(callbackOf(probe));
  postWorkspaceNotification(Trigger::DisplaySlept);
  QTRY_VERIFY_WITH_TIMEOUT(probe->count(Trigger::DisplaySlept) >= 1, 5000);
}

void OSXPointerHoldNotificationsTests::stop_fromTheCallback_doesNotDeadlockAndSilencesIt()
{
  auto calls = std::make_shared<std::atomic<int>>(0);
  OSXPointerHoldNotifications notifications;
  notifications.start([calls, &notifications](Trigger, const std::string &) {
    ++*calls;
    notifications.stop();
  });

  postWorkspaceNotification(Trigger::SpaceChanged);
  QTRY_VERIFY_WITH_TIMEOUT(calls->load() >= 1, 5000);
  const int before = calls->load();
  postWorkspaceNotification(Trigger::SpaceChanged);
  postWorkspaceNotification(Trigger::SpaceChanged);
  QTest::qWait(30);
  QCOMPARE(calls->load(), before);
}

void OSXPointerHoldNotificationsTests::destroying_withoutStarting_isFine()
{
  {
    OSXPointerHoldNotifications notifications;
  }
  {
    OSXPointerHoldNotifications notifications;
    notifications.stop();
  }
}

//
// the owner goes away while notifications are in flight
//
void OSXPointerHoldNotificationsTests::destroying_whileNotificationsAreInFlight_neverCallsTheCallbackAfterwards()
{
  Posters posters;
  QTRY_VERIFY_WITH_TIMEOUT(posters.posted() > 0, 5000);

  int callbacksAfterwards = 0;
  const int rounds = 150;
  for (int round = 0; round < rounds; ++round) {
    auto probe = std::make_shared<Probe>();
    auto alive = std::make_shared<std::atomic<bool>>(true);
    auto afterwards = std::make_shared<std::atomic<int>>(0);
    {
      OSXPointerHoldNotifications notifications;
      notifications.start([probe, alive, afterwards](Trigger trigger, const std::string &app) {
        if (!alive->load()) {
          ++*afterwards;
        }
        probe->note(trigger, app);
      });
      // let some of them run, and some be in the queue, when the owner goes
      std::this_thread::sleep_for(std::chrono::microseconds(50 + (round % 7) * 90));
    }
    // the owner is gone: a callback that runs now would use what the owner took with it
    *alive = false;
    std::this_thread::sleep_for(std::chrono::microseconds(300));
    callbacksAfterwards += afterwards->load();
    QCOMPARE(static_cast<long>(probe.use_count()), 1L);
  }
  QCOMPARE(callbacksAfterwards, 0);
  posters.stop();
}

void OSXPointerHoldNotificationsTests::stopping_waitsForTheCallbackThatIsRunning()
{
  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};
  std::atomic<bool> stopReturned{false};
  auto probe = std::make_shared<Probe>();

  OSXPointerHoldNotifications notifications;
  notifications.start([&entered, &release, probe](Trigger trigger, const std::string &app) {
    probe->note(trigger, app);
    entered = true;
    for (int i = 0; i < 4000 && !release.load(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  // the notification of the system arrives on a thread of its own, and the callback is running when stop() is called
  std::thread poster([] { postWorkspaceNotification(Trigger::SpaceChanged); });
  QTRY_VERIFY_WITH_TIMEOUT(entered.load(), 5000);

  std::thread stopper([&] {
    notifications.stop();
    stopReturned = true;
  });
  // stop() must not return while the callback still runs: what the callback uses is destroyed after it returns
  QTest::qWait(150);
  const bool returnedTooEarly = stopReturned.load();
  release = true;
  stopper.join();
  poster.join();
  QVERIFY(!returnedTooEarly);
  QVERIFY(stopReturned.load());
}

//
// the real glue of the system to a run loop
//
void OSXPointerHoldNotificationsTests::system_work_runsOnTheThreadOfTheRunLoop()
{
  std::atomic<int> ran{0};
  std::atomic<std::thread::id> where{};
  RunLoopThread loop;
  OSXPointerHoldSystem system(hooksOf(loop));

  system.runOnTapThread([&] {
    where = std::this_thread::get_id();
    ++ran;
  });
  QTRY_VERIFY_WITH_TIMEOUT(ran.load() == 1, 5000);
  QVERIFY(where.load() == loop.threadId());
  QVERIFY(where.load() != std::this_thread::get_id());
}

void OSXPointerHoldNotificationsTests::system_work_thatThrows_isContainedAndTheLoopGoesOn()
{
  // an exception that leaves a block of a run loop aborts the process
  std::atomic<int> after{0};
  RunLoopThread loop;
  OSXPointerHoldSystem system(hooksOf(loop));

  system.runOnTapThread([] { throw std::runtime_error("the work failed"); });
  system.runOnTapThread([&] { ++after; });
  QTRY_VERIFY_WITH_TIMEOUT(after.load() == 1, 5000);
}

void OSXPointerHoldNotificationsTests::system_theTimer_ticksOnTheRunLoopUntilItIsStopped()
{
  std::atomic<int> ticks{0};
  std::atomic<std::thread::id> where{};
  RunLoopThread loop;
  OSXPointerHoldSystem system(hooksOf(loop));

  system.startTimer(0.01, [&] {
    where = std::this_thread::get_id();
    ++ticks;
  });
  QTRY_VERIFY_WITH_TIMEOUT(ticks.load() >= 3, 5000);
  QVERIFY(where.load() == loop.threadId());

  system.stopTimer();
  // a tick that was running when the timer was stopped ends first
  QTest::qWait(40);
  const int stopped = ticks.load();
  QTest::qWait(100);
  QCOMPARE(ticks.load(), stopped);

  // stopped again, and started again: the timer ticks again
  system.stopTimer();
  system.startTimer(0.01, [&] { ++ticks; });
  QTRY_VERIFY_WITH_TIMEOUT(ticks.load() > stopped, 5000);
}

void OSXPointerHoldNotificationsTests::system_aTick_thatThrows_isContainedAndTheTimerGoesOn()
{
  std::atomic<int> ticks{0};
  RunLoopThread loop;
  OSXPointerHoldSystem system(hooksOf(loop));

  system.startTimer(0.01, [&] {
    ++ticks;
    throw std::runtime_error("the tick failed");
  });
  QTRY_VERIFY_WITH_TIMEOUT(ticks.load() >= 3, 5000);
  system.stopTimer();
}

void OSXPointerHoldNotificationsTests::system_destroyedWithWorkQueuedAndATimerDue_runsNoTickAndStillRunsTheWork()
{
  std::atomic<bool> blocked{false};
  std::atomic<bool> release{false};
  std::atomic<int> ticks{0};
  auto probe = std::make_shared<Probe>();
  RunLoopThread loop;
  {
    OSXPointerHoldSystem system(hooksOf(loop));
    // the run loop is busy: what is posted to it, and the tick that is due, wait
    system.runOnTapThread([&] {
      blocked = true;
      for (int i = 0; i < 5000 && !release.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
    QTRY_VERIFY_WITH_TIMEOUT(blocked.load(), 5000);
    system.runOnTapThread([probe] { probe->note(Trigger::Timer, {}); });
    system.startTimer(0.005, [&ticks] { ++ticks; });
    QTest::qWait(30);
  }
  // the system is gone, and its timer with it
  release = true;
  // the work that was posted still runs, on its own copy of what it holds
  QTRY_VERIFY_WITH_TIMEOUT(probe->count(Trigger::Timer) == 1, 5000);
  QTest::qWait(50);
  QCOMPARE(ticks.load(), 0);
}

void OSXPointerHoldNotificationsTests::system_withoutARunLoop_doesNothing()
{
  std::atomic<int> ran{0};
  {
    OSXPointerHoldHooks none;
    OSXPointerHoldSystem system(none);
    system.runOnTapThread([&] { ++ran; });
    system.startTimer(0.005, [&] { ++ran; });
    system.stopTimer();
  }
  {
    RunLoopThread loop;
    OSXPointerHoldHooks ended = hooksOf(loop);
    loop.stop();
    OSXPointerHoldSystem system(ended);
    system.runOnTapThread([&] { ++ran; });
    system.startTimer(0.005, [&] { ++ran; });
  }
  QTest::qWait(40);
  QCOMPARE(ran.load(), 0);
}

//
// the controller on top of the real source, with a fake system
//
void OSXPointerHoldNotificationsTests::controller_aNotification_holdsThePointerAgain()
{
  RecordingSystem system;
  OSXPointerHoldNotifications notifications;
  PointerHoldController controller(system);
  controller.attach(notifications);
  controller.arm();
  QVERIFY(controller.armed());

  // the system gave the pointer back: it is somewhere else when the application is activated
  system.moveCursor({400.0, 500.0});
  postWorkspaceNotification(Trigger::AppActivated);
  QTRY_VERIFY_WITH_TIMEOUT(system.warps() >= 1, 5000);
  QCOMPARE(system.lastWarp().x, 100.0);
  QCOMPARE(system.lastWarp().y, 200.0);
  QVERIFY(controller.detections() >= 1);

  controller.disarm();
  controller.detach();
}

void OSXPointerHoldNotificationsTests::
    controller_destroyedWhileNotificationsAreInFlight_neverTouchesTheSystemAfterwards()
{
  Posters posters;
  QTRY_VERIFY_WITH_TIMEOUT(posters.posted() > 0, 5000);

  int touchedAfterwards = 0;
  int held = 0;
  const int rounds = 100;
  for (int round = 0; round < rounds; ++round) {
    auto system = std::make_shared<RecordingSystem>();
    {
      // the order of the members of OSXComputer: the system, the source, the controller (destroyed first)
      OSXPointerHoldNotifications notifications;
      PointerHoldController controller(*system);
      controller.attach(notifications);
      controller.arm();
      system->moveCursor({400.0, 500.0});
      // a real notification reaches the controller, and the posters go on while it is destroyed
      for (int i = 0; i < 2000 && system->warps() < 1; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      held += system->warps() > 0 ? 1 : 0;
      std::this_thread::sleep_for(std::chrono::microseconds((round % 5) * 120));
    }
    // the owner of the system is gone
    system->close();
    std::this_thread::sleep_for(std::chrono::microseconds(300));
    touchedAfterwards += system->callsAfterClose();
  }
  posters.stop();
  QCOMPARE(touchedAfterwards, 0);
  // no round was empty: the real notification reached the controller each time
  QCOMPARE(held, rounds);
}

QTEST_MAIN(OSXPointerHoldNotificationsTests)
