/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

// The hold of the pointer of a Mac server while the pointer is on a client.
//
// OSXComputer::leave() freezes the pointer of the Mac once (CGAssociateMouseAndMouseCursorPosition(false) and a hidden
// cursor) and nothing looks at it again. Apple documents those calls as the ones of the foreground application, and
// the core is a background-only process that is never in the foreground, so the system is free to take the state back
// (another application is activated, the space changes, the display wakes, the screen is locked, a driver or a remote
// tool moves the pointer, ...). Then the pointer of the Mac moves together with the one of the client.
//
// The policy below is pure: it decides, from what the system reports, whether the pointer is still where it was parked,
// and what to do about it. The calls into the system are behind two small interfaces, so that tests need no event, no
// cursor and no desktop. Nothing here includes a system header.
namespace deskflow::platform {

//! A place on the desktop, in the points of the global display space (the space of CGEventGetLocation)
struct PointerHoldPoint
{
  double x = 0.0;
  double y = 0.0;
};

//! The distance between two places
double pointerHoldDistance(PointerHoldPoint a, PointerHoldPoint b);

//! What made the pointer be looked at
enum class PointerHoldTrigger
{
  Event,                //!< a mouse event that the tap saw, at another place than the parked one
  Timer,                //!< the low rate timer that runs while the pointer is on a client
  AppActivated,         //!< another application became the active one
  SpaceChanged,         //!< the active space changed (Mission Control, full screen)
  DisplayWoke,          //!< the displays woke
  DisplaySlept,         //!< the displays went to sleep
  SystemWoke,           //!< the Mac woke
  ScreenLocked,         //!< the screen was locked
  ScreenUnlocked,       //!< the screen was unlocked
  DisplayReconfigured,  //!< a display was added, removed or changed
  TapDisabledByTimeout, //!< the system disabled the event tap because the callback was slow
  TapDisabledByUser     //!< the system disabled the event tap (secure input, the lock screen)
};

//! The name that a trigger has in the log (stable: it is read by people and by the log filter of the GUI)
const char *pointerHoldTriggerName(PointerHoldTrigger trigger);

//! The numbers of the policy
struct PointerHoldTuning
{
  //! The pointer is held again when it is farther than this from the parked place (points; absorbs the rounding)
  double tolerance = 1.0;
  //! The period of the timer that runs while the pointer is on a client (seconds)
  double timerInterval = 0.25;
  //! After a warp the place that the system reports can still be the old one for a moment (the warp is not
  //! synchronous, and the system suppresses local events for its suppression interval): within this many seconds a
  //! mouse event or the timer is not a reason to warp again. A notification is always looked at (seconds)
  double settleSeconds = 0.015;
  //! After a display was reconfigured the system puts the pointer where it wants: the place it reports within this many
  //! seconds is accepted as the parked one instead of being fought (seconds)
  double reconfigureGraceSeconds = 1.0;
  //! The event tap is switched on again at most once per this many seconds (a tap that the system keeps switching off
  //! must not be switched on in a loop)
  double tapRetrySeconds = 1.0;
  //! The cursor is hidden again at most this many times in one stay on a client: when the system ignores the hide, the
  //! hide count must not pile up (it is undone with one show per hide when the pointer comes back)
  int maxRehides = 3;
  //! At most one line in the log per this many seconds
  double logIntervalSeconds = 5.0;
};

//! What one look at the pointer found, and what is to be done about it
struct PointerHoldDecision
{
  bool reenableTap = false; //!< switch the event tap on again
  bool reassociate = false; //!< dissociate the mouse from the cursor again (and set the suppression interval first)
  bool warp = false;        //!< warp the cursor to \c target
  PointerHoldPoint target;  //!< the parked place
  bool rehide = false;      //!< the cursor is shown: hide it again
  // what was found (for the log)
  bool moved = false;        //!< the pointer was not where it was parked
  double distance = 0.0;     //!< how far from there (points)
  bool wasShown = false;     //!< the cursor was shown
  bool tapWasOff = false;    //!< the event tap was off
  PointerHoldTrigger trigger = PointerHoldTrigger::Timer;
  //! this look found something: the number of looks that found something so far, this one included
  uint64_t detections = 0;
  //! a line is to be written for it (at most one per \c logIntervalSeconds)
  bool log = false;
};

//! What the system reported at one look
struct PointerHoldObservation
{
  PointerHoldTrigger trigger = PointerHoldTrigger::Timer;
  PointerHoldPoint cursor;   //!< where the pointer is now
  bool tapEnabled = true;    //!< the event tap is on
  bool cursorShown = false;  //!< the cursor is visible
  double now = 0.0;          //!< monotonic seconds
};

//! The pure part: where the pointer is parked, and what a look at it means
/*!
Not thread safe (the controller below locks).  It holds no clock: every call that needs the time is given it.
*/
class PointerHoldPolicy
{
public:
  explicit PointerHoldPolicy(PointerHoldTuning tuning = {});

