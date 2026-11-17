/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/CloudflaredLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace deskflow::tunnel {

namespace {

struct LocatorTexts
{
  Q_DECLARE_TR_FUNCTIONS(LocatorTexts)
};

QStringList pathDirectories(const QString &pathVariable, Platform platform)
{
  QStringList directories;
  const auto separator = platform == Platform::Windows ? QLatin1Char(';') : QLatin1Char(':');
  for (auto entry : pathVariable.split(separator, Qt::SkipEmptyParts)) {
    entry = entry.trimmed();
    if (entry.size() >= 2 && entry.startsWith(QLatin1Char('"')) && entry.endsWith(QLatin1Char('"'))) {
      entry = entry.mid(1, entry.size() - 2);
    }
    if (!entry.isEmpty() && isAbsolutePath(entry, platform)) {
      directories.append(entry);
    }
  }
  return directories;
}

QString joined(const QString &directory, const QString &fileName)
{
  auto dir = directory;
  while (dir.size() > 1 && (dir.endsWith(QLatin1Char('/')) || dir.endsWith(QLatin1Char('\\')))) {
    dir.chop(1);
  }
  return QStringLiteral("%1/%2").arg(dir, fileName);
}

} // namespace

bool isAbsolutePath(const QString &path, Platform platform)
{
  // the platform of the computer that is described may not be the one that this runs on (the tests)
  if (platform == Platform::Windows) {
    static const QRegularExpression drive(QStringLiteral("^[a-zA-Z]:[\\\\/]"));
    return drive.match(path).hasMatch() || path.startsWith(QStringLiteral("\\\\"));
  }
  return path.startsWith(QLatin1Char('/'));
}

Platform currentPlatform()
{
#if defined(Q_OS_WIN)
  return Platform::Windows;
#elif defined(Q_OS_MACOS)
  return Platform::MacOS;
#else
  return Platform::Linux;
#endif
}

QString cloudflaredFileName(Platform platform)
{
  return platform == Platform::Windows ? QStringLiteral("cloudflared.exe") : QStringLiteral("cloudflared");
}

QStringList cloudflaredKnownLocations(Platform platform)
{
  switch (platform) {
  case Platform::Windows:
    return {
        QStringLiteral("C:/Program Files (x86)/cloudflared/cloudflared.exe"),
        QStringLiteral("C:/Program Files/cloudflared/cloudflared.exe"),
    };
  case Platform::MacOS:
    return {QStringLiteral("/opt/homebrew/bin/cloudflared"), QStringLiteral("/usr/local/bin/cloudflared")};
  case Platform::Linux:
    return {QStringLiteral("/usr/local/bin/cloudflared"), QStringLiteral("/usr/bin/cloudflared")};
  }
  return {};
}

QString cloudflaredInstallHint(Platform platform)
{
  switch (platform) {
  case Platform::Windows:
    return QStringLiteral("winget install --id Cloudflare.cloudflared");
  case Platform::MacOS:
    return QStringLiteral("brew install cloudflared");
  case Platform::Linux:
    return LocatorTexts::tr("Install cloudflared with the package manager of your distribution");
  }
  return {};
}

FileCheck systemFileCheck()
{
  return [](const QString &path) {
    const QFileInfo info(path);
    return info.isFile() && info.isExecutable();
  };
}

CloudflaredLocation locateCloudflared(
    const QString &configuredPath, const QString &pathVariable, Platform platform, const FileCheck &fileCheck,
    const PathResolver &resolve
)
{
  using Source = CloudflaredLocation::Source;
  CloudflaredLocation location;

  // a candidate is taken when it is a file that can be run and, with a resolver, when it has one name
  const auto accept = [&](const QString &candidate, Source source) {
    if (!fileCheck(candidate)) {
      return false;
    }
    auto path = candidate;
    if (resolve) {
      path = resolve(candidate);
      if (path.isEmpty()) {
        return false;
      }
    }
    location.source = source;
    location.path = path;
    return true;
  };

  if (const auto configured = configuredPath.trimmed(); !configured.isEmpty()) {
    if (!isAbsolutePath(configured, platform)) {
      location.configuredPathRelative = true;
      return location;
    }
    if (accept(configured, Source::Configured)) {
      return location;
    }
    location.configuredPathUnusable = true;
  }

  for (const auto &candidate : cloudflaredKnownLocations(platform)) {
    if (accept(candidate, Source::KnownLocation)) {
      return location;
    }
  }

  const auto name = cloudflaredFileName(platform);
  for (const auto &directory : pathDirectories(pathVariable, platform)) {
    if (accept(joined(directory, name), Source::Path)) {
      return location;
    }
  }

  return location;
}

CloudflaredLocation locateCloudflared(const QString &configuredPath)
{
  const PathResolver canonical = [](const QString &path) { return QFileInfo(path).canonicalFilePath(); };
  return locateCloudflared(
      configuredPath, qEnvironmentVariable("PATH"), currentPlatform(), systemFileCheck(), canonical
  );
}

} // namespace deskflow::tunnel
