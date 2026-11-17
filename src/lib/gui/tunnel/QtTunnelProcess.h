/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/ITunnelProcess.h"

#include <QByteArray>

class QProcess;

namespace deskflow::tunnel {

/**
 * cloudflared as a QProcess. On Windows it has no console window (none is made and the one that is not shown is not
 * flashed) and it belongs to a job that ends it when this program ends, however that happens; on Linux the system ends
 * it when this program ends. The standard input is closed: cloudflared never needs it here.
 */
class QtTunnelProcess : public ITunnelProcess
{
  Q_OBJECT

public:
  explicit QtTunnelProcess(QObject *parent = nullptr);
  ~QtTunnelProcess() override;

  void start(const LaunchSpec &spec) override;
  void terminate() override;
  void kill() override;
  bool isRunning() const override;
  qint64 processId() const override;

  static TunnelProcessFactory factory();

private:
  void readOutput();
  void emitLines(bool flush);

  QProcess *m_process;
  QByteArray m_buffer;
};

} // namespace deskflow::tunnel
