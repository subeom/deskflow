/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QRect>
#include <QString>

#include <optional>

namespace deskflow::gui {

//! The screen that a computer told the server it has
struct ReportedScreen
{
  QString computer;
  //! In the coordinates of the computer: the position of its top left and the size of all of its monitors
  QRect shape;
};

//! The screen that a line of the log of the core says a computer reports, if it says one (experimental)
/*!
The core logs \c computer "X" reports its screen: x,y wxh when a computer that connects, or whose screen changes, tells
its size, while the experiment is on. The size is what the computer sent in the handshake that it has always sent.
*/
std::optional<ReportedScreen> parseReportedScreen(const QString &logLine);

} // namespace deskflow::gui
