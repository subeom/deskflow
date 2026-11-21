/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "AwdlModel.h"
#include "IAwdlSystem.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <functional>

namespace deskflow::awdl {

/*!
 * \brief The option "turn off the wireless link of AirDrop and Handoff while sharing" (macOS): it reads the state of
 * awdl0, asks the policy (decide()) what to do with it, and does it through IAwdlSystem. It owns no window and shows
 * nothing: it says its state and whether the hint is to be shown, and the window shows them.
 *
 * The link being up is no problem by itself: the mouse stutters only while AWDL is in use. So it also measures, now and
 * then, the latency to the gateway of the Wi-Fi that the computer is on (a burst of probes, see AwdlModel.h), and the
 * hint is only for a link that is up while the latency has the spikes.
 *
 * What it does, and what it never does:
 *  - the core starts (server or client) with the option on and the link up: the dialog of the password, once for this
 * run of the core; a no, or a failure, is logged in one line and the dialog is not shown again by itself
 *  - while the core runs the state is read again every few seconds (no privilege), and a burst of probes is made once a
 *    minute while the link is up and the computer is on Wi-Fi (not on a wire: that is said, and nothing is sent); there
 * is only the hint when the spikes are there, and its button asks for the password
 *  - the probes go to the gateway of the default route and to nothing else (canProbe())
 *  - when the core stops nothing is restored (that would ask for the password again); turnBackOn() is the button of the
 *    settings, and the link comes back by itself after a restart of the Mac
 *  - the option off, or not a Mac: no program is run at all
 */
class AwdlController : public QObject
{
  Q_OBJECT

public:
  struct Options
  {
    //! The option (the setting), read when it is needed
    std::function<bool()> optionOn;
    //! The sentence of the dialog of the password, in the language of the program
    std::function<QString(Direction)> prompt;
    //! The state is read again this often while the core runs
    int pollMs = 8000;
    //! A burst of probes is made this often while the core runs and the link is up
    int burstEveryMs = kBurstEveryMs;
    //! Not a Mac: the controller does nothing
    bool supported = true;
  };

  //! \p system is taken over (it is a child of the controller)
  AwdlController(IAwdlSystem *system, Options options, QObject *parent = nullptr);

  LinkState state() const;
  //! The hint is to be shown (the link is up again and the person was asked)
  bool hintVisible() const;
  //! The dialog of the password is open
  bool isAsking() const;
  bool coreRunning() const;
  //! The default route that was looked at last (what kind of connection this computer is on)
  Route route() const;
  //! What the last burst said (the numbers of the status line)
  BurstStats latency() const;
  //! What the status line says: the state of the link with what the measuring found
  Summary summary() const;
  //! A lookup of the route or a burst is under way
  bool isMeasuring() const;

  //! The core starts or stops (any state of it that is not stopped is running)
  void setCoreRunning(bool running);
  //! The setting changed
  void optionChanged();
  //! Reads the state now (the page of the settings is shown)
  void refresh();
  //! The button: a burst of probes now, on Wi-Fi, whether or not the option is on (to see what the link does before and
  //! after it is turned off)
  void measureNow();
  //! The button: asks for the password and turns the link off, whether or not the option is on
  void turnOffNow();
  //! The button: asks for the password and turns the link on; the program does not take it back in this run
  void turnBackOn();

Q_SIGNALS:
  void stateChanged(deskflow::awdl::LinkState state);
  void hintChanged(bool visible);
  void askingChanged(bool asking);
  //! The route or the numbers of the last burst changed
  void latencyChanged();
  void measuringChanged(bool measuring);
  //! One line for the log, without the name of the program in front
  void logLine(const QString &line);

private:
  AwdlController(const AwdlController &) = delete;

  bool optionOn() const;
  //! The option is on, on a Mac, and the core runs: the state is read again and again, and the bursts are made
  bool isActive() const;
  void updatePolling();
  //! What the policy says of the state that is known now: the dialog, the hint, and a burst when it is time
  void evaluate();
  void maybeProbe();
  void startLookup(bool thenMeasure);
  void requestChange(Direction direction);
  void setHint(bool visible);
  void setAsking(bool asking);
  void setMeasuring(bool measuring);
  void onStateRead(LinkState state);
  void onLinkSet(AdminResult result);
  void onRouteFound(const Route &route);
  void onMeasured(const QList<Sample> &samples);

  IAwdlSystem *m_system = nullptr;
  Options m_options;
  QTimer *m_poll = nullptr;
  LinkState m_state = LinkState::Unknown;
  bool m_hint = false;
  bool m_running = false;
  bool m_asking = false;
  SessionMemory m_memory;
  Route m_route;
  BurstStats m_stats;
  //! The last burst was taken while the link was up (its pattern is only then the link's)
  bool m_statsWhileUp = false;
  bool m_measuring = false;
  //! The lookup is for a burst (and not only for the kind of the connection)
  bool m_measureAfterLookup = false;
  QElapsedTimer m_sinceAttempt;
  Direction m_requested = Direction::Down;
  //! What the log was told of the last burst (it is told again when it changes, and for the button)
  Pattern m_loggedPattern = Pattern::NotMeasured;
  bool m_manualMeasure = false;
};

} // namespace deskflow::awdl
