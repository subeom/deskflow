/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/TunnelController.h"

#include "common/Settings.h"

#include <QTimer>

namespace deskflow::tunnel {

TunnelController::TunnelController(std::unique_ptr<TunnelSupervisor> supervisor, Options options, QObject *parent)
    : ITunnelGate(parent),
      m_supervisor(std::move(supervisor)),
      m_options(std::move(options)),
      m_lingerTimer(new QTimer(this))
{
  if (!m_options.readConfig) {
    m_options.readConfig = [] { return TunnelConfig::fromSettings(); };
  }

  m_lingerTimer->setSingleShot(true);
  m_lingerTimer->setInterval(m_options.lingerMs);
  connect(m_lingerTimer, &QTimer::timeout, this, [this] { m_supervisor->stop(); });

  connect(m_supervisor.get(), &TunnelSupervisor::statusChanged, this, &TunnelController::statusChanged);
  connect(m_supervisor.get(), &TunnelSupervisor::logLine, this, &TunnelController::logLine);
  connect(m_supervisor.get(), &TunnelSupervisor::listening, this, &ITunnelGate::listening);
  connect(m_supervisor.get(), &TunnelSupervisor::failed, this, &ITunnelGate::failed);
  connect(m_supervisor.get(), &TunnelSupervisor::activePortChanged, this, &TunnelController::onActivePortChanged);

  // a port of a run that ended without clearing it (a crash) is not the port of this run; a key that is not there is
  // not written
  Settings::setValue(Settings::Tunnel::ActivePort, QVariant());
}

TunnelController::~TunnelController()
{
  shutdown();
}

TunnelController *TunnelController::createSystem(QObject *parent)
{
  return new TunnelController(std::make_unique<TunnelSupervisor>(TunnelSupervisor::systemDependencies()), {}, parent);
}

TunnelConfig TunnelController::config() const
{
  return m_options.readConfig();
}

TunnelRequest TunnelController::currentRequest() const
{
  const auto current = config();
  TunnelRequest request;
  request.hostname = current.hostname;
  request.preferredPort = current.localPort;
  request.configuredPath = current.cloudflaredPath;
  request.tlsEnabled = current.tlsEnabled;
  return request;
}

bool TunnelController::isRequired() const
{
  // enabled is enough: a tunnel that is on and cannot start (TLS off, no hostname) stops the start of the core with its
  // reason, and does not let the core dial the saved address behind the back of the user
  return m_clientMode && config().enabled;
}

bool TunnelController::isListening() const
{
  return m_supervisor->status().state == TunnelState::Listening && m_supervisor->request() == currentRequest();
}

TunnelFailure TunnelController::validate() const
{
  const auto current = config();
  if (!m_clientMode || !current.enabled) {
    return TunnelFailure::SettingsChanged;
  }
  if (!current.tlsEnabled) {
    return TunnelFailure::TlsRequired;
  }
  if (!isValidHostname(current.hostname)) {
    return TunnelFailure::InvalidHostname;
  }
  // the hostname, the port or the path of cloudflared is not the one that the running tunnel was started with
  return isListening() ? TunnelFailure::None : TunnelFailure::SettingsChanged;
}

void TunnelController::acquire()
{
  m_lingerTimer->stop();

  const auto request = currentRequest();
  const auto state = m_supervisor->status().state;
  const bool alive = state == TunnelState::Starting || state == TunnelState::Listening || state == TunnelState::Backoff;
  const bool same = m_supervisor->request() == request;
  if (alive && same) {
    return;
  }
  if (alive) {
    // the configuration changed since the tunnel started: it starts again with the new one
    m_supervisor->stop();
  }
  m_supervisor->start(request);
}

void TunnelController::release()
{
  const auto state = m_supervisor->status().state;
  // a tunnel that failed keeps its state, so that the reason stays where the user can read it
  if (state != TunnelState::Starting && state != TunnelState::Listening && state != TunnelState::Backoff) {
    return;
  }
  m_lingerTimer->start();
}

void TunnelController::setClientMode(bool client)
{
  m_clientMode = client;
  if (!client) {
    m_lingerTimer->stop();
    m_supervisor->stop();
  }
}

void TunnelController::notifyServerConnected()
{
  m_supervisor->notifyServerConnected();
}

void TunnelController::shutdown()
{
  m_lingerTimer->stop();
  m_supervisor->stopAndWait();
}

TunnelStatus TunnelController::status() const
{
  return m_supervisor->status();
}

void TunnelController::onActivePortChanged(int port)
{
  // the port that was asked for is the one in the settings already: only another one is written, and none is removed
  const auto asked = config().localPort;
  const bool another = port > 0 && port != asked;
  Settings::setValue(Settings::Tunnel::ActivePort, another ? QVariant(port) : QVariant());
}

} // namespace deskflow::tunnel
