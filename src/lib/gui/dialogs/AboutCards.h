/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

class QDialog;

namespace deskflow::gui {

/*!
 * \brief Lays out the About dialog as cards (the new look): the logo, the name and the version on the backdrop, the
 * description, the copyright and the credits in cards under them that scroll.
 *
 * The controls are moved, not made again: the dialog sets them and connects to them as it did. The dialog is as small
 * as it was.
 *
 * \return false, and nothing is changed, when a control that is looked for is not in the dialog
 */
bool applyAboutCards(QDialog *dialog);

} // namespace deskflow::gui
