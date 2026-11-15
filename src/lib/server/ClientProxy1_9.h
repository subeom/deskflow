/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/InputCapabilities.h"
#include "server/ClientProxy1_8.h"

class ClientProxy1_9 : public ClientProxy1_8
{
public:
  ClientProxy1_9(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_9() override = default;

  const InputCapabilities &destinationInputCapabilities() const
  {
    return m_destinationInputCapabilities;
  }

protected:
  bool parseHandshakeMessage(const uint8_t *code) override;
  bool parseMessage(const uint8_t *code) override;
  bool isHandshakeCompleteReady() const override;

private:
  void sendInputCapabilities() const;
  bool receiveInputCapabilities();

  InputCapabilities m_destinationInputCapabilities;
  bool m_receivedInputCapabilities = false;
};
