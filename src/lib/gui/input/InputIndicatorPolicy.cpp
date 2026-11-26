/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorPolicy.h"

namespace deskflow::gui::input {

const char *inputIndicatorVisibilityReasonText(
    InputIndicatorVisibilityReason reason
)
{
  switch (reason) {
  case InputIndicatorVisibilityReason::Shown:
    return "shown";
  case InputIndicatorVisibilityReason::Disabled:
    return "disabled";
  case InputIndicatorVisibilityReason::AccessibilityUnavailable:
    return "not-trusted";
  case InputIndicatorVisibilityReason::CoreDisconnected:
    return "core-disconnected";
  case InputIndicatorVisibilityReason::NotPrimary:
    return "not-primary";
  case InputIndicatorVisibilityReason::OffComputer:
    return "off-computer";
  case InputIndicatorVisibilityReason::ScreenUnknown:
    return "screen-unknown";
  case InputIndicatorVisibilityReason::FocusSecureOrProtected:
    return "secure-focus";
  case InputIndicatorVisibilityReason::SourceUnknown:
    return "unknown-source";
  case InputIndicatorVisibilityReason::OwnApplication:
    return "own-application";
  case InputIndicatorVisibilityReason::GenerationMismatch:
    return "generation-mismatch";
  case InputIndicatorVisibilityReason::EventRejected:
    return "stale-event";
  case InputIndicatorVisibilityReason::NoRecentActivity:
    return "no-activity";
  case InputIndicatorVisibilityReason::InvalidDwell:
    return "invalid-dwell";
  case InputIndicatorVisibilityReason::FrontmostChanged:
    return "frontmost-changed";
  case InputIndicatorVisibilityReason::PanelUnavailable:
    return "panel-unavailable";
  }
  return "unknown";
}

bool InputIndicatorDiagnosticThrottle::allow(
    std::chrono::steady_clock::time_point now
)
{
  if (m_last && now - *m_last < std::chrono::seconds{5}) {
    return false;
  }
  m_last = now;
  return true;
}

bool inputIndicatorTriggerRequiresActivity(InputIndicatorTrigger trigger)
{
  return trigger == InputIndicatorTrigger::CoreActivity;
}

std::optional<int64_t> macAccessibilityProbeTargetPid(
    std::optional<int64_t> frontmostApplicationPid,
    int64_t ownApplicationPid
)
{
  if (!frontmostApplicationPid ||
      *frontmostApplicationPid <= 0 ||
      ownApplicationPid <= 0 ||
      *frontmostApplicationPid == ownApplicationPid) {
    return std::nullopt;
  }
  return frontmostApplicationPid;
}

InputIndicatorVisibilityDecision InputIndicatorPolicy::decidePersistent(
    const PersistentInputIndicatorPolicyInput &input
)
{
  if (!input.enabled) {
    return {.reason = InputIndicatorVisibilityReason::Disabled};
  }
  if (!input.isOnComputer) {
    return {.reason = InputIndicatorVisibilityReason::OffComputer};
  }
  if (input.source == InputIndicatorSource::Unknown) {
    return {.reason = InputIndicatorVisibilityReason::SourceUnknown};
  }
  return {
      .show = true,
      .source = input.source,
      .reason = InputIndicatorVisibilityReason::Shown,
  };
}

InputIndicatorVisibilityDecision
InputIndicatorPolicy::decide(const InputIndicatorPolicyInput &input, std::chrono::milliseconds dwell)
{
  if (!input.enabled) {
    return {.reason = InputIndicatorVisibilityReason::Disabled};
  }
  if (!input.accessibilityTrusted) {
    return {.reason = InputIndicatorVisibilityReason::AccessibilityUnavailable};
  }
  if (!input.coreConnected) {
    return {.reason = InputIndicatorVisibilityReason::CoreDisconnected};
  }
  if (!input.isPrimary) {
    return {.reason = InputIndicatorVisibilityReason::NotPrimary};
  }
  if (!input.isOnComputer) {
    return {.reason = InputIndicatorVisibilityReason::OffComputer};
  }
  if (!input.screenKnown) {
    return {.reason = InputIndicatorVisibilityReason::ScreenUnknown};
  }
  if (input.focus == EditableFocusState::SecureOrProtected) {
    return {
        .reason = InputIndicatorVisibilityReason::FocusSecureOrProtected
    };
  }
  if (input.source == InputIndicatorSource::Unknown) {
    return {.reason = InputIndicatorVisibilityReason::SourceUnknown};
  }
  if (input.probeGeneration != input.activityGeneration) {
    return {
        .reason = InputIndicatorVisibilityReason::GenerationMismatch
    };
  }
  if (inputIndicatorTriggerRequiresActivity(input.trigger)) {
    if (!input.eventAge) {
      return {
          .reason = InputIndicatorVisibilityReason::NoRecentActivity
      };
    }
    if (*input.eventAge < std::chrono::milliseconds{0} ||
        input.maximumEventAge <= std::chrono::milliseconds{0} ||
        *input.eventAge > input.maximumEventAge) {
      return {.reason = InputIndicatorVisibilityReason::EventRejected};
    }
  }
  if (dwell <= std::chrono::milliseconds{0}) {
    return {.reason = InputIndicatorVisibilityReason::InvalidDwell};
  }

  return {
      .show = true,
      .source = input.source,
      .dwell = dwell,
      .reason = InputIndicatorVisibilityReason::Shown,
  };
}

} // namespace deskflow::gui::input
