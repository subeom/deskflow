/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"
#include "server/MonitorGeometry.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

//! The arrangement canvas: computers and their monitors in one 2-D space, and the server links derived from it
/*!
Pure geometry and text: no Qt, no platform call, no part of the wire protocol. The model is in docs/dev/arrangement.md.
*/
namespace deskflow::server::arrangement {

//! Edges that are this close (arrangement units) touch
constexpr double kTouchTolerance = 0.5;
//! A touching stretch of edges shorter than this (arrangement units) is not a link
constexpr double kMinLink = 1.0;
constexpr double kMinZoom = 0.05;
constexpr double kMaxZoom = 20.0;
//! A coordinate of the arrangement is within this
constexpr double kMaxCoordinate = 1.0e6;

//! A rectangle in the arrangement space
struct Rect
{
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;

  double right() const
  {
    return x + w;
  }
  double bottom() const
  {
    return y + h;
  }
  double centerX() const
  {
    return x + w / 2;
  }
  double centerY() const
  {
    return y + h / 2;
  }
  bool operator==(const Rect &) const = default;
};

//! A computer: its monitors, in its own coordinates, and where and how big it is in the arrangement
struct Computer
{
  std::string name;
  //! In the coordinate units that the computer reports its screen in: points on macOS, physical pixels on Windows
  std::vector<MonitorRect> monitors;
  //! Arrangement units per coordinate unit of this computer
  double zoom = 1.0;
  //! Where the origin (0,0) of the coordinates of the computer is in the arrangement
  double originX = 0;
  double originY = 0;
  //! The screen of the server itself: it is the group that does not fall apart, and the computer that is reached first
  bool isServer = false;

  //! The monitors after the same rectangle was left out of the list once
  std::vector<MonitorRect> distinctMonitors() const;
  //! The monitors in the arrangement
  std::vector<Rect> monitorRects() const;
  //! The smallest rectangle that has all monitors in it, in the arrangement; empty for no monitors
  Rect box() const;
  //! The bounding box of the monitors in the coordinates of the computer, which is the shape it reports
  MonitorRect shape() const;
};

struct Arrangement
{
  std::vector<Computer> computers;

  //! Index of the computer \p name, or -1
  int indexOf(std::string_view name) const;
  //! The server, or -1
  int serverIndex() const;
};

//! What is wrong, or only not as it should be, with an arrangement
struct Issue
{
  enum class Kind
  {
    //! \c computer has no name or a name that another computer has (the other one is \c other)
    NameIsEmpty,
    DuplicateName,
    //! The monitors of \c computer are not a connected shape; \c detail says what is wrong
    InvalidMonitors,
    //! The zoom of \c computer is out of range
    InvalidZoom,
    //! A position that is not a number or is too far out
    InvalidPosition,
    //! A monitor of \c computer and one of \c other overlap
    Overlap,
    //! \c computer cannot be reached from the server through links
    NotConnected,
    //! A row of the \c side of \c computer has two exposed edges, each touching another computer; \c other lost its
    //! link
    AmbiguousSide,
    //! The link of \c side of \c computer to \c other covers rows in which another exposed edge of that side touches
    //! nothing: the pointer leaves there too
    EdgeInTheSameRows,
  };
  enum class Severity
  {
    Error,
    Warning
  };

  Kind kind;
  std::string computer;
  std::string other;
  Direction side = Direction::NoDirection;
  std::string detail;

  Severity severity() const;
  bool operator==(const Issue &) const = default;
};

//! A one-way link of the server configuration: \c side(srcStart,srcEnd) = to(dstStart,dstEnd), fractions of the sides
struct Link
{
  std::string from;
  Direction side = Direction::NoDirection;
  double srcStart = 0;
  double srcEnd = 1;
  std::string to;
  double dstStart = 0;
  double dstEnd = 1;

