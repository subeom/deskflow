/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/TunnelTypes.h"

#include <QString>

namespace deskflow::tunnel {

enum class LogEventKind
{
  Other,           //!< a line that says nothing the tunnel acts on
  ListenerStarted, //!< cloudflared is about to listen on the local port (it may still fail to; see BindFailed)
  BindFailed,      //!< cloudflared could not listen on the local port
  LoginRequired,   //!< Cloudflare Access asked for a login (cloudflared opens a browser and waits)
  OriginFailed     //!< a connection through the tunnel could not be opened; see LogEvent::problem
};

struct LogEvent
{
  LogEventKind kind = LogEventKind::Other;
  //! what the line means for the user: PortBusy, LoginRequired, or why the connection failed
  TunnelFailure problem = TunnelFailure::None;
  QString level;   //!< DBG, INF, WRN, ERR or FTL when the line has one, otherwise empty
  QString message; //!< the line without terminal escapes and control characters, cut to 4096 characters
};

/**
 * What a line of the output of `cloudflared access tcp` means. It matches the text of the messages that cloudflared
 * writes (case does not matter) anywhere in the line, so it reads the console format and the JSON format of the log
 * alike. A line it does not know is Other: a change in the words of cloudflared makes the tunnel less informative and
 * never wrong, because the process and the local port are what decide whether the tunnel works, not these lines.
 */
LogEvent parseCloudflaredLine(const QString &line);

} // namespace deskflow::tunnel
