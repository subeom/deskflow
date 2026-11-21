/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlPermissionTests.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#ifndef Q_OS_WIN
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#endif

using namespace deskflow::awdl;

Q_DECLARE_METATYPE(Listing)
Q_DECLARE_METATYPE(PermissionState)
Q_DECLARE_METATYPE(SudoResult)

namespace {

// What `sudo -n -ll /sbin/ifconfig awdl0 down` printed on a Mac whose account is an administrator and that has no
// permission of the program (sudo 1.9.17p2, macOS 27.3; read by the coordinator, exit 0): the rule that decides is the
// one of /etc/sudoers itself, there is no "Options:" line, and so a password is needed. This text is REAL.
const auto kRealPasswordNeeded = QStringLiteral(
    "Sudoers entry: /private/etc/sudoers\n"
    "    RunAsUsers: ALL\n"
    "    Commands:\n"
    "\tALL\n"
    "    Matched: /sbin/ifconfig awdl0 down\n"
);

// The same layout for the file of the program, with the option that the sudo source prints for a NOPASSWD tag
// (display.c, display_cmndspec_long: "Options: !authenticate"). This text is DERIVED from the source and the manual, it
// is not output of a real run: only the layout of the first one is real.
const auto kDerivedNoPassword = QStringLiteral(
    "Sudoers entry: /private/etc/sudoers.d/deskflow-awdl\n"
    "    RunAsUsers: root\n"
    "    Options: !authenticate\n"
    "    Commands:\n"
    "\t/sbin/ifconfig awdl0 down\n"
    "    Matched: /sbin/ifconfig awdl0 down\n"
);

// the literal of AppleScript that begins at \p pos in \p text, as the text that AppleScript makes of it; \p pos is left
// after the closing quote. A quote that is not escaped ends it, which is what the tests of the arguments rely on.
QString readLiteral(const QString &text, int &pos)
{
  QString value;
  if (pos >= text.size() || text.at(pos) != QLatin1Char('"'))
    return QStringLiteral("<no literal>");
  ++pos;
  while (pos < text.size()) {
    const auto c = text.at(pos++);
    if (c == QLatin1Char('\\') && pos < text.size()) {
      value += text.at(pos++);
      continue;
    }
    if (c == QLatin1Char('"'))
      return value;
    value += c;
  }
  return QStringLiteral("<unterminated>");
}

const auto kDefaultScript = QStringLiteral(
    "set -eu; umask 077; d=/etc/sudoers.d/deskflow-awdl; s=$d.new; "
    "t=$(/usr/bin/mktemp /private/var/root/deskflow-awdl.XXXXXXXXXX); "
    "trap '/bin/rm -f \"$t\" \"$s\"' EXIT; [ ! -d \"$d\" ]; "
    "printf '%s\\n' 'alice ALL=(root) NOPASSWD: /sbin/ifconfig awdl0 down, /sbin/ifconfig awdl0 up' > \"$t\"; "
    "/usr/sbin/visudo -cf \"$t\"; /usr/bin/install -m 0440 -o root -g wheel \"$t\" \"$s\"; "
    "[ \"$(/usr/bin/stat -f %Su:%Sg:%Lp \"$s\")\" = root:wheel:440 ]; /usr/sbin/visudo -cf \"$s\"; "
    "/bin/mv -f \"$s\" \"$d\""
);

} // namespace

void AwdlPermissionTests::accountName_mayGoInTheFileOnlyAsAShortName_data()
{
  QTest::addColumn<QString>("name");
  QTest::addColumn<bool>("valid");

  for (const auto *name : {"alice", "Alice", "_svc", "a.b-c", "a1", "user_name", "a", "x.y.z", "Subeom"})
    QTest::addRow("ok %s", name) << QString::fromLatin1(name) << true;
  QTest::addRow("ok 32 characters") << QString(QLatin1Char('a')).repeated(32) << true;

  QTest::addRow("empty") << QString() << false;
  QTest::addRow("33 characters") << QString(QLatin1Char('a')).repeated(33) << false;
  for (const auto &bad : {QStringLiteral("1abc"),    QStringLiteral("-x"),         QStringLiteral(".x"),
                          QStringLiteral("a b"),     QStringLiteral("a;b"),        QStringLiteral("a'b"),
                          QStringLiteral("a\"b"),    QStringLiteral("a$(id)"),     QStringLiteral("a`id`"),
                          QStringLiteral("a\nb"),    QStringLiteral("alice\n"),    QStringLiteral("alice "),
                          QStringLiteral(" alice"),  QStringLiteral("a,b"),        QStringLiteral("a:b"),
                          QStringLiteral("a=b"),     QStringLiteral("a\\b"),       QStringLiteral("#501"),
                          QStringLiteral("%admin"),  QStringLiteral("+group"),     QStringLiteral("!alice"),
                          QStringLiteral("a/b"),     QStringLiteral("a@b"),        QStringLiteral("a*"),
                          QStringLiteral("a?"),      QStringLiteral("\u00e9lise"), QStringLiteral("\uc9c0\uc218"),
                          QStringLiteral("a\u2028b")})
    QTest::addRow("refused %s", qPrintable(QString(bad).replace(QLatin1Char('\n'), QLatin1String("\\n"))))
        << bad << false;

  // words that sudoers reads as more than a name: ALL is everyone, a name of capitals is an alias
  for (const auto *word : {"ALL", "ADMIN", "FOO_BAR", "A1", "X", "A_"})
    QTest::addRow("refused alias %s", word) << QString::fromLatin1(word) << false;
  // root needs no permission
  QTest::addRow("refused root") << QStringLiteral("root") << false;
}

