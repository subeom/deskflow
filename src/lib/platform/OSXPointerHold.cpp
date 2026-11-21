/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXPointerHold.h"

#include <cmath>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <utility>

namespace deskflow::platform {

double pointerHoldDistance(PointerHoldPoint a, PointerHoldPoint b)
{
  return std::hypot(a.x - b.x, a.y - b.y);
}

const char *pointerHoldTriggerName(PointerHoldTrigger trigger)
{
  using enum PointerHoldTrigger;
  switch (trigger) {
  case Event:
    return "event";
  case Timer:
    return "timer";
  case AppActivated:
    return "app activated";
  case SpaceChanged:
    return "space changed";
  case DisplayWoke:
    return "display woke";
  case DisplaySlept:
    return "display slept";
  case SystemWoke:
    return "system woke";
  case ScreenLocked:
    return "screen locked";
  case ScreenUnlocked:
    return "screen unlocked";
  case DisplayReconfigured:
    return "display reconfigured";
  case TapDisabledByTimeout:
    return "tap disabled by timeout";
  case TapDisabledByUser:
    return "tap disabled by user input";
  }
  return "unknown";
}

//
// PointerHoldPolicy
//

PointerHoldPolicy::PointerHoldPolicy(PointerHoldTuning tuning) : m_tuning(tuning)
{
  // do nothing
}

void PointerHoldPolicy::arm(PointerHoldPoint where)
{
  m_armed = true;
  m_parked = where;
  m_hasHeld = false;
  m_hasGrace = false;
  m_hasTapRetry = false;
  m_rehides = 0;
}

void PointerHoldPolicy::disarm()
{
  m_armed = false;
}

bool PointerHoldPolicy::armed() const
{
  return m_armed;
}

void PointerHoldPolicy::repark(PointerHoldPoint where)
{
  if (!m_armed) {
    return;
  }
  m_parked = where;
  m_hasHeld = false;
}

PointerHoldPoint PointerHoldPolicy::parked() const
{
  return m_parked;
}

bool PointerHoldPolicy::isParked(PointerHoldPoint where) const
{
  return pointerHoldDistance(where, m_parked) <= m_tuning.tolerance;
}

PointerHoldDecision PointerHoldPolicy::observe(const PointerHoldObservation &observation)
{
  using enum PointerHoldTrigger;
  PointerHoldDecision decision;
  decision.trigger = observation.trigger;
  decision.target = m_parked;
  if (!m_armed) {
    return decision;
  }

  const auto now = observation.now;
  const bool notification = observation.trigger != Event && observation.trigger != Timer;

  if (observation.trigger == DisplayReconfigured) {
    m_hasGrace = true;
    m_graceUntil = now + m_tuning.reconfigureGraceSeconds;
  }

  double distance = pointerHoldDistance(observation.cursor, m_parked);
  bool moved = distance > m_tuning.tolerance;
  if (moved && m_hasGrace && now < m_graceUntil) {
    // the system places the pointer after a reconfiguration of the displays: that is where it is parked now
    m_parked = observation.cursor;
    decision.target = m_parked;
    moved = false;
  }
  if (moved && !notification && m_hasHeld && (now - m_lastHold) < m_tuning.settleSeconds) {
    // a place that is read right after a warp can still be the old one: do not warp for the same movement twice
    moved = false;
  }

  const bool tapOff = !observation.tapEnabled || observation.trigger == TapDisabledByUser;

  decision.moved = moved;
  decision.distance = moved ? distance : 0.0;
  decision.reassociate = moved || notification;
  decision.warp = moved;
  if (moved) {
    m_hasHeld = true;
    m_lastHold = now;
  }

  if (observation.cursorShown && m_rehides < m_tuning.maxRehides) {
    ++m_rehides;
    decision.rehide = true;
    decision.wasShown = true;
  }

  if (tapOff && (!m_hasTapRetry || (now - m_lastTapRetry) >= m_tuning.tapRetrySeconds)) {
    m_hasTapRetry = true;
    m_lastTapRetry = now;
    decision.reenableTap = true;
    decision.tapWasOff = true;
  }

  if (moved || decision.rehide || decision.reenableTap) {
    decision.detections = ++m_detections;
    if (!m_hasLogged || (now - m_lastLog) >= m_tuning.logIntervalSeconds) {
      m_hasLogged = true;
      m_lastLog = now;
      decision.log = true;
    }
  }
  return decision;
}

//
// the line of the log
//

std::string
formatPointerHoldLog(const PointerHoldDecision &decision, const std::string &computer, const std::string &app)
{
  const auto where = computer.empty() ? std::string("?") : computer;
  std::string line;
  std::string details;
  if (decision.moved) {
    line = "the pointer of this Mac moved while it is on \"" + where + "\": held it again";
    details = "moved " + std::to_string(std::llround(decision.distance)) + " px, ";
  } else if (decision.wasShown) {
    line = "the pointer of this Mac was shown while it is on \"" + where + "\": hid it again";
  } else {
    line = "the event tap was off while the pointer is on \"" + where + "\": switched it on again";
  }
  details += std::string("trigger: ") + pointerHoldTriggerName(decision.trigger);
  details += ", times: " + std::to_string(decision.detections);
  if (decision.moved && decision.wasShown) {
    details += ", the cursor was shown";
  }
  if ((decision.moved || decision.wasShown) && decision.tapWasOff) {
    details += ", the event tap was off";
  }
  if (!app.empty()) {
    details += ", app: " + app;
  }
  return line + " (" + details + ")";
}

std::string pointerHoldLogApp(
    PointerHoldTrigger trigger, const std::string &activated, const std::string &front, const std::string &own
)
{
  if (trigger == PointerHoldTrigger::AppActivated) {
    return activated;
  }
  if (!own.empty() && front == own) {
    return own;
  }
  return {};
}

//
// PointerHoldController
//

// What the controller shares with the work that it hands to the system: a notification, the work posted to the tap
// thread, a tick of the timer.  That work can run after the controller is gone, so it holds this by value and never the
// controller (a pointer to the controller dangles, and its destroyed mutex throws when it is locked: that was the crash
// of 2.0.4).  It lives as long as the last of them.
//
// Two mutexes, never held together by one thread in the order gate -> mutex:
//  - \c mutex is the lock of the policy, and of every call into the system that is not a hand-over; it is held across
//    system calls, so nothing that a notification of the system waits for may take it,
//  - \c gate guards the pointer to the system and the count of the hand-overs; it is held for a moment, never across a
//    call.  A notification of the system is posted by a thread that waits for it (the center runs the observers of a
//    queue and the poster waits until they are done): it takes the gate and nothing else.
struct PointerHoldState
{
  PointerHoldState(IPointerHoldSystem &s, PointerHoldTuning tuning) : policy(tuning), system(&s)
  {
    // do nothing
  }

