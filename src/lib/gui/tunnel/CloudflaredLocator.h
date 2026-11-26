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
  /**
   * the path that was set is not an absolute path (a name, ./name, dir/name): it is refused, and nothing else is used
   * instead. The system looks a name without a directory up in the PATH of the process, so the file that would run is
   * not the file that was checked, and the Service Token goes to the file that runs.
   */
  bool configuredPathRelative = false;

  bool found() const
  {
    return source != Source::NotFound;
  }
};

//! Whether \p path is a file that can be run; replaced in tests so that no file system is needed
using FileCheck = std::function<bool(const QString &path)>;

//! The one name of a file: a link is followed and the path is cleaned; empty when there is no such file. Replaced in
//! tests.
using PathResolver = std::function<QString(const QString &path)>;

//! Whether \p path is absolute in the syntax of \p platform: `/x`, `C:/x`, `C:\x` or `\\server\share\x`. `C:x` and `/x`
//! on Windows are not.
bool isAbsolutePath(const QString &path, Platform platform);

//! A regular file that is executable (on Windows: a regular file)
FileCheck systemFileCheck();

/**
 * Finds a cloudflared: the configured path first, then the places of the installers, then the directories of
 * \p pathVariable. A directory of PATH that is not absolute (a "." or an empty entry) is not used, so that nothing that
 * lies in the current directory is run. Nothing is started and nothing is downloaded.
 */
/**
 * Where cloudflared is: the configured path (an absolute one; a relative one is refused, see
 * CloudflaredLocation::configuredPathRelative), then the places of the installers, then the directories of PATH. What
 * is returned is one absolute path, resolved with \p resolve when there is one (a link is followed): the path that is
 * run is the path that was found, not a name that the system would look up again. A candidate that \p resolve cannot
 * resolve is not used.
 */
CloudflaredLocation locateCloudflared(
    const QString &configuredPath, const QString &pathVariable, Platform platform, const FileCheck &fileCheck,
    const PathResolver &resolve = {}
);

//! The same with the real file system (links are resolved with QFileInfo::canonicalFilePath) and the PATH of this
//! process
CloudflaredLocation locateCloudflared(const QString &configuredPath);

} // namespace deskflow::tunnel
