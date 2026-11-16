/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/Arrangement.h"
#include "server/MonitorGeometry.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

/*!
 * \file MonitorEditor.h
 * The geometry of the editor that draws the monitors of one computer with the mouse. Pure: no Qt, no platform call, no
 * part of the wire protocol. The model, the units and the rounding rules are in docs/dev/arrangement.md.
 *
 * A layout is drawn in the units of the arrangement (logical pixels at 100 %). A monitor has a drawn rectangle and a
 * display scale; the resolution that it shows is the drawn size times the scale. What the server needs, the monitors in
 * the coordinates that the computer reports, is made from the drawing by \c realise().
 */
namespace deskflow::server::editor {

using arrangement::Guide;
using arrangement::Rect;

constexpr double kMinScale = 25.0;
constexpr double kMaxScale = 500.0;
//! The smallest side of a monitor that is drawn, in units
constexpr double kMinSide = 160.0;
constexpr double kMaxSide = 40000.0;
//! Edges that are closer than this are one edge: the drawing of one that was snapped is exact, this is for the rest
constexpr double kEdge = arrangement::kTouchTolerance;
//! How near a ratio of sides has to be to a known one to snap to it (relative)
constexpr double kAspectSnap = 0.02;
//! How near a size in pixels has to be to a preset to snap to it (relative)
constexpr double kPresetSnap = 0.015;
//! How different the shape of a drawing may be from the screen that the computer reports (relative) for it to be fitted
constexpr double kFitTolerance = 0.03;

struct Monitor
{
  //! The top left corner, and the size, in units
  double x = 0;
  double y = 0;
  double w = 1920;
  double h = 1080;
  //! The display scale in percent: the pixels of the monitor, in units, are its size times the scale
  double scale = 100;

  int pixelsW() const
  {
    return static_cast<int>(std::lround(w * scale / 100.0));
  }
  int pixelsH() const
  {
    return static_cast<int>(std::lround(h * scale / 100.0));
  }
  double right() const
  {
    return x + w;
  }
  double bottom() const
  {
    return y + h;
  }
  Rect rect() const
  {
    return {x, y, w, h};
  }
  bool operator==(const Monitor &) const = default;
};

//! The first monitor is the main display: the origin of the computer is its top left corner when nothing says better
struct Layout
{
  std::vector<Monitor> monitors;

  Rect box() const;
  bool operator==(const Layout &) const = default;
};

struct Problem
{
  enum class Kind
  {
    NoMonitors,
    TooMany,
    BadSize,
    BadScale,
    Overlap,
    NotConnected,
  };

  Kind kind;
  size_t first = 0;
  size_t second = 0;

