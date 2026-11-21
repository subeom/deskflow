/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXPointerHold.h"

#include <cmath>
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

PointerHoldController::PointerHoldController(IPointerHoldSystem &system, PointerHoldTuning tuning)
    : m_system(system),
      m_policy(tuning)
{
  // do nothing
}

PointerHoldController::~PointerHoldController()
{
  detach();
  m_system.stopTimer();
}

void PointerHoldController::arm()
{
  std::scoped_lock lock(m_mutex);
  m_policy.arm(m_system.cursorPosition());
  m_system.startTimer(m_policy.tuning().timerInterval, [this] { look(PointerHoldTrigger::Timer, {}); });
}

void PointerHoldController::disarm()
{
  std::scoped_lock lock(m_mutex);
  m_policy.disarm();
  m_system.stopTimer();
}

void PointerHoldController::repark(PointerHoldPoint where)
{
  std::scoped_lock lock(m_mutex);
  m_policy.repark(where);
}

void PointerHoldController::setRemoteName(const std::string &name)
{
  std::scoped_lock lock(m_mutex);
  m_remoteName = name;
}

void PointerHoldController::setOwnBundleId(const std::string &id)
{
  std::scoped_lock lock(m_mutex);
  m_ownBundleId = id;
}

void PointerHoldController::attach(IPointerHoldNotifications &notifications)
{
  detach();
  m_notifications = &notifications;
  notifications.start([this](PointerHoldTrigger trigger, const std::string &app) { onNotification(trigger, app); });
}

void PointerHoldController::detach()
{
  if (m_notifications != nullptr) {
    m_notifications->stop();
    m_notifications = nullptr;
  }
}

void PointerHoldController::onPointerEvent(PointerHoldPoint eventLocation)
{
  {
    std::scoped_lock lock(m_mutex);
    if (!m_policy.armed() || m_policy.isParked(eventLocation)) {
      return;
    }
  }
  look(PointerHoldTrigger::Event, {});
}

void PointerHoldController::onNotification(PointerHoldTrigger trigger, const std::string &app)
{
  m_system.runOnTapThread([this, trigger, app] { look(trigger, app); });
}

bool PointerHoldController::armed() const
{
  std::scoped_lock lock(m_mutex);
  return m_policy.armed();
}

uint64_t PointerHoldController::detections() const
{
  std::scoped_lock lock(m_mutex);
  return m_policy.detections();
}

void PointerHoldController::look(PointerHoldTrigger trigger, const std::string &app)
{
  std::scoped_lock lock(m_mutex);
  if (!m_policy.armed()) {
    return;
  }

  PointerHoldObservation observation;
  observation.trigger = trigger;
  observation.now = m_system.now();
  // the live place: the place of an event can be old (it was queued before the pointer was held again)
  observation.cursor = m_system.cursorPosition();
  if (trigger == PointerHoldTrigger::Event && m_policy.isParked(observation.cursor)) {
    return;
  }
  observation.tapEnabled = m_system.tapEnabled();
  observation.cursorShown = m_system.cursorShown();

  const auto decision = m_policy.observe(observation);

  if (decision.reenableTap) {
    m_system.enableTap();
  }
  if (decision.reassociate) {
    m_system.prepareWarp();
    m_system.dissociate();
  }
  if (decision.warp) {
    m_system.warp(decision.target);
  }
  if (decision.rehide) {
    m_system.hideAgain();
  }
  if (decision.log) {
    const auto front = m_system.frontApplication();
    m_system.log(formatPointerHoldLog(decision, m_remoteName, pointerHoldLogApp(trigger, app, front, m_ownBundleId)));
  }
}

} // namespace deskflow::platform
