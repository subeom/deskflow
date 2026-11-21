/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlPermission.h"

#include <QRegularExpression>

namespace deskflow::awdl {

namespace {

// the one command that the permission is for, as sudo prints it
QString commandText(Direction direction)
{
  return QStringLiteral("%1 %2 %3")
      .arg(
          QLatin1String(kIfconfigPath), QLatin1String(kInterface),
          direction == Direction::Down ? QStringLiteral("down") : QStringLiteral("up")
      );
}

// a plain path or name: nothing a shell or sudoers could read as more than a word
bool isPlainWord(const QString &text)
{
  static const QRegularExpression plain(QStringLiteral(R"(\A[A-Za-z0-9_./-]+\z)"));
  return plain.match(text).hasMatch();
}

QString fill(QString text, const QList<QPair<QString, QString>> &values)
{
  for (const auto &value : values)
    text.replace(QLatin1Char('@') + value.first + QLatin1Char('@'), value.second);
  return text;
}

QString lowered(const QString &standardOutput, const QString &standardError)
{
  return (standardOutput + QLatin1Char('\n') + standardError).toLower();
}

} // namespace

bool isValidAccountName(const QString &name)
{
  // \A and \z: a "$" would let a line break at the end through
  static const QRegularExpression shape(QStringLiteral(R"(\A[A-Za-z_][A-Za-z0-9_.-]{0,31}\z)"));
  if (!shape.match(name).hasMatch())
    return false;
  // a name of capitals, digits and underscores is an alias (or ALL) in sudoers, not an account
  static const QRegularExpression alias(QStringLiteral(R"(\A[A-Z][A-Z0-9_]*\z)"));
  if (alias.match(name).hasMatch())
    return false;
  return name != QLatin1String("root");
}

QString permissionLine(const QString &user)
{
  if (!isValidAccountName(user))
    return {};
  return QStringLiteral("%1 ALL=(root) NOPASSWD: %2, %3")
      .arg(user, commandText(Direction::Down), commandText(Direction::Up));
}

bool ScriptPaths::isPlain() const
{
  for (const auto &word : {tempDirectory, sudoersDirectory, mktemp, install, stat, mv, rm, visudo, owner, group}) {
    if (!isPlainWord(word))
      return false;
  }
  return true;
}

QString installScript(const QString &user, const ScriptPaths &paths)
{
  const auto line = permissionLine(user);
  if (line.isEmpty() || !paths.isPlain())
    return {};

  // One line for sh. In the order that keeps sudo whole:
  //  1. the temporary file is made in a directory of root (a name that nobody can guess or reach), and the bytes of the
  //     line are written there
  //  2. visudo checks exactly those bytes; if it says no the script ends here, nothing was put anywhere
  //  3. the bytes are copied with the owner and the mode that sudo wants to a name that sudo does not read (it skips a
  //     name with a dot), and owner and mode are read back
  //  4. that copy is checked once more
  //  5. only then it is renamed to its place, which is atomic: sudo sees the old file or the new one, never half of one
  //  A name of the file that is a directory is refused, an old file is replaced by the rename. The trap removes the
  //  temporary file and the copy whatever happens.
  const auto script = QStringLiteral(
      "set -eu; umask 077; d=@DIR@/@NAME@; s=$d.new; t=$(@MKTEMP@ @TMP@/@NAME@.XXXXXXXXXX); "
      "trap '@RM@ -f \"$t\" \"$s\"' EXIT; [ ! -d \"$d\" ]; printf '%s\\n' '@LINE@' > \"$t\"; @VISUDO@ -cf \"$t\"; "
      "@INSTALL@ -m 0440 -o @OWNER@ -g @GROUP@ \"$t\" \"$s\"; "
      "[ \"$(@STAT@ -f %Su:%Sg:%Lp \"$s\")\" = @OWNER@:@GROUP@:440 ]; @VISUDO@ -cf \"$s\"; @MV@ -f \"$s\" \"$d\""
  );
  return fill(
      script, {{QStringLiteral("DIR"), paths.sudoersDirectory},
               {QStringLiteral("NAME"), QLatin1String(kSudoersFileName)},
               {QStringLiteral("TMP"), paths.tempDirectory},
               {QStringLiteral("MKTEMP"), paths.mktemp},
               {QStringLiteral("RM"), paths.rm},
               {QStringLiteral("VISUDO"), paths.visudo},
               {QStringLiteral("INSTALL"), paths.install},
               {QStringLiteral("STAT"), paths.stat},
               {QStringLiteral("MV"), paths.mv},
               {QStringLiteral("OWNER"), paths.owner},
               {QStringLiteral("GROUP"), paths.group},
               // the line last: it has the name of the account, which is not a place for another token
               {QStringLiteral("LINE"), line}}
  );
}

QString removeScript(const ScriptPaths &paths)
{
  if (!paths.isPlain())
    return {};
  return fill(
      QStringLiteral("@RM@ -f @DIR@/@NAME@ @DIR@/@NAME@.new"),
      {{QStringLiteral("RM"), paths.rm},
       {QStringLiteral("DIR"), paths.sudoersDirectory},
       {QStringLiteral("NAME"), QLatin1String(kSudoersFileName)}}
  );
}

QStringList permissionArguments(PermissionAction action, const QString &prompt, const QString &user)
{
  const auto script = action == PermissionAction::Allow ? installScript(user) : removeScript();
  if (script.isEmpty())
    return {};
  // the script is constant and has no line break: the literal of AppleScript is exactly the script
  const auto command = QStringLiteral("do shell script ") + appleScriptString(script) +
                       QStringLiteral(" with administrator privileges with prompt ") + appleScriptString(prompt);
  return {QStringLiteral("-e"), command};
}

QStringList sudoArguments(Direction direction)
{
  return {
      QStringLiteral("-n"), QLatin1String(kIfconfigPath), QLatin1String(kInterface),
      direction == Direction::Down ? QStringLiteral("down") : QStringLiteral("up")
  };
}

QStringList sudoListArguments()
{
  return {
      QStringLiteral("-n"), QStringLiteral("-ll"), QLatin1String(kIfconfigPath), QLatin1String(kInterface),
      QStringLiteral("down")
  };
}

Listing parseSudoListing(int exitCode, const QString &standardOutput, const QString &standardError)
{
  if (exitCode != 0) {
    const auto said = lowered(standardOutput, standardError);
    if (said.contains(QLatin1String("a password is required")) ||
        said.contains(QLatin1String("is not allowed to execute")) || said.contains(QLatin1String("may not run sudo")) ||
        said.contains(QLatin1String("is not in the sudoers")))
      return Listing::PasswordNeeded;
    return Listing::Unknown;
  }

  QString text = standardOutput;
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  const auto lines = text.split(QLatin1Char('\n'));
  const auto command = commandText(Direction::Down);

  int matched = -1;
  for (int i = 0; i < lines.size(); ++i) {
    if (lines.at(i).trimmed().startsWith(QLatin1String("Matched:")))
      matched = i;
  }

  if (matched < 0) {
    // the short form: nothing but the command (a sudo before 1.9.15)
    QStringList shown;
    for (const auto &line : lines) {
      if (!line.trimmed().isEmpty())
        shown << line.trimmed();
    }
    return (shown.size() == 1 && shown.first() == command) ? Listing::PathOnly : Listing::Unknown;
  }

  // the rule that decides for the command that was asked about is the one that ran into it, and nothing else is
  // allowed without a password
  if (lines.at(matched).trimmed().mid(QStringLiteral("Matched:").size()).trimmed() != command)
    return Listing::PasswordNeeded;

  int entry = -1;
  for (int i = matched - 1; i >= 0; --i) {
    if (lines.at(i).startsWith(QLatin1String("Sudoers entry:")) ||
        lines.at(i).startsWith(QLatin1String("LDAP Role:"))) {
      entry = i;
      break;
    }
  }
  if (entry < 0)
    return Listing::Unknown;

  bool noPassword = false;
  for (int i = entry + 1; i < matched; ++i) {
    const auto line = lines.at(i).trimmed();
    if (!line.startsWith(QLatin1String("Options:")))
      continue;
    const auto options = line.mid(QStringLiteral("Options:").size()).split(QLatin1Char(','));
    for (const auto &option : options) {
      // a tag comes after the defaults of the entry in the listing, so the last word about it is the one that counts
      const auto word = option.trimmed();
      if (word == QLatin1String("!authenticate"))
        noPassword = true;
      else if (word == QLatin1String("authenticate"))
        noPassword = false;
    }
  }
  return noPassword ? Listing::NoPassword : Listing::PasswordNeeded;
}

PermissionState decidePermission(Listing listing, bool filePresent, bool sudoFailed)
{
  if (sudoFailed)
    return PermissionState::NotAllowed;
  switch (listing) {
  case Listing::NoPassword:
    return PermissionState::Allowed;
  case Listing::PasswordNeeded:
    return PermissionState::NotAllowed;
  case Listing::PathOnly:
    return filePresent ? PermissionState::Allowed : PermissionState::Unknown;
  case Listing::Unknown:
    break;
  }
  return PermissionState::Unknown;
}

ChangeRoute chooseChangeRoute(PermissionState state)
{
  return state == PermissionState::Allowed ? ChangeRoute::Silent : ChangeRoute::Dialog;
}

SudoResult parseSudoRun(bool started, int exitCode, const QString &standardError)
{
  if (!started)
    return SudoResult::PermissionMissing;
  if (exitCode == 0)
    return SudoResult::Done;
  const auto said = standardError.toLower();
  // the diagnostics of sudo itself are on lines that begin with its name; a refusal has its own sentence
  for (const auto &line : said.split(QLatin1Char('\n'))) {
    if (line.trimmed().startsWith(QLatin1String("sudo:")))
      return SudoResult::PermissionMissing;
  }
  if (said.contains(QLatin1String("is not allowed to execute")) || said.contains(QLatin1String("may not run sudo")) ||
      said.contains(QLatin1String("is not in the sudoers")))
    return SudoResult::PermissionMissing;
  return SudoResult::Failed;
}

SilentOutcome silentOutcome(SudoResult result)
{
  switch (result) {
  case SudoResult::Done:
    return SilentOutcome::Done;
  case SudoResult::PermissionMissing:
    return SilentOutcome::FallBackToDialog;
  case SudoResult::Failed:
    break;
  }
  return SilentOutcome::Failed;
}

QString permissionStateId(PermissionState state)
{
  switch (state) {
  case PermissionState::Allowed:
    return QStringLiteral("allowed");
  case PermissionState::NotAllowed:
    return QStringLiteral("not-allowed");
  case PermissionState::Unknown:
    break;
  }
  return QStringLiteral("unknown");
}

QString listingId(Listing listing)
{
  switch (listing) {
  case Listing::NoPassword:
    return QStringLiteral("no-password");
  case Listing::PasswordNeeded:
    return QStringLiteral("password-needed");
  case Listing::PathOnly:
    return QStringLiteral("path-only");
  case Listing::Unknown:
    break;
  }
  return QStringLiteral("unknown");
}

} // namespace deskflow::awdl
