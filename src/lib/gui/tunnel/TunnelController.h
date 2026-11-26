/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/TunnelAddress.h"
#include "gui/tunnel/ITunnelGate.h"
#include "gui/tunnel/TunnelSupervisor.h"

#include <QObject>

#include <functional>
#include <memory>

class QTimer;

namespace deskflow::tunnel {

struct TunnelControllerOptions
{
  //! How long the tunnel stays after the core stopped
  int lingerMs = 3000;
  //! null: the settings
  std::function<TunnelConfig()> readConfig;
};

/**
 * The tunnel as the program uses it: the settings on one side, the TunnelSupervisor on the other, and the gate that the
 * core process asks before it starts the client. It reads the configuration from the settings each time it is needed
 * (the dialog that configures the client writes them), tells the supervisor what to run, writes the port that is in use
 * when it is not the one that was asked for (the core reads it with the address it dials) and keeps the tunnel for a
 * short time after the core stopped, so that a restart of the core does not restart the tunnel.
 *
 * With the tunnel off in the settings it does nothing and writes nothing.
 */
class TunnelController : public ITunnelGate
{
  Q_OBJECT

public:
  using Options = TunnelControllerOptions;

  explicit TunnelController(
      std::unique_ptr<TunnelSupervisor> supervisor, Options options = {}, QObject *parent = nullptr
  );
  ~TunnelController() override;

  //! A controller with the supervisor of the system (the real secret store, cloudflared, ports)
  static TunnelController *createSystem(QObject *parent = nullptr);

  // ITunnelGate
  bool isRequired() const override;
  bool isListening() const override;
  TunnelFailure validate() const override;
  void acquire() override;
  void release() override;

  //! The program is a client (the tunnel is for the client only): a switch to the server stops the tunnel at once
  void setClientMode(bool client);
  //! The client got a connection through the tunnel to the server
  void notifyServerConnected();
  //! The end of the program: stops cloudflared and waits for it
  void shutdown();

  //! What the tunnel is doing; Idle while it is not used
  TunnelStatus status() const;
  //! The configuration as the settings hold it now
  TunnelConfig config() const;
  TunnelSupervisor &supervisor()
  {
    return *m_supervisor;
  }

Q_SIGNALS:
  void statusChanged(const deskflow::tunnel::TunnelStatus &status);
  //! A line for the log (the supervisor's own and what cloudflared wrote, redacted)
  void logLine(const QString &line);

private:
  TunnelRequest currentRequest() const;
  void onActivePortChanged(int port);

  std::unique_ptr<TunnelSupervisor> m_supervisor;
  Options m_options;
  QTimer *m_lingerTimer;
  bool m_clientMode = true;
};

} // namespace deskflow::tunnel
