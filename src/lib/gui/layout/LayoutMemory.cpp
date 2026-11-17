/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/LayoutMemory.h"

#include "gui/layout/LayoutMatcher.h"

#include <algorithm>

namespace deskflow::gui::layout {

namespace {

QString displayCount(size_t count)
{
  return QStringLiteral("%1 display%2").arg(count).arg(count == 1 ? QString() : QStringLiteral("s"));
}

QString namesOf(const QStringList &names)
{
  return names.isEmpty() ? QStringLiteral("none") : names.join(QStringLiteral(", "));
}

} // namespace

LayoutMemory::LayoutMemory(ILayoutHost &host, LayoutProfileStore &store) : m_host(host), m_store(store)
{
}

bool LayoutMemory::active() const
{
  return m_on && m_host.enabled();
}

QString LayoutMemory::describe(const DisplaySignature &signature)
{
  return QStringLiteral("%1 (%2: %3)").arg(signature.id(), displayCount(signature.displays().size()), signature.summary());
}

QString LayoutMemory::restartText(Restart restart)
{
  switch (restart) {
  case Restart::Yes:
    return QStringLiteral("yes");
  case Restart::Later:
    return QStringLiteral("later");
  default:
    return QStringLiteral("no");
  }
}

void LayoutMemory::start(Millis now)
{
  m_pending.reset();
  m_undo.reset();
  m_notice.reset();
  m_restartAt.reset();
  m_known.reset();
  m_on = false;
  if (!m_host.enabled())
    return;

  if (m_store.isInert()) {
    const auto message = QStringLiteral("layout memory: off: the file of remembered layouts is of a newer version, it is "
                                        "not used and not changed (%1)")
                             .arg(m_store.path());
    m_host.warn(message);
    Notice notice;
    notice.kind = Notice::Kind::StoreProblem;
    notice.detail = message;
    tell(notice);
    return;
  }

  m_on = true;
  const auto count = m_store.profiles().size();
  const auto shown = DisplaySignature::fromDisplays(m_host.displays());
  m_host.info(
      QStringLiteral("layout memory: on, %1 remembered layout%2, displays=%3")
          .arg(count)
          .arg(count == 1 ? QString() : QStringLiteral("s"))
          .arg(shown.isValid() ? describe(shown) : QStringLiteral("not readable yet"))
  );

  m_pending = Pending{now, now, now, 0, false};
  lookNow(now);
}

void LayoutMemory::stop()
{
  m_on = false;
  m_pending.reset();
  m_undo.reset();
  m_notice.reset();
  m_restartAt.reset();
  m_known.reset();
}

void LayoutMemory::expect(Millis now)
{
  if (!m_pending)
    m_pending = Pending{now, now, 0, 0, false};
  m_pending->last = now;
  m_pending->blocked = false;
  m_pending->due = std::min(m_pending->last + kQuietMs, m_pending->first + kMaxWaitMs);
}

void LayoutMemory::displaysEvent(Millis now)
{
  if (!active())
    return;
  expect(now);
}

void LayoutMemory::poll(Millis now)
{
  if (!active() || m_pending)
    return;
  const auto sample = DisplaySignature::fromDisplays(m_host.displays());
  if (!sample.isValid())
    return;
  const auto known = m_store.active();
  if (known && *known == sample)
    return;
  m_host.debug(QStringLiteral("layout memory: the displays are %1 and no event said so").arg(sample.id()));
  expect(now);
}

void LayoutMemory::tick(Millis now)
{
  if (!active())
    return;

  if (m_restartAt && now >= *m_restartAt) {
    m_restartAt.reset();
    // the wait is over: it is only done if it is still needed
    if (restartIfNeeded(now) == Restart::Yes)
      m_host.info(QStringLiteral("layout memory: restarted the core after the minimum time since the last restart"));
  }

  if (!m_pending || now < m_pending->due)
    return;
  lookNow(now);
}

void LayoutMemory::lookNow(Millis now)
{
  if (!m_host.canApplyNow()) {
    m_pending->blocked = true;
    m_pending->due = now + kRetryMs;
    return;
  }
  m_pending->blocked = false;
  evaluate(now);
}

void LayoutMemory::evaluate(Millis now)
{
  const auto current = DisplaySignature::fromDisplays(m_host.displays());
  if (!current.isValid()) {
    // the system lists no display while it wakes up: later it does
    if (++m_pending->readFailures >= kMaxReadRetries) {
      m_host.warn(QStringLiteral("layout memory: the displays could not be read after %1 tries; the arrangement is kept")
                      .arg(kMaxReadRetries));
      m_pending.reset();
    } else {
      m_pending->due = now + kRetryMs;
    }
    return;
  }
  m_pending.reset();

  const auto known = m_store.active();
  if (!known) {
    // nothing says what the arrangement belongs to (the first time, or the feature was off): it is the user's, and it is
    // what is remembered for the displays that there are. A layout that is remembered for them is not put over it.
    adopt(current);
    return;
  }
  if (*known == current) {
    if (!m_known)
      m_known = m_host.currentLayout();
    m_host.debug(QStringLiteral("layout memory: displays event, signature unchanged (%1)").arg(current.id()));
    return;
  }
  if (current.truncated())
    m_host.warn(QStringLiteral("layout memory: more than %1 displays, only the first ones are used").arg(DisplaySignature::kMaxDisplays));
  applyFor(current, known, now);
}

void LayoutMemory::adopt(const DisplaySignature &current)
{
  const auto layout = m_host.currentLayout();
  m_store.setActive(current);
  if (layout.isEmpty()) {
    m_known.reset();
    saveStore();
    m_host.info(QStringLiteral("layout memory: no arrangement to remember for new=%1").arg(current.id()));
    return;
  }
  const auto result = m_store.upsert(current, layout, m_host.wallClockMs());
  m_known = layout;
  saveStore();
  if (result.created || result.changed) {
    const auto *profile = m_store.find(result.id);
    m_host.info(QStringLiteral("layout memory: remembered the arrangement for %1 as \"%2\" (%3)")
                    .arg(current.id(), profile ? profile->name : QString(),
                         result.created ? QStringLiteral("created") : QStringLiteral("updated")));
  }
}

void LayoutMemory::applyFor(const DisplaySignature &current, const std::optional<DisplaySignature> &before, Millis now)
{
  m_host.info(
      QStringLiteral("layout memory: displays changed old=%1 (%2) new=%3")
          .arg(
              before ? before->id() : QStringLiteral("none"), displayCount(before ? before->displays().size() : 0),
              describe(current)
          )
  );

  const auto oldId = before ? before->id() : QStringLiteral("none");
  const auto wall = m_host.wallClockMs();
  const Layout beforeLayout = m_host.currentLayout();

  // with no arrangement there is nothing to bring back: the situation is noted, and that is all
  if (beforeLayout.isEmpty()) {
    m_store.setActive(current);
    m_known.reset();
    saveStore();
    m_host.info(QStringLiteral("layout memory: no arrangement to remember for new=%1").arg(current.id()));
    return;
  }

  Notice notice;
  notice.oldId = before ? before->id() : QString();
  notice.newId = current.id();
  bool changed = false;
  bool failed = false;
  QString createdProfile;

  const auto match = bestMatch(current, m_store.candidates());
  if (match.index >= 0) {
    const Profile profile = m_store.profiles()[static_cast<size_t>(match.index)];
    const auto outcome = m_host.applyLayout(profile.layout);
    notice.kind = Notice::Kind::Restored;
    notice.exact = match.kind == MatchResult::Kind::Exact;
    notice.score = match.score;
    notice.profileName = profile.name;
    if (outcome.applied) {
      changed = outcome.changed;
      notice.notInProfile = outcome.notInLayout;
      m_store.touch(profile.id, wall);
    } else {
      failed = true;
      notice.kind = Notice::Kind::Failed;
      notice.detail = outcome.error;
      m_host.warn(QStringLiteral("layout memory: could not put back \"%1\" for new=%2 (%3); the arrangement is kept")
                      .arg(profile.name, current.id(), outcome.error));
    }
  } else {
    // nothing is remembered for these displays: what there is is kept, and what the change put out of reach is put
    // against the server again
    notice.kind = Notice::Kind::Remembered;
    ReattachOutcome reattached;
    if (before)
      reattached = m_host.reattach(*before, current);
    if (reattached.layout) {
      const auto outcome = m_host.applyLayout(*reattached.layout);
      if (outcome.applied) {
        changed = outcome.changed;
        notice.reattached = reattached.reattached;
      } else {
        m_host.warn(QStringLiteral("layout memory: could not put the computers against the server again for new=%1 "
                                   "(%2); the arrangement is kept")
                        .arg(current.id(), outcome.error));
      }
    }
    notice.notReattached = reattached.notReattached;
    const auto made = m_store.upsert(current, m_host.currentLayout(), wall);
    if (const auto *profile = m_store.find(made.id))
      notice.profileName = profile->name;
    if (made.created)
      createdProfile = made.id;
  }

  m_store.setActive(current);
  // what the arrangement is now, whether or not it is what was asked for: a Save of something else is not a change of it
  m_known = m_host.currentLayout();
  saveStore();

  auto restart = Restart::No;
  if (!failed)
    restart = restartIfNeeded(now);
  notice.restarted = restart == Restart::Yes;
  notice.restartLater = restart == Restart::Later;
  notice.canUndo = changed && !failed;

  if (failed) {
    m_undo.reset();
  } else if (changed) {
    m_undo = Undo{beforeLayout, createdProfile, notice.oldId, notice.newId};
  } else {
    m_undo.reset();
  }

  if (notice.kind == Notice::Kind::Restored) {
    const auto kind = notice.exact ? QStringLiteral("exact") : QStringLiteral("similar, score %1").arg(notice.score, 0, 'f', 3);
    m_host.info(QStringLiteral("layout memory: restored \"%1\" (%2) old=%3 new=%4 restart=%5")
                    .arg(notice.profileName, kind, oldId, current.id(), restartText(restart)));
  } else if (notice.kind == Notice::Kind::Remembered) {
    m_host.info(QStringLiteral("layout memory: no remembered layout for new=%1; kept the arrangement as \"%2\" "
                               "(re-attached: %3; left alone: %4) restart=%5")
                    .arg(current.id(), notice.profileName, namesOf(notice.reattached), namesOf(notice.notReattached),
                         restartText(restart)));
    // the first time there is nothing that changed: nobody is told
    if (!before)
      return;
  }
  tell(notice);
}

LayoutMemory::Restart LayoutMemory::restartIfNeeded(Millis now)
{
  // a core that does not run, or that already has the configuration that it would get, is not restarted
  if (!m_host.coreRunning() || !m_host.coreConfigDiffers()) {
    m_restartAt.reset();
    return Restart::No;
  }
  if (m_lastRestart && now - *m_lastRestart < kMinRestartGapMs) {
    m_restartAt = *m_lastRestart + kMinRestartGapMs;
    return Restart::Later;
  }
  m_host.restartCore();
  m_lastRestart = now;
  m_restartAt.reset();
  return Restart::Yes;
}

bool LayoutMemory::undo(Millis now)
{
  // a dialog that is open has a copy of the arrangement that this would not reach
  if (!active() || !m_undo || !m_host.canApplyNow())
    return false;
  const auto undo = *m_undo;
  m_undo.reset();

  const auto outcome = m_host.applyLayout(undo.before);
  if (!outcome.applied) {
    m_host.warn(QStringLiteral("layout memory: could not go back to the arrangement before new=%1 (%2)")
                    .arg(undo.newId, outcome.error));
    Notice notice;
    notice.kind = Notice::Kind::Failed;
    notice.detail = outcome.error;
    tell(notice);
    return false;
  }
  // a profile that this change made is not kept: it is what the user did not want
  if (!undo.createdProfileId.isEmpty())
    m_store.remove(undo.createdProfileId);
  m_known = m_host.currentLayout();
  saveStore();

  const auto restart = restartIfNeeded(now);
  m_host.info(QStringLiteral("layout memory: undo: back to the arrangement before old=%1 new=%2 restart=%3")
                  .arg(undo.oldId.isEmpty() ? QStringLiteral("none") : undo.oldId, undo.newId, restartText(restart)));
  Notice notice;
  notice.kind = Notice::Kind::Undone;
  notice.oldId = undo.oldId;
  notice.newId = undo.newId;
  notice.restarted = restart == Restart::Yes;
  notice.restartLater = restart == Restart::Later;
  tell(notice);
  return true;
}

void LayoutMemory::dropUndo()
{
  m_undo.reset();
  m_notice.reset();
}

void LayoutMemory::configSaved(Millis)
{
  if (!active())
    return;
  const auto layout = m_host.currentLayout();
  if (layout.isEmpty())
    return;
  // what the user took back, or changed, is not to be undone by the button of the notice any more
  m_undo.reset();
  m_notice.reset();

  // a Save of something else (the heartbeat, a hotkey) leaves the arrangement as it is: it says nothing about which
  // displays the arrangement is for, and does not make it the layout of the displays that there are now
  if (m_known && layout.sameAs(*m_known))
    return;

  auto signature = m_store.active();
  if (!signature) {
    const auto sample = DisplaySignature::fromDisplays(m_host.displays());
    if (!sample.isValid())
      return;
    signature = sample;
    m_store.setActive(sample);
  }
  const auto result = m_store.upsert(*signature, layout, m_host.wallClockMs());
  if (result.id.isEmpty())
    return;
  m_known = layout;
  if (result.created || result.changed) {
    const auto *profile = m_store.find(result.id);
    m_host.info(QStringLiteral("layout memory: remembered the arrangement for %1 as \"%2\" (%3)")
                    .arg(signature->id(), profile ? profile->name : QString(),
                         result.created ? QStringLiteral("created") : QStringLiteral("updated")));
    saveStore();
  }
}

QString LayoutMemory::saveCurrentAs(const QString &name, const Layout &layout, Millis)
{
  if (!active() || name.trimmed().isEmpty())
    return {};
  auto signature = m_store.active();
  if (!signature) {
    const auto sample = DisplaySignature::fromDisplays(m_host.displays());
    if (!sample.isValid())
      return {};
    signature = sample;
    m_store.setActive(sample);
  }
  const auto now = m_host.wallClockMs();
  const auto result = m_store.upsert(*signature, layout, now, name, true);
  if (result.id.isEmpty())
    return {};
  m_store.rename(result.id, name, now);
  saveStore();
  m_host.info(QStringLiteral("layout memory: saved the arrangement for %1 as \"%2\"").arg(signature->id(), name.trimmed()));
  return result.id;
}

LayoutMemory::State LayoutMemory::state() const
{
  if (!active())
    return State::Disabled;
  if (m_pending)
    return m_pending->blocked ? State::Deferred : State::Settling;
  return State::Idle;
}

std::optional<Millis> LayoutMemory::nextDeadline() const
{
  if (!active())
    return std::nullopt;
  std::optional<Millis> deadline;
  if (m_pending)
    deadline = m_pending->due;
  if (m_restartAt)
    deadline = deadline ? std::min(*deadline, *m_restartAt) : *m_restartAt;
  return deadline;
}

void LayoutMemory::saveStore()
{
  if (m_store.save()) {
    m_saveWarned = false;
    return;
  }
  // said once: a folder that cannot be written would say it at every change
  if (!m_saveWarned) {
    m_host.warn(QStringLiteral("layout memory: the remembered layouts could not be saved (%1)").arg(m_store.lastError()));
    m_saveWarned = true;
  }
}

void LayoutMemory::tell(Notice notice)
{
  if (notice.kind != Notice::Kind::StoreProblem)
    m_notice = notice;
  m_host.notify(notice);
}

} // namespace deskflow::gui::layout
