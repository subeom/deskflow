/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "DesignTokens.h"

#include <QColor>
#include <QList>
#include <QPixmap>
#include <QRectF>
#include <QSize>

class QPainter;

namespace deskflow::gui::style {

/*!
 * \brief The colours that the arrangement canvas and the monitor editor are painted with in the new look.
 *
 * Colour means four things only: the computer that is this one (accent tint), a computer that is connected (success),
 * the one that is selected (accent ring) and what is wrong (danger). Everything else is neutral, so that the colour
 * that there is can be found: the computers differ by their names, not by a hue of their own. A state is never told by
 * colour alone: a connection is a ring with a check, an error is hatched, a warning is dashed.
 *
 * Every pair that carries text is worked out to 4.5:1, every edge, ring and mark to 3:1, against the colours that are
 * behind it, in the light, the dark and the high contrast look alike (CanvasThemeTests).
 */
struct CanvasTheme
{
  // inside of the canvas, its 1 px frame, and the dots of its grid (not in high contrast)
  QColor background;
  QColor frame;
  QColor dots;
  bool showDots = true;

  // a computer, or a monitor, and the one that is this computer
  QColor fill;
  QColor border;
  QColor fillThis;
  QColor borderThis;
  // the name and the lines under it, on either fill
  QColor text;
  QColor textSecondary;
  // the bar of the main display, a mark of the lines under the name
  QColor mainBar;

  // the selected computer (its ring and its handles), and where the keys are (outside the ring of the selection)
  QColor selected;
  QColor focus;

  // where the computers touch, and the edge that parts that from what is under it
  QColor link;
  QColor linkEdge;

  // what is wrong: the outline and the hatch
  QColor error;
  QColor errorHatch;

  // a computer that is connected: the dot; one that is not: its ring
  QColor connected;
  QColor offline;

  // the lines that tell where an edge is in line while a computer is dragged
  QColor guide;

  // the size while an edge is dragged: a dark bubble with a light text (the other way round in the dark look)
  QColor bubble;
  QColor bubbleText;
};

//! The colours of the canvas for the tokens of a look
CanvasTheme canvasThemeFor(const Tokens &tokens, const Look &look);

//! The colours of the canvas for the look that the application is painted with now
CanvasTheme currentCanvasTheme();

/*!
 * The canvas as it is with nothing on it: the ground, rounded like the other controls of the look, with its frame and the
 * dots of its grid (when the look has them). For the size (in device independent pixels) and the device pixel ratio that
 * it is painted at; what is outside the rounded corners is transparent. The widgets keep it, because what is painted over
 * it is painted at every move of a drag.
 */
QPixmap paintCanvasBackdrop(const CanvasTheme &theme, const QSize &size, qreal dpr);

/*!
 * The ring that tells where the keys are: around each of the rectangles (the monitors of the selected computer, the
 * selected monitor), outside the ring of the selection, in the colour of the text; when there are none, along the edge of
 * the canvas.
 */
void paintCanvasFocus(QPainter *painter, const CanvasTheme &theme, const QSize &canvas, const QList<QRectF> &around);

} // namespace deskflow::gui::style
