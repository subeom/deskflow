/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <vector>

//! Maps the active macOS keyboard layout to a keyboard group index
/*!
Pure bookkeeping, no Text Input Sources calls, so the decision logic can be
tested without live input state.

The groups are the enabled keyboard input sources, in the order the key map
was built from.  A keyboard layout that an input method (for example Korean
2-Set) runs on top of is usually not one of them: macOS reports e.g.
"com.apple.keylayout.2SetHangul" as the current layout while only the input
mode "com.apple.inputmethod.Korean.2SetKorean" is enabled.  Such a layout is
resolved through the ASCII capable layout instead of silently using group 0,
and each distinct unresolved combination is reported once.
*/
class OSXKeyGroupResolver
{
public:
  struct Group
  {
    std::string id;
    //! Whether the source has key layout data, i.e. the key map has characters for it
    bool hasLayout = false;
  };

  enum class Match
  {
    Exact,       //!< current layout is an enabled group
    AsciiLayout, //!< current layout is not a group, its ASCII capable layout is
    Previous,    //!< nothing could be read, the last resolved group is kept
    Fallback     //!< nothing matched, first group with a key layout is used
  };

  struct Result
  {
    int32_t group = 0;
    Match match = Match::Fallback;
    //! True the first time this outcome is seen for a group list; callers log it once
    bool firstSighting = false;
  };

  using IdProvider = std::function<std::string()>;

  //! Replace the group list; forgets the last group and what was already reported
  void setGroups(std::vector<Group> groups);

  //! Resolve the group for the current layout id
  /*!
  \p layoutId is the id of the current keyboard layout (empty if it could not
  be read).  \p asciiLayoutId is only called when \p layoutId is not a group.
  */
  Result resolve(const std::string &layoutId, const IdProvider &asciiLayoutId);

private:
  std::mutex m_mutex;
  std::vector<Group> m_groups;
  int32_t m_lastGroup = -1;
  std::set<std::string> m_reported;
};
