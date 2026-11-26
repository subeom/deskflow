/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstddef>
#include <deque>
#include <string>

namespace deskflow::clipboard {

//! What was read from a platform clipboard for each format, kept until the clipboard changes
/*!
Reading a format can be costly (a platform may have to convert an image to a bitmap) and the server reads the clipboard
of its own computer every time the cursor leaves it, so the result is kept for as long as the clipboard has not
changed.  The owner calls invalidate() when it has.  The cache holds at most a budget of bytes, and drops the oldest
entries first.
*/
class ClipboardReadCache
{
public:
  static constexpr size_t kDefaultBudgetBytes = 8 * 1024 * 1024;

  explicit ClipboardReadCache(size_t budgetBytes = kDefaultBudgetBytes) : m_budget(budgetBytes)
  {
  }

  //! What was stored for \p format, or nothing
  const std::string *find(int format) const;

  //! Keeps \p data for \p format; data that does not fit the budget is not kept
  void store(int format, std::string data);

  //! Forgets everything: the clipboard changed
  void invalidate();

private:
  struct Entry
  {
    int format;
    std::string data;
  };

  size_t m_budget;
  size_t m_bytes = 0;
  std::deque<Entry> m_entries;
};

} // namespace deskflow::clipboard
