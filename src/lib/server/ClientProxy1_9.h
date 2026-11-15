/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/InputCapabilities.h"
#include "deskflow/CrossPlatformInputPolicy.h"
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

  void keyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override;
  void keyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override;
  void keyUp(KeyID, KeyModifierMask, KeyButton) override;
  void resetOptions() override;
  void setOptions(const OptionsList &options) override;

protected:
  bool parseHandshakeMessage(const uint8_t *code) override;
  bool parseMessage(const uint8_t *code) override;
  bool isHandshakeCompleteReady() const override;
  virtual InputCapabilities localInputCapabilities() const;
  InputPlatform legacyInputPlatform() const override;

private:
  void sendInputCapabilities() const;
  bool receiveInputCapabilities();
  CrossPlatformInputPolicy inputPolicy() const;
  KeyModifierMask modifierMaskForWire(KeyID id, KeyModifierMask mask) const;

  InputCapabilities m_destinationInputCapabilities;
  bool m_receivedInputCapabilities = false;
  RightCommandAction m_rightCommandAction = RightCommandAction::Preserve;
  bool m_leftSuperDown = false;
  bool m_rightSuperDown = false;
  bool m_rightSuperMapped = false;
};
