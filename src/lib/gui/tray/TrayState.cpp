/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayState.h"

namespace deskflow::gui::tray {

using core::ConnectionState;
using core::ProcessState;

TrayState trayStateFor(ProcessState process, ConnectionState connection, bool hasMode, bool startFailed)
{
  if (startFailed)
    return TrayState::Error;
  switch (process) {
  case ProcessState::Stopped:
  case ProcessState::Stopping:
    return hasMode ? TrayState::Stopped : TrayState::Idle;
  case ProcessState::Starting:
    return TrayState::Waiting;
  case ProcessState::RetryPending:
    return TrayState::Error;
  case ProcessState::Started:
    switch (connection) {
    case ConnectionState::Connected:
      return TrayState::Connected;
    case ConnectionState::Disconnected:
      return TrayState::Error;
    case ConnectionState::Connecting:
    case ConnectionState::Listening:
      return TrayState::Waiting;
    }
  }
  return TrayState::Stopped;
}

QString trayStateName(TrayState state)
{
  switch (state) {
  case TrayState::Idle:
    return QStringLiteral("idle");
  case TrayState::Stopped:
    return QStringLiteral("stopped");
  case TrayState::Waiting:
    return QStringLiteral("waiting");
  case TrayState::Connected:
    return QStringLiteral("connected");
  case TrayState::Error:
    return QStringLiteral("error");
  }
  return QStringLiteral("idle");
}

} // namespace deskflow::gui::tray