  std::mutex mutex;
  PointerHoldPolicy policy;
  std::string remoteName;
  std::string ownBundleId;
  bool failureReported = false;

  std::mutex gate;
  std::condition_variable idle;
  IPointerHoldSystem *system; //!< null when the controller is gone
  int handingOver = 0;        //!< the callers that use the system without \c mutex
};

namespace {

using State = std::shared_ptr<PointerHoldState>;

IPointerHoldSystem *systemOf(PointerHoldState &state)
{
  std::scoped_lock lock(state.gate);
  return state.system;
}

// The reason of the exception that is being handled (call it from a catch block)
std::string currentExceptionText() noexcept
{
  try {
    throw;
  } catch (const std::exception &e) {
    try {
      return e.what();
    } catch (...) {
      return "an error";
    }
  } catch (...) {
    return "an unknown error";
  }
}

// Something threw inside a call of the controller.  The call is given up (the pointer is not held for it) and one line
// says so, once: nothing may leave the controller, because the callers are callbacks of the system, and an exception
// that leaves one of those aborts the program.  Call it from a catch block, with no lock held
void report(PointerHoldState &state) noexcept
{
  try {
    const auto what = currentExceptionText();
    std::scoped_lock lock(state.mutex);
    if (state.failureReported) {
      return;
    }
    state.failureReported = true;
    if (auto *system = systemOf(state)) {
      system->log("the pointer of this Mac could not be held (" + what + "); reported once");
    }
  } catch (...) {
    // nothing more can be done
  }
}

void look(PointerHoldState &state, PointerHoldTrigger trigger, const std::string &app)
{
  std::scoped_lock lock(state.mutex);
  auto *system = systemOf(state);
  if (system == nullptr || !state.policy.armed()) {
    return;
  }

  PointerHoldObservation observation;
  observation.trigger = trigger;
  observation.now = system->now();
  // the live place: the place of an event can be old (it was queued before the pointer was held again)
  observation.cursor = system->cursorPosition();
  if (trigger == PointerHoldTrigger::Event && state.policy.isParked(observation.cursor)) {
    return;
  }
  observation.tapEnabled = system->tapEnabled();
  observation.cursorShown = system->cursorShown();

  const auto decision = state.policy.observe(observation);

  if (decision.reenableTap) {
    system->enableTap();
  }
  if (decision.reassociate) {
    system->prepareWarp();
    system->dissociate();
  }
  if (decision.warp) {
    system->warp(decision.target);
  }
  if (decision.rehide) {
    system->hideAgain();
  }
  if (decision.log) {
    const auto front = system->frontApplication();
    const auto activeApp = pointerHoldLogApp(trigger, app, front, state.ownBundleId);
    system->log(formatPointerHoldLog(decision, state.remoteName, activeApp));
  }
}

void lookContained(const State &state, PointerHoldTrigger trigger, const std::string &app) noexcept
{
  try {
    look(*state, trigger, app);
  } catch (...) {
    report(*state);
  }
}

// Counts the caller as one that uses the system without the mutex until it is done, so that the destructor of the
// controller waits for it
class HandOver
{
public:
  explicit HandOver(PointerHoldState &state) : m_state(state)
  {
    std::scoped_lock lock(m_state.gate);
    m_system = m_state.system;
    if (m_system != nullptr) {
      ++m_state.handingOver;
    }
  }
  ~HandOver()
  {
    if (m_system == nullptr) {
      return;
    }
    try {
      std::scoped_lock lock(m_state.gate);
      --m_state.handingOver;
      m_state.idle.notify_all();
    } catch (...) {
      // the gate cannot be locked: nothing more can be done
    }
  }
  HandOver(const HandOver &) = delete;
  HandOver &operator=(const HandOver &) = delete;

