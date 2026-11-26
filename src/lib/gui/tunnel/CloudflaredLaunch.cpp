/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/CloudflaredLaunch.h"

#include "common/TunnelAddress.h"

#include <QDir>

namespace deskflow::tunnel {

namespace {

// Every variable of the tunnel in the environment of this process can change what cloudflared does (another hostname,
// another local address, another log format, the origin certificate, a token of a tunnel, another token than the one of
// the user): none is passed on, not only the ones that are known, and the two of the token are set after. The names of
// variables are not case sensitive on Windows, so there `tunnel_foo` is `TUNNEL_FOO` for cloudflared as well.
bool isTunnelVariable(const QString &name)
{
#ifdef Q_OS_WIN
  const auto sensitivity = Qt::CaseInsensitive;
#else
  const auto sensitivity = Qt::CaseSensitive;
#endif
  return name.startsWith(QLatin1String("TUNNEL_"), sensitivity);
}

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
  // Only an absolute path is run. A name without a directory is looked up by the system in the PATH of the process, and
  // the Service Token goes to whatever file that finds: it has to be the file that was found by the locator.
  if (request.program.trimmed().isEmpty() || request.program.startsWith(QLatin1Char(':')) ||
      !QDir::isAbsolutePath(request.program) || !isValidHostname(request.hostname) ||
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
  for (const auto &name : spec.environment.keys()) {
    if (isTunnelVariable(name)) {
      spec.environment.remove(name);
    }
  }
  spec.environment.insert(QLatin1String(kTokenIdVariable), token.clientId);
  spec.environment.insert(QLatin1String(kTokenSecretVariable), token.secret);
  return spec;
}

} // namespace deskflow::tunnel
