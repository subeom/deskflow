/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/layout/DisplaySignature.h"
#include "gui/layout/Layout.h"
#include "gui/layout/LayoutProfileStore.h"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <vector>

namespace deskflow::gui::layout {

//! Milliseconds of a clock that only goes forward; the caller says what time it is, nothing here waits or reads a clock
using Millis = int64_t;

//! How long things take (milliseconds)
//! A burst of display events (plug, sleep, wake, lid) is one change: it is looked at when this long has passed since the
//! last event ...
constexpr Millis kQuietMs = 1500;
//! ... or this long after the first one, if the events do not stop
constexpr Millis kMaxWaitMs = 8000;
//! When the displays cannot be read, or the application cannot change the arrangement now, it is tried again after this
constexpr Millis kRetryMs = 1000;
constexpr int kMaxReadRetries = 5;
//! The core is restarted by this feature at most once in this time
constexpr Millis kMinRestartGapMs = 10000;
//! How often the displays are looked at when no event came (a safety net: the system does not always say)
constexpr Millis kPollMs = 3000;

//! What the user is told (the words are the window's; this says what happened)
struct Notice
{
  enum class Kind
  {
    //! A remembered layout of these displays was put back
    Restored,
    //! No layout was remembered for these displays: the arrangement was kept (computers that would be out of reach were
    //! put against the server again) and is remembered now
    Remembered,
    //! The arrangement was put back as it was before the last change
    Undone,
    //! A remembered layout could not be put back; the arrangement is as it was
    Failed,
    //! The file of remembered layouts cannot be used
    StoreProblem
  };
  Kind kind = Kind::Restored;
  //! Restored: it was the layout of exactly these displays; else of similar ones
  bool exact = false;
  double score = 0.0;
  QString profileName;
  QString oldId;
  QString newId;
  //! Computers that were put against the server again, those that could not be, and those that the layout did not know
  QStringList reattached;
  QStringList notReattached;
  QStringList notInProfile;
  //! The core was restarted for it
  bool restarted = false;
  //! The restart waits for the end of the minimum time between two restarts
  bool restartLater = false;
  bool canUndo = false;
  QString detail;
};

//! What changing the arrangement did
struct ApplyOutcome
{
  //! It was made, as a whole; false: nothing was changed
  bool applied = false;
  //! A place or a zoom was another
  bool changed = false;
  //! Computers of the configuration that the layout did not know (they were kept, or put beside the others)
  QStringList notInLayout;
  QString error;
};

//! The computers that the fallback put against the server again, and the arrangement that has them there
struct ReattachOutcome
{
  std::optional<Layout> layout;
  QStringList reattached;
  QStringList notReattached;
};

//! Everything outside: the platform, the arrangement of the settings, the core, the clock, the log, the window
class ILayoutHost
{
public:
  virtual ~ILayoutHost() = default;

  //! The feature is on and can work: it is switched on, with the arrangement, in server mode
  virtual bool enabled() const = 0;
  //! The arrangement can be changed now: no dialog is open that has a copy of it
  virtual bool canApplyNow() const = 0;
  //! The displays of this computer now; nothing when they cannot be read
  virtual std::vector<DisplayInfo> displays() = 0;
  //! Where the computers are now; nothing when there is no arrangement
  virtual Layout currentLayout() const = 0;
  //! Puts the computers of \p layout where it says, as a whole or not at all, and saves the settings
  virtual ApplyOutcome applyLayout(const Layout &layout) = 0;
  //! The arrangement that has the computers against the server again, for the displays \p to after \p from
  virtual ReattachOutcome reattach(const DisplaySignature &from, const DisplaySignature &to) = 0;
  //! The core runs, and the configuration that it would be given now is not the one that it was started with
  virtual bool coreRunning() const = 0;
  virtual bool coreConfigDiffers() const = 0;
  virtual void restartCore() = 0;
  //! Milliseconds since the epoch, for the times that are kept
  virtual int64_t wallClockMs() const = 0;

