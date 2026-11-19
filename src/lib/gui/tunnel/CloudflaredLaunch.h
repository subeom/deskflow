/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/ServiceToken.h"

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <optional>

namespace deskflow::tunnel {

//! The variables that cloudflared reads the Service Token from. The only place the token is passed in.
inline constexpr auto kTokenIdVariable = "TUNNEL_SERVICE_TOKEN_ID";
inline constexpr auto kTokenSecretVariable = "TUNNEL_SERVICE_TOKEN_SECRET";

//! The log level of cloudflared. Fixed: at `debug` cloudflared logs the request headers, and the Secret is one of them.
inline constexpr auto kLogLevel = "info";

struct LaunchRequest
{
  QString program;  //!< the path of cloudflared
  QString hostname; //!< of the Access application, see isValidHostname
  int localPort = 0;
};

/**
 * What to start: the program, its arguments and the environment of the child. The arguments name no secret: the Service
 * Token is in the environment and only there.
 */
struct LaunchSpec
{
  QString program;
  QStringList arguments;
  QProcessEnvironment environment;

  /**
   * The command as one line to paste into a terminal, for the log. Only the program and the arguments: never the
   * environment.
   */
  QString commandLine() const;
};

/**
 * `cloudflared access tcp --hostname <host> --url 127.0.0.1:<port> --loglevel info` with the token in the environment.
 * The environment is \p baseEnvironment (the one of this process) with the variables that would change what cloudflared
 * does removed, and then the token set. Nothing of \p baseEnvironment is changed. Empty when the request or the token
 * is not usable (no program, a hostname that is not a host name, a port out of range, a token without a part).
 */
std::optional<LaunchSpec>
buildLaunchSpec(const LaunchRequest &request, const ServiceToken &token, const QProcessEnvironment &baseEnvironment);

} // namespace deskflow::tunnel
