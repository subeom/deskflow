/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorPolicy.h"

namespace deskflow::gui::input {

InputIndicatorVisibilityDecision
InputIndicatorPolicy::decide(const InputIndicatorPolicyInput &input, std::chrono::milliseconds dwell)
{
  if (!input.enabled || !input.accessibilityTrusted || !input.coreConnected || !input.isPrimary ||
      !input.isOnComputer || !input.screenKnown || input.focus != EditableFocusState::SafeEditable ||
      input.source == InputIndicatorSource::Unknown || input.probeGeneration != input.activityGeneration ||
      input.eventAge < std::chrono::milliseconds{0} || input.maximumEventAge <= std::chrono::milliseconds{0} ||
      input.eventAge > input.maximumEventAge || dwell <= std::chrono::milliseconds{0}) {
    return {};
  }

  return {.show = true, .source = input.source, .dwell = dwell};
}

} // namespace deskflow::gui::input
