/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "common/PlatformInfo.h"
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

bool ClientProxy1_9::isHandshakeCompleteReady() const
{
  return m_receivedInputCapabilities;
}
