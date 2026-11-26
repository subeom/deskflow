/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

namespace deskflow::gui {

/*!
 * A line of the log of the core in the language of the program.  The core writes its log in English (it has no
 * translation of its own); the one line that the hold of the pointer of a Mac server writes (see OSXPointerHold.h) is
 * the exception that the Log page translates.  What comes after the sentence (the numbers and the name of the trigger,
 * between parentheses) is kept as the core wrote it.  Any other line, and every line when there is no translation of
 * the sentence, is returned as it is.
 */
QString localizeCoreLogLine(const QString &line);

} // namespace deskflow::gui
