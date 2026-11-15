/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/KeyTypes.h"

#include <Carbon/Carbon.h>

#include <cstdint>
#include <optional>

struct OSXModifierTransition
{
  uint32_t keyCode = 0;
  KeyID id = kKeyNone;
  bool down = false;
};

class OSXModifierState
{
public:
  OSXModifierState() = default;
  OSXModifierState(uint32_t leftKeyCode, uint32_t rightKeyCode);

  void configure(uint32_t leftKeyCode, uint32_t rightKeyCode);
  void setDown(uint32_t keyCode, bool down);
  std::optional<OSXModifierTransition> update(uint32_t keyCode, bool down);

  bool handles(uint32_t keyCode) const;
  bool leftDown() const;
  bool rightDown() const;

  static KeyButton leftCommandButton()
  {
    return static_cast<KeyButton>(kVK_Command + kKeyButtonOffset);
  }

  static KeyButton rightCommandButton()
  {
    return static_cast<KeyButton>(kVK_RightCommand + kKeyButtonOffset);
  }

  static KeyButton legacyCommandButton(KeyButton button)
  {
    return button == rightCommandButton() ? leftCommandButton() : button;
  }

private:
  uint32_t m_leftKeyCode = 0;
  uint32_t m_rightKeyCode = 0;
  bool m_leftDown = false;
  bool m_rightDown = false;

  static constexpr KeyButton kKeyButtonOffset = 1;
};