void AwdlPermissionTests::accountName_mayGoInTheFileOnlyAsAShortName()
{
  QFETCH(QString, name);
  QFETCH(bool, valid);
  QCOMPARE(isValidAccountName(name), valid);
}

void AwdlPermissionTests::permissionLine_isTheOneLineAndNothingElse()
{
  // exact arguments, no wildcard, no SETENV, no other command, the user is the only variable
  QCOMPARE(
      permissionLine(QStringLiteral("alice")),
      QStringLiteral("alice ALL=(root) NOPASSWD: /sbin/ifconfig awdl0 down, /sbin/ifconfig awdl0 up")
  );
  QCOMPARE(
      permissionLine(QStringLiteral("a.b-c")),
      QStringLiteral("a.b-c ALL=(root) NOPASSWD: /sbin/ifconfig awdl0 down, /sbin/ifconfig awdl0 up")
  );
  const auto line = permissionLine(QStringLiteral("alice"));
  for (const auto &forbidden : {"*", "?", "SETENV", "ALL)", "(ALL", "NOEXEC", "\n", "\t", "\\"})
    QVERIFY2(!line.contains(QLatin1String(forbidden)), forbidden);
  // nobody else gets a line
  for (const auto &name :
       {QString(), QStringLiteral("ALL"), QStringLiteral("a b"), QStringLiteral("root"), QStringLiteral("a\nb")})
    QVERIFY2(permissionLine(name).isEmpty(), qPrintable(name));
}

void AwdlPermissionTests::installScript_isTheConstantScript()
{
  QCOMPARE(installScript(QStringLiteral("alice")), kDefaultScript);
  // a script for sh in one line: AppleScript puts it in a string, and a line break in it would be lost
  QVERIFY(!installScript(QStringLiteral("alice")).contains(QLatin1Char('\n')));
}

void AwdlPermissionTests::installScript_hasOnlyTheNameOfTheAccountThatIsNotConstant()
{
  const auto alice = installScript(QStringLiteral("alice"));
  const auto bob = installScript(QStringLiteral("b.o-b_2"));
  QCOMPARE(
      QString(alice).replace(QLatin1String("alice"), QLatin1String("@")),
      QString(bob).replace(QLatin1String("b.o-b_2"), QLatin1String("@"))
  );
  // the name is in the line that is written, once, and nowhere else
  QCOMPARE(alice.count(QLatin1String("alice")), 1);
  // the default script is the script of the system: nothing of the tests' paths is in it
  QVERIFY(!alice.contains(m_dir.path()));
}

void AwdlPermissionTests::installScript_putsTheStepsInTheOrderThatKeepsSudoWhole()
{
  const auto script = installScript(QStringLiteral("alice"));
  const auto at = [&](const char *part) -> int {
    const auto index = static_cast<int>(script.indexOf(QLatin1String(part)));
    if (index < 0)
      QTest::qFail(part, __FILE__, __LINE__);
    return index;
  };
  const int mktemp = at("/usr/bin/mktemp");
  const int refuseDirectory = at("[ ! -d \"$d\" ]");
  const int write = at("printf '%s");
  const int checkBytes = at("/usr/sbin/visudo -cf \"$t\"");
  const int copy = at("/usr/bin/install -m 0440 -o root -g wheel \"$t\" \"$s\"");
  const int readBack = at("/usr/bin/stat -f %Su:%Sg:%Lp");
  const int checkCopy = at("/usr/sbin/visudo -cf \"$s\"");
  const int rename = at("/bin/mv -f \"$s\" \"$d\"");
  // the bytes are checked before anything is put anywhere, the copy is checked before it has the real name, and the
  // rename is the last thing that the script does
  QVERIFY(mktemp < refuseDirectory && refuseDirectory < write && write < checkBytes && checkBytes < copy);
  QVERIFY(copy < readBack && readBack < checkCopy && checkCopy < rename);
  QCOMPARE(script.lastIndexOf(QLatin1String("/bin/mv")), rename);
  QVERIFY(script.endsWith(QLatin1String("\"$s\" \"$d\"")));
  // it ends at the first step that fails, and cleans up in every way that it ends
  QVERIFY(script.startsWith(QLatin1String("set -eu;")));
  QVERIFY(script.contains(QLatin1String("trap '/bin/rm -f \"$t\" \"$s\"' EXIT")));
  // the real name is only ever written by the rename; the copy has a name that sudo skips (a dot in it)
  QVERIFY(script.contains(QLatin1String("s=$d.new")));
  QCOMPARE(script.count(QLatin1String("/etc/sudoers.d/deskflow-awdl")), 1);
  QCOMPARE(script.count(QLatin1String("/etc/sudoers.d")), 1);
  // nothing that edits /etc/sudoers, runs a program of the person's, or reads from them
  for (const auto &forbidden :
       {"/etc/sudoers;", "/etc/sudoers ", "; sudo ", "/usr/bin/sudo", "tee ", ">> ", "cat ", "eval", "$(read"})
    QVERIFY2(!script.contains(QLatin1String(forbidden)), forbidden);
}

void AwdlPermissionTests::installScript_isNothingForAnAccountOrAPathThatIsNotPlain()
{
  QVERIFY(installScript(QStringLiteral("a b")).isEmpty());
  QVERIFY(installScript(QStringLiteral("ALL")).isEmpty());
  QVERIFY(installScript(QString()).isEmpty());
  for (const auto &odd :
       {QStringLiteral("/tmp/a b"), QStringLiteral("/tmp/a;id"), QStringLiteral("/tmp/$x"), QStringLiteral("/tmp/a'b"),
        QStringLiteral(""), QStringLiteral("/tmp/a\nb")}) {
    ScriptPaths paths;
    paths.sudoersDirectory = odd;
    QVERIFY2(installScript(QStringLiteral("alice"), paths).isEmpty(), qPrintable(odd));
    QVERIFY2(removeScript(paths).isEmpty(), qPrintable(odd));
    ScriptPaths other;
    other.visudo = odd;
    QVERIFY2(installScript(QStringLiteral("alice"), other).isEmpty(), qPrintable(odd));
  }
}

