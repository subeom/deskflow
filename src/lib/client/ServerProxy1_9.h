/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "client/ServerProxy1_8.h"
#include "common/InputCapabilities.h"
#include "deskflow/CrossPlatformInputPolicy.h"

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
  void onOptionsChanged(const OptionsList &options) override;
  bool handleSemanticMediaKey(KeyID id) override;

  // a server that sent its capabilities speaks protocol 1.9, whose receivers reassemble any chunk size
  bool shouldPaceClipboard() const override;
  // only a server that advertised the paced clipboard capability is known to keep a transfer from starving keepalives
  bool dataIsProofOfLife() const override;
  bool shouldSuppressKeyRepeat(KeyID id, KeyModifierMask mask) const override;
  void transformMouseWheel(int32_t &xDelta, int32_t &yDelta) override;
  virtual InputCapabilities localInputCapabilities() const;

protected:
  virtual bool synthesizeSemanticMediaKey(KeyID id);

private:
  bool receiveInputCapabilities();
  void sendInputCapabilities() const;
  CrossPlatformInputPolicy inputPolicy() const;

  Client *m_client = nullptr;
  InputCapabilities m_sourceInputCapabilities;
  bool m_receivedSourceCapabilities = false;
  bool m_invertScrollX = false;
  bool m_invertScrollY = false;
  ScrollInvertScope m_scrollInvertScope = ScrollInvertScope::All;
  bool m_semanticMedia = false;
};