  virtual void debug(const QString &line) = 0;
  virtual void info(const QString &line) = 0;
  virtual void warn(const QString &line) = 0;
  virtual void notify(const Notice &notice) = 0;
};

//! Remembers the arrangement for each situation of the displays of the server, and brings it back (experimental)
/*!
The state machine of the feature, with no timer of its own: the caller says what time it is with every call and asks
\c nextDeadline() when it has to call \c tick() next. It does nothing, at all, while the host says it is not \c enabled().

\code
Disabled --enabled--> Idle --event, or a poll that sees another situation--> Settling --quiet for kQuietMs, or
kMaxWaitMs after the first event--> (look) --> Idle
                                       |--> Deferred while the host cannot change the arrangement, tried again each
                                            kRetryMs
\endcode

When it looks, the signature of the displays is made; the displays that cannot be read are tried again a few times,
then given up (the arrangement is kept); a signature that is the one that the arrangement already belongs to is
ignored; else the remembered layout of exactly these displays is put back, or else the most similar one that is similar
enough, or else nothing is remembered for them: then the arrangement is kept, the computers that the change left out of
reach are put against the server again (\c ILayoutHost::reattach), and what there is is remembered for these displays.
After that the core is restarted if (and only if) it runs and the configuration that it would get is another one, and
not more often than every \c kMinRestartGapMs. The one step before the change is kept, for \c undo().
*/
class LayoutMemory
{
public:
  enum class State
  {
    Disabled,
    Idle,
    Settling,
    Deferred
  };

  LayoutMemory(ILayoutHost &host, LayoutProfileStore &store);

  //! The feature was switched on (or the program started with it on): looks at the displays at once
  void start(Millis now);
  //! The feature was switched off: nothing pending, no undo
  void stop();

  //! The system said that displays may have changed
  void displaysEvent(Millis now);
  //! Looks at the displays without an event: a change that no event told of is treated as one
  void poll(Millis now);
  //! Time has passed: looks at the displays when it is time, and restarts the core when it was waiting for that
  void tick(Millis now);
  //! The user saved the arrangement: it is what is remembered for the situation that the arrangement belongs to
  void configSaved(Millis now);
  //! Puts the arrangement back as it was before the last change, once
  bool undo(Millis now);
  //! Where the computers were before the last change, for a dialog that works on a copy of the arrangement
  const Layout *undoLayout() const
  {
    return m_undo ? &m_undo->before : nullptr;
  }
  //! The change cannot be undone any more (the user took it back himself)
  void dropUndo();
  //! Remembers \p layout for the situation of the displays under \p name (the user's name for it)
  QString saveCurrentAs(const QString &name, const Layout &layout, Millis now);

  State state() const;
  //! When \c tick() has to be called at the latest, if it has to be
  std::optional<Millis> nextDeadline() const;
  bool canUndo() const
  {
    return m_undo.has_value();
  }
  //! The last thing the user was told, if it can still be undone or has not been replaced
  const std::optional<Notice> &lastNotice() const
  {
    return m_notice;
  }
  LayoutProfileStore &store()
  {
    return m_store;
  }

private:
  struct Pending
  {
    Millis first = 0;
    Millis last = 0;
    Millis due = 0;
    int readFailures = 0;
    bool blocked = false;
  };
  struct Undo
  {
    Layout before;
    QString createdProfileId;
    QString oldId;
    QString newId;
  };
  enum class Restart
  {
    No,
    Yes,
    Later
  };

  bool active() const;
  void expect(Millis now);
  void lookNow(Millis now);
  void evaluate(Millis now);
  void adopt(const DisplaySignature &current);
  void applyFor(const DisplaySignature &now, const std::optional<DisplaySignature> &before, Millis time);
  Restart restartIfNeeded(Millis now);
  void saveStore();
  void tell(Notice notice);
  static QString describe(const DisplaySignature &signature);
  static QString restartText(Restart restart);

  ILayoutHost &m_host;
  LayoutProfileStore &m_store;
  bool m_on = false;
  std::optional<Pending> m_pending;
  std::optional<Undo> m_undo;
  std::optional<Notice> m_notice;
  std::optional<Millis> m_restartAt;
  std::optional<Millis> m_lastRestart;
  //! The arrangement as this object last knew it to be: after a change that it made and after the user saved it. A save
  //! that finds it unchanged teaches nothing: what the arrangement belongs to is not what the user did
  std::optional<Layout> m_known;
  bool m_saveWarned = false;
};

} // namespace deskflow::gui::layout
