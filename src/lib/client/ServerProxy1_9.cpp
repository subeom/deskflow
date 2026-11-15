/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy1_9.h"

#include "client/Client.h"
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
  sendInputCapabilities();
  return true;
}

void ServerProxy1_9::onOptionsChanged(const OptionsList &options)
{
  m_rightCommandAction = RightCommandAction::Preserve;
  m_invertScrollX = false;
  m_invertScrollY = false;
  m_semanticMedia = false;
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
    } else if (options[i] == kOptionCrossOSSemanticMedia) {
      m_semanticMedia = options[i + 1] != 0;
    }
  }
}

void ServerProxy1_9::transformInputKey(KeyID &id, KeyModifierMask &mask)
{
  const auto transformed = inputPolicy().toDestinationKey(id, mask);
  id = transformed.id;
  mask = transformed.mask;
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
  ProtocolUtil::writef(getStream(), kMsgCInputCapabilities, static_cast<uint8_t>(local.platform), local.capabilities);
}

CrossPlatformInputPolicy ServerProxy1_9::inputPolicy() const
{
  return {
      m_sourceInputCapabilities, localInputCapabilities(),
      CrossPlatformInputOptions{
          .rightCommand = m_rightCommandAction,
          .invertScrollX = m_invertScrollX,
          .invertScrollY = m_invertScrollY,
          .semanticMedia = m_semanticMedia
      }
  };
}
