/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy1_9.h"

#include "client/Client.h"
#include "common/ClipboardCapabilities.h"
#include "deskflow/ClipboardIdPolicy.h"
#include "deskflow/OptionTypes.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <cstring>

ServerProxy1_9::ServerProxy1_9(Client *client, deskflow::IStream *stream, IEventQueue *events)
    : ServerProxy1_8(client, stream, events),
      m_client(client)
{
}

ServerProxy::ConnectionResult ServerProxy1_9::parseHandshakeMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDInputCapabilities, 4) == 0) {
    return receiveInputCapabilities() ? ConnectionResult::Okay : ConnectionResult::Unknown;
  }
  return ServerProxy1_8::parseHandshakeMessage(code);
}

ServerProxy::ConnectionResult ServerProxy1_9::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDInputCapabilities, 4) == 0) {
    return receiveInputCapabilities() ? ConnectionResult::Okay : ConnectionResult::Unknown;
  }
  return ServerProxy1_8::parseMessage(code);
}

bool ServerProxy1_9::receiveInputCapabilities()
{
  uint8_t platform = 0;
  InputCapabilityMask capabilities = 0;
  if (!ProtocolUtil::readf(getStream(), kMsgDInputCapabilities + 4, &platform, &capabilities)) {
    return false;
  }

  const auto parsedPlatform = static_cast<InputPlatform>(platform);
  m_sourceInputCapabilities = {
      isKnownInputPlatform(parsedPlatform) ? parsedPlatform : InputPlatform::Unknown, capabilities
  };
  m_receivedSourceCapabilities = true;
  sendInputCapabilities();
  return true;
}

void ServerProxy1_9::onOptionsChanged(const OptionsList &options)
{
  m_invertScrollX = false;
  m_invertScrollY = false;
  m_scrollInvertScope = ScrollInvertScope::All;
  m_semanticMedia = false;
  for (size_t i = 0; i + 1 < options.size(); i += 2) {
    if (options[i] == kOptionCrossOSInvertScrollX) {
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
}

bool ServerProxy1_9::handleSemanticMediaKey(KeyID id)
{
  return inputPolicy().shouldSynthesizeSemanticMedia(id) && synthesizeSemanticMediaKey(id);
}

bool ServerProxy1_9::shouldSuppressKeyRepeat(KeyID id, KeyModifierMask) const
{
  return inputPolicy().shouldSuppressRepeat(id);
}

void ServerProxy1_9::transformMouseWheel(int32_t &xDelta, int32_t &yDelta)
{
  const auto transformed = inputPolicy().toDestinationScroll({xDelta, yDelta});
  xDelta = transformed.x;
  yDelta = transformed.y;
}

bool ServerProxy1_9::shouldPaceClipboard() const
{
  return m_receivedSourceCapabilities;
}

bool ServerProxy1_9::dataIsProofOfLife() const
{
  return m_receivedSourceCapabilities && (m_sourceInputCapabilities.capabilities & kClipboardCapabilityPaced) != 0;
}

bool ServerProxy1_9::sendsClipboardId(ClipboardID id) const
{
  // The selection of a computer on X11 is its own, and the server may pass it on to another computer on X11: it is only
  // left out when both computers are Windows or macOS, where it is the same clipboard that the server stores as both
  return !m_receivedSourceCapabilities ||
         (m_sourceInputCapabilities.capabilities & kClipboardCapabilitySingleId) == 0 ||
         deskflow::clipboard::exchangesId(m_sourceInputCapabilities.platform, id) ||
         deskflow::clipboard::exchangesId(localInputCapabilities().platform, id);
}

bool ServerProxy1_9::synthesizeSemanticMediaKey(KeyID id)
{
  return m_client->semanticMediaKey(id);
}

InputCapabilities ServerProxy1_9::localInputCapabilities() const
{
  return m_client->inputCapabilities();
}

void ServerProxy1_9::sendInputCapabilities() const
{
  const auto local = localInputCapabilities();
  ProtocolUtil::writef(
      getStream(), kMsgCInputCapabilities, static_cast<uint8_t>(local.platform),
      local.capabilities | kClipboardCapabilityPaced | kClipboardCapabilitySingleId
  );
}

CrossPlatformInputPolicy ServerProxy1_9::inputPolicy() const
{
  return {
      m_sourceInputCapabilities, localInputCapabilities(),
      CrossPlatformInputOptions{
          .invertScrollX = m_invertScrollX,
          .invertScrollY = m_invertScrollY,
          .scrollInvertScope = m_scrollInvertScope,
          .semanticMedia = m_semanticMedia
      }
  };
}
