/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/TunnelTypes.h"

#include <QWidget>

class QLabel;

namespace deskflow::tunnel {
class TunnelController;
}

namespace deskflow::gui {

/**
 * The two lines that the main window shows while the client uses a Cloudflare Tunnel: which host is dialled (in place
 * of the combo box with the saved address, so that the host name of the tunnel and 127.0.0.1 are not mixed up) and what
 * the tunnel and the connection through it are doing. Hidden while the tunnel is not used.
 *
 * It reads the controller and never writes the settings: it is a view. The window that holds it hides its address field
 * while usedChanged(true) says the tunnel is used.
 */
class TunnelStatusWidget : public QWidget
{
  Q_OBJECT

public:
  explicit TunnelStatusWidget(deskflow::tunnel::TunnelController *controller, QWidget *parent = nullptr);

  //! The connection of the client to the server, as the core tells it
  void setServerLink(deskflow::tunnel::ServerLink link);
  //! Reads the settings and the state again (after the configuration changed)
  void refresh();

  bool isTunnelUsed() const
  {
    return m_used;
  }
  //! The text of the status line, for the tray menu and the tool tip
  QString statusText() const;
  //! The text of the path line: what is dialled instead of the saved address. The new look shows it in its own widget,
  //! with this widget as the one that knows what it says
  QString pathText() const;

Q_SIGNALS:
  void usedChanged(bool used);
  void statusTextChanged(const QString &text);
  void pathTextChanged(const QString &text);

private:
  deskflow::tunnel::TunnelController *m_controller;
  QLabel *m_pathLabel;
  QLabel *m_statusLabel;
  deskflow::tunnel::ServerLink m_link = deskflow::tunnel::ServerLink::Unknown;
  bool m_used = false;
};

} // namespace deskflow::gui