  bool operator==(const Link &) const = default;
};

//! A place where an exposed edge of one computer touches an exposed edge of another, in the arrangement
struct Contact
{
  std::string a;
  Direction sideA = Direction::NoDirection;
  std::string b;
  Direction sideB = Direction::NoDirection;
  //! The x of the touching edges for left and right, the y for top and bottom
  double line = 0;
  //! The stretch of the edge that touches
  double from = 0;
  double to = 0;
};

struct DerivedLinks
{
  //! In the order of the computers, then right, left, up, down, then along the edge
  std::vector<Link> links;
  //! Where the links are, for drawing: one for each stretch that touches, whichever way it is linked
  std::vector<Contact> contacts;
  //! The warnings of the derivation
  std::vector<Issue> issues;

  //! The links that leave \p from through \p side
  std::vector<Link> of(std::string_view from, Direction side) const;
};

//! The errors and warnings of \p arrangement that do not need the links; the errors are what makes it unusable
std::vector<Issue> validate(const Arrangement &arrangement);

//! The links where the exposed edges of computers touch
/*!
Computers or monitors that are not valid are left out. The warnings are those of \c validate() that need the links.
*/
DerivedLinks deriveLinks(const Arrangement &arrangement);

//! All issues of \p arrangement: what \c validate() says, and what the derivation says
std::vector<Issue> check(const Arrangement &arrangement);

//! A guide that is drawn when an edge of a computer that is dragged is in line with one of another
struct Guide
{
  //! A vertical line at x, or a horizontal one at y
  bool vertical = true;
  double position = 0;
  double from = 0;
  double to = 0;
  //! The edges touch (rather than being in line)
  bool touching = false;
};

struct SnapResult
{
  //! Where the origin of the computer is, once snapped
  double originX = 0;
  double originY = 0;
  std::vector<Guide> guides;
  //! The computer was pushed off a computer that it would have overlapped
  bool pushedOut = false;
  //! There is no place near that does not overlap another computer; the origin is where the drag began
  bool refused = false;
};

//! Where the computer \p moving ends up when it is dragged so that its origin is at \p originX, \p originY
/*!
The edges of its monitors snap to the edges of the monitors of the other computers that are within \p radius
(arrangement units): the ones that touch (left against right, top against bottom), the ones that are in line, and the
middles; the result never overlaps another computer.
*/
SnapResult snapMove(const Arrangement &arrangement, size_t moving, double originX, double originY, double radius);

//! A cell of the classic grid that has a computer; the origin of its computer is not used
struct GridCell
{
  int column = 0;
  int row = 0;
  Computer computer;
};

struct GridConversion
{
  Arrangement arrangement;
  //! Links that the classic grid has and the arrangement has not, for sizes that make them impossible
  std::vector<Link> notKept;
};

//! Place the computers of the classic grid in the arrangement
/*!
The server (\p serverName; the first cell when it has no cell) is placed first, then every computer touching the
computer before it that the grid has it next to, centred along the shared edge, sliding along it when that would overlap
another computer. The computers that the grid does not connect to the server are placed below the rest. The origin of
the server is 0,0.
*/
GridConversion arrangementFromGrid(const std::vector<GridCell> &cells, std::string_view serverName);

//! The range of a link as it is written in the server configuration: \c (0,38.7513), percentages of up to four decimals
/*!
Empty when the range is the whole side, as it is for the links of the classic grid.
*/
std::string formatInterval(double start, double end);

//! The \c links section of the server configuration for \p links, from \c section: links to \c end and the empty line
/*!
The same text, line for line, that the classic grid writes for the links it has: a computer, then its links, right,
left, up and down; with no range where the link is the whole side.
*/
std::string formatLinks(const Arrangement &arrangement, const DerivedLinks &links);

//! The name of \p side in the server configuration: left, right, up or down
const char *directionName(Direction side);

//! The factor of the display scale that a Windows block to paste has for its primary (or first) monitor, 1.5 for 150%
/*!
\c scale=150% is the scale of Windows; \c scale=2.0 (no percent sign) is the pixels per point of a Mac, which does not
change the units of a Mac, and gives nothing.
*/
std::optional<double> windowsDisplayScale(std::string_view diagnosticText);

//! The zoom for a Windows display scale: 100 / percent; 1 for a scale that is not sensible
double zoomForScale(double percent);

} // namespace deskflow::server::arrangement
