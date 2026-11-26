/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXKeyGroupResolver.h"

#include <algorithm>

void OSXKeyGroupResolver::setGroups(std::vector<Group> groups)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_groups = std::move(groups);
  m_lastGroup = -1;
  m_reported.clear();
}

OSXKeyGroupResolver::Result OSXKeyGroupResolver::resolve(const std::string &layoutId, const IdProvider &asciiLayoutId)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  const auto find = [this](const std::string &id) {
    const auto it = std::ranges::find(m_groups, id, &Group::id);
    return it == m_groups.end() ? -1 : static_cast<int32_t>(std::distance(m_groups.begin(), it));
  };

  Result result;
  const auto exact = layoutId.empty() ? -1 : find(layoutId);
  if (exact >= 0) {
    m_lastGroup = exact;
    result.group = exact;
    result.match = Match::Exact;
    return result;
  }

  const auto asciiId = asciiLayoutId ? asciiLayoutId() : std::string();
  const auto viaAscii = asciiId.empty() ? -1 : find(asciiId);
  if (viaAscii >= 0) {
    m_lastGroup = viaAscii;
    result.group = viaAscii;
    result.match = Match::AsciiLayout;
  } else if (layoutId.empty() && asciiId.empty() && m_lastGroup >= 0) {
    result.group = m_lastGroup;
    result.match = Match::Previous;
  } else {
    // group 0 is not necessarily a group the key map has characters for
    const auto it = std::ranges::find(m_groups, true, &Group::hasLayout);
    result.group = it == m_groups.end() ? 0 : static_cast<int32_t>(std::distance(m_groups.begin(), it));
    result.match = Match::Fallback;
  }

  result.firstSighting = m_reported.insert(layoutId + '\n' + asciiId).second;
  return result;
}
