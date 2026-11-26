/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QString>

namespace deskflow::gui::chrome {

//! What the menus of the main window (File, Edit, View, Help) could do. The window of the new look has no menu bar on
//! Windows and Linux: each of these has a place that is not the menu bar, and this is the list that says where
enum class MenuAction
{
  Start,
  Restart,
  Stop,
  TestConnection,
  ViewFingerprint,
  MinimizeToTray,
  Quit,
  Preferences,
  ToggleLog,
  ToggleSidebar,
  About,
  ViewHelp
};

//! Where something can be done in the window of the new look
enum class Place
{
  //! The menu of the header (the button with the three dots): it has all of them
  HeaderMenu,
  //! An entry or a button of the sidebar
  Sidebar,
  //! The state at the top of the main page (its Start and Restart buttons)
  StatusHero,
  //! The page of the connection of a client (its Test connection button)
  ClientPage,
  //! A page of the settings
  SettingsPage,
  //! The menu of the tray icon
  TrayMenu
};

struct MenuMigration
{
  MenuAction action;
  //! The menu of the menu bar that had it, in English (File, Edit, View, Help; Sharing on a Mac)
  QString legacyMenu;
  //! The key that the window gave it, in words (empty: none). The window of the new look keeps every one of them: the
  //! tests compare the key of each action in the window with the system look and in the window of the new look
  QString shortcutNote;
  //! The group of the header menu that it is in: a line is drawn between two groups
  int group = 0;
  //! Where it can be done now: every one has the menu of the header, and most have a place of their own as well
  QList<Place> places;
};

/*!
 * \brief The table of the actions of the old menus and where they are now, in the order of the menu of the header. The
 * menu of the header is made from it, and the tests check that every action of the old menus is in it.
 *
 * \p mac says whether the menu bar of the system is the one of macOS (it has Preferences and Quit by their roles, and
 * its key for the sidebar is Control+Command+S).
 */
QList<MenuMigration> menuMigrations(bool mac);

//! The entry of an action of the table
MenuMigration migrationOf(MenuAction action, bool mac);

} // namespace deskflow::gui::chrome
