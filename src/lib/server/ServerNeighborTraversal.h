/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <optional>
#include <set>
#include <string>
#include <utility>

namespace deskflow::server {

struct NeighborPosition
{
  std::string name;
  float position = 0.0f;
};

enum class NeighborTraversalStatus
{
  Connected,
  NoNeighbor,
  Cycle
};

struct NeighborTraversalResult
{
  NeighborTraversalStatus status = NeighborTraversalStatus::NoNeighbor;
  NeighborPosition destination;
};

template <typename LookupNeighbor, typename IsConnected, typename OnSkipped>
NeighborTraversalResult traverseConnectedNeighbor(
    const std::string &source, float position, LookupNeighbor lookup, IsConnected isConnected, OnSkipped onSkipped
)
{
  std::set<std::string> visited{source};
  auto current = source;

  for (;;) {
    auto next = lookup(current, position);
    if (!next) {
      return {NeighborTraversalStatus::NoNeighbor, {}};
    }
    if (!visited.insert(next->name).second) {
      return {NeighborTraversalStatus::Cycle, *next};
    }
    if (isConnected(next->name)) {
      return {NeighborTraversalStatus::Connected, *next};
    }

    onSkipped(current, *next);
    current = next->name;
    position = next->position;
  }
}

} // namespace deskflow::server
