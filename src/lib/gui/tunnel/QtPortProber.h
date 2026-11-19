/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/IPortProber.h"

#include <QObject>

namespace deskflow::tunnel {

//! IPortProber with QTcpServer and QTcpSocket
class QtPortProber : public QObject, public IPortProber
{
  Q_OBJECT

public:
  explicit QtPortProber(QObject *parent = nullptr);

  bool canListen(int port) override;
  int findFreePort() override;
  void probeOpen(int port, int timeoutMs, std::function<void(bool open)> done) override;
};

} // namespace deskflow::tunnel
