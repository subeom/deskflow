/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelAddress.h"

#include "common/Settings.h"

#include <QRegularExpression>

namespace deskflow::tunnel {

bool isValidLocalPort(int port)
{
  return port >= kMinLocalPort && port <= kMaxLocalPort;
}

int localPortFromSetting(const QVariant &raw)
{
  if (!raw.isValid() || raw.isNull()) {
    return 0;
  }

  // a settings file holds text; a list is what a comma in the value turns into
  if (raw.metaType().id() == QMetaType::QStringList || raw.metaType().id() == QMetaType::QVariantList) {
    return 0;
  }

  bool ok = false;
  const auto port = raw.toString().trimmed().toInt(&ok);
  return ok && isValidLocalPort(port) ? port : 0;
}

QString normalizedHostname(const QString &hostname)
{
  auto name = hostname.trimmed().toLower();
  if (name.endsWith(QLatin1Char('.'))) {
    name.chop(1);
  }
  return name;
}

bool isValidHostname(const QString &hostname)
{
  // the text as it is: a blank inside, or a trailing dot, is not forgiven here (see normalizedHostname)
  if (hostname.isEmpty() || hostname.size() > 253) {
    return false;
  }

  static const QRegularExpression label(QStringLiteral("^[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?$"));
  const auto labels = hostname.split(QLatin1Char('.'));
  if (labels.size() < 2) {
    return false;
  }

  for (const auto &part : labels) {
    if (!label.match(part).hasMatch()) {
      return false;
    }
  }

  static const QRegularExpression numeric(QStringLiteral("^[0-9]+$"));
  return !numeric.match(labels.last()).hasMatch();
}

bool TunnelConfig::isActive() const
{
  return enabled && tlsEnabled && isValidHostname(hostname);
}

int TunnelConfig::port() const
{
  if (isValidLocalPort(activePort)) {
    return activePort;
  }
  return isValidLocalPort(localPort) ? localPort : kDefaultLocalPort;
}

QString TunnelConfig::localAddress() const
{
  return QStringLiteral("%1:%2").arg(QLatin1String(kLoopbackHost)).arg(port());
}

TunnelConfig TunnelConfig::fromSettings()
{
  TunnelConfig config;
  config.enabled = Settings::value(Settings::Tunnel::Enabled).toBool();
  config.hostname = normalizedHostname(Settings::value(Settings::Tunnel::Hostname).toString());
  if (const auto port = localPortFromSetting(Settings::value(Settings::Tunnel::LocalPort)); port != 0) {
    config.localPort = port;
  }
  config.cloudflaredPath = Settings::value(Settings::Tunnel::CloudflaredPath).toString().trimmed();
  config.activePort = localPortFromSetting(Settings::value(Settings::Tunnel::ActivePort));
  config.tlsEnabled = Settings::value(Settings::Security::TlsEnabled).toBool();
  return config;
}

QString effectiveRemoteHost(const TunnelConfig &config, const QString &configuredRemoteHost)
{
  return config.isActive() ? config.localAddress() : configuredRemoteHost;
}

QString effectiveRemoteHost()
{
  return effectiveRemoteHost(TunnelConfig::fromSettings(), Settings::textValue(Settings::Client::RemoteHost));
}

} // namespace deskflow::tunnel
