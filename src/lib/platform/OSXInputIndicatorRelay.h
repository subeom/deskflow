/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ipc/InputIndicatorIpc.h"

#include <functional>
#include <memory>

namespace deskflow::platform {

class OSXInputIndicatorRelay
{
public:
  using Work = std::function<void()>;
  using Scheduler = std::function<void(Work)>;
  using Sender = std::function<void(const deskflow::input::InputIndicatorActivity &)>;

  OSXInputIndicatorRelay(Scheduler schedule, Sender send);
  ~OSXInputIndicatorRelay();

  OSXInputIndicatorRelay(const OSXInputIndicatorRelay &) = delete;
  OSXInputIndicatorRelay &operator=(const OSXInputIndicatorRelay &) = delete;

  void notify(
      deskflow::input::InputIndicatorActivityKind kind, uint64_t eventTimestampMach, bool isPrimary, bool isOnComputer
  );
  void invalidate();

private:
  struct State;
  std::shared_ptr<State> m_state;
};

} // namespace deskflow::platform