void AwdlPermissionTests::removeScript_isTheConstantScript()
{
  QCOMPARE(removeScript(), QStringLiteral("/bin/rm -f /etc/sudoers.d/deskflow-awdl /etc/sudoers.d/deskflow-awdl.new"));
}

void AwdlPermissionTests::permissionArguments_areTheScriptInOneDialog()
{
  const auto prompt = QStringLiteral("Deskflow adds the file.");
  for (const auto action : {PermissionAction::Allow, PermissionAction::Remove}) {
    const auto script = action == PermissionAction::Allow ? installScript(QStringLiteral("alice")) : removeScript();
    const auto arguments = permissionArguments(action, prompt, QStringLiteral("alice"));
    QCOMPARE(arguments.size(), 2);
    QCOMPARE(arguments.first(), QStringLiteral("-e"));
    const auto &text = arguments.at(1);
    QVERIFY(text.startsWith(QLatin1String("do shell script \"")));
    int pos = QStringLiteral("do shell script ").size();
    // what AppleScript makes of the first literal is exactly the script: nothing was lost or added by the quoting
    QCOMPARE(readLiteral(text, pos), script);
    const auto middle = QStringLiteral(" with administrator privileges with prompt ");
    QCOMPARE(text.mid(pos, middle.size()), middle);
    pos += middle.size();
    QCOMPARE(readLiteral(text, pos), prompt);
    // and that is all
    QCOMPARE(pos, text.size());
  }
}

void AwdlPermissionTests::permissionArguments_cannotBeEndedByTheSentenceOfTheDialog()
{
  // a sentence that tries to end its string and add a command stays text
  const auto evil = QStringLiteral("x\" & (do shell script \"id\") & \"\nand a second line\\");
  const auto arguments = permissionArguments(PermissionAction::Allow, evil, QStringLiteral("alice"));
  QCOMPARE(arguments.size(), 2);
  const auto &text = arguments.at(1);
  int pos = QStringLiteral("do shell script ").size();
  QCOMPARE(readLiteral(text, pos), installScript(QStringLiteral("alice")));
  const auto middle = QStringLiteral(" with administrator privileges with prompt ");
  QCOMPARE(text.mid(pos, middle.size()), middle);
  pos += middle.size();
  const auto shown = readLiteral(text, pos);
  QCOMPARE(pos, text.size());
  // the line break became a space (the literal has none), every quote and backslash is still in it
  QCOMPARE(shown, QStringLiteral("x\" & (do shell script \"id\") & \" and a second line\\"));
  QVERIFY(!text.contains(QLatin1Char('\n')));
}

void AwdlPermissionTests::permissionArguments_areNothingForAnAccountThatCannotBeUsed()
{
  QVERIFY(permissionArguments(PermissionAction::Allow, QStringLiteral("p"), QStringLiteral("a b")).isEmpty());
  QVERIFY(permissionArguments(PermissionAction::Allow, QStringLiteral("p"), QString()).isEmpty());
  QVERIFY(permissionArguments(PermissionAction::Allow, QStringLiteral("p"), QStringLiteral("ALL")).isEmpty());
  // removing needs no name
  QVERIFY(!permissionArguments(PermissionAction::Remove, QStringLiteral("p"), QString()).isEmpty());
}

void AwdlPermissionTests::sudoArguments_areWhatTheLineAllowsWordForWord()
{
  QCOMPARE(
      sudoArguments(Direction::Down),
      (QStringList{
          QStringLiteral("-n"), QStringLiteral("/sbin/ifconfig"), QStringLiteral("awdl0"), QStringLiteral("down")
      })
  );
  QCOMPARE(
      sudoArguments(Direction::Up),
      (QStringList{
          QStringLiteral("-n"), QStringLiteral("/sbin/ifconfig"), QStringLiteral("awdl0"), QStringLiteral("up")
      })
  );
  QCOMPARE(
      sudoListArguments(), (QStringList{
                               QStringLiteral("-n"), QStringLiteral("-ll"), QStringLiteral("/sbin/ifconfig"),
                               QStringLiteral("awdl0"), QStringLiteral("down")
                           })
  );
  // the commands of the line, word for word: what sudo is asked to run is what the file allows
  const auto line = permissionLine(QStringLiteral("alice"));
  for (const auto direction : {Direction::Down, Direction::Up}) {
    auto arguments = sudoArguments(direction);
    arguments.removeFirst();
    QVERIFY(line.contains(arguments.join(QLatin1Char(' '))));
  }
  // no way to ask a password: -n is first, always
  QCOMPARE(sudoArguments(Direction::Down).first(), QStringLiteral("-n"));
  QCOMPARE(sudoListArguments().first(), QStringLiteral("-n"));
}

