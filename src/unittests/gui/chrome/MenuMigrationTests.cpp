/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MenuMigrationTests.h"

#include "gui/chrome/MenuMigration.h"

using namespace deskflow::gui::chrome;

namespace {

const QList<MenuAction> kAllActions = {
    MenuAction::Start,           MenuAction::Restart,        MenuAction::Stop,  MenuAction::TestConnection,
    MenuAction::ViewFingerprint, MenuAction::MinimizeToTray, MenuAction::Quit,  MenuAction::Preferences,
    MenuAction::ToggleLog,       MenuAction::ToggleSidebar,  MenuAction::About, MenuAction::ViewHelp
};

} // namespace

void MenuMigrationTests::table_hasEveryActionOfTheOldMenusOnce_data()
{
  QTest::addColumn<bool>("mac");
  QTest::newRow("Windows and Linux") << false;
  QTest::newRow("a Mac") << true;
}

void MenuMigrationTests::table_hasEveryActionOfTheOldMenusOnce()
{
  QFETCH(bool, mac);
  const auto table = menuMigrations(mac);
  QCOMPARE(table.size(), kAllActions.size());
  for (const auto action : kAllActions) {
    int found = 0;
    for (const auto &entry : table)
      found += entry.action == action ? 1 : 0;
    QCOMPARE(found, 1);
  }
}

void MenuMigrationTests::table_everyActionIsInTheMenuOfTheHeader()
{
  for (const bool mac : {false, true}) {
    for (const auto &entry : menuMigrations(mac))
      QVERIFY2(entry.places.contains(Place::HeaderMenu), qPrintable(QString::number(int(entry.action))));
  }
}

void MenuMigrationTests::table_everyActionExceptHelpHasAPlaceOfItsOwnToo()
{
  // the menu of the header is the place of all of them; most can also be done where the person is looking
  for (const auto &entry : menuMigrations(false)) {
    if (entry.action == MenuAction::ViewHelp) {
      QCOMPARE(entry.places, QList<Place>{Place::HeaderMenu});
      continue;
    }
    QVERIFY2(entry.places.size() >= 2, qPrintable(QString::number(int(entry.action))));
  }
  const auto has = [](MenuAction action, Place place) { return migrationOf(action, false).places.contains(place); };
  QVERIFY(has(MenuAction::Start, Place::StatusHero));
  QVERIFY(has(MenuAction::Stop, Place::StatusHero));
  QVERIFY(has(MenuAction::Start, Place::TrayMenu));
  QVERIFY(has(MenuAction::TestConnection, Place::ClientPage));
  QVERIFY(has(MenuAction::ViewFingerprint, Place::Sidebar));
  QVERIFY(has(MenuAction::ViewFingerprint, Place::SettingsPage));
  QVERIFY(has(MenuAction::Preferences, Place::Sidebar));
  QVERIFY(has(MenuAction::Preferences, Place::TrayMenu));
  QVERIFY(has(MenuAction::ToggleLog, Place::Sidebar));
  QVERIFY(has(MenuAction::ToggleSidebar, Place::Sidebar));
  QVERIFY(has(MenuAction::About, Place::Sidebar));
  QVERIFY(has(MenuAction::MinimizeToTray, Place::TrayMenu));
  QVERIFY(has(MenuAction::Quit, Place::TrayMenu));
}

void MenuMigrationTests::table_saysWhereTheKeysAre()
{
  // the keys of the actions of the old menus: they are kept, with the menu bar hidden as well
  QCOMPARE(migrationOf(MenuAction::Start, false).shortcutNote, QStringLiteral("Ctrl+S"));
  QCOMPARE(migrationOf(MenuAction::Restart, false).shortcutNote, QStringLiteral("Ctrl+S"));
  QCOMPARE(migrationOf(MenuAction::Stop, false).shortcutNote, QStringLiteral("Ctrl+T"));
  QCOMPARE(migrationOf(MenuAction::ViewHelp, false).shortcutNote, QStringLiteral("F1"));
  QCOMPARE(migrationOf(MenuAction::ToggleSidebar, false).shortcutNote, QStringLiteral("Ctrl+B"));
  QCOMPARE(migrationOf(MenuAction::ToggleSidebar, true).shortcutNote, QStringLiteral("Control+Command+S"));
  QVERIFY(!migrationOf(MenuAction::Quit, false).shortcutNote.isEmpty());
  QVERIFY(!migrationOf(MenuAction::Preferences, false).shortcutNote.isEmpty());
  // the others have none
  for (const auto action :
       {MenuAction::TestConnection, MenuAction::ViewFingerprint, MenuAction::MinimizeToTray, MenuAction::ToggleLog,
        MenuAction::About})
    QVERIFY2(migrationOf(action, false).shortcutNote.isEmpty(), qPrintable(QString::number(int(action))));
}

void MenuMigrationTests::table_theOldMenusAreFileEditViewHelp_andOnAMacTheMenuOfSharing()
{
  const auto menuOf = [](MenuAction action, bool mac) { return migrationOf(action, mac).legacyMenu; };
  for (const auto action :
       {MenuAction::Start, MenuAction::Restart, MenuAction::Stop, MenuAction::TestConnection,
        MenuAction::ViewFingerprint, MenuAction::MinimizeToTray, MenuAction::Quit}) {
    QCOMPARE(menuOf(action, false), QStringLiteral("File"));
    QCOMPARE(menuOf(action, true), QStringLiteral("Sharing"));
  }
  QCOMPARE(menuOf(MenuAction::Preferences, false), QStringLiteral("Edit"));
  // the menu of the application of a Mac takes Preferences by its role, from the menu of sharing
  QCOMPARE(menuOf(MenuAction::Preferences, true), QStringLiteral("Sharing"));
  QCOMPARE(menuOf(MenuAction::ToggleLog, false), QStringLiteral("View"));
  QCOMPARE(menuOf(MenuAction::ToggleSidebar, false), QStringLiteral("View"));
  QCOMPARE(menuOf(MenuAction::About, false), QStringLiteral("Help"));
  QCOMPARE(menuOf(MenuAction::ViewHelp, false), QStringLiteral("Help"));
}

void MenuMigrationTests::table_groupsAreInTheOrderOfTheMenu()
{
  // sharing, then the window, then the application: the groups never go back, and Quit is the last
  for (const bool mac : {false, true}) {
    const auto table = menuMigrations(mac);
    int last = 0;
    for (const auto &entry : table) {
      QVERIFY2(entry.group >= last, qPrintable(QString::number(int(entry.action))));
      last = entry.group;
    }
    QCOMPARE(table.last().action, MenuAction::Quit);
    QCOMPARE(table.first().action, MenuAction::Start);
  }
  QCOMPARE(migrationOf(MenuAction::Start, false).group, migrationOf(MenuAction::ViewFingerprint, false).group);
  QVERIFY(migrationOf(MenuAction::MinimizeToTray, false).group > migrationOf(MenuAction::Start, false).group);
  QVERIFY(migrationOf(MenuAction::Preferences, false).group > migrationOf(MenuAction::MinimizeToTray, false).group);
}

void MenuMigrationTests::migrationOf_findsTheEntryOfAnAction()
{
  for (const auto action : kAllActions)
    QCOMPARE(migrationOf(action, false).action, action);
}

QTEST_MAIN(MenuMigrationTests)
