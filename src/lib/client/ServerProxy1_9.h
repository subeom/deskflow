/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "client/ServerProxy1_8.h"
#include "common/InputCapabilities.h"

class ServerProxy1_9 : public ServerProxy1_8
{
public:
  ServerProxy1_9(Client *client, deskflow::IStream *stream, IEventQueue *events);
  ~ServerProxy1_9() override = default;

  const InputCapabilities &sourceInputCapabilities() const
  {
    return m_sourceInputCapabilities;
  }

protected:
  ConnectionResult parseHandshakeMessage(const uint8_t *code) override;
  ConnectionResult parseMessage(const uint8_t *code) override;

private:
  bool receiveInputCapabilities();
  void sendInputCapabilities() const;

  InputCapabilities m_sourceInputCapabilities;
};
