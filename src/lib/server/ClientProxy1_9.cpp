/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "base/Log.h"
#include "common/ClipboardCapabilities.h"
#include "common/PlatformInfo.h"
#include "deskflow/ClipboardIdPolicy.h"
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
  ProtocolUtil::writef(
      getStream(), kMsgDInputCapabilities, static_cast<uint8_t>(local.platform),
      local.capabilities | kClipboardCapabilityPaced | kClipboardCapabilitySingleId
  );
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
  if (id == kKeyF18) {
    const auto policy = inputPolicy();
    if (policy.mapsF18ToHangul()) {
      // a misreported second press of the key that is still held must not toggle the input mode again
      if (m_f18HangulButtons.insert(button).second) {
        const auto wire = policy.toWireKey(id, mask);
        LOG_DEBUG("send F18 as Hangul to \"%s\"", getName().c_str());
        ClientProxy1_8::keyDown(wire.id, wire.mask, button, language);
      }
      return;
    }
  }
  ClientProxy1_8::keyDown(id, mask, button, language);
}

void ClientProxy1_9::keyRepeat(
    KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &language
)
{
  // the key was sent as Hangul, which must toggle the input mode once however long F18 is held
  if (m_f18HangulButtons.contains(button) || inputPolicy().shouldSuppressRepeat(id)) {
    return;
  }
  ClientProxy1_8::keyRepeat(id, mask, count, button, language);
}

void ClientProxy1_9::keyUp(KeyID id, KeyModifierMask mask, KeyButton button)
{
  // The release follows how the press was sent, whatever the options say by now. A macOS source releases a key with
  // kKeyNone ("the id isn't used, we just need the same button"), so the held press is found by its button alone.
  if (m_f18HangulButtons.erase(button) > 0) {
    ClientProxy1_8::keyUp(kKeyHangul, mask, button);
    return;
  }
  ClientProxy1_8::keyUp(id, mask, button);
}

bool ClientProxy1_9::leave()
{
  // the destination releases every key when the cursor leaves it
  m_f18HangulButtons.clear();
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
  m_f18Action = F18Action::PassThrough;
  m_invertScrollX = false;
  m_invertScrollY = false;
  m_scrollInvertScope = ScrollInvertScope::All;
  m_semanticMedia = false;
  ClientProxy1_8::resetOptions();
}

void ClientProxy1_9::setOptions(const OptionsList &options)
{
  for (size_t i = 0; i + 1 < options.size(); i += 2) {
    if (options[i] == kOptionCrossOSF18Key) {
      m_f18Action =
          options[i + 1] == static_cast<OptionValue>(F18Action::Hangul) ? F18Action::Hangul : F18Action::PassThrough;
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

bool ClientProxy1_9::shouldPaceClipboard() const
{
  return m_receivedInputCapabilities;
}

bool ClientProxy1_9::sendsClipboardId(ClipboardID id) const
{
  // the receivers of Windows and macOS ignore the id, so only what is known about the peer drops the selection
  return !m_receivedInputCapabilities || deskflow::clipboard::exchangesId(m_destinationInputCapabilities.platform, id);
}

bool ClientProxy1_9::mirrorsSelection() const
{
  return m_receivedInputCapabilities && (m_destinationInputCapabilities.capabilities & kClipboardCapabilitySingleId) &&
         !deskflow::clipboard::exchangesId(m_destinationInputCapabilities.platform, kClipboardSelection);
}

bool ClientProxy1_9::keepsClipboardWhenGrabbed() const
{
  return m_receivedInputCapabilities && m_destinationInputCapabilities.platform == InputPlatform::MacOS;
}

bool ClientProxy1_9::isHandshakeCompleteReady() const
{
  return m_receivedInputCapabilities;
}

InputCapabilities ClientProxy1_9::localInputCapabilities() const
{
  return deskflow::platform::currentInputCapabilities();
}

CrossPlatformInputPolicy ClientProxy1_9::inputPolicy() const
{
  return {
      localInputCapabilities(), m_destinationInputCapabilities,
      CrossPlatformInputOptions{
          .f18Action = m_f18Action,
          .invertScrollX = m_invertScrollX,
          .invertScrollY = m_invertScrollY,
          .scrollInvertScope = m_scrollInvertScope,
          .semanticMedia = m_semanticMedia
      }
  };
}
