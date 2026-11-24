/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <functional>

namespace deskflow::gui::input {

struct MacInputPointerMonitorHandle
{
  bool complete = false;
  std::function<void()> stop;
};

MacInputPointerMonitorHandle startMacInputPointerMonitor(
    std::function<void()> changed
);

} // namespace deskflow::gui::input
