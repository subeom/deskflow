/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QFont>

namespace deskflow::gui::style {

//! The font \p ratio times as large as \p font: a size in points where the font has one, a size in pixels where it was
//! set in pixels. The sizes of the roles are ratios of the size that the system gives text, so that they follow the
//! text size that the user set.
inline QFont scaleFont(QFont font, qreal ratio)
{
  if (font.pointSizeF() > 0)
    font.setPointSizeF(font.pointSizeF() * ratio);
  else if (font.pixelSize() > 0)
    font.setPixelSize(qMax(1, qRound(font.pixelSize() * ratio)));
  return font;
}

} // namespace deskflow::gui::style
