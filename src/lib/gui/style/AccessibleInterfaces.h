/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

/*!
 * What a screen reader is told about the widgets of the new look (SettingRow, SettingCard, SectionHeader, StatusChip,
 * StatusHero, and ToggleSwitch when the application has the style layer): their role, their name and their state.
 *
 *  - ToggleSwitch: a switch that is on or off, that can be toggled and pressed
 *  - SettingRow, SettingCard, StatusHero: a group, named by its title (the headline)
 *  - SectionHeader: a heading
 *  - StatusChip: static text, named "<kind>: <text>" so that the kind is not told by the colour alone
 *
 * The widgets set their own accessible names; this only gives them the roles and, for the switch, the state and the
 * actions. Call it once before the first widget is made: the widgets do it themselves.
 */
void installAccessibleInterfaces();