void AwdlPermissionTests::listing_saysWhetherThePasswordIsNeeded_data()
{
  QTest::addColumn<int>("exitCode");
  QTest::addColumn<QString>("standardOutput");
  QTest::addColumn<QString>("standardError");
  QTest::addColumn<Listing>("expected");

  // the real run on a Mac with no permission: the rule of /etc/sudoers decides, with no option, so a password is needed
  QTest::newRow("REAL: the admin rule decides, no option")
      << 0 << kRealPasswordNeeded << QString() << Listing::PasswordNeeded;
  // derived: the rule of the file of the program, with the option of a NOPASSWD tag
  QTest::newRow("DERIVED: our file, !authenticate") << 0 << kDerivedNoPassword << QString() << Listing::NoPassword;
  QTest::newRow("windows line ends") << 0
                                     << QString(kDerivedNoPassword).replace(QLatin1Char('\n'), QStringLiteral("\r\n"))
                                     << QString() << Listing::NoPassword;
  QTest::newRow("other options with it") << 0
                                         << QString(kDerivedNoPassword)
                                                .replace(
                                                    QStringLiteral("Options: !authenticate"),
                                                    QStringLiteral("Options: setenv, !authenticate, log_output")
                                                )
                                         << QString() << Listing::NoPassword;
  QTest::newRow("the default of the entry is overridden by the tag after it")
      << 0
      << QString(kDerivedNoPassword)
             .replace(QStringLiteral("Options: !authenticate"), QStringLiteral("Options: authenticate, !authenticate"))
      << QString() << Listing::NoPassword;
  QTest::newRow("a tag that asks for a password after it")
      << 0
      << QString(kDerivedNoPassword)
             .replace(QStringLiteral("Options: !authenticate"), QStringLiteral("Options: !authenticate, authenticate"))
      << QString() << Listing::PasswordNeeded;
  QTest::newRow("an option that only looks like it")
      << 0
      << QString(kDerivedNoPassword)
             .replace(QStringLiteral("Options: !authenticate"), QStringLiteral("Options: !authenticated, noexec"))
      << QString() << Listing::PasswordNeeded;
  QTest::newRow(
      "an entry with other options and no tag"
  ) << 0
    << QString(kDerivedNoPassword).replace(QStringLiteral("Options: !authenticate"), QStringLiteral("Options: setenv"))
    << QString() << Listing::PasswordNeeded;
  QTest::newRow("the matched line is another command") << 0
                                                       << QString(kDerivedNoPassword)
                                                              .replace(
                                                                  QStringLiteral("Matched: /sbin/ifconfig awdl0 down"),
                                                                  QStringLiteral("Matched: /sbin/ifconfig awdl0 up")
                                                              )
                                                       << QString() << Listing::PasswordNeeded;
  QTest::newRow("the matched line is the command with more")
      << 0
      << QString(kDerivedNoPassword)
             .replace(
                 QStringLiteral("Matched: /sbin/ifconfig awdl0 down"),
                 QStringLiteral("Matched: /sbin/ifconfig awdl0 down extra")
             )
      << QString() << Listing::PasswordNeeded;
  QTest::newRow("the matched line is the command in another place")
      << 0
      << QString(kDerivedNoPassword)
             .replace(
                 QStringLiteral("Matched: /sbin/ifconfig awdl0 down"),
                 QStringLiteral("Matched: /tmp/ifconfig awdl0 down")
             )
      << QString() << Listing::PasswordNeeded;
  QTest::newRow("the long form with no entry line")
      << 0 << QStringLiteral("    Options: !authenticate\n    Matched: /sbin/ifconfig awdl0 down\n") << QString()
      << Listing::Unknown;

  // a sudo before 1.9.15 shows nothing but the command
  QTest::newRow("the short form") << 0 << QStringLiteral("/sbin/ifconfig awdl0 down\n") << QString()
                                  << Listing::PathOnly;
  QTest::newRow("the short form of another command")
      << 0 << QStringLiteral("/sbin/ifconfig awdl0 up\n") << QString() << Listing::Unknown;
  QTest::newRow("the short form with more lines")
      << 0 << QStringLiteral("/sbin/ifconfig awdl0 down\n/bin/ls\n") << QString() << Listing::Unknown;

  QTest::newRow("empty") << 0 << QString() << QString() << Listing::Unknown;
  QTest::newRow("only blanks") << 0 << QStringLiteral("  \n\n") << QString() << Listing::Unknown;
  QTest::newRow("garbage") << 0 << QStringLiteral("Hello\nthere\n") << QString() << Listing::Unknown;

  QTest::newRow("a password is required")
      << 1 << QString() << QStringLiteral("sudo: a password is required\n") << Listing::PasswordNeeded;
  QTest::newRow("not allowed to execute")
      << 1 << QString()
      << QStringLiteral("Sorry, user alice is not allowed to execute '/sbin/ifconfig awdl0 down' as root on a-mac.\n")
      << Listing::PasswordNeeded;
  QTest::newRow("may not run sudo") << 1 << QString()
                                    << QStringLiteral("Sorry, user alice may not run sudo on a-mac.\n")
                                    << Listing::PasswordNeeded;
  QTest::newRow("another failure") << 1 << QString()
                                   << QStringLiteral("sudo: unable to open /etc/sudoers: Permission denied\n")
                                   << Listing::Unknown;
  QTest::newRow("ended (no exit code)") << -1 << QString() << QString() << Listing::Unknown;
  // a failed run is not trusted for what it printed
  QTest::newRow("failed with the text of an allow") << 1 << kDerivedNoPassword << QString() << Listing::Unknown;
}

void AwdlPermissionTests::listing_saysWhetherThePasswordIsNeeded()
{
  QFETCH(int, exitCode);
  QFETCH(QString, standardOutput);
  QFETCH(QString, standardError);
  QFETCH(Listing, expected);
  QCOMPARE(parseSudoListing(exitCode, standardOutput, standardError), expected);
}

