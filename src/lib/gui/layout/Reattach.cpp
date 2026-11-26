/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/Reattach.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>

namespace deskflow::gui::layout {

namespace {

using namespace deskflow::server::arrangement;

// how far, in arrangement units, a group slides along the side between two tries
constexpr double kSlideStep = 20.0;

bool isHorizontalSide(Direction side)
{
  return side == Direction::Left || side == Direction::Right;
}

// a contact between the server and one of \p members, as the side of the server it is on and how long it is
struct ServerContact
{
  size_t member = 0;
  Direction serverSide = Direction::NoDirection;
  double length = 0;
};

std::vector<ServerContact> serverContacts(
    const Arrangement &arrangement, const std::string &server, const std::vector<size_t> &members
)
{
  std::vector<ServerContact> found;
  for (const auto &contact : deriveLinks(arrangement).contacts) {
    const bool serverIsA = contact.a == server;
    const bool serverIsB = contact.b == server;
    if (serverIsA == serverIsB)
      continue;
    const auto &other = serverIsA ? contact.b : contact.a;
    const int index = arrangement.indexOf(other);
    if (index < 0 || std::ranges::find(members, static_cast<size_t>(index)) == members.end())
      continue;
    found.push_back({static_cast<size_t>(index), serverIsA ? contact.sideA : contact.sideB, contact.to - contact.from});
  }
  return found;
}

// whether a computer of \p members is in an error that the arrangement has: an overlap, a bad place
bool membersHaveErrors(const Arrangement &arrangement, const std::vector<size_t> &members)
{
  for (const auto &issue : validate(arrangement)) {
    if (issue.severity() != Issue::Severity::Error)
      continue;
    for (const auto member : members) {
      const auto &name = arrangement.computers[member].name;
      if (issue.computer == name || issue.other == name)
        return true;
    }
  }
  return false;
}

void translate(Arrangement &arrangement, const std::vector<size_t> &members, double dx, double dy)
{
  for (const auto member : members) {
    arrangement.computers[member].originX += dx;
    arrangement.computers[member].originY += dy;
  }
}

} // namespace

ReattachResult reattachToServer(const Arrangement &before, const std::vector<deskflow::server::MonitorRect> &newMonitors)
{
  ReattachResult result;
  result.arrangement = before;

  const int serverIndex = before.serverIndex();
  if (serverIndex < 0 || newMonitors.empty())
    return result;
  const auto server = static_cast<size_t>(serverIndex);
  if (before.computers[server].monitors == newMonitors)
    return result;

  const auto serverName = before.computers[server].name;
  const auto oldBox = before.computers[server].box();
  result.arrangement.computers[server].monitors = newMonitors;
  const auto newBox = result.arrangement.computers[server].box();
  if (oldBox.w <= 0 || oldBox.h <= 0 || newBox.w <= 0 || newBox.h <= 0)
    return result;

  // the computers that touch each other, as they touched before, are one group that moves as it is
  const auto count = before.computers.size();
  std::vector<size_t> parent(count);
  std::iota(parent.begin(), parent.end(), size_t(0));
  const auto root = [&](size_t i) {
    while (parent[i] != i)
      i = parent[i] = parent[parent[i]];
    return i;
  };
  for (const auto &contact : deriveLinks(before).contacts) {
    const int a = before.indexOf(contact.a);
    const int b = before.indexOf(contact.b);
    if (a < 0 || b < 0 || static_cast<size_t>(a) == server || static_cast<size_t>(b) == server)
      continue;
    const auto ra = root(static_cast<size_t>(a));
    const auto rb = root(static_cast<size_t>(b));
    if (ra != rb)
      parent[std::max(ra, rb)] = std::min(ra, rb);
  }

  std::vector<std::vector<size_t>> groups;
  for (size_t i = 0; i < count; ++i) {
    if (i == server)
      continue;
    const auto r = root(i);
    const auto existing = std::ranges::find_if(groups, [&](const std::vector<size_t> &g) { return root(g.front()) == r; });
    if (existing == groups.end())
      groups.push_back({i});
    else
      existing->push_back(i);
  }

  for (const auto &group : groups) {
    const auto touchedBefore = serverContacts(before, serverName, group);
    if (touchedBefore.empty() || !serverContacts(result.arrangement, serverName, group).empty())
      continue;

    // the computer with the longest contact is the one that is put against the server; the name decides a tie
    const auto anchorContact = *std::ranges::min_element(touchedBefore, [&](const ServerContact &x, const ServerContact &y) {
      if (std::abs(x.length - y.length) > 1e-9)
        return x.length > y.length;
      return before.computers[x.member].name < before.computers[y.member].name;
    });
    const auto &anchor = before.computers[anchorContact.member];
    const auto anchorBox = anchor.box();
    const auto side = anchorContact.serverSide;
    const bool horizontal = isHorizontalSide(side);

    // across the side: against the box of the new displays
    double dx = 0;
    double dy = 0;
    switch (side) {
    case Direction::Right:
      dx = newBox.right() - anchorBox.x;
      break;
    case Direction::Left:
      dx = newBox.x - anchorBox.right();
      break;
    case Direction::Top:
      dy = newBox.y - anchorBox.bottom();
      break;
    default:
      dy = newBox.bottom() - anchorBox.y;
      break;
    }
    // along the side: the middle of the computer at the same fraction of the side
    const double alongOld = horizontal ? anchorBox.centerY() : anchorBox.centerX();
    const double oldStart = horizontal ? oldBox.y : oldBox.x;
    const double oldLength = horizontal ? oldBox.h : oldBox.w;
    const double newStart = horizontal ? newBox.y : newBox.x;
    const double newLength = horizontal ? newBox.h : newBox.w;
    const double fraction = std::clamp((alongOld - oldStart) / oldLength, 0.0, 1.0);
    const double along = newStart + fraction * newLength - alongOld;
    (horizontal ? dy : dx) = along;

    // the place that was worked out, and then the places beside it along the side, the nearest first
    const double reach = newLength + (horizontal ? anchorBox.h : anchorBox.w);
    std::optional<Arrangement> placed;
    for (int step = 0; step * kSlideStep <= reach && !placed; ++step) {
      for (const int direction : {1, -1}) {
        if (step == 0 && direction == -1)
          continue;
        const double slide = step * kSlideStep * direction;
        auto candidate = result.arrangement;
        translate(candidate, group, dx + (horizontal ? 0 : slide), dy + (horizontal ? slide : 0));
        if (!membersHaveErrors(candidate, group) && !serverContacts(candidate, serverName, group).empty()) {
          placed = std::move(candidate);
          break;
        }
      }
    }

    if (placed) {
      result.arrangement = std::move(*placed);
      result.reattached.push_back(anchor.name);
      for (const auto member : group)
        result.moved.push_back(before.computers[member].name);
    } else {
      result.notReattached.push_back(anchor.name);
    }
  }
  return result;
}

} // namespace deskflow::gui::layout
