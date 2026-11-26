/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class MenuMigrationTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void table_hasEveryActionOfTheOldMenusOnce_data();
  void table_hasEveryActionOfTheOldMenusOnce();
  void table_everyActionIsInTheMenuOfTheHeader();
  void table_everyActionExceptHelpHasAPlaceOfItsOwnToo();
  void table_saysWhereTheKeysAre();
  void table_theOldMenusAreFileEditViewHelp_andOnAMacTheMenuOfSharing();
  void table_groupsAreInTheOrderOfTheMenu();
  void migrationOf_findsTheEntryOfAnAction();
};