void AwdlPermissionTests::listing_onlyTheEntryThatMatchedCounts()
{
  // two entries are in a listing: the option of the first is not the option of the one that matched
  const auto first = QStringLiteral(
      "Sudoers entry: /private/etc/sudoers.d/other\n    RunAsUsers: root\n    Options: !authenticate\n    "
      "Commands:\n\tALL\n\n"
  );
  const auto second = QStringLiteral(
      "Sudoers entry: /private/etc/sudoers\n    RunAsUsers: ALL\n    Commands:\n\tALL\n    Matched: /sbin/ifconfig "
      "awdl0 down\n"
  );
  QCOMPARE(parseSudoListing(0, first + second, QString()), Listing::PasswordNeeded);
  // and the other way round: the entry that matched has the option, the one before it has none
  const auto matching = QStringLiteral(
      "Sudoers entry: /private/etc/sudoers.d/deskflow-awdl\n    RunAsUsers: root\n    Options: !authenticate\n    "
      "Commands:\n"
      "\t/sbin/ifconfig awdl0 down\n    Matched: /sbin/ifconfig awdl0 down\n"
  );
  QCOMPARE(
      parseSudoListing(
          0, second.left(second.indexOf(QLatin1String("    Matched"))) + QLatin1Char('\n') + matching, QString()
      ),
      Listing::NoPassword
  );
}

void AwdlPermissionTests::permission_isDecidedByTheListingTheFileAndTheLastTry_data()
{
  QTest::addColumn<Listing>("listing");
  QTest::addColumn<bool>("filePresent");
  QTest::addColumn<bool>("sudoFailed");
  QTest::addColumn<PermissionState>("expected");

  const auto row = [](const char *name, Listing listing, bool present, bool failed, PermissionState state) {
    QTest::newRow(name) << listing << present << failed << state;
  };
  row("no password, file there", Listing::NoPassword, true, false, PermissionState::Allowed);
  // a rule of someone else's that allows it counts too (no dialog is needed): the file of the program is not the point
  row("no password, no file", Listing::NoPassword, false, false, PermissionState::Allowed);
  row("password needed, file there", Listing::PasswordNeeded, true, false, PermissionState::NotAllowed);
  row("password needed, no file", Listing::PasswordNeeded, false, false, PermissionState::NotAllowed);
  // an old sudo cannot say: only the file of the program, that sudo lists the command, is a reason
  row("path only, file there", Listing::PathOnly, true, false, PermissionState::Allowed);
  row("path only, no file", Listing::PathOnly, false, false, PermissionState::Unknown);
  row("unknown, file there", Listing::Unknown, true, false, PermissionState::Unknown);
  row("unknown, no file", Listing::Unknown, false, false, PermissionState::Unknown);
  // sudo -n was tried and said no: whatever the listing says, until the file is added again
  row("failed, no password", Listing::NoPassword, true, true, PermissionState::NotAllowed);
  row("failed, path only", Listing::PathOnly, true, true, PermissionState::NotAllowed);
  row("failed, unknown", Listing::Unknown, false, true, PermissionState::NotAllowed);
}

void AwdlPermissionTests::permission_isDecidedByTheListingTheFileAndTheLastTry()
{
  QFETCH(Listing, listing);
  QFETCH(bool, filePresent);
  QFETCH(bool, sudoFailed);
  QFETCH(PermissionState, expected);
  QCOMPARE(decidePermission(listing, filePresent, sudoFailed), expected);
}

void AwdlPermissionTests::chooseChangeRoute_isSilentOnlyForAKnownPermission()
{
  QCOMPARE(chooseChangeRoute(PermissionState::Allowed), ChangeRoute::Silent);
  // everything that is not known to be there is the dialog, as it was before the permission existed
  QCOMPARE(chooseChangeRoute(PermissionState::NotAllowed), ChangeRoute::Dialog);
  QCOMPARE(chooseChangeRoute(PermissionState::Unknown), ChangeRoute::Dialog);
}

void AwdlPermissionTests::sudoRun_saysWhetherSudoOrTheCommandFailed_data()
{
  QTest::addColumn<bool>("started");
  QTest::addColumn<int>("exitCode");
  QTest::addColumn<QString>("standardError");
  QTest::addColumn<SudoResult>("expected");

  QTest::newRow("done") << true << 0 << QString() << SudoResult::Done;
  QTest::newRow("done with a warning") << true << 0 << QStringLiteral("sudo: unable to resolve host\n")
                                       << SudoResult::Done;
  QTest::newRow("a password is required")
      << true << 1 << QStringLiteral("sudo: a password is required\n") << SudoResult::PermissionMissing;
  QTest::newRow("no terminal") << true << 1
                               << QStringLiteral(
                                      "sudo: a terminal is required to read the password; either use the -S option to "
                                      "read from standard input or configure an askpass helper\n"
                                  )
                               << SudoResult::PermissionMissing;
  QTest::newRow("no tty present") << true << 1
                                  << QStringLiteral("sudo: no tty present and no askpass program specified\n")
                                  << SudoResult::PermissionMissing;
  QTest::newRow(
      "not allowed"
  ) << true
    << 1
    << QStringLiteral("Sorry, user alice is not allowed to execute '/sbin/ifconfig awdl0 down' as root on a-mac.\n")
    << SudoResult::PermissionMissing;
  QTest::newRow("sudo then more lines") << true << 1 << QStringLiteral("something\nsudo: a password is required\n")
                                        << SudoResult::PermissionMissing;
  QTest::newRow("could not be started") << false << -1 << QString() << SudoResult::PermissionMissing;
  // the command ran and failed: a dialog would fail the same
  QTest::newRow("ifconfig failed") << true << 1 << QStringLiteral("ifconfig: interface awdl0 does not exist\n")
                                   << SudoResult::Failed;
  QTest::newRow("ifconfig failed, no message") << true << 1 << QString() << SudoResult::Failed;
  QTest::newRow("ended") << true << -1 << QString() << SudoResult::Failed;
  QTest::newRow("a message that only contains the word")
      << true << 1 << QStringLiteral("ifconfig: error, see sudo: docs\n") << SudoResult::Failed;
}

