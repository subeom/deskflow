/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

class QWidget;

namespace deskflow::gui {

/*!
 * \brief Lays out the Advanced tab of the server configuration as cards (the new look), out of the controls that the
 * tab has: each group box becomes a SettingCard and each control, with its text, a SettingRow on a scrolling backdrop.
 *
 * The controls are moved, not made again: the form that owns them reads and sets them, and connects to them, as it
 * did, and what it hides or disables is followed by the row that has them. The group boxes stay, hidden and empty, for
 * the code that asks one of them (the group box of the config file is a check box too).
 *
 * \return false, and nothing is changed, when a control that is looked for is not on the tab
 */
bool applyAdvancedCards(QWidget *tab);

} // namespace deskflow::gui