  //! The system, or null when the controller is gone
  IPointerHoldSystem *system() const
  {
    return m_system;
  }

private:
  PointerHoldState &m_state;
  IPointerHoldSystem *m_system = nullptr;
};

void handOver(const State &state, PointerHoldTrigger trigger, const std::string &app) noexcept
{
  try {
    HandOver use(*state);
    if (use.system() != nullptr) {
      use.system()->runOnTapThread([state, trigger, app] { lookContained(state, trigger, app); });
    }
  } catch (...) {
    report(*state);
  }
}

} // namespace

PointerHoldController::PointerHoldController(IPointerHoldSystem &system, PointerHoldTuning tuning)
    : m_state(std::make_shared<PointerHoldState>(system, tuning))
{
  // do nothing
}

PointerHoldController::~PointerHoldController()
{
  detach();
  try {
    // from here nothing that the system still runs for us touches it: the work that is queued, the tick that is in
    // flight, a notification that is being handed over.  The system is gone after this, the state lives on
    if (auto *system = systemOf(*m_state)) {
      std::scoped_lock lock(m_state->mutex);
      try {
        system->stopTimer();
      } catch (...) {
        // the timer is gone with the system, and a tick that is left finds no system
      }
    }
    {
      std::unique_lock lock(m_state->gate);
      m_state->system = nullptr;
      m_state->idle.wait(lock, [this] { return m_state->handingOver == 0; });
    }
    // a look that was running when the system was taken away is finished when the mutex is free
    std::scoped_lock lock(m_state->mutex);
  } catch (...) {
    // the program is going away: nothing more can be done
  }
}

void PointerHoldController::arm() noexcept
{
  try {
    std::scoped_lock lock(m_state->mutex);
    auto *system = systemOf(*m_state);
    if (system == nullptr) {
      return;
    }
    m_state->policy.arm(system->cursorPosition());
    system->startTimer(m_state->policy.tuning().timerInterval, [state = m_state] {
      lookContained(state, PointerHoldTrigger::Timer, {});
    });
  } catch (...) {
    report(*m_state);
  }
}

void PointerHoldController::disarm() noexcept
{
  try {
    std::scoped_lock lock(m_state->mutex);
    m_state->policy.disarm();
    if (auto *system = systemOf(*m_state)) {
      system->stopTimer();
    }
  } catch (...) {
    report(*m_state);
  }
}

void PointerHoldController::repark(PointerHoldPoint where) noexcept
{
  try {
    std::scoped_lock lock(m_state->mutex);
    m_state->policy.repark(where);
  } catch (...) {
    report(*m_state);
  }
}

void PointerHoldController::setRemoteName(const std::string &name) noexcept
{
  try {
    std::scoped_lock lock(m_state->mutex);
    m_state->remoteName = name;
  } catch (...) {
    report(*m_state);
  }
}

void PointerHoldController::setOwnBundleId(const std::string &id) noexcept
{
  try {
    std::scoped_lock lock(m_state->mutex);
    m_state->ownBundleId = id;
  } catch (...) {
    report(*m_state);
  }
}

void PointerHoldController::attach(IPointerHoldNotifications &notifications) noexcept
{
  try {
    detach();
    m_notifications = &notifications;
    notifications.start([state = m_state](PointerHoldTrigger trigger, const std::string &app) {
      handOver(state, trigger, app);
    });
  } catch (...) {
    report(*m_state);
  }
}

void PointerHoldController::detach() noexcept
{
  try {
    if (m_notifications != nullptr) {
      m_notifications->stop();
      m_notifications = nullptr;
    }
  } catch (...) {
    m_notifications = nullptr;
    report(*m_state);
  }
}

void PointerHoldController::onPointerEvent(PointerHoldPoint eventLocation) noexcept
{
  try {
    {
      std::scoped_lock lock(m_state->mutex);
      if (!m_state->policy.armed() || m_state->policy.isParked(eventLocation)) {
        return;
      }
    }
    lookContained(m_state, PointerHoldTrigger::Event, {});
  } catch (...) {
    report(*m_state);
  }
}

void PointerHoldController::onNotification(PointerHoldTrigger trigger, const std::string &app) noexcept
{
  handOver(m_state, trigger, app);
}

bool PointerHoldController::armed() const noexcept
{
  try {
    std::scoped_lock lock(m_state->mutex);
    return m_state->policy.armed();
  } catch (...) {
    return false;
  }
}

uint64_t PointerHoldController::detections() const noexcept
{
  try {
    std::scoped_lock lock(m_state->mutex);
    return m_state->policy.detections();
  } catch (...) {
    return 0;
  }
}

} // namespace deskflow::platform