void AwdlPermissionTests::sudoRun_saysWhetherSudoOrTheCommandFailed()
{
  QFETCH(bool, started);
  QFETCH(int, exitCode);
  QFETCH(QString, standardError);
  QFETCH(SudoResult, expected);
  QCOMPARE(parseSudoRun(started, exitCode, standardError), expected);
}

void AwdlPermissionTests::silentOutcome_isTheDialogOnlyWhenSudoSaidNo()
{
  QCOMPARE(silentOutcome(SudoResult::Done), SilentOutcome::Done);
  QCOMPARE(silentOutcome(SudoResult::PermissionMissing), SilentOutcome::FallBackToDialog);
  QCOMPARE(silentOutcome(SudoResult::Failed), SilentOutcome::Failed);
}

// ---- the scripts, run for real in a tree of the test
// -----------------------------------------------------------------

void AwdlPermissionTests::initTestCase()
{
  qRegisterMetaType<Listing>();
  qRegisterMetaType<PermissionState>();
  qRegisterMetaType<SudoResult>();
#ifdef Q_OS_MACOS
  QVERIFY(m_dir.isValid());
  if (const auto *user = getpwuid(getuid()))
    m_owner = QString::fromUtf8(user->pw_name);
  if (const auto *group = getgrgid(getgid()))
    m_group = QString::fromUtf8(group->gr_name);
  // the scripts are for macOS (BSD stat and install); a path of the test that is not plain cannot be in one
  ScriptPaths probe;
  probe.sudoersDirectory = m_dir.path();
  probe.tempDirectory = m_dir.path();
  probe.owner = m_owner;
  probe.group = m_group;
  m_canRun = !m_owner.isEmpty() && !m_group.isEmpty() && probe.isPlain();
#endif
}

struct AwdlPermissionTests::Tree
{
  QString root;
  QString sudoers;
  QString temp;
  QString bin;
  ScriptPaths paths;

  QString target() const
  {
    return sudoers + QStringLiteral("/deskflow-awdl");
  }
  QString staged() const
  {
    return target() + QStringLiteral(".new");
  }
  QString mole() const
  {
    return sudoers + QStringLiteral("/mole");
  }
};

namespace {

void writeFile(const QString &path, const QByteArray &content, bool executable = false)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly))
    QFAIL(qPrintable(QStringLiteral("cannot write %1").arg(path)));
  file.write(content);
  file.close();
  if (executable &&
      !QFile::setPermissions(path, QFile::permissions(path) | QFile::ExeUser | QFile::ReadUser | QFile::WriteUser))
    QFAIL("cannot make the stand-in executable");
}

QString readText(const QString &path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return QStringLiteral("<not there>");
  return QString::fromUtf8(file.readAll());
}

} // namespace

// A tree like the one of the system: a directory of sudoers files that holds a file of another tool, a directory of
// root for the temporary file, and a directory of programs. The programs are those of macOS, except visudo: a stand-in
// that records what it was asked to check and when, and says no for the call numbers that the test asks for.
static AwdlPermissionTests::Tree
makeTree(const QString &base, const QString &name, const QString &owner, const QString &group)
{
  AwdlPermissionTests::Tree tree;
  tree.root = base + QLatin1Char('/') + name;
  tree.sudoers = tree.root + QStringLiteral("/sudoers.d");
  tree.temp = tree.root + QStringLiteral("/rootdir");
  tree.bin = tree.root + QStringLiteral("/bin");
  for (const auto &dir : {tree.sudoers, tree.temp, tree.bin}) {
    if (!QDir().mkpath(dir))
      QTest::qFail(qPrintable(dir), __FILE__, __LINE__);
  }
  writeFile(tree.mole(), "mole\n");
  const auto visudo =
      QStringLiteral(
          "#!/bin/sh\n"
          "n=$(/bin/cat %1/visudo.count 2>/dev/null || echo 0); n=$((n+1)); echo $n > %1/visudo.count\n"
          "echo \"$1 $2\" >> %1/visudo.args\n"
          "if [ -e %2 ]; then echo present >> %1/visudo.target; else echo absent >> %1/visudo.target; fi\n"
          "/usr/bin/stat -f %Lp \"$2\" >> %1/visudo.modes\n"
          "/bin/cp \"$2\" %1/visudo.seen.$n\n"
          "if [ -e %1/visudo.fail.$n ]; then echo 'visudo: stand-in says no' >&2; exit 1; fi\n"
          "exit 0\n"
      )
          .arg(tree.bin, tree.target());
  writeFile(tree.bin + QStringLiteral("/visudo"), visudo.toUtf8(), true);
  tree.paths.tempDirectory = tree.temp;
  tree.paths.sudoersDirectory = tree.sudoers;
  tree.paths.visudo = tree.bin + QStringLiteral("/visudo");
  tree.paths.owner = owner;
  tree.paths.group = group;
  return tree;
}

namespace {

// Runs the script with sh, the way `do shell script` does (one string for sh -c). The script must have nothing of the
// real system in it.
int runScript(const QString &script, const AwdlPermissionTests::Tree &tree, QString *errorOutput = nullptr)
{
  if (script.isEmpty()) {
    QTest::qFail("the script is empty", __FILE__, __LINE__);
    return -2;
  }
  // the test never runs a script that could reach the real /etc: every path in it is in the tree of the test
  if (script.contains(QLatin1String("/etc/")) || script.contains(QLatin1String("/private/")) ||
      !script.contains(tree.root)) {
    QTest::qFail(
        qPrintable(QStringLiteral("a path of the real system is in the script: %1").arg(script)), __FILE__, __LINE__
    );
    return -2;
  }
  QProcess process;
  process.start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), script});
  if (!process.waitForFinished(20000)) {
    process.kill();
    return -3;
  }
  if (errorOutput)
    *errorOutput = QString::fromUtf8(process.readAllStandardError());
  return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

