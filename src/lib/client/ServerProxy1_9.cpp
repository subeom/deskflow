/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "client/ServerProxy1_9.h"

#include "base/Log.h"
#include "client/Client.h"
#include "common/ClipboardCapabilities.h"
#include "common/MonitorCapabilities.h"
#include "deskflow/ClipboardIdPolicy.h"
#include "deskflow/OptionTypes.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"

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
  m_receivedSourceCapabilities = true;
  // The monitors are reported to a server that asked for them and to nobody else; a computer that cannot list its
  // monitors does not acknowledge the bit, so the server never expects a report of it. The platform is not asked
  // unless the server invited it: a link that did not negotiate this does exactly what it always did.
  m_reportsMonitors = (capabilities & kLayoutCapabilityMonitors) != 0 && !localMonitors().monitors.empty();
  sendInputCapabilities();
  sendMonitorReport();
  return true;
}

deskflow::server::MonitorRect ServerProxy1_9::localShape() const
{
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  m_client->getShape(x, y, w, h);
  return {x, y, w, h};
}

ServerProxy1_9::LocalMonitors ServerProxy1_9::localMonitors() const
{
  return {localShape(), m_client->monitorInfo()};
}

void ServerProxy1_9::onMonitorsChanged()
{
  sendMonitorReport();
}

void ServerProxy1_9::onInfoSent()
{
  // the server checks the report against the shape that it has just been sent
  m_shapeSent = localShape();
  sendMonitorReport();
}

void ServerProxy1_9::sendMonitorReport()
{
  if (!m_reportsMonitors) {
    return;
  }

  const auto local = localMonitors();
  if (!m_shapeSent || *m_shapeSent != local.shape) {
    // the shape of the computer is not the one that the server was told: its DINF is on its way and brings the report
    return;
  }
  const auto prepared = deskflow::monitors::prepareMonitors(local.monitors, local.shape);
  if (!prepared.ok()) {
    // the server is not told of monitors that it cannot use; one that it was told of before is not valid any more
    if (prepared.problem != m_lastMonitorProblem) {
      m_lastMonitorProblem = prepared.problem;
      LOG_INFO("monitors are not reported to the server: %s", prepared.problem.c_str());
    }
    if (m_monitorsTold && !m_monitorsTold->empty()) {
      m_monitorsTold.emplace();
      m_monitorGeneration = m_monitorGeneration == UINT16_MAX ? 1 : m_monitorGeneration + 1;
      const auto none = deskflow::monitors::encodeReport({m_monitorGeneration, {}});
      getStream()->write(none.data(), static_cast<uint32_t>(none.size()));
    }
    return;
  }
  m_lastMonitorProblem.clear();

  if (m_monitorsTold && *m_monitorsTold == prepared.monitors) {
    return;
  }
  m_monitorsTold = prepared.monitors;
  m_monitorGeneration = m_monitorGeneration == UINT16_MAX ? 1 : m_monitorGeneration + 1;
  const auto report = deskflow::monitors::encodeReport({m_monitorGeneration, prepared.monitors});
  getStream()->write(report.data(), static_cast<uint32_t>(report.size()));
  LOG_INFO("reported %zu monitors to the server", prepared.monitors.size());
}

void ServerProxy1_9::onOptionsChanged(const OptionsList &options)
{
  m_invertScrollX = false;
  m_invertScrollY = false;
  m_scrollInvertScope = ScrollInvertScope::All;
  m_semanticMedia = false;
  for (size_t i = 0; i + 1 < options.size(); i += 2) {
    if (options[i] == kOptionCrossOSInvertScrollX) {
      m_invertScrollX = options[i + 1] != 0;
    } else if (options[i] == kOptionCrossOSInvertScrollY) {
      m_invertScrollY = options[i + 1] != 0;
    } else if (options[i] == kOptionCrossOSInvertScope) {
      m_scrollInvertScope = options[i + 1] == static_cast<OptionValue>(ScrollInvertScope::MouseWheelOnly)
                                ? ScrollInvertScope::MouseWheelOnly
                                : ScrollInvertScope::All;
    } else if (options[i] == kOptionCrossOSSemanticMedia) {
      m_semanticMedia = options[i + 1] != 0;
    }
  }
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

bool ServerProxy1_9::shouldPaceClipboard() const
{
  return m_receivedSourceCapabilities;
}

bool ServerProxy1_9::dataIsProofOfLife() const
{
  return m_receivedSourceCapabilities && (m_sourceInputCapabilities.capabilities & kClipboardCapabilityPaced) != 0;
}

bool ServerProxy1_9::sendsClipboardId(ClipboardID id) const
{
  // The selection of a computer on X11 is its own, and the server may pass it on to another computer on X11: it is only
  // left out when both computers are Windows or macOS, where it is the same clipboard that the server stores as both
  return !m_receivedSourceCapabilities ||
         (m_sourceInputCapabilities.capabilities & kClipboardCapabilitySingleId) == 0 ||
         deskflow::clipboard::exchangesId(m_sourceInputCapabilities.platform, id) ||
         deskflow::clipboard::exchangesId(localInputCapabilities().platform, id);
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
  ProtocolUtil::writef(
      getStream(), kMsgCInputCapabilities, static_cast<uint8_t>(local.platform),
      local.capabilities | kClipboardCapabilityPaced | kClipboardCapabilitySingleId |
          (m_reportsMonitors ? kLayoutCapabilityMonitors : 0)
  );
}

CrossPlatformInputPolicy ServerProxy1_9::inputPolicy() const
{
  return {
      m_sourceInputCapabilities, localInputCapabilities(),
      CrossPlatformInputOptions{
          .invertScrollX = m_invertScrollX,
          .invertScrollY = m_invertScrollY,
          .scrollInvertScope = m_scrollInvertScope,
          .semanticMedia = m_semanticMedia
      }
  };
}
