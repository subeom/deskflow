/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/InputCapabilities.h"
#include "deskflow/CrossPlatformInputPolicy.h"
#include "server/ClientProxy1_8.h"
#include "server/ReportedMonitors.h"

#include <optional>
#include <set>
#include <string>

class IEventQueue;

class ClientProxy1_9 : public ClientProxy1_8
{
public:
  ClientProxy1_9(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_9() override = default;

  const InputCapabilities &destinationInputCapabilities() const
  {
    return m_destinationInputCapabilities;
  }

  //! The monitors that the computer reported, while they are of the shape that it reports now (experimental)
  /*!
  Null unless the server invited the computer to report them (the experiment is on), the computer acknowledged it, and
  its last report is valid and fits its shape. Every report that is not valid is dropped, and never costs the
  connection or what is behind it in the stream.
  */
  const deskflow::server::ReportedMonitors *reportedMonitors() const override;

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

  // a destination that reported its capabilities speaks protocol 1.9, whose receivers reassemble any chunk size
  bool shouldPaceClipboard() const override;
  bool sendsClipboardId(ClipboardID id) const override;
  bool mirrorsSelection() const override;
  bool keepsClipboardWhenGrabbed() const override;

private:
  void sendInputCapabilities() const;
  bool receiveInputCapabilities();
  bool receiveMonitorReport();
  bool monitorsNegotiated() const;
  void dropMonitorReport(const std::string &problem);
  CrossPlatformInputPolicy inputPolicy() const;

  InputCapabilities m_destinationInputCapabilities;
  bool m_receivedInputCapabilities = false;
  // the invitation to report the monitors was in the DIPC (the experiment was on when the computer connected)
  bool m_invitedToReportMonitors = false;
  IEventQueue *m_monitorEvents = nullptr;
  std::optional<deskflow::server::ReportedMonitors> m_reportedMonitors;
  // what the last report that was not valid said, so that one that is not valid again says nothing more
  std::string m_lastMonitorProblem;
  F18Action m_f18Action = F18Action::PassThrough;
  bool m_invertScrollX = false;
  bool m_invertScrollY = false;
  ScrollInvertScope m_scrollInvertScope = ScrollInvertScope::All;
  bool m_semanticMedia = false;
  // the F18 presses written as Hangul and not released yet, by the KeyButton the source gave them (the key id of a
  // macOS release is kKeyNone, so the button is the only thing that pairs a release with its press)
  std::set<KeyButton> m_f18HangulButtons;
};
