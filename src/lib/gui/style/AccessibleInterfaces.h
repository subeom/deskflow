/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QAccessible>

/*!
 * What a screen reader is told about the widgets of the new look (SettingRow, SettingCard, SectionHeader, StatusChip,
 * StatusHero, and ToggleSwitch when the application has the style layer): their role, their name and their state.
 *
 *  - ToggleSwitch: a switch that is on or off, that can be toggled and pressed
 *  - SettingRow, SettingCard, StatusHero: a group, named by its title (the headline)
 *  - SectionHeader: a heading
 *  - StatusChip: static text, named "<kind>: <text>" so that the kind is not told by the colour alone
 *  - ArrangementCanvas, MonitorEditorWidget (when the application has the style layer): a list whose items are the
 *    computers, or the monitors, that are painted: selectable list items with a name and a description
 *
 * The widgets set their own accessible names; this only gives them the roles and, for the switch, the state and the
 * actions. Call it once before the first widget is made: the widgets do it themselves.
 */
void installAccessibleInterfaces();

/*!
 * The role that a screen reader is told for a ToggleSwitch. QAccessible::Switch exists since Qt 6.11; Qt 6.7 to 6.10
 * (the Windows builds use 6.9) has no such role, so there a switch is a check box: the same state (checkable, checked
 * or not), the same name and description, the same actions.
 */
inline QAccessible::Role switchRole()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
  return QAccessible::Switch;
#else
  return QAccessible::CheckBox;
#endif
}
