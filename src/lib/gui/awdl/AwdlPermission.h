/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "AwdlModel.h"

#include <QString>
#include <QStringList>

namespace deskflow::awdl {

/*!
 * \file
 * The permission that lets the account of the person turn awdl0 down and up without a password (macOS). The dialog of
 * the administrator that `osascript` shows offers only the password (Touch ID is not offered there), so the person may
 * add, once, one small file for sudo: /etc/sudoers.d/deskflow-awdl, with the one line
 *
 *   <account> ALL=(root) NOPASSWD: /sbin/ifconfig awdl0 down, /sbin/ifconfig awdl0 up
 *
 * and from then on the program runs `/usr/bin/sudo -n /sbin/ifconfig awdl0 down|up` (a list of arguments, never a
 * shell) with no dialog. This is what the program knows of it as plain values and text, so that every rule can be
 * tested without a Mac and without a program of the system: who the line is for, the two scripts that an administrator
 * dialog runs (constant, with nothing in them that a person typed), how to read what sudo says of the permission, and
 * how to choose between the permission and the dialog. Nothing here touches the system (see IAwdlSystem).
 *
 * The one thing that must never happen is a sudoers file that breaks sudo (it would lock the person out of sudo on
 * their own Mac). The install script therefore checks the exact bytes with `visudo -cf` before anything is put in
 * place, puts them in a file that sudo does not read (a name with a dot), checks the owner and the mode and the syntax
 * of that file again, and only then renames it into place (an atomic rename: sudo sees the old file or the new one,
 * never half of one). Any failure ends the script with nothing left behind.
 */

//! The one file that the permission is
inline constexpr auto kSudoPath = "/usr/bin/sudo";
inline constexpr auto kSudoersDirectory = "/etc/sudoers.d";
inline constexpr auto kSudoersFileName = "deskflow-awdl";
//! The path of the file (what the program looks for, and removes)
inline constexpr auto kSudoersFile = "/etc/sudoers.d/deskflow-awdl";

//! What the person asks for: add the file, or remove it
enum class PermissionAction
{
  Allow,
  Remove
};

/*!
 * Whether \p name may be put in the line of the file: the name of an account of the OS as a short name (a letter or an
 * underscore, then letters, digits, underscores, dots and hyphens, at most 32 characters, nothing else, not even a line
 * break at the end). A name of capital letters, digits and underscores is refused too: in sudoers it can be the name of
 * an alias or the word ALL, which would give the permission to someone else, or to everyone. root is refused (it needs
 * no permission).
 */
bool isValidAccountName(const QString &name);

//! The one line of the file for \p user, or nothing when the name is not valid
QString permissionLine(const QString &user);

/*!
 * The programs and the places that the scripts use. The defaults are those of macOS. A test gives its own (all in a
 * directory of its own) to run the script for real against a stand-in tree, without the system's programs and without
 * any path of the real system in it. Every value is a plain path or name (letters, digits and . _ - /): anything else
 * makes the script empty, so that no text of a person can come into a script by this way.
 */
struct ScriptPaths
{
  //! A directory that only root can use: the temporary file is made there
  QString tempDirectory = QStringLiteral("/private/var/root");
  QString sudoersDirectory = QLatin1String(kSudoersDirectory);
  QString mktemp = QStringLiteral("/usr/bin/mktemp");
  QString install = QStringLiteral("/usr/bin/install");
  QString stat = QStringLiteral("/usr/bin/stat");
  QString mv = QStringLiteral("/bin/mv");
  QString rm = QStringLiteral("/bin/rm");
  QString visudo = QStringLiteral("/usr/sbin/visudo");
  QString owner = QStringLiteral("root");
  QString group = QStringLiteral("wheel");

