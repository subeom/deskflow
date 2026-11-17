/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/CloudflaredLaunch.h"

#include <QObject>

#include <functional>

namespace deskflow::tunnel {

/**
 * One run of cloudflared. The supervisor starts it, reads what it writes and stops it; tests give it a fake, the
 * program gives it a QProcess (QtTunnelProcess).
 */
class ITunnelProcess : public QObject
{
  Q_OBJECT

public:
  using QObject::QObject;

  virtual void start(const LaunchSpec &spec) = 0;
  //! Asks the process to go (the signal of the system that a program can handle; on Windows the process is ended)
  virtual void terminate() = 0;
  //! Ends the process
  virtual void kill() = 0;
  virtual bool isRunning() const = 0;
  virtual qint64 processId() const = 0;

Q_SIGNALS:
  void started();
  //! The program could not be started at all (not there, not allowed); \p detail is the system's own text
  void startFailed(const QString &detail);
  //! One line of what the process wrote (output and errors together), without the end of the line
  void outputLine(const QString &line);
  //! The process is gone; \p exitCode is -1 when it crashed or was ended by a signal
  void finished(int exitCode);
};

using TunnelProcessFactory = std::function<ITunnelProcess *(QObject *parent)>;

} // namespace deskflow::tunnel
