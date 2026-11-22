/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ipc/InputIndicatorIpc.h"

#include <chrono>
#include <cstdint>

namespace deskflow::gui::input {

using deskflow::input::InputIndicatorSource;

enum class EditableFocusState
{
  SafeEditable,
  SecureOrProtected,
  NotEditable,
  Unknown
};

struct InputIndicatorPolicyInput
{
  bool enabled = false;
  bool accessibilityTrusted = false;
  bool coreConnected = false;
  bool isPrimary = false;
  bool isOnComputer = false;
  bool screenKnown = false;
  EditableFocusState focus = EditableFocusState::Unknown;
  InputIndicatorSource source = InputIndicatorSource::Unknown;
  uint64_t probeGeneration = 0;
  uint64_t activityGeneration = 0;
  std::chrono::milliseconds eventAge{0};
  std::chrono::milliseconds maximumEventAge{300};
};

struct InputIndicatorVisibilityDecision
{
  bool show = false;
  InputIndicatorSource source = InputIndicatorSource::Unknown;
  std::chrono::milliseconds dwell{0};
};

class InputIndicatorPolicy
{
public:
  static InputIndicatorVisibilityDecision
  decide(const InputIndicatorPolicyInput &input, std::chrono::milliseconds dwell);
};

} // namespace deskflow::gui::input
