/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClipboardReadCache.h"

#include <algorithm>

namespace deskflow::clipboard {

const std::string *ClipboardReadCache::find(int format) const
{
  const auto entry = std::ranges::find(m_entries, format, &Entry::format);
  return entry == m_entries.end() ? nullptr : &entry->data;
}

void ClipboardReadCache::store(int format, std::string data)
{
  // what is stored for the format is out of date
  if (const auto old = std::ranges::find(m_entries, format, &Entry::format); old != m_entries.end()) {
    m_bytes -= old->data.size();
    m_entries.erase(old);
  }

  if (data.size() > m_budget) {
    return;
  }

  while (!m_entries.empty() && m_bytes + data.size() > m_budget) {
    m_bytes -= m_entries.front().data.size();
    m_entries.pop_front();
  }
  m_bytes += data.size();
  m_entries.push_back({format, std::move(data)});
}

void ClipboardReadCache::invalidate()
{
  m_entries.clear();
  m_bytes = 0;
}

} // namespace deskflow::clipboard
