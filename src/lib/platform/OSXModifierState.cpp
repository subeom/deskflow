/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXModifierState.h"

OSXModifierState::OSXModifierState(uint32_t leftKeyCode, uint32_t rightKeyCode)
{
  configure(leftKeyCode, rightKeyCode);
}

void OSXModifierState::configure(uint32_t leftKeyCode, uint32_t rightKeyCode)
{
  m_leftKeyCode = leftKeyCode;
  m_rightKeyCode = rightKeyCode;
  m_leftDown = false;
  m_rightDown = false;
}

void OSXModifierState::setDown(uint32_t keyCode, bool down)
{
  if (keyCode == m_leftKeyCode) {
    m_leftDown = down;
  } else if (keyCode == m_rightKeyCode) {
    m_rightDown = down;
  }
}

std::optional<OSXModifierTransition> OSXModifierState::update(uint32_t keyCode, bool down)
{
  if (keyCode == m_leftKeyCode) {
    if (m_leftDown == down) {
      return std::nullopt;
    }
    m_leftDown = down;
    return OSXModifierTransition{keyCode, kKeySuper_L, down};
  }

  if (keyCode == m_rightKeyCode) {
    if (m_rightDown == down) {
      return std::nullopt;
    }
    m_rightDown = down;
    return OSXModifierTransition{keyCode, kKeySuper_R, down};
  }

  return std::nullopt;
}

bool OSXModifierState::handles(uint32_t keyCode) const
{
  return keyCode == m_leftKeyCode || keyCode == m_rightKeyCode;
}

bool OSXModifierState::leftDown() const
{
  return m_leftDown;
}

bool OSXModifierState::rightDown() const
{
  return m_rightDown;
}
