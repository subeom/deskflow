/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QColor>

class QLabel;

namespace deskflow::gui::style {

enum class Status
{
  Success,
  Warning,
  Danger
};

/*!
 * Colours the text of a label that tells a state (the status line of the monitor editor). With the style layer it is
 * the colour of the state in the tokens of the look (that has the contrast with the surface in the light and the dark
 * look alike); without it, the colours that the program has always had, as the style sheets that it has always set.
 */
void applyStatusColor(QLabel *label, Status status);

struct Fill
{
  QColor background;
  QColor text;
};

/*!
 * The ground and the text of a message that tells an error under an input: crimson and white as it has always been
 * without the style layer, with it the colour of danger and the text that has the contrast with it.
 */
Fill errorFill();

} // namespace deskflow::gui::style
