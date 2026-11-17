/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/TunnelTypes.h"

#include <QObject>

namespace deskflow::tunnel {

/**
 * What the process of the core asks of the Cloudflare Tunnel: the tunnel has to listen before the client core starts,
 * and may stop once the core has stopped. The implementation is the TunnelController; tests use a fake.
 */
class ITunnelGate : public QObject
{
  Q_OBJECT

public:
  using QObject::QObject;

  //! The tunnel is used for the next start of the client core: client mode and a usable tunnel configuration
  virtual bool isRequired() const = 0;
  //! cloudflared runs for the configuration that is in use now, and its local port is open
  virtual bool isListening() const = 0;
  //! The core is to start: makes sure the tunnel runs (starts it when it does not); listening() or failed() follows
  virtual void acquire() = 0;
  //! The core has stopped: the tunnel is not needed any more (it stops after a short pause, so that a restart of the
  //! core keeps it)
  virtual void release() = 0;

Q_SIGNALS:
  void listening();
  void failed(deskflow::tunnel::TunnelFailure failure);
};

} // namespace deskflow::tunnel
