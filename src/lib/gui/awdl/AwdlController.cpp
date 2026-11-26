/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlController.h"

namespace deskflow::awdl {

AwdlController::AwdlController(IAwdlSystem *system, Options options, QObject *parent)
    : QObject(parent),
      m_system(system),
      m_options(std::move(options)),
      m_poll(new QTimer(this))
{
  m_system->setParent(this);
  m_poll->setInterval(m_options.pollMs);
  connect(m_poll, &QTimer::timeout, this, [this] { m_system->readState(); });
  connect(m_system, &IAwdlSystem::stateRead, this, &AwdlController::onStateRead);
  connect(m_system, &IAwdlSystem::linkSet, this, &AwdlController::onLinkSet);
  connect(m_system, &IAwdlSystem::routeFound, this, &AwdlController::onRouteFound);
  connect(m_system, &IAwdlSystem::measured, this, &AwdlController::onMeasured);
  connect(m_system, &IAwdlSystem::permissionChecked, this, &AwdlController::onPermissionChecked);
  connect(m_system, &IAwdlSystem::silentRan, this, &AwdlController::onSilentRan);
  connect(m_system, &IAwdlSystem::permissionSet, this, &AwdlController::onPermissionSet);
}

LinkState AwdlController::state() const
{
  return m_state;
}

bool AwdlController::hintVisible() const
{
  return m_hint;
}

bool AwdlController::isAsking() const
{
  return m_asking;
}

bool AwdlController::coreRunning() const
{
  return m_running;
}

Route AwdlController::route() const
{
  return m_route;
}

BurstStats AwdlController::latency() const
{
  return m_stats;
}

Summary AwdlController::summary() const
{
  return summarize(m_state, m_route.kind, m_statsWhileUp ? m_stats.pattern : Pattern::NotMeasured);
}

bool AwdlController::isMeasuring() const
{
  return m_measuring;
}

PermissionState AwdlController::permission() const
{
  return m_permission;
}

bool AwdlController::permissionFilePresent() const
{
  return m_filePresent;
}

PermissionNote AwdlController::permissionNote() const
{
  return m_note;
}

bool AwdlController::optionOn() const
{
  return m_options.optionOn && m_options.optionOn();
}

bool AwdlController::isActive() const
{
  return m_options.supported && m_running && optionOn();
}

void AwdlController::updatePolling()
{
  if (isActive()) {
    if (!m_poll->isActive())
      m_poll->start();
  } else {
    m_poll->stop();
  }
}

void AwdlController::setCoreRunning(bool running)
{
  if (!m_options.supported || m_running == running)
    return;
  m_running = running;
  updatePolling();
  if (running && optionOn())
    m_system->readState();
  else
    // the run is over (or there is no run): the hint goes and the run is forgotten, and nothing is turned on again
    evaluate();
}

void AwdlController::optionChanged()
{
  if (!m_options.supported)
    return;
  updatePolling();
  if (isActive())
    m_system->readState();
  else
    evaluate();
}

void AwdlController::refresh()
{
  if (!m_options.supported)
    return;
  m_system->readState();
  // the kind of the connection is looked at too (a lookup of the route, which sends nothing)
  if (!m_measuring)
    startLookup(false);
  // and the permission, at most once in a while (each look is a line in the log of sudo)
  if (!m_sincePermissionCheck.isValid() || m_sincePermissionCheck.elapsed() >= m_options.permissionEveryMs)
    m_system->checkPermission();
}

void AwdlController::measureNow()
{
  if (!m_options.supported || m_measuring || m_asking)
    return;
  m_manualMeasure = true;
  startLookup(true);
}

void AwdlController::turnOffNow()
{
  requestChange(Direction::Down);
}

void AwdlController::turnBackOn()
{
  requestChange(Direction::Up);
}

void AwdlController::requestChange(Direction direction)
{
  if (!m_options.supported || m_asking)
    return;
  // the dialog (if there is one) is the answer to the person who pressed the button: it is not asked again by itself in
  // this run
  m_memory.askRunning = true;
  if (direction == Direction::Down)
    m_memory.asked = true;
  beginChange(direction);
}

void AwdlController::beginChange(Direction direction)
{
  m_requested = direction;
  setAsking(true);
  // what sudo says of the permission is looked at before every change: it costs nothing, and a file that was added or
  // removed by hand is not taken for what it was
  m_pendingChange = direction;
  m_system->checkPermission();
}

void AwdlController::allowWithoutPassword()
{
  if (!m_options.supported || m_asking)
    return;
  m_permissionAction = PermissionAction::Allow;
  m_note = PermissionNote::None;
  // the policy does not ask for another password while this dialog is open
  m_memory.askRunning = true;
  setAsking(true);
  m_system->installPermission(
      m_options.permissionPrompt ? m_options.permissionPrompt(PermissionAction::Allow) : QString()
  );
}

void AwdlController::removePermission()
{
  if (!m_options.supported || m_asking)
    return;
  m_permissionAction = PermissionAction::Remove;
  m_note = PermissionNote::None;
  m_memory.askRunning = true;
  setAsking(true);
  m_system->removePermission(
      m_options.permissionPrompt ? m_options.permissionPrompt(PermissionAction::Remove) : QString()
  );
}

void AwdlController::startLookup(bool thenMeasure)
{
  setMeasuring(true);
  m_measureAfterLookup = thenMeasure;
  if (thenMeasure)
    m_sinceAttempt.start();
  m_system->findRoute();
}

void AwdlController::maybeProbe()
{
  ProbeInput input;
  input.supported = m_options.supported;
  input.optionOn = optionOn();
  input.coreRunning = m_running;
  input.link = m_state;
  input.askRunning = m_memory.askRunning;
  input.busy = m_measuring;
  input.sinceLastMs = m_sinceAttempt.isValid() ? m_sinceAttempt.elapsed() : -1;
  input.everyMs = m_options.burstEveryMs;
  if (shouldProbe(input))
    startLookup(true);
}

void AwdlController::evaluate()
{
  PolicyInput input;
  input.supported = m_options.supported;
  input.optionOn = optionOn();
  input.coreRunning = m_running;
  input.link = m_state;
  // the spikes of a burst are the link's only while it is up
  input.latency = (m_state == LinkState::Up && m_statsWhileUp) ? m_stats.pattern : Pattern::NotMeasured;
  input.memory = m_memory;
  const auto decision = decide(input);
  m_memory = decision.memory;
  setHint(decision.hint);
  if (decision.ask)
    beginChange(Direction::Down);
  maybeProbe();
}

void AwdlController::setHint(bool visible)
{
  if (m_hint == visible)
    return;
  m_hint = visible;
  Q_EMIT hintChanged(visible);
}

void AwdlController::setAsking(bool asking)
{
  if (m_asking == asking)
    return;
  m_asking = asking;
  Q_EMIT askingChanged(asking);
}

void AwdlController::setMeasuring(bool measuring)
{
  if (m_measuring == measuring)
    return;
  m_measuring = measuring;
  Q_EMIT measuringChanged(measuring);
}

void AwdlController::onStateRead(LinkState state)
{
  if (!m_options.supported)
    return;
  const bool changed = state != m_state;
  m_state = state;
  // what a burst said while the link was up says nothing of the link when it is not
  if (state != LinkState::Up)
    m_statsWhileUp = false;
  if (changed)
    Q_EMIT stateChanged(state);
  evaluate();
}

void AwdlController::onLinkSet(AdminResult result)
{
  if (!m_asking)
    return;
  finishChange(result, false);
}

void AwdlController::finishChange(AdminResult result, bool silent)
{
  const bool on = m_requested == Direction::Up;
  setAsking(false);
  m_memory.askRunning = false;
  switch (result) {
  case AdminResult::Done:
    Q_EMIT logLine(QStringLiteral("the wireless link is turned %1%2")
                       .arg(
                           on ? QStringLiteral("on") : QStringLiteral("off"),
                           silent ? QStringLiteral(" (no password)") : QString()
                       ));
    // the person turned it on: it is not taken back in this run
    if (on)
      m_memory.restoredByUser = true;
    break;
  case AdminResult::Cancelled:
    Q_EMIT logLine(QStringLiteral("the password was not given: the wireless link stays as it is"));
    break;
  case AdminResult::Failed:
  case AdminResult::Refused:
    Q_EMIT logLine(QStringLiteral("could not change the wireless link"));
    break;
  }
  // what the system says is the state (not what the command said)
  m_system->readState();
}

void AwdlController::updatePermission()
{
  const auto before = m_permission;
  m_permission = decidePermission(m_listing, m_filePresent, m_sudoFailed);
  // a computer that has no permission and no file says nothing (it is the way things were); the rest is worth a line
  const bool dull = m_permission != PermissionState::Allowed && before != PermissionState::Allowed && !m_filePresent;
  if (m_permission != before && !dull)
    Q_EMIT logLine(QStringLiteral("permission to change the link with no password: %1 (sudo says %2, file %3)")
                       .arg(
                           permissionStateId(m_permission), listingId(m_listing),
                           m_filePresent ? QStringLiteral("there") : QStringLiteral("not there")
                       ));
  Q_EMIT permissionChanged();
}

void AwdlController::onPermissionChecked(Listing listing, bool filePresent)
{
  m_listing = listing;
  m_filePresent = filePresent;
  m_sincePermissionCheck.start();
  updatePermission();
  if (!m_pendingChange)
    return;

  const auto direction = *m_pendingChange;
  m_pendingChange.reset();
  if (chooseChangeRoute(m_permission) == ChangeRoute::Silent)
    m_system->changeLinkSilently(direction);
  else
    m_system->setLink(direction, m_options.prompt ? m_options.prompt(direction) : QString());
}

void AwdlController::onSilentRan(SudoResult result)
{
  if (!m_asking)
    return;
  switch (silentOutcome(result)) {
  case SilentOutcome::Done:
    // it works: whatever the listing said before is settled
    if (m_sudoFailed) {
      m_sudoFailed = false;
      updatePermission();
    }
    finishChange(AdminResult::Done, true);
    return;
  case SilentOutcome::FallBackToDialog:
    // the permission is not there after all: the dialog, once, and no more silent tries until the file is added again
    m_sudoFailed = true;
    updatePermission();
    Q_EMIT logLine(QStringLiteral("sudo asked for a password: the permission is missing, asking for the password"));
    m_system->setLink(m_requested, m_options.prompt ? m_options.prompt(m_requested) : QString());
    return;
  case SilentOutcome::Failed:
    finishChange(AdminResult::Failed, true);
    return;
  }
}

void AwdlController::onPermissionSet(AdminResult result)
{
  if (!m_asking)
    return;
  const bool adding = m_permissionAction == PermissionAction::Allow;
  setAsking(false);
  m_memory.askRunning = false;
  switch (result) {
  case AdminResult::Done:
    m_note = adding ? PermissionNote::Added : PermissionNote::Removed;
    // a file that was added is tried again, whatever failed before
    if (adding)
      m_sudoFailed = false;
    Q_EMIT logLine(
        adding ? QStringLiteral("the file of the permission was added")
               : QStringLiteral("the file of the permission was removed")
    );
    break;
  case AdminResult::Cancelled:
    m_note = PermissionNote::Cancelled;
    Q_EMIT logLine(QStringLiteral("the password was not given: the permission stays as it is"));
    break;
  case AdminResult::Failed:
    m_note = PermissionNote::Failed;
    Q_EMIT logLine(
        adding ? QStringLiteral("could not add the file of the permission")
               : QStringLiteral("could not remove the file of the permission")
    );
    break;
  case AdminResult::Refused:
    m_note = PermissionNote::Refused;
    Q_EMIT logLine(
        QStringLiteral("the name of this account cannot be used in the file of the permission: nothing was changed")
    );
    break;
  }
  Q_EMIT permissionChanged();
  // what sudo says now is the state, not what the script said
  m_system->checkPermission();
}

void AwdlController::onRouteFound(const Route &route)
{
  m_route = route;
  const bool measure = m_measuring && m_measureAfterLookup && canProbe(route);
  m_measureAfterLookup = false;
  if (measure) {
    m_system->measure(route);
  } else if (m_measuring) {
    // a wire, a VPN, a gateway that is not of the local network: there is nothing to send
    m_manualMeasure = false;
    setMeasuring(false);
  }
  Q_EMIT latencyChanged();
}

void AwdlController::onMeasured(const QList<Sample> &samples)
{
  if (!m_measuring)
    return;
  m_stats = analyzeBurst(samples);
  m_statsWhileUp = m_state == LinkState::Up;
  setMeasuring(false);
  if (m_stats.pattern != m_loggedPattern || m_manualMeasure) {
    m_loggedPattern = m_stats.pattern;
    Q_EMIT logLine(QStringLiteral("latency to the gateway: median %1 ms, worst %2 ms, %3 answers, %4 above %5 ms")
                       .arg(m_stats.medianMs, 0, 'f', 1)
                       .arg(m_stats.worstMs, 0, 'f', 1)
                       .arg(m_stats.received)
                       .arg(m_stats.spikes)
                       .arg(m_stats.thresholdMs, 0, 'f', 0));
  }
  m_manualMeasure = false;
  Q_EMIT latencyChanged();
  evaluate();
}

} // namespace deskflow::awdl
