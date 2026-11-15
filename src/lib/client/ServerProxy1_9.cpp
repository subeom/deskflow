/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy1_9.h"

#include "common/PlatformInfo.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"

#include <cstring>

ServerProxy1_9::ServerProxy1_9(Client *client, deskflow::IStream *stream, IEventQueue *events)
    : ServerProxy1_8(client, stream, events)
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

void ServerProxy1_9::sendInputCapabilities() const
{
  const auto local = deskflow::platform::currentInputCapabilities();
  ProtocolUtil::writef(getStream(), kMsgCInputCapabilities, static_cast<uint8_t>(local.platform), local.capabilities);
}
