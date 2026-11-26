/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <functional>
#include <memory>

namespace deskflow::gui::input {

class MacInputFocusMonitor
{
public:
  explicit MacInputFocusMonitor(std::function<void()> changed);
  ~MacInputFocusMonitor();

  MacInputFocusMonitor(const MacInputFocusMonitor &) = delete;
  MacInputFocusMonitor &operator=(const MacInputFocusMonitor &) = delete;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::gui::input
