/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The tray icon and its menu in the main window. Every window here is made in a directory of its own for the settings
// and for the temporary files (the single-instance check of the window opens a local socket there): none of them
// reaches the settings or the socket of a program that is running. The core is never started and no tray icon is
// shown on a screen (the tests run on the offscreen platform).
class TrayWindowTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void systemLook_trayMenuIsTheMenuItHasAlwaysBeen();
  void systemLook_trayMenuFollowsTheStateOfTheCoreAsItAlwaysDid();
  void systemLook_aClientSaysConnectAndDisconnect();
  void systemLook_theWindowAndTheMenuTakeTurnsToOfferTheWindow();

  void systemLook_hasNoTrayController_andNoStateInTheTrayIcon();
  void systemLook_menuBarIsTheOneItHasAlwaysBeen();

  void deskflowLook_trayMenuSaysTheStateOfTheCore();
  void deskflowLook_trayHasRowsOfItsOwn_theActionsOfTheWindowAreNotInIt();
  void deskflowLook_chosenRowsDoWhatTheMenuAlwaysDid();
  void deskflowLook_anUpdateIsInTheMenuAndOnTheIcon();
  void updateApplying_blocksTrayQuitAndClose();
  void deskflowLook_aClientTestsItsConnectionFromTheTray();
  void deskflowLook_menuBarHasASharingMenuWithEveryRowAndDimsTheOnesThatDoNotApply();
  void deskflowLook_menuBarOfAClientCanTestTheConnection();
  void deskflowLook_theColourOfTheIconFollowsTheSettingAndNeverTheShape();

  void closeToTray_withATray_hidesTheWindow();
  void closeToTray_withoutATray_keepsTheWindowWhereItCanBeFoundAgain();
  void nativeQuit_withCloseToTray_isAccepted();
  void nativeQuit_canceledBeforeTheCloseKeepsTheNextCloseBehavior();
  void systemShutdownDuringUpdate_isAccepted();

private:
  QTemporaryDir m_dir;
};
