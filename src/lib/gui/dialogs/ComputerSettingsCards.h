/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

class QDialog;

namespace validators {
class ValidationError;
}

namespace deskflow::gui {

/*!
 * \brief Lays out the settings of one computer as cards (the new look), out of the controls that the dialog has: each
 * group box becomes a SettingCard and each control, with its label, a SettingRow, on a scrolling backdrop.
 *
 * The controls are moved, not made again: the dialog reads and sets them, and connects to them, as it did. What it
 * hides or disables is followed by the row that has them. The group boxes stay, hidden and empty. The error of the name
 * and of the alias is shown in the row of the field, where the label of the error was.
 *
 * The dialog is as small as it was: the cards scroll in the room that the groups needed.
 *
 * \return false, and nothing is changed, when a control that is looked for is not in the dialog
 */
bool applyComputerSettingsCards(
    QDialog *dialog, validators::ValidationError *nameError, validators::ValidationError *aliasError
);

} // namespace deskflow::gui
