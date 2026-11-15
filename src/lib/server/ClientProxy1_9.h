/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/InputCapabilities.h"
#include "deskflow/CrossPlatformInputPolicy.h"
#include "server/ClientProxy1_8.h"

#include <set>

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
  using ClientProxy1_8::mouseWheel;
  void mouseWheel(int32_t xDelta, int32_t yDelta, ScrollSource source) override;
  bool leave() override;
  void resetOptions() override;
  void setOptions(const OptionsList &options) override;

protected:
  bool parseHandshakeMessage(const uint8_t *code) override;
  bool parseMessage(const uint8_t *code) override;
  bool isHandshakeCompleteReady() const override;
  virtual InputCapabilities localInputCapabilities() const;

private:
  void sendInputCapabilities() const;
  bool receiveInputCapabilities();
  CrossPlatformInputPolicy inputPolicy() const;

  InputCapabilities m_destinationInputCapabilities;
  bool m_receivedInputCapabilities = false;
  F18Action m_f18Action = F18Action::PassThrough;
  bool m_invertScrollX = false;
  bool m_invertScrollY = false;
  ScrollInvertScope m_scrollInvertScope = ScrollInvertScope::All;
  bool m_semanticMedia = false;
  // the F18 presses written as Hangul and not released yet, by the KeyButton the source gave them
  std::set<KeyButton> m_f18HangulButtons;
};