QStringList names(const QString &dir)
{
  auto list = QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
  return list;
}

} // namespace

#define NEED_MAC()                                                                                                     \
  do {                                                                                                                 \
    if (!m_canRun)                                                                                                     \
      QSKIP("the scripts are run on macOS only (BSD install and stat), in a directory of the test");                   \
  } while (false)

void AwdlPermissionTests::install_putsTheFileInPlaceAtomically_withTheOwnerAndTheModeThatSudoWants()
{
  NEED_MAC();
  const auto tree = makeTree(m_dir.path(), QStringLiteral("ok"), m_owner, m_group);
  QString error;
  QCOMPARE(runScript(installScript(QStringLiteral("alice"), tree.paths), tree, &error), 0);
  QVERIFY2(error.isEmpty(), qPrintable(error));

  // the line, and only the line, with a line end
  QCOMPARE(readText(tree.target()), permissionLine(QStringLiteral("alice")) + QLatin1Char('\n'));
  const QFileInfo info(tree.target());
  QCOMPARE(info.owner(), m_owner);
  QCOMPARE(info.group(), m_group);
  // mode 0440: read for the owner and the group, nothing else
  QCOMPARE(info.permissions(), QFileDevice::ReadOwner | QFileDevice::ReadUser | QFileDevice::ReadGroup);
  // nothing is left: no copy, no temporary file, and the file of the other tool is as it was
  QCOMPARE(names(tree.sudoers), (QStringList{QStringLiteral("deskflow-awdl"), QStringLiteral("mole")}));
  QCOMPARE(readText(tree.mole()), QStringLiteral("mole\n"));
  QVERIFY2(names(tree.temp).isEmpty(), qPrintable(names(tree.temp).join(QLatin1Char(' '))));

  // visudo was asked twice: for the bytes (in the directory of root, no file of the program in place yet), and for the
  // copy (under a name that sudo skips, with the mode of the file), and the file was not in place at either of them
  const auto args = readText(tree.bin + QStringLiteral("/visudo.args")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  QCOMPARE(args.size(), 2);
  QVERIFY2(args.at(0).startsWith(QStringLiteral("-cf ") + tree.temp + QLatin1Char('/')), qPrintable(args.at(0)));
  QCOMPARE(args.at(1), QStringLiteral("-cf ") + tree.staged());
  QVERIFY(QFileInfo(tree.staged()).fileName().contains(QLatin1Char('.')));
  QCOMPARE(readText(tree.bin + QStringLiteral("/visudo.target")), QStringLiteral("absent\nabsent\n"));
  QCOMPARE(
      readText(tree.bin + QStringLiteral("/visudo.modes")).split(QLatin1Char('\n'), Qt::SkipEmptyParts).last(),
      QStringLiteral("440")
  );
  // and it checked the line that is written
  QCOMPARE(
      readText(tree.bin + QStringLiteral("/visudo.seen.1")), permissionLine(QStringLiteral("alice")) + QLatin1Char('\n')
  );
  QCOMPARE(
      readText(tree.bin + QStringLiteral("/visudo.seen.2")), permissionLine(QStringLiteral("alice")) + QLatin1Char('\n')
  );
}

void AwdlPermissionTests::install_aNoOfVisudoOnTheBytesLeavesNothing()
{
  NEED_MAC();
  const auto tree = makeTree(m_dir.path(), QStringLiteral("no1"), m_owner, m_group);
  writeFile(tree.bin + QStringLiteral("/visudo.fail.1"), "");
  QString error;
  QVERIFY(runScript(installScript(QStringLiteral("alice"), tree.paths), tree, &error) != 0);
  QVERIFY(error.contains(QStringLiteral("stand-in says no")));
  // not one file was put in place, not even the copy, and the temporary file is gone; visudo was asked once
  QCOMPARE(names(tree.sudoers), (QStringList{QStringLiteral("mole")}));
  QVERIFY(names(tree.temp).isEmpty());
  QCOMPARE(readText(tree.bin + QStringLiteral("/visudo.count")).trimmed(), QStringLiteral("1"));
}

void AwdlPermissionTests::install_aNoOfVisudoOnTheCopyLeavesTheOldFileAsItWas()
{
  NEED_MAC();
  const auto tree = makeTree(m_dir.path(), QStringLiteral("no2"), m_owner, m_group);
  writeFile(tree.target(), "an old file\n");
  writeFile(tree.bin + QStringLiteral("/visudo.fail.2"), "");
  QVERIFY(runScript(installScript(QStringLiteral("alice"), tree.paths), tree) != 0);
  // the old file is exactly what it was, the copy is gone, nothing is left in the directory of root
  QCOMPARE(readText(tree.target()), QStringLiteral("an old file\n"));
  QCOMPARE(names(tree.sudoers), (QStringList{QStringLiteral("deskflow-awdl"), QStringLiteral("mole")}));
  QVERIFY(names(tree.temp).isEmpty());
  QCOMPARE(readText(tree.bin + QStringLiteral("/visudo.count")).trimmed(), QStringLiteral("2"));
}

void AwdlPermissionTests::install_aNameThatIsADirectoryIsRefusedAndNothingIsTouched()
{
  NEED_MAC();
  const auto tree = makeTree(m_dir.path(), QStringLiteral("dir"), m_owner, m_group);
  QVERIFY(QDir().mkpath(tree.target()));
  writeFile(tree.target() + QStringLiteral("/inside"), "x");
  QVERIFY(runScript(installScript(QStringLiteral("alice"), tree.paths), tree) != 0);
  // the directory is as it was (the rename would have put the file INTO it), and visudo was not even asked
  QCOMPARE(names(tree.target()), (QStringList{QStringLiteral("inside")}));
  QVERIFY(QFileInfo(tree.target()).isDir());
  QCOMPARE(names(tree.sudoers), (QStringList{QStringLiteral("deskflow-awdl"), QStringLiteral("mole")}));
  QVERIFY(names(tree.temp).isEmpty());
  QVERIFY(!QFileInfo::exists(tree.bin + QStringLiteral("/visudo.count")));
}

void AwdlPermissionTests::install_anOldFileIsReplacedAndALeftoverCopyIsGone()
{
  NEED_MAC();
  const auto tree = makeTree(m_dir.path(), QStringLiteral("old"), m_owner, m_group);
  writeFile(tree.target(), "an old file\n");
  // a copy that an earlier run left (killed before it could clean up): sudo skips it, and the script replaces it
  writeFile(tree.staged(), "a leftover\n");
  QCOMPARE(runScript(installScript(QStringLiteral("alice"), tree.paths), tree), 0);
  QCOMPARE(readText(tree.target()), permissionLine(QStringLiteral("alice")) + QLatin1Char('\n'));
  QCOMPARE(names(tree.sudoers), (QStringList{QStringLiteral("deskflow-awdl"), QStringLiteral("mole")}));
  QVERIFY(names(tree.temp).isEmpty());
}

void AwdlPermissionTests::install_aStepThatFailsLeavesNothingBehind_data()
{
  QTest::addColumn<QString>("program");
  // the step that fails, by the program that it runs: the old file must be as it was, and nothing may be left
  QTest::newRow("mktemp") << QStringLiteral("mktemp");
  QTest::newRow("install") << QStringLiteral("install");
  QTest::newRow("mv") << QStringLiteral("mv");
}

void AwdlPermissionTests::install_aStepThatFailsLeavesNothingBehind()
{
  NEED_MAC();
  QFETCH(QString, program);
  auto tree = makeTree(m_dir.path(), QStringLiteral("fail-") + program, m_owner, m_group);
  writeFile(tree.target(), "an old file\n");
  const auto stand = tree.bin + QLatin1Char('/') + program;
  writeFile(stand, "#!/bin/sh\necho 'stand-in: no' >&2\nexit 1\n", true);
  if (program == QLatin1String("mktemp"))
    tree.paths.mktemp = stand;
  else if (program == QLatin1String("install"))
    tree.paths.install = stand;
  else
    tree.paths.mv = stand;
  QString error;
  QVERIFY(runScript(installScript(QStringLiteral("alice"), tree.paths), tree, &error) != 0);
  QVERIFY2(error.contains(QStringLiteral("stand-in: no")), qPrintable(error));
  QCOMPARE(readText(tree.target()), QStringLiteral("an old file\n"));
  QCOMPARE(names(tree.sudoers), (QStringList{QStringLiteral("deskflow-awdl"), QStringLiteral("mole")}));
  QVERIFY2(names(tree.temp).isEmpty(), qPrintable(names(tree.temp).join(QLatin1Char(' '))));
}

void AwdlPermissionTests::install_aWrongOwnerOrModeIsCaughtBeforeTheFileIsInPlace()
{
  NEED_MAC();
  auto tree = makeTree(m_dir.path(), QStringLiteral("stat"), m_owner, m_group);
  // what stat reads back is not what was asked for (a file system that does not keep the mode, a wrong owner): nothing
  // goes in place, and the copy is removed
  for (const auto &said :
       {QStringLiteral("someone:%1:440"), QStringLiteral("%1:wheelx:440"), QStringLiteral("%1:%2:644"),
        QStringLiteral("")}) {
    const auto stand = tree.bin + QStringLiteral("/stat");
    writeFile(
        stand,
        QStringLiteral("#!/bin/sh\necho '%1'\n")
            .arg(QString(said).replace(QStringLiteral("%1"), m_owner).replace(QStringLiteral("%2"), m_group))
            .toUtf8(),
        true
    );
    tree.paths.stat = stand;
    QVERIFY2(runScript(installScript(QStringLiteral("alice"), tree.paths), tree) != 0, qPrintable(said));
    QCOMPARE(names(tree.sudoers), (QStringList{QStringLiteral("mole")}));
    QVERIFY(names(tree.temp).isEmpty());
  }
}

void AwdlPermissionTests::remove_removesOnlyItsOwnFiles_andMindsNothingThatIsNotThere()
{
  NEED_MAC();
  const auto tree = makeTree(m_dir.path(), QStringLiteral("remove"), m_owner, m_group);
  writeFile(tree.target(), "x\n");
  writeFile(tree.staged(), "y\n");
  writeFile(tree.sudoers + QStringLiteral("/deskflow-awdl-other"), "z\n");
  QCOMPARE(runScript(removeScript(tree.paths), tree), 0);
  QCOMPARE(names(tree.sudoers), (QStringList{QStringLiteral("deskflow-awdl-other"), QStringLiteral("mole")}));
  QCOMPARE(readText(tree.mole()), QStringLiteral("mole\n"));
  // again, with nothing there: still no error
  QCOMPARE(runScript(removeScript(tree.paths), tree), 0);
  QCOMPARE(names(tree.sudoers), (QStringList{QStringLiteral("deskflow-awdl-other"), QStringLiteral("mole")}));
}

QTEST_GUILESS_MAIN(AwdlPermissionTests)
