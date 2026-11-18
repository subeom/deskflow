/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MenuMigration.h"

namespace deskflow::gui::chrome {

QList<MenuMigration> menuMigrations(bool mac)
{
  // the menu bar of the new look on a Mac is the menu of sharing (it has Preferences and Quit by their roles);
  // elsewhere it is File, Edit, View and Help
  const auto sharing = mac ? QStringLiteral("Sharing") : QStringLiteral("File");
  const auto preferencesMenu = mac ? QStringLiteral("Sharing") : QStringLiteral("Edit");

  return {
      // sharing
      {MenuAction::Start,
       sharing,
       QStringLiteral("Ctrl+S"),
       0,
       {Place::HeaderMenu, Place::StatusHero, Place::TrayMenu}},
      {MenuAction::Restart,
       sharing,
       QStringLiteral("Ctrl+S"),
       0,
       {Place::HeaderMenu, Place::StatusHero, Place::TrayMenu}},
      {MenuAction::Stop, sharing, QStringLiteral("Ctrl+T"), 0, {Place::HeaderMenu, Place::StatusHero, Place::TrayMenu}},
      {MenuAction::TestConnection, sharing, {}, 0, {Place::HeaderMenu, Place::ClientPage, Place::TrayMenu}},
      {MenuAction::ViewFingerprint, sharing, {}, 0, {Place::HeaderMenu, Place::Sidebar, Place::SettingsPage}},
      // the window
      {MenuAction::MinimizeToTray, sharing, {}, 1, {Place::HeaderMenu, Place::TrayMenu}},
      {MenuAction::ToggleSidebar,
       QStringLiteral("View"),
       mac ? QStringLiteral("Control+Command+S") : QStringLiteral("Ctrl+B"),
       1,
       {Place::HeaderMenu, Place::Sidebar}},
      {MenuAction::ToggleLog, QStringLiteral("View"), {}, 1, {Place::HeaderMenu, Place::Sidebar}},
      // the application
      {MenuAction::Preferences,
       preferencesMenu,
       QStringLiteral("the key of the platform for Preferences"),
       2,
       {Place::HeaderMenu, Place::Sidebar, Place::TrayMenu}},
      {MenuAction::ViewHelp, QStringLiteral("Help"), QStringLiteral("F1"), 2, {Place::HeaderMenu}},
      {MenuAction::About, QStringLiteral("Help"), {}, 2, {Place::HeaderMenu, Place::Sidebar}},
      {MenuAction::Quit,
       sharing,
       QStringLiteral("the key of the platform for Quit (Ctrl+Q on Windows)"),
       3,
       {Place::HeaderMenu, Place::TrayMenu}},
  };
}

MenuMigration migrationOf(MenuAction action, bool mac)
{
  for (const auto &entry : menuMigrations(mac)) {
    if (entry.action == action)
      return entry;
  }
  return {action, {}, {}, 0, {}};
}

} // namespace deskflow::gui::chrome
