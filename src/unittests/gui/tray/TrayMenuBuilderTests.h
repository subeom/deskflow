/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The QMenu that is made of the content of the menu of the tray: the rows, the header on each OS, the commands and what a
// screen reader is told. No window and no tray: the menu is not shown
class TrayMenuBuilderTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void header_isTwoRowsOfTextWhereTheOsDrawsTheMenu();
  void header_isOneWidgetWithTheChipWhereQtDrawsTheMenu();
  void rows_followTheSpec_withSeparators_andKeepTheRowsThatStay();
  void command_isToldWhenItIsChosen_andNotWhenItIsDimmed();
  void shortcuts_areShownButBelongToTheMenuAlone();
  void icons_areTemplatesOnMacAndNamesOnLinux();
  void openRow_isInBold_andOnlyIt();
  void screenReader_isToldTheRowsAndTheirState();
};
