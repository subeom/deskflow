/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace deskflow::tunnel {

enum class Platform
{
  Windows,
  MacOS,
  Linux
};

//! The computer this was built for
Platform currentPlatform();

//! `cloudflared.exe` on Windows, `cloudflared` elsewhere
QString cloudflaredFileName(Platform platform);

//! The places where the installers put it, in the order they are tried (before the directories of PATH)
QStringList cloudflaredKnownLocations(Platform platform);

//! What to run to install it, for the message that says it was not found
QString cloudflaredInstallHint(Platform platform);

struct CloudflaredLocation
{
  enum class Source
  {
    NotFound,
    Configured,    //!< the path that the user set
    KnownLocation, //!< where the installers put it
    Path           //!< a directory of PATH
  };

  Source source = Source::NotFound;
  QString path;
  //! a path was set and is not a file that can be run, so the usual places were searched instead
  bool configuredPathUnusable = false;

  bool found() const
  {
    return source != Source::NotFound;
  }
};

//! Whether \p path is a file that can be run; replaced in tests so that no file system is needed
using FileCheck = std::function<bool(const QString &path)>;

//! A regular file that is executable (on Windows: a regular file)
FileCheck systemFileCheck();

/**
 * Finds a cloudflared: the configured path first, then the places of the installers, then the directories of
 * \p pathVariable. A directory of PATH that is not absolute (a "." or an empty entry) is not used, so that nothing that
 * lies in the current directory is run. Nothing is started and nothing is downloaded.
 */
CloudflaredLocation locateCloudflared(
    const QString &configuredPath, const QString &pathVariable, Platform platform, const FileCheck &fileCheck
);

//! The same with the real file system and the PATH of this process
CloudflaredLocation locateCloudflared(const QString &configuredPath);

} // namespace deskflow::tunnel