  bool operator==(const Problem &) const = default;
};

//! What is wrong with a layout; empty when it can be used
std::vector<Problem> check(const Layout &layout);
std::string describe(const Problem &problem);

struct Preset
{
  const char *name;
  int w;
  int h;
};
//! The resolutions that can be picked, in pixels, with the sizes of the MacBooks in points
std::span<const Preset> presets();
const Preset *presetOf(int w, int h);

struct Aspect
{
  const char *name;
  int w;
  int h;
};
//! The ratios of sides that a monitor snaps to when it is resized, landscape: the portrait ones are the inverse
std::span<const Aspect> aspects();
//! The name of the ratio, of either orientation, that \p w : \p h is within \c kAspectSnap of, or empty
std::string aspectNameOf(double w, double h);

//! The display scales that are offered, in percent
std::span<const int> commonScales();

struct MoveResult
{
  Layout layout;
  std::vector<Guide> guides;
  //! The place is not one that a monitor can be: the layout is as it was
  bool refused = false;
};

/*!
 * Moves the monitor \p index to \p x, \p y: its edges snap to those of the other monitors, and to those of the
 * \p obstacles, that are within \p radius; the ones that touch (left against right, top against bottom), the ones in
 * line, and the middles. A monitor is never put over another.
 */
MoveResult
move(const Layout &layout, size_t index, double x, double y, double radius, const std::vector<Rect> &obstacles = {});

enum class Handle
{
  Left,
  Right,
  Top,
  Bottom,
  TopLeft,
  TopRight,
  BottomLeft,
  BottomRight
};

struct Constraints
{
  //! What else is in the way, and what an edge snaps to
  std::vector<Rect> obstacles;
  //! A size, in units, that a side snaps to when it is near it: the screen that the computer reports
  std::optional<std::pair<double, double>> targetSize;
  bool snapAspect = true;
  bool snapPresets = true;
  /*!
   * Keep this ratio (width over height): the side that is not dragged follows the one that is (for a corner, the side
   * that was changed the most). The opposite edge stays, and for an edge that is dragged the near edge of the other
   * side does too (the top of a monitor that is resized by its left or right edge, its left side when it is resized by
   * the top or the bottom). The ratios and the presets are not snapped then, nor is the target size.
   */
  std::optional<double> lockAspect;
};

struct ResizeResult
{
  Layout layout;
  std::vector<Guide> guides;
  bool refused = false;
  //! What the size snapped to, for the tooltip
  std::string aspect;
  bool preset = false;
  bool target = false;
};

/*!
 * Drags the edge or the corner \p handle of the monitor \p index to \p toX, \p toY: the opposite edge stays. The
 * edge snaps to those of the other monitors within \p radius; the sides snap to the common ratios, to the presets and
 * to the target size, when they are near, and a side is never smaller than \c kMinSide or put over another monitor.
 */
ResizeResult resize(
    const Layout &layout, size_t index, Handle handle, double toX, double toY, double radius,
    const Constraints &constraints = {}
);

//! The monitor with this resolution at its scale; it stays where the most of its edges touch the others
Layout withResolution(const Layout &layout, size_t index, int pixelsW, int pixelsH);
//! The same pixels at another scale, which is another size in the drawing
Layout withScale(const Layout &layout, size_t index, double scale);
//! Portrait for landscape and back, about the middle when it fits, else where the most of its edges touch the others
Layout rotated(const Layout &layout, size_t index);
//! A monitor like the first one, to the right of the last one, with its top in line with the first
Layout added(const Layout &layout);
Layout removed(const Layout &layout, size_t index);
//! The monitor \p index first
Layout withMain(const Layout &layout, size_t index);

/*!
 * The drawing of monitors that were stored: \p rects in the coordinates of the computer, the \p zoom and the position
 * of its origin in the arrangement, and the display scale of each monitor (the scale of the zoom for those that are
 * missing).
 */
Layout fromStored(
    const std::vector<MonitorRect> &rects, double zoom, double originX, double originY,
    const std::vector<double> &scales
);

enum class Fit
{
  //! The layout cannot be used: see \c error
  Invalid,
  //! No screen is known for the computer: the layout was made with the scale of its main display
  NotVerified,
  //! The box of the monitors is the screen that the computer reports
  Matches,
  //! The shape of the drawing is not the shape of the screen that the computer reports; see \c differsPercent
  Differs,
};

struct Realised
{
  Fit fit = Fit::Invalid;
  //! In the coordinates of the computer, in the order of the drawing
  std::vector<MonitorRect> monitors;
  std::vector<double> scales;
  //! Arrangement units for a unit of the computer, and where the origin of the computer is in the arrangement
  double zoom = 1.0;
  double originX = 0;
  double originY = 0;
  //! The monitors as \c MonitorLayout::parse() reads them
  std::string text;
  //! How far the shape of the drawing is from the shape of the screen, in percent, when it \c Differs
  double differsPercent = 0;
  //! The size of the drawing in the units of the computer, with the scale of its main display
  int drawingW = 0;
  int drawingH = 0;
  std::optional<MonitorRect> reported;
  std::string error;
};

/*!
 * The monitors of the drawing in the coordinates of the computer.
 *
 * With the screen that the computer \p reported, the drawing is scaled to it: each edge is put at its place in the
 * box, and rounded as an edge, so the monitors that share an edge go on sharing it exactly, there is no gap and no
 * overlap, and the box of the monitors is the screen. That is done when the shape of the drawing is within
 * \c kFitTolerance of the shape of the screen (or always, with \p stretch); else the layout is made as without a
 * screen, and the result \c Differs.
 *
 * Without one, the drawing is made in the units of the scale of the main display, with its top left at 0,0.
 */
Realised realise(const Layout &layout, const std::optional<MonitorRect> &reported, bool stretch = false);

//! The states of a layout that is being edited, to go back and forward over
class History
{
public:
  explicit History(Layout initial);

  const Layout &current() const
  {
    return m_states[m_position];
  }
  //! A layout that is not the current one is the new current one, and what could be redone is gone
  void push(Layout layout);
  bool canUndo() const
  {
    return m_position > 0;
  }
  bool canRedo() const
  {
    return m_position + 1 < m_states.size();
  }
  void undo();
  void redo();

private:
  static constexpr size_t kLimit = 200;
  std::vector<Layout> m_states;
  size_t m_position = 0;
};

} // namespace deskflow::server::editor