  bool isPlain() const;
};

/*!
 * The script that adds the file, for the dialog of the administrator (`do shell script`): one line for sh, run as root.
 * Nothing in it is typed by a person: the name of the account is validated (isValidAccountName) and the rest is
 * constant. Empty when the account is not valid or a path is not plain.
 */
QString installScript(const QString &user, const ScriptPaths &paths = {});

//! The script that removes the file (and the name that the install script uses while it works): constant
QString removeScript(const ScriptPaths &paths = {});

/*!
 * The arguments of osascript (never a shell string): `-e` and `do shell script "<script>" with administrator
 * privileges with prompt "<sentence>"`. The sentence is the program's own, in the language of the program, and is a
 * string of AppleScript that cannot end. Empty when there is nothing to run (Allow with an account that is not valid).
 */
QStringList permissionArguments(PermissionAction action, const QString &prompt, const QString &user);

//! The arguments of `/usr/bin/sudo` that make the change with no password: -n (never ask), the program, the interface,
//! the direction. They are what the line of the file allows, word for word
QStringList sudoArguments(Direction direction);

//! The arguments of `/usr/bin/sudo` that say which rule decides for the command that turns awdl0 down, in the long form
//! (-ll shows the matching rule and its options), never asking for a password (-n). Nothing is run by it.
QStringList sudoListArguments();

/*!
 * What `sudo -n -ll /sbin/ifconfig awdl0 down` says. The plain `-l` is not enough: with the default listpw=any it needs
 * no password when ANY rule of the user has NOPASSWD, so it can say yes for a command that still asks for a password
 * (that is what a Mac with an unrelated NOPASSWD rule and the admin rule `ALL` does).
 */
enum class Listing
{
  //! Nothing that can be read (empty, other text, sudo did not run, it timed out)
  Unknown,
  //! The rule that decides is not allowed or has a password: `Matched:` is there and its entry has no `!authenticate`,
  //! or sudo said that a password is needed or that the command is not allowed
  PasswordNeeded,
  //! The command is allowed and nothing more is said (a sudo before 1.9.15 shows only the path): it cannot be told
  //! whether a password is needed
  PathOnly,
  //! The rule that decides for exactly this command has `!authenticate` (NOPASSWD)
  NoPassword
};

/*!
 * The listing from the exit code and the output of `sudo -n -ll`. The command is allowed without a password only when
 * the entry that `Matched:` has the option `!authenticate` (the last one of authenticate and !authenticate counts) and
 * the matched line is exactly this command. Any other matched line, or an entry without that option, is
 * PasswordNeeded. Empty and unreadable output is Unknown.
 */
Listing parseSudoListing(int exitCode, const QString &standardOutput, const QString &standardError = {});

//! What the program knows of the permission
enum class PermissionState
{
  //! Not checked yet, or it could not be told
  Unknown,
  //! A change needs the password of an administrator (the dialog)
  NotAllowed,
  //! The change is made with `sudo -n`, with no dialog
  Allowed
};

/*!
 * The state from the listing, from whether the file of the program is there (a file that anyone can look for, the
 * directory is readable), and from whether `sudo -n` was tried and failed in this run (then the listing did not tell
 * the truth: NotAllowed until the file is added again). The listing says it with NoPassword; a PathOnly listing only
 * counts when the file of the program is there; PasswordNeeded is NotAllowed; anything else is Unknown.
 */
PermissionState decidePermission(Listing listing, bool filePresent, bool sudoFailed);

//! How a change of the link is made
enum class ChangeRoute
{
  //! `sudo -n`, no dialog
  Silent,
  //! The dialog of the administrator
  Dialog
};

//! Silent only for a permission that is known to be there; the dialog for everything else (also when it is not known)
ChangeRoute chooseChangeRoute(PermissionState state);

//! What `sudo -n /sbin/ifconfig awdl0 down|up` came to
enum class SudoResult
{
  //! The command ran and succeeded
  Done,
  //! sudo itself said no (a password is required, the command is not allowed, no terminal), or could not be started
  PermissionMissing,
  //! sudo ran the command and the command failed (ifconfig said so), or it was ended: a dialog would not help
  Failed
};

/*!
 * The result from the exit code and the error output. sudo's own diagnostics (`sudo: a password is required`,
 * `sudo: a terminal is required ...`, `Sorry, user ... is not allowed to execute ...`) are PermissionMissing; any other
 * non-zero exit is the command's own failure.
 */
SudoResult parseSudoRun(bool started, int exitCode, const QString &standardError);

//! What to do after a silent change
enum class SilentOutcome
{
  Done,
  //! The permission is not there (any more): the dialog, once, and the state is NotAllowed from now on
  FallBackToDialog,
  Failed
};

SilentOutcome silentOutcome(SudoResult result);

//! What came of the last thing that the person asked for in the settings (the line of the permission says it)
enum class PermissionNote
{
  None,
  //! The file was added
  Added,
  //! The file was removed
  Removed,
  //! The dialog was closed
  Cancelled,
  //! The script ran and said no (nothing is left behind)
  Failed,
  //! Nothing was started: the name of the account cannot be used in the file
  Refused
};

//! A stable word for each, for the log and for the tests
QString permissionStateId(PermissionState state);
QString listingId(Listing listing);

} // namespace deskflow::awdl
