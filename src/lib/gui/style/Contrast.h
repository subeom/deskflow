/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QColor>

namespace deskflow::gui::style {

/*!
 * WCAG 2.x relative luminance of an sRGB colour, 0 (black) to 1 (white); the opacity of the colour is not used.
 */
double relativeLuminance(const QColor &color);

/*!
 * WCAG 2.x contrast ratio of two opaque colours, 1 (the same) to 21 (black on white). Text needs 4.5, the boundary of a
 * control 3.
 */
double contrastRatio(const QColor &first, const QColor &second);

/*!
 * The colour that is seen where a layer with the opacity (0 to 1) is painted over an opaque colour: how the contrast
 * of text on a translucent panel is worked out against the colours of what is behind it.
 */
QColor over(const QColor &layer, qreal opacity, const QColor &under);

/*!
 * The colour nearest to the foreground (by the same hue, lighter or darker) that has at least the contrast against the
 * background. The foreground itself when it has it already.
 */
QColor ensureContrast(const QColor &foreground, const QColor &background, double minimum);

} // namespace deskflow::gui::style
