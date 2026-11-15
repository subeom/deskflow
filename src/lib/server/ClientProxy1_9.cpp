/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "common/PlatformInfo.h"
#include "deskflow/OptionTypes.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <cstring>

ClientProxy1_9::ClientProxy1_9(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_8(name, adoptedStream, server, events)
{
  sendInputCapabilities();
}

bool ClientProxy1_9::parseHandshakeMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgCInputCapabilities, 4) == 0) {
    return receiveInputCapabilities();
  }
  return ClientProxy1_8::parseHandshakeMessage(code);
}

bool ClientProxy1_9::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgCInputCapabilities, 4) == 0) {
    return receiveInputCapabilities();
  }
  return ClientProxy1_8::parseMessage(code);
}

void ClientProxy1_9::sendInputCapabilities() const
{
  const auto local = deskflow::platform::currentInputCapabilities();
  ProtocolUtil::writef(getStream(), kMsgDInputCapabilities, static_cast<uint8_t>(local.platform), local.capabilities);
}

bool ClientProxy1_9::receiveInputCapabilities()
{
  uint8_t platform = 0;
  InputCapabilityMask capabilities = 0;
  if (!ProtocolUtil::readf(getStream(), kMsgCInputCapabilities + 4, &platform, &capabilities)) {
    return false;
  }

  const auto parsedPlatform = static_cast<InputPlatform>(platform);
  m_destinationInputCapabilities = {
      isKnownInputPlatform(parsedPlatform) ? parsedPlatform : InputPlatform::Unknown, capabilities
  };
  m_receivedInputCapabilities = true;
  maybeCompleteHandshake();
  return true;
}

void ClientProxy1_9::keyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &language)
{
  if (id == kKeySuper_L) {
    m_leftSuperDown = true;
  } else if (id == kKeySuper_R) {
    m_rightSuperDown = true;
    m_rightSuperMapped = inputPolicy().mapsRightCommandToHangul();
  }

  InputKeyEvent key{id, mask};
  if (!(id == kKeySuper_R && m_rightSuperMapped)) {
    if (!prepareLegacySuperDown(key.id, button)) {
      return;
    }
    key.mask = modifierMaskForWire(id, mask);
  } else {
    key = inputPolicy().toWireKey(id, mask);
  }
  ProtocolUtil::writef(getStream(), kMsgDKeyDown, key.id, key.mask, button, &language);
}

void ClientProxy1_9::keyRepeat(
    KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &language
)
{
  if ((id == kKeySuper_R && m_rightSuperMapped) || inputPolicy().shouldSynthesizeSemanticMedia(id)) {
    return;
  }

  InputKeyEvent key{id, mask};
  if (!prepareLegacySuperRepeat(key.id)) {
    return;
  }
  key.mask = modifierMaskForWire(id, key.mask);
  ProtocolUtil::writef(getStream(), kMsgDKeyRepeat, key.id, key.mask, count, button, &language);
}

void ClientProxy1_9::keyUp(KeyID id, KeyModifierMask mask, KeyButton button)
{
  InputKeyEvent key{id, mask};
  const auto wireMask = modifierMaskForWire(id, key.mask);
  const bool rightWasMapped = m_rightSuperMapped;

  if (id == kKeySuper_L) {
    m_leftSuperDown = false;
  } else if (id == kKeySuper_R) {
    m_rightSuperDown = false;
    m_rightSuperMapped = false;
  }

  if (!(id == kKeySuper_R && rightWasMapped)) {
    if (!prepareLegacySuperUp(key.id, button)) {
      return;
    }
  }

  ProtocolUtil::writef(getStream(), kMsgDKeyUp, key.id, wireMask, button);
}

bool ClientProxy1_9::leave()
{
  m_leftSuperDown = false;
  m_rightSuperDown = false;
  m_rightSuperMapped = false;
  return ClientProxy1_8::leave();
}

