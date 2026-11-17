/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The menus and the menu bar in DeskflowStyle: rows of 36 px, the highlight of a row, the colours of the tokens with the
// contrast that text needs, a menu with a frame of its own, the menu bar
class MenuStyleTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void cleanup();

  void rows_are36PixelsHigh_andSeparatorsHaveTheirOwnHeight();
  void rows_makeTheMenuAsWideAsTheirTextIconAndShortcutNeed();
  void highlight_isTheSoftAccentOnTheRowThatIsActive_data();
  void highlight_isTheSoftAccentOnTheRowThatIsActive();
  void textOfAMenu_hasTheContrastThatTextNeeds_data();
  void textOfAMenu_hasTheContrastThatTextNeeds();
  void aDimmedRow_isDrawnInTheColourOfWhatIsDisabled();
  void aRowThatIsChecked_hasItsMark();
  void aMenuWithAFrameOfItsOwn_isRoundAndClearAtTheCorners();
  void menuBar_marksTheMenuThatIsOpen();
};
