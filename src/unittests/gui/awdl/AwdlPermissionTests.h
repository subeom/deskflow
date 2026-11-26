/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/awdl/AwdlPermission.h"

#include <QTemporaryDir>
#include <QTest>

// The permission that lets the account change awdl0 with no password: the account name that may go in the file, the one
// line, the two constant scripts, how sudo is read and when the program uses it. The scripts are also RUN, for real, in
// a directory of the test (the real mktemp, install, stat, mv and rm of macOS, a stand-in for visudo, paths of the
// temporary directory only: no path of the real system is in what runs, and the test refuses to run a script that has
// one). Nothing here runs sudo, osascript or ifconfig.
class AwdlPermissionTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void accountName_mayGoInTheFileOnlyAsAShortName_data();
  void accountName_mayGoInTheFileOnlyAsAShortName();
  void permissionLine_isTheOneLineAndNothingElse();
  void installScript_isTheConstantScript();
  void installScript_hasOnlyTheNameOfTheAccountThatIsNotConstant();
  void installScript_putsTheStepsInTheOrderThatKeepsSudoWhole();
  void installScript_isNothingForAnAccountOrAPathThatIsNotPlain();
  void removeScript_isTheConstantScript();
  void permissionArguments_areTheScriptInOneDialog();
  void permissionArguments_cannotBeEndedByTheSentenceOfTheDialog();
  void permissionArguments_areNothingForAnAccountThatCannotBeUsed();
  void sudoArguments_areWhatTheLineAllowsWordForWord();
  void listing_saysWhetherThePasswordIsNeeded_data();
  void listing_saysWhetherThePasswordIsNeeded();
  void listing_onlyTheEntryThatMatchedCounts();
  void permission_isDecidedByTheListingTheFileAndTheLastTry_data();
  void permission_isDecidedByTheListingTheFileAndTheLastTry();
  void chooseChangeRoute_isSilentOnlyForAKnownPermission();
  void sudoRun_saysWhetherSudoOrTheCommandFailed_data();
  void sudoRun_saysWhetherSudoOrTheCommandFailed();
  void silentOutcome_isTheDialogOnlyWhenSudoSaidNo();

  void initTestCase();
  void install_putsTheFileInPlaceAtomically_withTheOwnerAndTheModeThatSudoWants();
  void install_aNoOfVisudoOnTheBytesLeavesNothing();
  void install_aNoOfVisudoOnTheCopyLeavesTheOldFileAsItWas();
  void install_aNameThatIsADirectoryIsRefusedAndNothingIsTouched();
  void install_anOldFileIsReplacedAndALeftoverCopyIsGone();
  void install_aStepThatFailsLeavesNothingBehind_data();
  void install_aStepThatFailsLeavesNothingBehind();
  void install_aWrongOwnerOrModeIsCaughtBeforeTheFileIsInPlace();
  void remove_removesOnlyItsOwnFiles_andMindsNothingThatIsNotThere();

public:
  struct Tree;

private:
  QTemporaryDir m_dir;
  QString m_owner;
  QString m_group;
  bool m_canRun = false;
};
