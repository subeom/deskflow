/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/CloudflaredLaunch.h"

#include "common/TunnelAddress.h"

namespace deskflow::tunnel {

namespace {

// what the environment of this process may hold that would change the tunnel that is started: another hostname,
// another local address, another log format, or another token than the one of the user
const QStringList kInheritedVariablesToDrop = {
    QStringLiteral("TUNNEL_SERVICE_HOSTNAME"),     QStringLiteral("TUNNEL_SERVICE_URL"),
    QStringLiteral("TUNNEL_SERVICE_DESTINATION"),  QStringLiteral("TUNNEL_SERVICE_TOKEN_ID"),
    QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET"), QStringLiteral("TUNNEL_LOG_OUTPUT"),
    QStringLiteral("TUNNEL_MANAGEMENT_OUTPUT"),
};

QString quoted(const QString &item)
{
  const auto simplified = item.simplified();
  if (simplified.contains(QLatin1Char(' ')) &&
      !(simplified.startsWith(QLatin1Char('"')) && simplified.endsWith(QLatin1Char('"')))) {
    return QStringLiteral("\"%1\"").arg(simplified);
  }
  return simplified;
}

} // namespace

QString LaunchSpec::commandLine() const
{
  QStringList items = {quoted(program)};
  for (const auto &argument : arguments) {
    items.append(quoted(argument));
  }
  return items.join(QLatin1Char(' '));
}

std::optional<LaunchSpec>
buildLaunchSpec(const LaunchRequest &request, const ServiceToken &token, const QProcessEnvironment &baseEnvironment)
{
  if (request.program.trimmed().isEmpty() || !isValidHostname(request.hostname) ||
      !isValidLocalPort(request.localPort) || !token.isComplete()) {
    return std::nullopt;
  }

  LaunchSpec spec;
  spec.program = request.program;
  spec.arguments = {
      QStringLiteral("access"),     QStringLiteral("tcp"),
      QStringLiteral("--hostname"), request.hostname,
      QStringLiteral("--url"),      QStringLiteral("%1:%2").arg(QLatin1String(kLoopbackHost)).arg(request.localPort),
      QStringLiteral("--loglevel"), QLatin1String(kLogLevel),
  };

  spec.environment = baseEnvironment;
  for (const auto &name : kInheritedVariablesToDrop) {
    spec.environment.remove(name);
  }
  spec.environment.insert(QLatin1String(kTokenIdVariable), token.clientId);
  spec.environment.insert(QLatin1String(kTokenSecretVariable), token.secret);
  return spec;
}

} // namespace deskflow::tunnel
