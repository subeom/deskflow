/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The tray icon, its tool tip and its menu, kept by the controller as the main window tells it (the tray icon is not shown)
class TrayControllerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void input_makesTheMenuTheIconAndTheToolTip();
  void theSameInput_changesNothing_andDoesNotSetTheToolTipAgain();
  void externalRow_isShownUnderTheHeader_andTakenAway();
  void commandsOfTheMenu_areToldToTheOwner();
  void colourAndTone_choosTheIcon();
  void iconOfALinuxDesktop_isAskedOfTheTheme();
};
