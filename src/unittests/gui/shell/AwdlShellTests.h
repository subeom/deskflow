/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The option "turn off the wireless link of AirDrop and Handoff while sharing" in the main window (macOS): what it asks
// of the system and when, that it asks nothing with the option off, nothing where there is no such link, and that the
// banner is shown and hidden by the controller and its button asks for the password. The system is a fake: no program
// is run, no dialog is shown, and awdl0 is not touched.
class AwdlShellTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void optionOff_nothingIsReadOrAsked_whateverTheCoreDoes();
  void optionOn_aCoreThatStarts_readsTheLink_andAsksForThePasswordOnceForTheRun();
  void optionOn_aStoppedCore_restoresNothing_andTheNextRunAsksAgain();
  void optionOn_aRetryOfTheCoreIsTheSameRun();
  void theSettingChangedWhileTheCoreRuns_isFollowedAtOnce();
  void theBanner_isShownOnlyForAStutteringLink_andItsButtonAsksForThePassword();
  void notAMac_thereIsNoControllerNoCardAndNoCall();
  void aWindowOfThePlatform_hasNoSystemWithoutAScreen();
  void oldLook_theControllerRuns_andThereIsNoBanner();
  void withThePermission_theStartOfSharingAndTheBanner_askNoPassword();
  void withoutThePermission_theWindowAsksForThePasswordAsBefore();
  void thePermissionButtons_ofTheWindow_askTheQuestion_thenShowTheDialogWithTheSentencesOfTheProgram();

private:
  QTemporaryDir m_dir;
};