void ClientProxy1_9::mouseWheel(int32_t xDelta, int32_t yDelta, ScrollSource source)
{
  // The default scope, trackpad and unknown events never reach the policy: they take the legacy path
  // untouched and the destination applies the inversion itself.
  if (m_scrollInvertScope != ScrollInvertScope::MouseWheelOnly || source != ScrollSource::Mouse) {
    ClientProxy1_8::mouseWheel(xDelta, yDelta);
    return;
  }

  const auto delta = inputPolicy().toWireScroll({xDelta, yDelta}, source);
  ClientProxy1_8::mouseWheel(delta.x, delta.y);
}

void ClientProxy1_9::resetOptions()
{
  m_rightCommandAction = RightCommandAction::Preserve;
  m_invertScrollX = false;
  m_invertScrollY = false;
  m_scrollInvertScope = ScrollInvertScope::All;
  m_semanticMedia = false;
  ClientProxy1_8::resetOptions();
}

void ClientProxy1_9::setOptions(const OptionsList &options)
{
  for (size_t i = 0; i + 1 < options.size(); i += 2) {
    if (options[i] == kOptionCrossOSRightCommand) {
      const auto value = options[i + 1];
      m_rightCommandAction = value == static_cast<OptionValue>(RightCommandAction::Hangul)
                                 ? RightCommandAction::Hangul
                                 : RightCommandAction::Preserve;
    } else if (options[i] == kOptionCrossOSInvertScrollX) {
      m_invertScrollX = options[i + 1] != 0;
    } else if (options[i] == kOptionCrossOSInvertScrollY) {
      m_invertScrollY = options[i + 1] != 0;
    } else if (options[i] == kOptionCrossOSInvertScope) {
      m_scrollInvertScope = options[i + 1] == static_cast<OptionValue>(ScrollInvertScope::MouseWheelOnly)
                                ? ScrollInvertScope::MouseWheelOnly
                                : ScrollInvertScope::All;
    } else if (options[i] == kOptionCrossOSSemanticMedia) {
      m_semanticMedia = options[i + 1] != 0;
    }
  }
  // With MouseWheelOnly this source applies the inversion (to mouse wheel events only), so the destination must
  // not be told to invert: a destination that predates the scope would invert every packet, trackpad included.
  if (m_scrollInvertScope == ScrollInvertScope::MouseWheelOnly && options.size() % 2 == 0) {
    OptionsList forwarded;
    forwarded.reserve(options.size());
    for (size_t i = 0; i < options.size(); i += 2) {
      if (options[i] != kOptionCrossOSInvertScrollX && options[i] != kOptionCrossOSInvertScrollY) {
        forwarded.push_back(options[i]);
        forwarded.push_back(options[i + 1]);
      }
    }
    ClientProxy1_8::setOptions(forwarded);
    return;
  }
  ClientProxy1_8::setOptions(options);
}

bool ClientProxy1_9::isHandshakeCompleteReady() const
{
  return m_receivedInputCapabilities;
}

InputCapabilities ClientProxy1_9::localInputCapabilities() const
{
  return deskflow::platform::currentInputCapabilities();
}

InputPlatform ClientProxy1_9::legacyInputPlatform() const
{
  return localInputCapabilities().platform;
}

CrossPlatformInputPolicy ClientProxy1_9::inputPolicy() const
{
  return {
      localInputCapabilities(), m_destinationInputCapabilities,
      CrossPlatformInputOptions{
          .rightCommand = m_rightCommandAction,
          .invertScrollX = m_invertScrollX,
          .invertScrollY = m_invertScrollY,
          .scrollInvertScope = m_scrollInvertScope,
          .semanticMedia = m_semanticMedia
      }
  };
}

KeyModifierMask ClientProxy1_9::modifierMaskForWire(KeyID id, KeyModifierMask mask) const
{
  if (id != kKeySuper_R && m_rightSuperMapped && m_rightSuperDown && !m_leftSuperDown) {
    mask &= ~KeyModifierSuper;
  }
  return mask;
}
