/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QRectF>

#include <algorithm>
#include <cmath>

namespace deskflow::gui::style {

// A screen of 125 % has 1.25 device pixels in a logical one: an edge that is not on a whole device pixel is a blend of
// two rows. Whatever is drawn with a line puts its edges on whole device pixels, and a line is a whole number of them.

inline qreal snapToDevice(qreal logical, qreal dpr)
{
  return std::round(logical * dpr) / dpr;
}

inline QRectF snapRect(const QRectF &rect, qreal dpr)
{
  return QRectF(
      QPointF(snapToDevice(rect.left(), dpr), snapToDevice(rect.top(), dpr)),
      QPointF(snapToDevice(rect.right(), dpr), snapToDevice(rect.bottom(), dpr))
  );
}

// The width in logical pixels of a line that is wanted as wide as \p logical: one device pixel at least
inline qreal deviceLineWidth(qreal logical, qreal dpr)
{
  return std::max(1.0, std::round(logical * dpr)) / dpr;
}

} // namespace deskflow::gui::style
