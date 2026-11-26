/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ipc/InputIndicatorIpc.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace deskflow::platform {

inline constexpr std::string_view kMacABCInputSourceId = "com.apple.keylayout.ABC";
inline constexpr std::string_view kMacKorean2SetInputSourceId = "com.apple.inputmethod.Korean.2SetKorean";

struct OSXInputSourceSnapshot
{
  std::string identifier;
  deskflow::input::InputIndicatorSource source = deskflow::input::InputIndicatorSource::Unknown;
};

deskflow::input::InputIndicatorSource classifyMacInputSourceId(std::string_view identifier);
bool shouldActivateMacInputSourceTransition(
    deskflow::input::InputIndicatorSource previous,
    deskflow::input::InputIndicatorSource current
);
OSXInputSourceSnapshot makeMacInputSourceSnapshot(std::string identifier);
OSXInputSourceSnapshot copyCurrentMacInputSource();

class OSXInputSourceMonitor
{
public:
  explicit OSXInputSourceMonitor(std::function<void()> changed);
  ~OSXInputSourceMonitor();

  OSXInputSourceMonitor(const OSXInputSourceMonitor &) = delete;
  OSXInputSourceMonitor &operator=(const OSXInputSourceMonitor &) = delete;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::platform