  //! The pointer left this computer: it is parked at \p where
  void arm(PointerHoldPoint where);
  //! The pointer is on this computer again
  void disarm();
  bool armed() const;
  //! The server moved the pointer on purpose while it is parked: that is the parked place now
  void repark(PointerHoldPoint where);
  PointerHoldPoint parked() const;
  //! True when \p where is the parked place (within the tolerance): the check of the hot path, no system call
  bool isParked(PointerHoldPoint where) const;

  //! Decide.  Nothing to do (a decision with nothing set) when the pointer is not parked
  PointerHoldDecision observe(const PointerHoldObservation &observation);

  const PointerHoldTuning &tuning() const
  {
    return m_tuning;
  }
  uint64_t detections() const
  {
    return m_detections;
  }

private:
  PointerHoldTuning m_tuning;
  bool m_armed = false;
  PointerHoldPoint m_parked;
  bool m_hasHeld = false;
  double m_lastHold = 0.0;
  bool m_hasGrace = false;
  double m_graceUntil = 0.0;
  bool m_hasTapRetry = false;
  double m_lastTapRetry = 0.0;
  bool m_hasLogged = false;
  double m_lastLog = 0.0;
  int m_rehides = 0;
  uint64_t m_detections = 0;
};

//! What the log line says about one decision.  \p computer is the client that has the pointer ("" when not known),
//! \p app a bundle id (or "" for none).  English, one line, no window title, no content of the user
std::string
formatPointerHoldLog(const PointerHoldDecision &decision, const std::string &computer, const std::string &app);

//! The application to name in the log: only the one that took the foreground (\p activated) for an activation, and
//! for any other trigger the front one only when it is Deskflow itself (\p own).  "" when none is to be named
std::string pointerHoldLogApp(
    PointerHoldTrigger trigger, const std::string &activated, const std::string &front, const std::string &own
);

//! The calls into the system (a fake in the tests, OSXPointerHoldSystem in the program)
class IPointerHoldSystem
{
public:
  virtual ~IPointerHoldSystem() = default;

  //! The live place of the pointer (not the place of an event: those can be old)
  virtual PointerHoldPoint cursorPosition() = 0;
  virtual bool tapEnabled() = 0;
  virtual bool cursorShown() = 0;
  virtual void enableTap() = 0;
  //! Sets the suppression interval of local events so that the warp does not hold the next events back
  virtual void prepareWarp() = 0;
  virtual void dissociate() = 0;
  virtual void warp(PointerHoldPoint where) = 0;
  //! Hides the cursor again, keeping the hide count at one
  virtual void hideAgain() = 0;
  //! The bundle id of the front application ("" when not known)
  virtual std::string frontApplication() = 0;
  //! Monotonic seconds
  virtual double now() = 0;
  virtual void log(const std::string &line) = 0;
  //! Runs \p work on the thread of the event tap (at once on a fake)
  virtual void runOnTapThread(std::function<void()> work) = 0;
  //! Calls \p tick every \p intervalSeconds on the thread of the event tap, until \c stopTimer()
  virtual void startTimer(double intervalSeconds, std::function<void()> tick) = 0;
  virtual void stopTimer() = 0;
};

//! The source of the notifications that mean "the system may have taken the pointer back"
class IPointerHoldNotifications
{
public:
  using Callback = std::function<void(PointerHoldTrigger, const std::string &app)>;
  virtual ~IPointerHoldNotifications() = default;
  //! \p callback can be called from any thread
  virtual void start(Callback callback) = 0;
  virtual void stop() = 0;
};

//! The policy, the system and the lock between the threads that look at the pointer
/*!
The pointer is looked at from the tap thread (mouse events, the timer), and notifications are moved to that thread; the
core thread arms and disarms in leave() and enter().  All the state is under one mutex, and everything that touches the
system runs under it, so two looks never run at once.
*/
class PointerHoldController
{
public:
  explicit PointerHoldController(IPointerHoldSystem &system, PointerHoldTuning tuning = {});
  ~PointerHoldController();

  PointerHoldController(const PointerHoldController &) = delete;
  PointerHoldController &operator=(const PointerHoldController &) = delete;

  //! The pointer left this computer: park it where it is, and start the timer
  void arm();
  //! The pointer is on this computer again: stop the timer
  void disarm();
  //! The server moved the pointer on purpose
  void repark(PointerHoldPoint where);
  //! The name of the client that has the pointer, for the log
  void setRemoteName(const std::string &name);
  //! Bundle id of this program, so that the log names the front application when it is this one
  void setOwnBundleId(const std::string &id);
  //! Listens to \p notifications until \c detach()
  void attach(IPointerHoldNotifications &notifications);
  void detach();

  //! A mouse event with this place: cheap when it is the parked place, otherwise the live place is looked at
  void onPointerEvent(PointerHoldPoint eventLocation);
  //! A notification that the system may have taken the pointer back (or the tap was disabled)
  void onNotification(PointerHoldTrigger trigger, const std::string &app = {});

  bool armed() const;
  uint64_t detections() const;

private:
  void look(PointerHoldTrigger trigger, const std::string &app);

  IPointerHoldSystem &m_system;
  mutable std::mutex m_mutex;
  PointerHoldPolicy m_policy;
  std::string m_remoteName;
  std::string m_ownBundleId;
  IPointerHoldNotifications *m_notifications = nullptr;
};

} // namespace deskflow::platform
