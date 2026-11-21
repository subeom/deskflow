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
  m_requested = direction;
  // the dialog is the answer to the person who pressed the button: it is not asked again by itself in this run
  m_memory.askRunning = true;
  if (direction == Direction::Down)
    m_memory.asked = true;
  setAsking(true);
  m_system->setLink(direction, m_options.prompt ? m_options.prompt(direction) : QString());
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
  if (decision.ask) {
    m_requested = Direction::Down;
    setAsking(true);
    m_system->setLink(Direction::Down, m_options.prompt ? m_options.prompt(Direction::Down) : QString());
  }
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
  const bool on = m_requested == Direction::Up;
  setAsking(false);
  m_memory.askRunning = false;
  switch (result) {
  case AdminResult::Done:
    Q_EMIT logLine(
        on ? QStringLiteral("the wireless link is turned on") : QStringLiteral("the wireless link is turned off")
    );
    // the person turned it on: it is not taken back in this run
    if (on)
      m_memory.restoredByUser = true;
    break;
  case AdminResult::Cancelled:
    Q_EMIT logLine(QStringLiteral("the password was not given: the wireless link stays as it is"));
    break;
  case AdminResult::Failed:
    Q_EMIT logLine(QStringLiteral("could not change the wireless link"));
    break;
  }
  // what the system says is the state (not what the command said)
  m_system->readState();
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
