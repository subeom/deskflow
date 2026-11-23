/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ipc/InputIndicatorIpc.h"

#include <chrono>
#include <cstdint>
#include <optional>

namespace deskflow::gui::input {

using deskflow::input::InputIndicatorSource;

enum class EditableFocusState
{
  SafeEditable,
  SecureOrProtected,
  NotEditable,
  Unknown
};

enum class InputIndicatorTrigger
{
  CoreActivity,
  FocusChanged,
  SourceChanged
};

enum class InputIndicatorVisibilityReason
{
  Shown,
  Disabled,
  AccessibilityUnavailable,
  CoreDisconnected,
  NotPrimary,
  OffComputer,
  ScreenUnknown,
  FocusSecureOrProtected,
  FocusNotEditable,
  FocusUnknown,
  SourceUnknown,
  GenerationMismatch,
  EventRejected,
  NoRecentActivity,
  InvalidDwell,
  FrontmostChanged,
  PanelUnavailable
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
  InputIndicatorTrigger trigger = InputIndicatorTrigger::CoreActivity;
  uint64_t probeGeneration = 0;
  uint64_t activityGeneration = 0;
  std::optional<std::chrono::milliseconds> eventAge;
  std::chrono::milliseconds maximumEventAge{
      deskflow::input::kInputIndicatorMaximumEventAgeMilliseconds
  };
};

struct InputIndicatorVisibilityDecision
{
  bool show = false;
  InputIndicatorSource source = InputIndicatorSource::Unknown;
  std::chrono::milliseconds dwell{0};
  InputIndicatorVisibilityReason reason =
      InputIndicatorVisibilityReason::Disabled;
};

const char *inputIndicatorVisibilityReasonText(
    InputIndicatorVisibilityReason reason
);
bool inputIndicatorTriggerRequiresActivity(InputIndicatorTrigger trigger);

class InputIndicatorDiagnosticThrottle
{
public:
  bool allow(std::chrono::steady_clock::time_point now);

private:
  std::optional<std::chrono::steady_clock::time_point> m_last;
};

class InputIndicatorPolicy
{
public:
  static InputIndicatorVisibilityDecision
  decide(const InputIndicatorPolicyInput &input, std::chrono::milliseconds dwell);
};

} // namespace deskflow::gui::input
