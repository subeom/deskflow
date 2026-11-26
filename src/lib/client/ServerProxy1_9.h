/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "client/ServerProxy1_8.h"
#include "common/InputCapabilities.h"
#include "deskflow/CrossPlatformInputPolicy.h"
#include "deskflow/MonitorReport.h"
#include "server/MonitorGeometry.h"

#include <optional>
#include <string>
#include <vector>

class ServerProxy1_9 : public ServerProxy1_8
{
public:
  ServerProxy1_9(Client *client, deskflow::IStream *stream, IEventQueue *events);
  ~ServerProxy1_9() override = default;

  const InputCapabilities &sourceInputCapabilities() const
  {
    return m_sourceInputCapabilities;
  }

  void onMonitorsChanged() override;

protected:
  //! The shape that the computer reports in DINF and its monitors, to be reported to a server that asked for them
  struct LocalMonitors
  {
    deskflow::server::MonitorRect shape;
    std::vector<deskflow::monitors::MonitorInfo> monitors;
  };

  virtual LocalMonitors localMonitors() const;
  //! The shape that the computer reports now, which is what a DINF that is sent now says
  virtual deskflow::server::MonitorRect localShape() const;
  void onInfoSent() override;
  ConnectionResult parseHandshakeMessage(const uint8_t *code) override;
  ConnectionResult parseMessage(const uint8_t *code) override;
  void onOptionsChanged(const OptionsList &options) override;
  bool handleSemanticMediaKey(KeyID id) override;

  // a server that sent its capabilities speaks protocol 1.9, whose receivers reassemble any chunk size
  bool shouldPaceClipboard() const override;
  // only a server that advertised the paced clipboard capability is known to keep a transfer from starving keepalives
  bool dataIsProofOfLife() const override;

public:
  // a server that mirrors the clipboard into the selection does not need the selection sent to it
  bool sendsClipboardId(ClipboardID id) const override;

protected:
  bool shouldSuppressKeyRepeat(KeyID id, KeyModifierMask mask) const override;
  void transformMouseWheel(int32_t &xDelta, int32_t &yDelta) override;
  virtual InputCapabilities localInputCapabilities() const;

protected:
  virtual bool synthesizeSemanticMediaKey(KeyID id);

private:
  bool receiveInputCapabilities();
  void sendInputCapabilities() const;
  void sendMonitorReport();
  CrossPlatformInputPolicy inputPolicy() const;

  Client *m_client = nullptr;
  InputCapabilities m_sourceInputCapabilities;
  bool m_receivedSourceCapabilities = false;
  // the server advertised the monitors capability (the experiment is on) and this computer acknowledged it, so it
  // reports its monitors; neither alone is enough
  bool m_reportsMonitors = false;
  // what the server was last told: nothing yet, or the monitors, or (an empty list) that there are none to tell
  std::optional<std::vector<deskflow::monitors::MonitorEntry>> m_monitorsTold;
  // The shape of the last DINF that was sent. A report is checked by the server against the DINF that it has, so it is
  // only sent when its shape is that one: when the shape changed and its DINF is still to be sent, that DINF is what
  // brings the report.
  std::optional<deskflow::server::MonitorRect> m_shapeSent;
  uint16_t m_monitorGeneration = 0;
  std::string m_lastMonitorProblem;
  bool m_invertScrollX = false;
  bool m_invertScrollY = false;
  ScrollInvertScope m_scrollInvertScope = ScrollInvertScope::All;
  bool m_semanticMedia = false;
};
