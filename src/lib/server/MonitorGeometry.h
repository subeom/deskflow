/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace deskflow::server {

//! A monitor of a computer, in the coordinate space of the shape that the computer reports (DINF)
struct MonitorRect
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;

  int32_t right() const
  {
    return x + w;
  }
  int32_t bottom() const
  {
    return y + h;
  }
  bool contains(int32_t px, int32_t py) const
  {
    return px >= x && px < right() && py >= y && py < bottom();
  }
  bool operator==(const MonitorRect &) const = default;
};

struct MonitorPoint
{
  int32_t x = 0;
  int32_t y = 0;

  bool operator==(const MonitorPoint &) const = default;
};

//! What came of moving a pointer over the monitors of a computer
struct MonitorMove
{
  MonitorPoint position;
  //! What was left of the horizontal move when the pointer could not go on: it has the sign of the move
  int32_t restX = 0;
  //! What was left of the vertical move when the pointer could not go on: it has the sign of the move
  int32_t restY = 0;
};

struct MonitorLayoutResult;

//! The monitors of one computer: where its desktop is, and is not
/*!
Deskflow models a computer as one rectangle, the bounding box of its monitors. That is exact when the monitors make up a
rectangle and wrong when they do not (monitors of different sizes side by side, an L-shaped desktop, a portrait monitor
next to a landscape one): the part of the box that no monitor covers is a hole, which the pointer cannot be in but which
Deskflow would put it in. A layout knows the holes, and maps the pointer around them with the silhouette model:

- a position on an edge is a fraction of the side of the bounding box, as it always was: the monitors are one connected
  shape, so every row and every column of the box has a monitor on it, and a fraction never points at nothing
- the pointer enters on the outermost monitor pixel of the row (or column) that the fraction points at: always on a
  monitor, where Deskflow would have put it on the edge of the box and perhaps in a hole
- the pointer leaves through any pixel of the desktop that has nothing beyond it, which includes the inner edges of an
  L-shape, and moves like the pointer of the operating system does: it slides along an edge it is pushed against, so
  that it never has to be brought back from where it could not go (no dead travel)
- the row of a fraction and the fraction of a row map to each other exactly, so a pointer that goes to another
  computer and straight back is where it started

For a desktop that is a rectangle all of this is the mapping Deskflow has always used, to the bit.

A layout is checked when it is made: 1 to 16 monitors that do not overlap, each of a size, that together form one
connected shape (monitors share part of an edge, touching at a corner is not enough), that a shape of a computer can
describe (16 bits). Monitors that are exactly the same (a mirrored display) are one monitor.
*/
class MonitorLayout
{
public:
  static constexpr size_t kMaxMonitors = 16;

  //! Make a layout of \p monitors, or say what is wrong with them
  static MonitorLayoutResult create(std::vector<MonitorRect> monitors);

  //! Make a layout of the monitors that \p text lists
  /*!
  A list is \c x,y,w,h[;x,y,w,h...]. Spaces, empty items, a separator at the end and a line for each monitor are all
  right. What the diagnostic tools print as the block to paste is accepted as it is: \c computer: and \c space: lines
  are skipped, and \c monitor x,y,w,h may be followed by \c scale=... and \c primary.
  */
  static MonitorLayoutResult parse(std::string_view text);

  //! The monitors of the block to paste that a diagnostic tool printed somewhere in \p diagnosticText, as a list
  /*!
  Empty if the text has none.
  */
  static std::string extractPasteBlock(std::string_view diagnosticText);

  //! The monitors, the way \c parse() reads them
  std::string format() const;

  const std::vector<MonitorRect> &monitors() const
  {
    return m_monitors;
  }

  //! The smallest rectangle that has all monitors in it: what the computer reports as its shape
  MonitorRect boundingBox() const
  {
    return m_box;
  }

  //! Whether the monitors cover all of the bounding box, which is when this layout does nothing that Deskflow does not
  bool isPlainRectangle() const
  {
    return m_plain;
  }

  bool contains(int32_t x, int32_t y) const;

  //! The pixel of a monitor that is closest to \p x, \p y
  MonitorPoint nearest(int32_t x, int32_t y) const;

  //! The fraction of \p side that the row or column \p line (a y for left and right, an x for top and bottom) is at
  /*!
  The middle of the line as a fraction of the side of the bounding box, clamped to the edge. This is, to the bit, what
  \c getCursorEdgeFraction() makes of it.
  */
  float edgeFraction(Direction side, int32_t line) const;

  //! Where the pointer enters when it comes in through \p side at \p fraction of it, then goes \p depth pixels inward
  /*!
  Always a pixel of a monitor: the outermost one of the row (or column) that \p fraction is at, or the one that is
  further in when the monitor goes that far. For a rectangle that is, to the bit, what \c Server::mapToPixel() makes
  of it.
  */
  MonitorPoint entryPoint(Direction side, float fraction, int32_t depth = 0) const;

  //! Move the pointer from \p from by \p dx, \p dy: as far as there are monitors, sliding along an edge it is pushed to
  MonitorMove move(MonitorPoint from, int32_t dx, int32_t dy) const;

private:
  MonitorLayout() = default;

  // the monitor that has the pixel, if any
  const MonitorRect *monitorAt(int32_t x, int32_t y) const;
  // goes up to \p delta pixels along an axis from \p from; what it could not go is returned
  int32_t slide(MonitorPoint &from, int32_t delta, bool horizontal) const;

  std::vector<MonitorRect> m_monitors;
  MonitorRect m_box;
  bool m_plain = false;
};

//! A layout, or why there is none
struct MonitorLayoutResult
{
  std::optional<MonitorLayout> layout;
  //! What is wrong, when there is no layout
  std::string error;
  //! How many monitors were the same as another, and left out
  size_t duplicatesRemoved = 0;

  explicit operator bool() const
  {
    return layout.has_value();
  }
};

} // namespace deskflow::server
