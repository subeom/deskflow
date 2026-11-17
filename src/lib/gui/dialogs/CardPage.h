/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QSize>

class QDialog;
class QScrollArea;
class QWidget;
class SettingCard;

namespace deskflow::gui::cards {

/*!
 * \brief Puts cards on a scrolling backdrop in a dialog, with a header over them and the buttons of the dialog under
 * them: the way that the settings of a computer and the About dialog are laid out in the new look.
 *
 * The backdrop is the first item of the layout of the dialog, which has no margin. \p header and \p buttons are moved
 * (not made again). The cards scroll when the dialog is shorter than they are, and are never cut at the side.
 *
 * \param oldMinimum the minimum size that the dialog had with the layout that this replaces: the dialog is as small as
 * it was, and the cards scroll in the room that it gave them
 * \return the scroll area of the cards
 */
QScrollArea *assemblePage(
    QDialog *dialog, const QList<SettingCard *> &cards, QWidget *buttons, const QSize &oldMinimum,
    QWidget *header = nullptr
);

} // namespace deskflow::gui::cards
