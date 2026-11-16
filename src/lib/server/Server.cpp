/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers.
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/Server.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/AppUtil.h"
#include "deskflow/ClipboardTransferLog.h"
#include "deskflow/Computer.h"
#include "deskflow/DeskflowException.h"
#include "deskflow/IPlatformComputer.h"
#include "deskflow/MonitorReport.h"
#include "deskflow/OptionTypes.h"
#include "deskflow/PacketStreamFilter.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/StreamChunker.h"
#include "deskflow/ipc/CoreIpc.h"
#include "net/TCPSocket.h"
#include "server/ClientListener.h"
#include "server/ClientProxy.h"
#include "server/ClientProxyUnknown.h"
#include "server/PrimaryClient.h"
#include "server/ReportedMonitors.h"
#include "server/ServerCursorEdges.h"
#include "server/ServerNeighborTraversal.h"

#ifdef _WIN32
#include <algorithm>
#include <array>
#endif
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>

using namespace deskflow::server;
using deskflow::clipboard::formatSkipLine;
using deskflow::clipboard::SkipReason;

namespace {

Direction oppositeOf(Direction side)
{
  using enum Direction;
  switch (side) {
  case Left:
    return Right;
  case Right:
    return Left;
  case Top:
    return Bottom;
  case Bottom:
    return Top;
  default:
    return NoDirection;
  }
}

const char *sideText(Direction side)
{
  using enum Direction;
  switch (side) {
  case Left:
    return "left";
  case Right:
    return "right";
  case Top:
    return "top";
  case Bottom:
    return "bottom";
  default:
    return "no";
  }
}

} // namespace

//
// Server
//

Server::Server(ServerConfig &config, PrimaryClient *primaryClient, deskflow::Computer *computer, IEventQueue *events)
    : m_primaryClient(primaryClient),
      m_active(primaryClient),
      m_config(&config),
      m_inputFilter(config.getInputFilter()),
      m_computer(computer),
      m_events(events)
{
  // must have a primary client and it must have a canonical name
  assert(m_primaryClient != nullptr);
  assert(config.isComputer(primaryClient->getName()));
  assert(m_computer != nullptr);

  std::string primaryName = getName(primaryClient);

  // clear clipboards
  for (auto &clipboard : m_clipboards) {
    clipboard.m_clipboardOwner = primaryName;
    clipboard.m_clipboardSeqNum = m_seqNum;
    if (clipboard.m_clipboard.open(0)) {
      clipboard.m_clipboard.empty();
      clipboard.m_clipboard.close();
    }
    clipboard.m_clipboardData = clipboard.m_clipboard.marshall();
  }

  // install event handlers
  m_events->addHandler(EventTypes::Timer, this, [this](const auto &) { handleSwitchWaitTimeout(); });
  m_events->addHandler(EventTypes::KeyStateKeyDown, m_inputFilter, [this](const auto &e) { handleKeyDownEvent(e); });
  m_events->addHandler(EventTypes::KeyStateKeyUp, m_inputFilter, [this](const auto &e) { handleKeyUpEvent(e); });
  m_events->addHandler(EventTypes::KeyStateKeyRepeat, m_inputFilter, [this](const auto &e) {
    handleKeyRepeatEvent(e);
  });
  m_events->addHandler(EventTypes::PrimaryComputerButtonDown, m_inputFilter, [this](const auto &e) {
    handleButtonDownEvent(e);
  });
  m_events->addHandler(EventTypes::PrimaryComputerButtonUp, m_inputFilter, [this](const auto &e) {
    handleButtonUpEvent(e);
  });
  m_events->addHandler(
      EventTypes::PrimaryComputerMotionOnPrimary, m_primaryClient->getEventTarget(),
      [this](const auto &e) { handleMotionPrimaryEvent(e); }
  );
  m_events->addHandler(
      EventTypes::PrimaryComputerMotionOnSecondary, m_primaryClient->getEventTarget(),
      [this](const auto &e) { handleMotionSecondaryEvent(e); }
  );
  m_events->addHandler(EventTypes::PrimaryComputerWheel, m_primaryClient->getEventTarget(), [this](const auto &e) {
    handleWheelEvent(e);
  });
  m_events->addHandler(
      EventTypes::PrimaryScreenSaverActivated, m_primaryClient->getEventTarget(),
      [this](const auto &) { onScreensaver(true); }
  );
  m_events->addHandler(
      EventTypes::PrimaryScreenSaverDeactivated, m_primaryClient->getEventTarget(),
      [this](const auto &) { onScreensaver(false); }
  );
  m_events->addHandler(EventTypes::ServerSwitchToComputer, m_inputFilter, [this](const auto &e) {
    handleSwitchToComputerEvent(e);
  });
  m_events->addHandler(EventTypes::ServerSwitchInDirection, m_inputFilter, [this](const auto &e) {
    handleSwitchInDirectionEvent(e);
  });
  m_events->addHandler(EventTypes::ServerToggleComputer, m_inputFilter, [this](const auto &e) {
    handleToggleComputerEvent(e);
  });
  m_events->addHandler(EventTypes::ServerKeyboardBroadcast, m_inputFilter, [this](const auto &e) {
    handleKeyboardBroadcastEvent(e);
  });
  m_events->addHandler(EventTypes::ServerLockCursorToComputer, m_inputFilter, [this](const auto &e) {
    handleLockCursorToComputerEvent(e);
  });
  m_events->addHandler(EventTypes::PrimaryComputerFakeInputBegin, m_inputFilter, [this](const auto &) {
    m_primaryClient->fakeInputBegin();
  });
  m_events->addHandler(EventTypes::PrimaryComputerFakeInputEnd, m_inputFilter, [this](const auto &) {
    m_primaryClient->fakeInputEnd();
  });

  // add connection
  addClient(m_primaryClient);

  // set initial configuration
  setConfig(config);

  // If we are using libportal, we need to set the primary client first.
  // This enables the rules of the primary client. The hotkeys are then enabled in EiComputer.cpp after the rules
  // registered them.
#if WINAPI_LIBPORTAL
  // enable primary client
  m_inputFilter->setPrimaryClient(m_primaryClient);
  m_primaryClient->enable();
  // If we are on Mac, X11 or Windows, we need a key map, which is created in computer->enable().
  // So we need to enable the computer first, then enable the primary client.
#else
  m_primaryClient->enable();
  m_inputFilter->setPrimaryClient(m_primaryClient);
#endif
  // Determine if scroll lock is already set. If so, lock the cursor to the
  // primary computer (unless the user has disabled lock to computer in config)
  if (!m_disableLockToComputer && (m_primaryClient->getToggleMask() & KeyModifierScrollLock)) {
    LOG_INFO("scroll lock is on, locking cursor to computer");
    m_lockedToComputer = true;
  } else if (m_defaultLockToComputerState) {
    LOG_INFO("default computer lock is on, locking cursor to computer");
    m_lockedToComputer = true;
  }
}

Server::~Server()
{
  // remove event handlers and timers
  using enum EventTypes;
  m_events->removeHandler(KeyStateKeyDown, m_inputFilter);
  m_events->removeHandler(KeyStateKeyUp, m_inputFilter);
  m_events->removeHandler(KeyStateKeyRepeat, m_inputFilter);
  m_events->removeHandler(PrimaryComputerButtonDown, m_inputFilter);
  m_events->removeHandler(PrimaryComputerButtonUp, m_inputFilter);
  m_events->removeHandler(PrimaryComputerMotionOnPrimary, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryComputerMotionOnSecondary, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryComputerWheel, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryScreenSaverActivated, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryScreenSaverDeactivated, m_primaryClient->getEventTarget());
  m_events->removeHandler(PrimaryComputerFakeInputBegin, m_inputFilter);
  m_events->removeHandler(PrimaryComputerFakeInputEnd, m_inputFilter);
  m_events->removeHandler(Timer, this);
  stopSwitch();

  try {
    // force immediate disconnection of secondary clients
    disconnect();
  } catch (std::exception &e) { // NOSONAR
    LOG_ERR("failed to disconnect: %s", e.what());
  }

  for (auto index = m_oldClients.begin(); index != m_oldClients.end(); ++index) {
    BaseClientProxy *client = index->first;
    m_events->deleteTimer(index->second);
    m_events->removeHandler(Timer, client);
    m_events->removeHandler(ClientProxyDisconnected, client);
    delete client;
  }

  // See at Server::Server for why we do this in this order.
#if WINAPI_LIBPORTAL
  // disable and disconnect primary client
  m_primaryClient->disable();
  // remove input filter
  m_inputFilter->setPrimaryClient(nullptr);
#else
  m_inputFilter->setPrimaryClient(nullptr);
  m_primaryClient->disable();
#endif
  removeClient(m_primaryClient);
}

size_t Server::getMaximumClipboardSizeBytes() const
{
  return m_maximumClipboardSize * 1024;
}

bool Server::setConfig(const ServerConfig &config)
{
  // refuse configuration if it doesn't include the primary computer
  if (!config.isComputer(m_primaryClient->getName())) {
    return false;
  }

  // close clients that are connected but being dropped from the
  // configuration.
  closeClients(config);

  // cut over
  processOptions();

  // add ScrollLock as a hotkey to lock to the computer.  this was a
  // built-in feature in earlier releases and is now supported via
  // the user configurable hotkey mechanism.  if the user has already
  // registered ScrollLock for something else then that will win but
  // we will unfortunately generate a warning.  if the user has
  // configured a LockCursorToComputerAction then we don't add
  // ScrollLock as a hotkey.
  if (!m_disableLockToComputer && !m_config->hasLockToComputerAction()) {
    IPlatformComputer::KeyInfo *key = IPlatformComputer::KeyInfo::alloc(kKeyScrollLock, 0, 0, 0);
    InputFilter::Rule rule(new InputFilter::KeystrokeCondition(m_events, key));
    rule.adoptAction(new InputFilter::LockCursorToComputerAction(m_events), true);
    m_inputFilter->addFilterRule(rule);
  }

  // tell primary computer about reconfiguration
  m_primaryClient->reconfigure(getActivePrimarySides());

  // tell all (connected) clients about current options
  for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
    BaseClientProxy *client = index->second;
    sendOptions(client);
  }

  return true;
}

void Server::adoptClient(BaseClientProxy *client)
{
  assert(client != nullptr);

  // watch for client disconnection
  m_events->addHandler(EventTypes::ClientProxyDisconnected, client, [this, client](const auto &) {
    handleClientDisconnected(client);
  });

  // name must be in our configuration
  if (!m_config->isComputer(client->getName())) {
    LOG_WARN("unrecognised client name \"%s\", check server config", client->getName().c_str());
    ipcSendToClient("unrecognisedClient", QString::fromStdString(client->getName()));
    closeClient(client, kMsgEUnknown);
    return;
  }

  // add client to client list
  if (!addClient(client)) {
    // can only have one computer with a given name at any given time
    LOG_WARN("a client with name \"%s\" is already connected", getName(client).c_str());
    closeClient(client, kMsgEBusy);
    return;
  }
  LOG_DEBUG("client \"%s\" has connected", getName(client).c_str());
  ipcSendConnectionState(deskflow::core::ConnectionState::Connected);
  sendConnectedClientsIpc();

  // send configuration options to client
  sendOptions(client);

  // activate computer saver on new client if active on the primary computer
  if (m_activeSaver != nullptr) {
    client->screensaver(true);
  }

  // send notification
  auto *info = new Server::ComputerConnectedInfo(getName(client));
  m_events->addEvent(Event(EventTypes::ServerConnected, m_primaryClient->getEventTarget(), info));
}

void Server::disconnect()
{
  // close all secondary clients
  if (m_clients.size() > 1 || !m_oldClients.empty()) {
    Config emptyConfig(m_events);
    closeClients(emptyConfig);
  } else {
    m_events->addEvent(Event(EventTypes::ServerDisconnected, this));
  }
}

std::string Server::protocolString() const
{
  if (m_protocol == NetworkProtocol::Unknown)
    throw InvalidProtocolException();
  return networkProtocolToName(m_protocol).toStdString();
}

uint32_t Server::getNumClients() const
{
  return (int32_t)m_clients.size();
}

void Server::getClients(std::vector<std::string> &list) const
{
  list.clear();
  for (auto index = m_clients.begin(); index != m_clients.end(); ++index) {
    list.push_back(index->first);
  }
}

void Server::sendConnectedClientsIpc() const
{
  const auto primaryName = getName(m_primaryClient);
  QStringList clientList;
  for (const auto &[name, _] : m_clients) {
    if (name != primaryName) {
      clientList.append(QString::fromStdString(name));
    }
  }
  ipcSendToClient("connectedClients", clientList.join(","));
}

std::string Server::getName(const BaseClientProxy *client) const
{
  std::string name = m_config->getCanonicalName(client->getName());
  if (name.empty()) {
    name = client->getName();
  }
  return name;
}

uint32_t Server::getActivePrimarySides() const
{
  using enum DirectionMask;
  using enum Direction;
  uint32_t sides = 0;
  if (!isLockedToComputerServer()) {
    if (hasAnyNeighbor(m_primaryClient, Left)) {
      sides |= static_cast<int>(LeftMask);
    }
    if (hasAnyNeighbor(m_primaryClient, Right)) {
      sides |= static_cast<int>(RightMask);
    }
    if (hasAnyNeighbor(m_primaryClient, Top)) {
      sides |= static_cast<int>(TopMask);
    }
    if (hasAnyNeighbor(m_primaryClient, Bottom)) {
      sides |= static_cast<int>(BottomMask);
    }
  }
  return sides;
}

bool Server::isLockedToComputerServer() const
{
  // locked if scroll-lock is toggled on
  return m_lockedToComputer;
}

bool Server::isLockedToComputer() const
{
  if (m_disableLockToComputer) {
    return false;
  }

  // locked if we say we're locked
  if (isLockedToComputerServer()) {
    if (!m_defaultLockToComputerState) {
      LOG_INFO("cursor is locked to computer, check scroll lock key");
    }
    return true;
  }

  // locked if primary says we're locked
  if (m_primaryClient->isLockedToComputer()) {
    return true;
  }

  // not locked
  return false;
}

int32_t Server::getJumpZoneSize(const BaseClientProxy *client) const
{
  if (client == m_primaryClient) {
    return m_primaryClient->getJumpZoneSize();
  } else {
    return 0;
  }
}

void Server::switchComputer(BaseClientProxy *dst, int32_t x, int32_t y, bool forScreensaver)
{
  assert(dst != nullptr);

  int32_t dx;
  int32_t dy;
  int32_t dw;
  int32_t dh;
  dst->getShape(dx, dy, dw, dh);

  // any of these conditions seem to trigger when the portal permission dialog
  // is visible on wayland. this was previously an assert, but that's pretty
  // annoying since it makes the mouse unusable on the server and you'll have to
  // ssh into your machine to kill it. better to just log a warning.
  if (x < dx) {
    LOG_WARN(
        "on switch, x (%d) is less than the left boundary dx (%d)", //
        x, dx
    );
  }
  if (y < dy) {
    LOG_WARN(
        "on switch, y (%d) is less than the top boundary dy (%d)", //
        y, dy
    );
  }
  if (x >= dx + dw) {
    LOG_WARN(
        "on switch, x (%d) exceeds the right boundary (dx + width = %d)", //
        x, dx + dw
    );
  }
  if (y >= dy + dh) {
    LOG_WARN(
        "on switch, y (%d) exceeds the bottom boundary (dy + height = %d)", //
        y, dy + dh
    );
  }

  assert(m_active != nullptr);

  LOG_INFO("switch from \"%s\" to \"%s\" at %d,%d", getName(m_active).c_str(), getName(dst).c_str(), x, y);

  if (m_multiMonitor && m_active != dst) {
    sayMonitorGeometrySwitch(dst, x, y);
  }

  // stop waiting to switch
  stopSwitch();

  // record new position
  m_x = x;
  m_y = y;
  m_xDelta = 0;
  m_yDelta = 0;
  m_xDelta2 = 0;
  m_yDelta2 = 0;

  // wrapping means leaving the active computer and entering it again.
  // since that's a waste of time we skip that and just warp the
  // mouse.
  if (m_active != dst) {
    // leave active computer
    if (!m_active->leave()) {
      // cannot leave computer
      LOG_WARN("can't leave computer");
      return;
    }

    // update the primary client's clipboards if we're leaving the
    // primary computer.
    if (m_active == m_primaryClient && m_enableClipboard && sharesClipboardWith(m_primaryClient)) {
      for (ClipboardID id = 0; id < kClipboardEnd; ++id) {
        const ClipboardInfo &clipboard = m_clipboards[id];
        if (clipboard.m_clipboardOwner == getName(m_primaryClient)) {
          onClipboardChanged(m_primaryClient, id, clipboard.m_clipboardSeqNum);
        }
      }
    }

#if defined(__APPLE__)
    if (dst != m_primaryClient) {
      std::string secureInputApplication = m_primaryClient->getSecureInputApp();
      if (secureInputApplication != "") {
        // display notification on the server
        m_primaryClient->secureInputNotification(secureInputApplication);
        // display notification on the client
        dst->secureInputNotification(secureInputApplication);
      }
    }
#endif

    // cut over
    m_active = dst;

    // increment enter sequence number
    ++m_seqNum;

    // enter new computer
    m_active->enter(x, y, m_seqNum, m_primaryClient->getToggleMask(), forScreensaver);

    if (m_enableClipboard && sharesClipboardWith(m_active)) {
      // send the clipboard data to new active computer
      for (ClipboardID id = 0; id < kClipboardEnd; ++id) {
        // Hackity hackity hack
        if (m_clipboards[id].m_oversize ||
            m_clipboards[id].m_clipboard.marshall().size() > (m_maximumClipboardSize * 1024)) {
          continue;
        }
        // The primary computer that does not share its clipboard stays the owner of the empty clipboards the Server
        // starts with, and sending those would empty the clipboard of every computer the cursor enters
        if (m_clipboards[id].m_clipboardOwner == getName(m_primaryClient) && !sharesClipboardWith(m_primaryClient)) {
          continue;
        }
        m_active->setClipboard(id, &m_clipboards[id].m_clipboard);
        if (m_active == m_primaryClient) {
          m_primaryClipboardWritten[id] = std::chrono::steady_clock::now();
        }
      }
    }

    auto *info = new Server::SwitchToComputerInfo(m_active->getName());
    m_events->addEvent(Event(EventTypes::ServerComputerSwitched, this, info));
  } else {
    m_active->mouseMove(x, y);
  }
}

void Server::jumpToComputer(BaseClientProxy *newComputer)
{
  assert(newComputer != nullptr);

  // record the current cursor position on the active computer
  m_active->setJumpCursorPos(m_x, m_y);

  // get the last cursor position on the target computer
  int32_t x;
  int32_t y;
  newComputer->getJumpCursorPos(x, y);

  // there is no edge that the pointer was taken over
  if (m_multiMonitor) {
    m_crossing.known = false;
  }

  switchComputer(newComputer, x, y, false);
}

float Server::mapToFraction(const BaseClientProxy *client, Direction dir, int32_t x, int32_t y) const
{
  int32_t sx;
  int32_t sy;
  int32_t sw;
  int32_t sh;
  client->getShape(sx, sy, sw, sh);
  switch (dir) {
    using enum Direction;
  case Left:
  case Right:
    return deskflow::server::getCursorEdgeFraction(y, sy, sh);

  case Top:
  case Bottom:
    return deskflow::server::getCursorEdgeFraction(x, sx, sw);

  case NoDirection:
    assert(0 && "bad direction");
    break;
  }
  return 0.0f;
}

void Server::mapToPixel(const BaseClientProxy *client, Direction dir, float f, int32_t &x, int32_t &y) const
{
  int32_t sx;
  int32_t sy;
  int32_t sw;
  int32_t sh;
  client->getShape(sx, sy, sw, sh);
  switch (dir) {
    using enum Direction;
  case Left:
  case Right:
    y = static_cast<int32_t>(f * sh) + sy;
    break;

  case Top:
  case Bottom:
    x = static_cast<int32_t>(f * sw) + sx;
    break;

  case NoDirection:
    assert(0 && "bad direction");
    break;
  }
}

bool Server::hasAnyNeighbor(const BaseClientProxy *client, Direction dir) const
{
  assert(client != nullptr);

  return m_config->hasNeighbor(getName(client), dir);
}

BaseClientProxy *Server::getNeighbor(const BaseClientProxy *src, Direction dir, int32_t &x, int32_t &y) const
{
  // note -- must be locked on entry

  assert(src != nullptr);

  // get source computer name
  std::string srcName = getName(src);
  assert(!srcName.empty());
  LOG_VERBOSE("find neighbor on %s of \"%s\"", Config::dirName(dir), srcName.c_str());

  // convert position to fraction
  float t = mapToFraction(src, dir, x, y);

  const auto traversal = deskflow::server::traverseConnectedNeighbor(
      srcName, t,
      [this, dir](const std::string &source, float position) -> std::optional<deskflow::server::NeighborPosition> {
        float positionOut = 0.0f;
        const auto destination = m_config->getNeighbor(source, dir, position, &positionOut);
        if (destination.empty()) {
          return std::nullopt;
        }
        return deskflow::server::NeighborPosition{destination, positionOut};
      },
      [this](const std::string &computer) { return m_clients.contains(computer); },
      [dir](const std::string &source, const deskflow::server::NeighborPosition &destination) {
        LOG_VERBOSE("ignored \"%s\" on %s of \"%s\"", destination.name.c_str(), Config::dirName(dir), source.c_str());
      }
  );

  if (traversal.status != deskflow::server::NeighborTraversalStatus::Connected) {
    if (traversal.status == deskflow::server::NeighborTraversalStatus::Cycle) {
      LOG_WARN(
          "neighbor cycle at \"%s\" while searching %s from \"%s\"", traversal.destination.name.c_str(),
          Config::dirName(dir), srcName.c_str()
      );
    } else {
      LOG_VERBOSE("no neighbor on %s of \"%s\"", Config::dirName(dir), srcName.c_str());
    }
    return nullptr;
  }

  ClientList::const_iterator index = m_clients.find(traversal.destination.name);
  assert(index != m_clients.end());
  LOG_VERBOSE(
      "\"%s\" is on %s of \"%s\" at %f", traversal.destination.name.c_str(), Config::dirName(dir), srcName.c_str(),
      traversal.destination.position
  );
  mapToPixel(index->second, dir, traversal.destination.position, x, y);
  return index->second;
}

BaseClientProxy *Server::mapToNeighbor(BaseClientProxy *src, Direction srcSide, int32_t &x, int32_t &y) const
{
  // note -- must be locked on entry

  assert(src != nullptr);

  // what is said about the switch, if there is one
  if (m_multiMonitor) {
    m_crossing = {true, srcSide, mapToFraction(src, srcSide, x, y)};
  }

  // get the first neighbor
  BaseClientProxy *dst = getNeighbor(src, srcSide, x, y);
  if (dst == nullptr) {
    return nullptr;
  }

  // get the source computer's size
  int32_t dx;
  int32_t dy;
  int32_t dw;
  int32_t dh;
  BaseClientProxy *lastGoodComputer = src;
  lastGoodComputer->getShape(dx, dy, dw, dh);

  // find destination computer, adjusting x or y (but not both).  the
  // searches are done in a sort of canonical computer space where
  // the upper-left corner is 0,0 for each computer.  we adjust from
  // actual to canonical position on entry to and from canonical to
  // actual on exit from the search.
  switch (srcSide) {
    using enum Direction;
  case Left:
    x -= dx;
    while (dst != nullptr) {
      lastGoodComputer = dst;
      lastGoodComputer->getShape(dx, dy, dw, dh);
      x += dw;
      if (x >= 0) {
        break;
      }
      LOG_VERBOSE("skipping over computer %s", getName(dst).c_str());
      dst = getNeighbor(lastGoodComputer, srcSide, x, y);
    }
    assert(lastGoodComputer != nullptr);
    x += dx;
    break;

  case Right:
    x -= dx;
    while (dst != nullptr) {
      x -= dw;
      lastGoodComputer = dst;
      lastGoodComputer->getShape(dx, dy, dw, dh);
      if (x < dw) {
        break;
      }
      LOG_VERBOSE("skipping over computer %s", getName(dst).c_str());
      dst = getNeighbor(lastGoodComputer, srcSide, x, y);
    }
    assert(lastGoodComputer != nullptr);
    x += dx;
    break;

  case Top:
    y -= dy;
    while (dst != nullptr) {
      lastGoodComputer = dst;
      lastGoodComputer->getShape(dx, dy, dw, dh);
      y += dh;
      if (y >= 0) {
        break;
      }
      LOG_VERBOSE("skipping over computer %s", getName(dst).c_str());
      dst = getNeighbor(lastGoodComputer, srcSide, x, y);
    }
    assert(lastGoodComputer != nullptr);
    y += dy;
    break;

  case Bottom:
    y -= dy;
    while (dst != nullptr) {
      y -= dh;
      lastGoodComputer = dst;
      lastGoodComputer->getShape(dx, dy, dw, dh);
      if (y < dh) {
        break;
      }
      LOG_VERBOSE("skipping over computer %s", getName(dst).c_str());
      dst = getNeighbor(lastGoodComputer, srcSide, x, y);
    }
    assert(lastGoodComputer != nullptr);
    y += dy;
    break;

  case NoDirection:
    assert(0 && "bad direction");
    return nullptr;
  }

  // save destination computer
  assert(lastGoodComputer != nullptr);
  dst = lastGoodComputer;

  // a computer that has a layout is entered on a monitor of it, and the primary one away from the jump zone of its edge
  const auto *layout = monitorLayoutFor(dst);
  if (layout != nullptr) {
    enterOnMonitor(*layout, srcSide, x, y, dst == m_primaryClient ? jumpZoneDepth(*layout, srcSide, x, y) : 0);
  }

  // if entering primary computer then be sure to move in far enough
  // to avoid the jump zone.  if entering a side that doesn't have
  // a neighbor (i.e. an asymmetrical side) then we don't need to
  // move inwards because that side can't provoke a jump.
  if (layout == nullptr || dst != m_primaryClient)
    avoidJumpZone(dst, srcSide, x, y);

  return dst;
}

void Server::avoidJumpZone(const BaseClientProxy *dst, Direction dir, int32_t &x, int32_t &y) const
{
  // we only need to avoid jump zones on the primary computer
  if (dst != m_primaryClient) {
    return;
  }

  const std::string dstName(getName(dst));
  int32_t dx;
  int32_t dy;
  int32_t dw;
  int32_t dh;
  dst->getShape(dx, dy, dw, dh);
  float t = mapToFraction(dst, dir, x, y);
  int32_t z = getJumpZoneSize(dst);

  // move in far enough to avoid the jump zone.  if entering a side
  // that doesn't have a neighbor (i.e. an asymmetrical side) then we
  // don't need to move inwards because that side can't provoke a jump.
  switch (dir) {
    using enum Direction;
  case Left:
    if (!m_config->getNeighbor(dstName, Right, t, nullptr).empty() && x > dx + dw - 1 - z)
      x = dx + dw - 1 - z;
    break;

  case Right:
    if (!m_config->getNeighbor(dstName, Left, t, nullptr).empty() && x < dx + z)
      x = dx + z;
    break;

  case Top:
    if (!m_config->getNeighbor(dstName, Bottom, t, nullptr).empty() && y > dy + dh - 1 - z)
      y = dy + dh - 1 - z;
    break;

  case Bottom:
    if (!m_config->getNeighbor(dstName, Top, t, nullptr).empty() && y < dy + z)
      y = dy + z;
    break;

  case NoDirection:
    assert(0 && "bad direction");
  }
}

bool Server::isSwitchOkay(
    BaseClientProxy *newComputer, Direction dir, int32_t x, int32_t y, int32_t xActive, int32_t yActive
)
{
  LOG_VERBOSE("try to leave \"%s\" on %s", getName(m_active).c_str(), Config::dirName(dir));

  // is there a neighbor?
  if (newComputer == nullptr) {
    // there's no neighbor.  we don't want to switch and we don't
    // want to try to switch later.
    LOG_VERBOSE("no neighbor %s", Config::dirName(dir));
    stopSwitch();
    return false;
  }

  // should we switch or not?
  bool preventSwitch = false;
  bool allowSwitch = false;

  // note if the switch direction has changed.  save the new
  // direction and computer if so.  with the arrangement used, a side has several neighbours, one for each part of the
  // edge, and sliding along the edge into the rows of another one is as new as another direction.
  const bool slidToAnother =
      m_multiMonitor && m_useArrangement && m_switchComputer != nullptr && newComputer != m_switchComputer;
  bool isNewDirection = (dir != m_switchDir) || slidToAnother;
  if (isNewDirection || m_switchComputer == nullptr) {
    m_switchDir = dir;
    m_switchComputer = newComputer;
  }

  // is this a double tap and do we care?
  if (!allowSwitch && m_switchTwoTapDelay > 0.0) {
    if (isNewDirection || !isSwitchTwoTapStarted() || !shouldSwitchTwoTap()) {
      // tapping a different or new edge or second tap not
      // fast enough.  prepare for second tap.
      preventSwitch = true;
      startSwitchTwoTap();
    } else {
      // got second tap
      allowSwitch = true;
    }
  }

  // if waiting before a switch then prepare to switch later
  if (!allowSwitch && m_switchWaitDelay > 0.0) {
    if (isNewDirection || !isSwitchWaitStarted()) {
      startSwitchWait(x, y);
    }
    preventSwitch = true;
  }

  // are we in a locked corner?  first check if computer has the option set
  // and, if not, check the global options.
  const Config::ScreenOptions *options = m_config->getOptions(getName(m_active));
  if (options == nullptr || !options->contains(kOptionComputerSwitchCorners)) {
    options = m_config->getOptions("");
  }
  if (options != nullptr && options->contains(kOptionComputerSwitchCorners)) {
    // get corner mask and size
    Config::ScreenOptions::const_iterator i = options->find(kOptionComputerSwitchCorners);
    auto corners = static_cast<uint32_t>(i->second);
    i = options->find(kOptionComputerSwitchCornerSize);
    int32_t size = 0;
    if (i != options->end()) {
      size = i->second;
    }
    // see if we're in a locked corner
    if ((getCorner(m_active, xActive, yActive, size) & corners) != 0) {
      // yep, no switching
      LOG_VERBOSE("locked in corner");
      preventSwitch = true;
      stopSwitch();
    }
  }

  // ignore if mouse is locked to computer and don't try to switch later
  if (!preventSwitch && isLockedToComputer()) {
    LOG_VERBOSE("locked to computer");
    preventSwitch = true;
    stopSwitch();
  }

  return !preventSwitch;
}

void Server::noSwitch(int32_t x, int32_t y)
{
  armSwitchTwoTap(x, y);
  stopSwitchWait();
}

void Server::stopSwitch()
{
  if (m_switchComputer != nullptr) {
    m_switchComputer = nullptr;
    m_switchDir = Direction::NoDirection;
    stopSwitchTwoTap();
    stopSwitchWait();
  }
}

void Server::startSwitchTwoTap()
{
  m_switchTwoTapEngaged = true;
  m_switchTwoTapArmed = false;
  m_switchTwoTapTimer.reset();
  LOG_VERBOSE("waiting for second tap");
}

void Server::armSwitchTwoTap(int32_t x, int32_t y)
{
  if (m_switchTwoTapEngaged) {
    if (m_switchTwoTapTimer.getTime() > m_switchTwoTapDelay) {
      // second tap took too long.  disengage.
      stopSwitchTwoTap();
    } else if (!m_switchTwoTapArmed) {
      // still time for a double tap.  see if we left the tap
      // zone and, if so, arm the two tap.
      int32_t ax;
      int32_t ay;
      int32_t aw;
      int32_t ah;
      m_active->getShape(ax, ay, aw, ah);
      int32_t tapZone = m_primaryClient->getJumpZoneSize();
      if (tapZone < m_switchTwoTapZone) {
        tapZone = m_switchTwoTapZone;
      }
      // the tap zone is as far from an edge of the screen that is an inner edge as from one on the bounding box, for
      // the screen of the server when the arrangement is used; every other computer is its bounding box, as it was
      const auto *layout = m_active == m_primaryClient && m_useArrangement ? monitorLayoutFor(m_active) : nullptr;
      const bool awayFromTheEdges =
          layout != nullptr ? layout->contains(x - tapZone, y) && layout->contains(x + tapZone, y) &&
                                  layout->contains(x, y - tapZone) && layout->contains(x, y + tapZone)
                            : x >= ax + tapZone && x < ax + aw - tapZone && y >= ay + tapZone && y < ay + ah - tapZone;
      if (awayFromTheEdges) {
        // win32 can generate bogus mouse events that appear to
        // move in the opposite direction that the mouse actually
        // moved.  try to ignore that crap here.
        switch (m_switchDir) {
          using enum Direction;
        case Left:
          m_switchTwoTapArmed = (m_xDelta > 0 && m_xDelta2 > 0);
          break;

        case Right:
          m_switchTwoTapArmed = (m_xDelta < 0 && m_xDelta2 < 0);
          break;

        case Top:
          m_switchTwoTapArmed = (m_yDelta > 0 && m_yDelta2 > 0);
          break;

        case Bottom:
          m_switchTwoTapArmed = (m_yDelta < 0 && m_yDelta2 < 0);
          break;

        default:
          break;
        }
      }
    }
  }
}

void Server::stopSwitchTwoTap()
{
  m_switchTwoTapEngaged = false;
  m_switchTwoTapArmed = false;
}

bool Server::isSwitchTwoTapStarted() const
{
  return m_switchTwoTapEngaged;
}

bool Server::shouldSwitchTwoTap() const
{
  // this is the second tap if two-tap is armed and this tap
  // came fast enough
  return (m_switchTwoTapArmed && m_switchTwoTapTimer.getTime() <= m_switchTwoTapDelay);
}

void Server::startSwitchWait(int32_t x, int32_t y)
{
  stopSwitchWait();
  m_switchWaitX = x;
  m_switchWaitY = y;
  m_switchWaitTimer = m_events->newOneShotTimer(m_switchWaitDelay, this);
  LOG_VERBOSE("waiting to switch");
}

void Server::stopSwitchWait()
{
  if (m_switchWaitTimer != nullptr) {
    m_events->deleteTimer(m_switchWaitTimer);
    m_switchWaitTimer = nullptr;
  }
}

bool Server::isSwitchWaitStarted() const
{
  return (m_switchWaitTimer != nullptr);
}

uint32_t Server::getCorner(const BaseClientProxy *client, int32_t x, int32_t y, int32_t size) const
{
  assert(client != nullptr);

  // get client computer shape
  int32_t ax;
  int32_t ay;
  int32_t aw;
  int32_t ah;
  client->getShape(ax, ay, aw, ah);

  // check for x,y on the left or right
  int32_t xSide;
  if (x <= ax) {
    xSide = -1;
  } else if (x >= ax + aw - 1) {
    xSide = 1;
  } else {
    xSide = 0;
  }

  // check for x,y on the top or bottom
  int32_t ySide;
  if (y <= ay) {
    ySide = -1;
  } else if (y >= ay + ah - 1) {
    ySide = 1;
  } else {
    ySide = 0;
  }

  // if against the left or right then check if y is within size
  if (xSide != 0) {
    if (y < ay + size) {
      return (xSide < 0) ? s_topLeftCornerMask : s_topRightCornerMask;
    } else if (y >= ay + ah - size) {
      return (xSide < 0) ? s_bottomLeftCornerMask : s_bottomRightCornerMask;
    }
  }

  // if against the left or right then check if y is within size
  if (ySide != 0) {
    if (x < ax + size) {
      return (ySide < 0) ? s_topLeftCornerMask : s_bottomLeftCornerMask;
    } else if (x >= ax + aw - size) {
      return (ySide < 0) ? s_topRightCornerMask : s_bottomRightCornerMask;
    }
  }

  return s_noCornerMask;
}

void Server::stopRelativeMoves()
{
  if (m_relativeMoves && m_active != m_primaryClient) {
    // warp to the center of the active client so we know where we are
    int32_t ax;
    int32_t ay;
    int32_t aw;
    int32_t ah;
    m_active->getShape(ax, ay, aw, ah);
    m_x = ax + (aw >> 1);
    m_y = ay + (ah >> 1);
    m_xDelta = 0;
    m_yDelta = 0;
    m_xDelta2 = 0;
    m_yDelta2 = 0;
    LOG_VERBOSE("synchronize move on %s by %d,%d", getName(m_active).c_str(), m_x, m_y);
    m_active->mouseMove(m_x, m_y);
  }
}

void Server::sendOptions(BaseClientProxy *client) const
{
  OptionsList optionsList;

  // look up options for client
  const Config::ScreenOptions *options = m_config->getOptions(getName(client));
  if (options != nullptr) {
    // convert options to a more convenient form for sending
    optionsList.reserve(2 * options->size());
    for (auto [optionId, optionValue] : *options) {
      optionsList.push_back(optionId);
      optionsList.push_back(static_cast<uint32_t>(optionValue));
    }
  }

  // look up global options
  options = m_config->getOptions("");
  if (options != nullptr) {
    // convert options to a more convenient form for sending
    optionsList.reserve(optionsList.size() + 2 * options->size());
    for (auto [optionId, optionValue] : *options) {
      optionsList.push_back(optionId);
      optionsList.push_back(static_cast<uint32_t>(optionValue));
    }
  }

  // a client reads the options in order and the last one wins, so what a computer is set to is said last, after the
  // global option it overrides
  if (!sharesClipboardWith(client)) {
    optionsList.push_back(kOptionClipboardSharing);
    optionsList.push_back(0);
  }

  // send the options
  client->resetOptions();
  client->setOptions(optionsList);
}

void Server::processOptions()
{
  const Config::ScreenOptions *options = m_config->getOptions("");
  if (options == nullptr) {
    return;
  }

  bool newRelativeMoves = m_relativeMoves;
  for (auto [optionId, optionValue] : *options) {
    const OptionID id = optionId;
    const OptionValue value = optionValue;
    if (id == kOptionComputerSwitchDelay) {
      m_switchWaitDelay = 1.0e-3 * static_cast<double>(value);
      if (m_switchWaitDelay < 0.0) {
        m_switchWaitDelay = 0.0;
      }
      stopSwitchWait();
    } else if (id == kOptionComputerSwitchTwoTap) {
      m_switchTwoTapDelay = 1.0e-3 * static_cast<double>(value);
      if (m_switchTwoTapDelay < 0.0) {
        m_switchTwoTapDelay = 0.0;
      }
      stopSwitchTwoTap();
    } else if (id == kOptionRelativeMouseMoves) {
      newRelativeMoves = (value != 0);
    } else if (id == kOptionDefaultLockToComputerState) {
      m_defaultLockToComputerState = (value != 0);
    } else if (id == kOptionDisableLockToComputer) {
      m_disableLockToComputer = (value != 0);
    } else if (id == kOptionClipboardSharing) {
      m_enableClipboard = value;
      if (!m_enableClipboard) {
        LOG_INFO("clipboard sharing is disabled");
      }
    } else if (id == kOptionClipboardSharingSize) {
      if (value <= 0) {
        m_maximumClipboardSize = 0;
        LOG_INFO(
            "clipboard sharing is disabled because the "
            "maximum shared clipboard size is set to 0"
        );
      } else {
        m_maximumClipboardSize = static_cast<size_t>(value);
      }
    }
  }
  if (m_relativeMoves && !newRelativeMoves) {
    stopRelativeMoves();
  }
  m_relativeMoves = newRelativeMoves;
  m_protocol = Settings::networkProtocol();
  setClipboardTextOnly(Settings::value(Settings::Server::ClipboardTextOnly).toBool());
  const auto useArrangement = Settings::value(Settings::Server::UseArrangement).toBool();
  if (useArrangement != m_useArrangement) {
    m_useArrangement = useArrangement;
    // what is decided for the screen of the server is decided again, and said if it is another
    m_monitorGeometrySaid.erase(getName(m_primaryClient));
  }
  setMonitorGeometry(Settings::value(Settings::Server::ExperimentalMultiMonitor).toBool());
}

bool Server::sharesClipboardWith(const BaseClientProxy *client) const
{
  const auto *options = m_config->getOptions(getName(client));
  if (options == nullptr) {
    return true;
  }
  const auto option = options->find(kOptionClipboardSharing);
  return option == options->end() || option->second != 0;
}

void Server::logDisabledForComputer(ClipboardID id, const BaseClientProxy *client) const
{
  // the clipboard and the selection are told apart by id, but are one copy for the user: say it once
  if (id == kClipboardClipboard) {
    LOG_INFO("%s", formatSkipLine(id, getName(client), SkipReason::DisabledForComputer).c_str());
  } else {
    LOG_DEBUG("%s", formatSkipLine(id, getName(client), SkipReason::DisabledForComputer).c_str());
  }
}

bool Server::clipboardUnchanged(const BaseClientProxy *sender, ClipboardID id) const
{
  // 4 bytes: a clipboard of no format, which is what nothing known looks like, and is never "the same"
  const auto &known = m_clipboards[id].m_clipboardData;
  if (known.size() <= 4) {
    return false;
  }

  // what would be shared of it now, read the way onClipboardChanged() reads it
  Clipboard current;
  current.setTextOnly(m_clipboardPolicy.textOnly());
  current.setSizeLimit(m_maximumClipboardSize * 1024);
  sender->getClipboard(id, &current);
  if (m_clipboardPolicy.apply(current).action == ClipboardSharePolicy::Action::Drop) {
    return false;
  }
  return current.marshall() == known;
}

bool Server::primaryClipboardWrittenRecently(ClipboardID id) const
{
  const auto &written = m_primaryClipboardWritten[id];
  return written && std::chrono::steady_clock::now() - *written < m_clipboardEchoWindow;
}

void Server::setClipboardTextOnly(bool textOnly)
{
  if (m_clipboardPolicy.textOnly() != textOnly) {
    if (textOnly) {
      LOG_INFO("clipboard sharing is limited to text of up to %zu bytes", ClipboardSharePolicy::kTextOnlyMaxTextBytes);
    } else {
      LOG_INFO("clipboard sharing is not limited to text any more");
    }
  }

  m_clipboardPolicy = ClipboardSharePolicy(textOnly);
  for (auto &clipboard : m_clipboards) {
    clipboard.m_clipboard.setTextOnly(textOnly);
    if (textOnly) {
      // what was kept from before is held to it as well
      m_clipboardPolicy.apply(clipboard.m_clipboard);
      clipboard.m_clipboardData = clipboard.m_clipboard.marshall();
    }
  }
}

void Server::setMonitorGeometry(bool on)
{
  if (m_multiMonitor != on) {
    m_multiMonitor = on;
    m_monitorGeometrySaid.clear();
    m_reportedScreenSaid.clear();
    m_reportedMonitorsSaid.clear();
    m_crossing.known = false;
    LOG_INFO("experimental multi-monitor aware cursor mapping is %s", on ? "on" : "off");
  }
  if (!on) {
    return;
  }
  for (const auto &[name, client] : m_clients) {
    sayMonitorGeometry(client);
  }
}

const MonitorLayout *Server::monitorLayoutFor(const BaseClientProxy *client, std::string *reason) const
{
  if (!m_multiMonitor) {
    return nullptr;
  }

  if (client == m_primaryClient && m_useArrangement)
    return primaryLayout(reason);

  if (client == m_primaryClient) {
    if (reason != nullptr && m_config->getMonitorLayout(getName(client)) != nullptr) {
      *reason = "the screen of the server itself is not supported yet";
    }
    return nullptr;
  }

  // what the user set for the computer is the user's choice, and wins while it fits the screen of the computer
  // (this runs for every move of the pointer: the texts are made only when they are asked for)
  bool fits = false;
  std::string typedReason;
  const auto *typed = typedLayoutFor(client, reason != nullptr ? &typedReason : nullptr, &fits);
  if (fits) {
    if (reason != nullptr) {
      *reason = typedReason;
    }
    return typed;
  }

  // otherwise the monitors that the computer reported, which are better than the one rectangle that there is without
  std::string reportedReason;
  const auto *reported = reportedLayoutFor(client, reason != nullptr ? &reportedReason : nullptr);
  if (reported != nullptr) {
    if (reason != nullptr) {
      *reason = "reported by the computer, " + reportedReason;
      if (m_config->getMonitorLayout(getName(client)) != nullptr) {
        *reason += " (the layout that is set is not used: " + typedReason + ")";
      }
    }
    return reported;
  }

  // a report that cannot be used says why; with none, it is the layout that the user set, or the absence of one
  if (reason != nullptr) {
    *reason = reportedReason.empty() ? typedReason : reportedReason;
  }
  return nullptr;
}

const MonitorLayout *Server::typedLayoutFor(const BaseClientProxy *client, std::string *reason, bool *fits) const
{
  *fits = false;
  const auto *setting = m_config->getMonitorLayout(getName(client));
  if (setting == nullptr) {
    if (reason != nullptr) {
      *reason = "no monitor layout is set";
    }
    return nullptr;
  }
  if (!setting->parsed) {
    if (reason != nullptr) {
      *reason = "the monitor layout is not valid: " + setting->parsed.error;
    }
    return nullptr;
  }

  // the layout is of the screen that the computer reports now, or it is of another one
  const auto &layout = *setting->parsed.layout;
  const auto box = layout.boundingBox();
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  client->getShape(x, y, w, h);
  if (box.x != x || box.y != y || box.w != w || box.h != h) {
    if (reason != nullptr) {
      *reason = std::format(
          "the monitor layout does not match the screen size the computer reported (layout {},{} {}x{}, reported "
          "{},{} {}x{})",
          box.x, box.y, box.w, box.h, x, y, w, h
      );
    }
    return nullptr;
  }

  // it is the layout of the screen of the computer: the choice of the user, which says that the monitors are these
  *fits = true;

  // a rectangle is mapped as it always was
  if (layout.isPlainRectangle()) {
    if (reason != nullptr) {
      *reason = "the monitors make up a rectangle, nothing to correct";
    }
    return nullptr;
  }

  if (reason != nullptr) {
    *reason = std::format("{} monitors, {},{} {}x{}", layout.monitors().size(), box.x, box.y, box.w, box.h);
  }
  return &layout;
}

const MonitorLayout *Server::reportedLayoutFor(const BaseClientProxy *client, std::string *reason) const
{
  const auto *reported = client->reportedMonitors();
  if (reported == nullptr) {
    return nullptr;
  }

  // the report is of the screen that the computer reports now, or of another one (its shape changed after it)
  const auto &layout = reported->layout;
  const auto box = layout.boundingBox();
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  client->getShape(x, y, w, h);
  if (box.x != x || box.y != y || box.w != w || box.h != h) {
    if (reason != nullptr) {
      *reason = std::format(
          "the monitors that the computer reports do not match the screen size it reported (monitors {},{} {}x{}, "
          "screen {},{} {}x{})",
          box.x, box.y, box.w, box.h, x, y, w, h
      );
    }
    return nullptr;
  }

  // a rectangle is mapped as it always was
  if (layout.isPlainRectangle()) {
    if (reason != nullptr) {
      *reason = "the monitors make up a rectangle, nothing to correct";
    }
    return nullptr;
  }

  if (reason != nullptr) {
    *reason = std::format("{} monitors, {},{} {}x{}", layout.monitors().size(), box.x, box.y, box.w, box.h);
  }
  return &layout;
}

const MonitorLayout *Server::primaryLayout(std::string *reason) const
{
  // the monitors are asked for on every move of the pointer on the primary computer; they are only made into a layout
  // again when they are others
  const auto monitors = m_primaryClient->getMonitors();
  if (monitors.empty()) {
    if (reason != nullptr)
      *reason = "the platform does not tell the monitors of the screen of this computer";
    return nullptr;
  }
  if (!m_primaryMonitors || *m_primaryMonitors != monitors) {
    m_primaryMonitors = monitors;
    m_primaryLayout = MonitorLayout::create(monitors);
  }
  if (!m_primaryLayout) {
    if (reason != nullptr)
      *reason = "the monitors of the screen of this computer are not valid: " + m_primaryLayout.error;
    return nullptr;
  }

  const auto &layout = *m_primaryLayout.layout;
  const auto box = layout.boundingBox();
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  m_primaryClient->getShape(x, y, w, h);
  if (box.x != x || box.y != y || box.w != w || box.h != h) {
    if (reason != nullptr) {
      *reason = std::format(
          "the monitors do not match the screen size of this computer (monitors {},{} {}x{}, screen {},{} {}x{})",
          box.x, box.y, box.w, box.h, x, y, w, h
      );
    }
    return nullptr;
  }
  if (layout.isPlainRectangle()) {
    if (reason != nullptr)
      *reason = "the monitors make up a rectangle, nothing to correct";
    return nullptr;
  }

  if (reason != nullptr)
    *reason = std::format("{} monitors, {},{} {}x{}", layout.monitors().size(), box.x, box.y, box.w, box.h);
  return &layout;
}

void Server::sayReportedScreen(const BaseClientProxy *client)
{
  // the screen of the server itself is its own
  if (!m_multiMonitor || client == m_primaryClient) {
    return;
  }

  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  client->getShape(x, y, w, h);
  const auto name = getName(client);
  const auto screen = std::format("{},{} {}x{}", x, y, w, h);
  auto &said = m_reportedScreenSaid[name];
  if (said != screen) {
    said = screen;
    LOG_INFO("computer \"%s\" reports its screen: %s", name.c_str(), screen.c_str());
  }
}

void Server::sayReportedMonitors(const BaseClientProxy *client)
{
  // the screen of the server itself is its own
  if (!m_multiMonitor || client == m_primaryClient) {
    return;
  }

  const auto name = deskflow::server::loggableName(getName(client));
  const auto *reported = client->reportedMonitors();
  auto &said = m_reportedMonitorsSaid[getName(client)];
  if (reported == nullptr) {
    // a computer that never reported has nothing to take back
    if (!said.empty()) {
      said.clear();
      LOG_INFO("computer \"%s\" reports no monitors", name.c_str());
    }
    return;
  }

  const auto monitors = deskflow::monitors::formatMonitorEntries(reported->monitors);
  if (said != monitors) {
    said = monitors;
    LOG_INFO("computer \"%s\" reports %zu monitors: %s", name.c_str(), reported->monitors.size(), monitors.c_str());
  }
}

void Server::handleMonitorsReported(BaseClientProxy *client)
{
  LOG_DEBUG("computer \"%s\" reported its monitors", getName(client).c_str());
  sayMonitorGeometry(client);
}

void Server::sayMonitorGeometry(const BaseClientProxy *client)
{
  if (!m_multiMonitor) {
    return;
  }

  sayReportedScreen(client);
  sayReportedMonitors(client);

  std::string reason;
  const bool on = monitorLayoutFor(client, &reason) != nullptr;
  if (reason.empty()) {
    return;
  }

  const auto name = getName(client);
  const auto decision = std::format("{}: {}", on ? "on" : "off", reason);
  auto &said = m_monitorGeometrySaid[name];
  if (said != decision) {
    said = decision;
    LOG_INFO("monitor geometry %s for \"%s\": %s", on ? "on" : "off", name.c_str(), reason.c_str());
  }
}

void Server::sayMonitorGeometrySwitch(const BaseClientProxy *destination, int32_t x, int32_t y)
{
  const auto describe = [this](const BaseClientProxy *client) {
    int32_t sx;
    int32_t sy;
    int32_t sw;
    int32_t sh;
    client->getShape(sx, sy, sw, sh);
    return std::format("\"{}\" ({},{} {}x{})", getName(client), sx, sy, sw, sh);
  };

  const auto edge = m_crossing.known ? std::format("{} edge at {:.4f}", sideText(m_crossing.side), m_crossing.fraction)
                                     : std::string("jump");
  m_crossing.known = false;
  const auto text = std::format(
      "monitor geometry: switch {} -> {}: {}, enters at {},{}, geometry of \"{}\" {}, geometry of \"{}\" {}",
      describe(m_active), describe(destination), edge, x, y, getName(m_active),
      monitorLayoutFor(m_active) != nullptr ? "on" : "off", getName(destination),
      monitorLayoutFor(destination) != nullptr ? "on" : "off"
  );
  LOG_INFO("%s", text.c_str());
}

void Server::enterOnMonitor(
    const MonitorLayout &layout, Direction srcSide, int32_t &x, int32_t &y, int32_t minDepth
) const
{
  const auto entered = oppositeOf(srcSide);
  if (entered == Direction::NoDirection) {
    return;
  }

  // the row or the column is the one that the mapping of the computers gave; the way in is not the edge of the bounding
  // box, which may be a hole, but the edge of the monitor that has the row or the column, as far in as the pointer was
  // taken over it
  const auto box = layout.boundingBox();
  const bool alongY = srcSide == Direction::Left || srcSide == Direction::Right;
  int32_t depth = 0;
  switch (entered) {
    using enum Direction;
  case Left:
    depth = x - box.x;
    break;
  case Right:
    depth = box.right() - 1 - x;
    break;
  case Top:
    depth = y - box.y;
    break;
  default:
    depth = box.bottom() - 1 - y;
    break;
  }
  const auto at =
      layout.entryPoint(entered, layout.edgeFraction(entered, alongY ? y : x), std::max({depth, minDepth, int32_t{0}}));
  x = at.x;
  y = at.y;
}

int32_t Server::jumpZoneDepth(const MonitorLayout &layout, Direction srcSide, int32_t x, int32_t y) const
{
  const auto entered = oppositeOf(srcSide);
  if (entered == Direction::NoDirection)
    return 0;
  const bool alongY = srcSide == Direction::Left || srcSide == Direction::Right;
  const auto fraction = layout.edgeFraction(entered, alongY ? y : x);
  // the jump zone only matters where the pointer could jump out of it: the side it is entered through has a neighbour
  if (m_config->getNeighbor(getName(m_primaryClient), entered, fraction, nullptr).empty())
    return 0;
  return getJumpZoneSize(m_primaryClient);
}

void Server::onMouseMoveSecondaryOnMonitors(const MonitorLayout &layout, int32_t dx, int32_t dy)
{
  // save old position
  const int32_t xOld = m_x;
  const int32_t yOld = m_y;

  // save last delta
  m_xDelta2 = m_xDelta;
  m_yDelta2 = m_yDelta;
  m_xDelta = dx;
  m_yDelta = dy;

  // the pointer goes over the monitors, and along an edge that it is pushed against, not into a hole
  const auto moved = layout.move({xOld, yOld}, dx, dy);
  m_x = moved.position.x;
  m_y = moved.position.y;

  // The side it is pushed out through, a horizontal one before a vertical one, like it is when the computer is a
  // rectangle. The computer on that side is found from where the pointer would be beyond the edge.
  using enum Direction;
  const auto box = layout.boundingBox();
  auto dir = NoDirection;
  int32_t x = m_x;
  int32_t y = m_y;
  if (moved.restX < 0) {
    dir = Left;
    x = box.x + moved.restX;
  } else if (moved.restX > 0) {
    dir = Right;
    x = box.right() - 1 + moved.restX;
  } else if (moved.restY < 0) {
    dir = Top;
    y = box.y + moved.restY;
  } else if (moved.restY > 0) {
    dir = Bottom;
    y = box.bottom() - 1 + moved.restY;
  }

  BaseClientProxy *newComputer = m_active;
  bool jump = false;
  if (dir == NoDirection) {
    // we haven't left the computer.  if waiting and the pointer is not on the edge we're waiting on then stop waiting,
    // and arm the double tap
    if (m_switchComputer != nullptr) {
      bool clearWait;
      switch (m_switchDir) {
      case Left:
        clearWait = layout.contains(m_x - 1, m_y);
        break;

      case Right:
        clearWait = layout.contains(m_x + 1, m_y);
        break;

      case Top:
        clearWait = layout.contains(m_x, m_y - 1);
        break;

      case Bottom:
        clearWait = layout.contains(m_x, m_y + 1);
        break;

      default:
        clearWait = false;
        break;
      }
      if (clearWait) {
        noSwitch(m_x, m_y);
      }
    }
  } else {
    // try to switch computer.  get the neighbor and see if we should switch
    newComputer = mapToNeighbor(m_active, dir, x, y);
    jump = isSwitchOkay(newComputer, dir, x, y, m_x, m_y);
  }

  if (jump) {
    switchComputer(newComputer, x, y, false);
  } else if (m_x != xOld || m_y != yOld) {
    // same computer.  warp cursor if it moved.
    LOG_VERBOSE("move on %s to %d,%d", getName(m_active).c_str(), m_x, m_y);
    m_active->mouseMove(m_x, m_y);
  }
}

void Server::handleShapeChanged(BaseClientProxy *client)
{
  if (!m_clientSet.contains(client)) {
    return;
  }

  LOG_DEBUG("computer \"%s\" shape changed", getName(client).c_str());

  // the layout of the computer may not be of its screen any more, or be again
  sayMonitorGeometry(client);

  // update jump coordinate
  int32_t x;
  int32_t y;
  client->getCursorPos(x, y);
  client->setJumpCursorPos(x, y);

  // update the mouse coordinates
  if (client == m_active) {
    m_x = x;
    m_y = y;
  }

  // handle resolution change to primary computer
  if (client == m_primaryClient) {
    if (client == m_active) {
      onMouseMovePrimary(m_x, m_y);
    } else {
      onMouseMoveSecondary(0, 0);
    }
  }
}

void Server::handleClipboardGrabbed(const Event &event, BaseClientProxy *grabber)
{
  if (!m_enableClipboard || (m_maximumClipboardSize == 0)) {
    return;
  }

  // ignore events from unknown clients
  if (!m_clientSet.contains(grabber)) {
    return;
  }
  const auto *info = static_cast<const IComputer::ClipboardInfo *>(event.getData());

  // a computer that is set to not share its clipboard takes no part
  if (!sharesClipboardWith(grabber)) {
    logDisabledForComputer(info->m_id, grabber);
    return;
  }

  // ignore grab if sequence number is old.  always allow primary
  // computer to grab.
  ClipboardInfo &clipboard = m_clipboards[info->m_id];
  if (grabber != m_primaryClient && info->m_sequenceNumber < clipboard.m_clipboardSeqNum) {
    LOG_DEBUG("ignored computer \"%s\" grab of clipboard %d", getName(grabber).c_str(), info->m_id);
    return;
  }

  // The primary computer grabs the clipboard again with what is already shared when another program rewrites it
  // (Universal Clipboard and Handoff do it with what Deskflow wrote to another Mac, a clipboard manager with what it
  // saw), which is what feeds a loop of grabs and transfers. Nothing differs but the change count, so nothing is
  // announced, cleared or sent. That is only looked at when the clipboard is not about to be read anyway for the
  // computer the cursor is on, because a read costs: while the cursor is on another computer, or right after Deskflow
  // wrote the clipboard. A grab by another computer carries no data yet and is always taken.
  if (grabber == m_primaryClient && (m_active != m_primaryClient || primaryClipboardWrittenRecently(info->m_id)) &&
      clipboardUnchanged(grabber, info->m_id)) {
    if (info->m_id == kClipboardClipboard) {
      LOG_INFO("%s", formatSkipLine(info->m_id, getName(grabber), SkipReason::IdenticalContent).c_str());
    } else {
      LOG_DEBUG("%s", formatSkipLine(info->m_id, getName(grabber), SkipReason::IdenticalContent).c_str());
    }
    return;
  }

  // mark computer as owning clipboard
  LOG_DEBUG(
      "computer \"%s\" grabbed clipboard %d from \"%s\"", getName(grabber).c_str(), info->m_id,
      clipboard.m_clipboardOwner.c_str()
  );
  clipboard.m_clipboardOwner = getName(grabber);
  clipboard.m_clipboardSeqNum = info->m_sequenceNumber;
  clipboard.m_oversize = false;

  // clear the clipboard data (since it's not known at this point)
  if (clipboard.m_clipboard.open(0)) {
    clipboard.m_clipboard.empty();
    clipboard.m_clipboard.close();
  }
  clipboard.m_clipboardData = clipboard.m_clipboard.marshall();

  // tell all other computers to take ownership of clipboard.  tell the
  // grabber that it's clipboard isn't dirty.
  for (auto index = m_clients.begin(); index != m_clients.end(); ++index) {
    BaseClientProxy *client = index->second;
    if (client == grabber) {
      client->setClipboardDirty(info->m_id, false);
    } else if (sharesClipboardWith(client)) {
      client->grabClipboard(info->m_id);
    }
  }

  if (grabber == m_primaryClient && m_active != m_primaryClient) {
    LOG_DEBUG("clipboard grabbed while active computer was changed, resending clipboard data");
    onClipboardChanged(m_primaryClient, info->m_id, clipboard.m_clipboardSeqNum);
  }
}

void Server::handleClipboardChanged(const Event &event, BaseClientProxy *client)
{
  // ignore events from unknown clients
  if (!m_clientSet.contains(client)) {
    return;
  }
  const auto *info = static_cast<const IComputer::ClipboardInfo *>(event.getData());
  if (!sharesClipboardWith(client)) {
    logDisabledForComputer(info->m_id, client);
    return;
  }
  onClipboardChanged(client, info->m_id, info->m_sequenceNumber);
}

void Server::handleKeyDownEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformComputer::KeyInfo *>(event.getData());
  auto lang = AppUtil::instance().getCurrentLanguageCode();
  onKeyDown(info->m_key, info->m_mask, info->m_button, lang, info->m_computers.c_str());
}

void Server::handleKeyUpEvent(const Event &event)
{
  auto *info = static_cast<IPlatformComputer::KeyInfo *>(event.getData());
  onKeyUp(info->m_key, info->m_mask, info->m_button, info->m_computers.c_str());
}

void Server::handleKeyRepeatEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformComputer::KeyInfo *>(event.getData());
  auto lang = AppUtil::instance().getCurrentLanguageCode();
  onKeyRepeat(info->m_key, info->m_mask, info->m_count, info->m_button, lang);
}

void Server::handleButtonDownEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformComputer::ButtonInfo *>(event.getData());
  onMouseDown(info->m_button);
}

void Server::handleButtonUpEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformComputer::ButtonInfo *>(event.getData());
  onMouseUp(info->m_button);
}

void Server::handleMotionPrimaryEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformComputer::MotionInfo *>(event.getData());
  onMouseMovePrimary(info->m_x, info->m_y);
}

void Server::handleMotionSecondaryEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformComputer::MotionInfo *>(event.getData());
  onMouseMoveSecondary(info->m_x, info->m_y);
}

void Server::handleWheelEvent(const Event &event)
{
  const auto *info = static_cast<IPlatformComputer::WheelInfo *>(event.getData());
  onMouseWheel(info->m_xDelta, info->m_yDelta, info->m_source);
}

void Server::handleSwitchWaitTimeout()
{
  // ignore if mouse is locked to computer
  if (isLockedToComputer()) {
    LOG_VERBOSE("locked to computer");
    stopSwitch();
    return;
  }

  // switch computer
  switchComputer(m_switchComputer, m_switchWaitX, m_switchWaitY, false);
}

void Server::handleClientDisconnected(BaseClientProxy *client)
{
  // client has disconnected.  it might be an old client or an
  // active client.  we don't care so just handle it both ways.
  removeActiveClient(client);
  removeOldClient(client);

  // m_clients always contains the primary (server) computer, so 1 means no remote clients.
  using enum deskflow::core::ConnectionState;
  ipcSendConnectionState(m_clients.size() <= 1 ? Listening : Connected);
  sendConnectedClientsIpc();

  delete client;
}

void Server::handleClientCloseTimeout(BaseClientProxy *client)
{
  // client took too long to disconnect.  just dump it.
  LOG_INFO("forced disconnection of client \"%s\"", getName(client).c_str());
  removeOldClient(client);

  delete client;
}

void Server::handleSwitchToComputerEvent(const Event &event)
{
  const auto *info = static_cast<SwitchToComputerInfo *>(event.getData());

  ClientList::const_iterator index = m_clients.find(info->m_computer);
  if (index == m_clients.end()) {
    LOG_VERBOSE("computer \"%s\" not active", info->m_computer.c_str());
  } else {
    jumpToComputer(index->second);
  }
}

void Server::handleSwitchInDirectionEvent(const Event &event)
{
  const auto *info = static_cast<SwitchInDirectionInfo *>(event.getData());

  // jump to computer in chosen direction from center of this computer
  int32_t x = m_x;
  int32_t y = m_y;
  BaseClientProxy *newComputer = getNeighbor(m_active, info->m_direction, x, y);
  if (newComputer == nullptr) {
    LOG_VERBOSE("no neighbor %s", Config::dirName(info->m_direction));
  } else {
    jumpToComputer(newComputer);
  }
}

void Server::handleToggleComputerEvent(const Event &)
{
  // Get the list of connected computers in config order
  std::vector<std::string> computers;
  getClients(computers);

  if (computers.size() < 2) {
    LOG_ERR("not enough computers to toggle");
    return;
  }

  // Find the current active computer
  std::string currentComputer = getName(m_active);
  auto it = std::ranges::find(computers, currentComputer);
  if (it == computers.end()) {
    LOG_ERR("current computer not found in list");
    return;
  }

  // Find the next computer
  auto nextIt = it + 1;
  if (nextIt == computers.end()) {
    nextIt = computers.begin();
  }

  // Find the client for the next computer
  ClientList::const_iterator clientIt = m_clients.find(*nextIt);
  if (clientIt == m_clients.end()) {
    LOG_ERR("next computer not active");
    return;
  }

  jumpToComputer(clientIt->second);
}

void Server::handleKeyboardBroadcastEvent(const Event &event)
{
  const auto *info = static_cast<KeyboardBroadcastInfo *>(event.getData());

  // choose new state
  bool newState;
  switch (info->m_state) {
  default:
  case KeyboardBroadcastInfo::kOn:
    newState = true;
    break;

  case KeyboardBroadcastInfo::kOff:
    newState = false;
    break;

  case KeyboardBroadcastInfo::kToggle:
    newState = !m_keyboardBroadcasting;
    break;
  }

  // enter new state
  if (newState != m_keyboardBroadcasting || info->m_computers != m_keyboardBroadcastingComputers) {
    m_keyboardBroadcasting = newState;
    m_keyboardBroadcastingComputers = info->m_computers;
    LOG(
        (CLOG_DEBUG "keyboard broadcasting %s: %s", m_keyboardBroadcasting ? "on" : "off",
         m_keyboardBroadcastingComputers.c_str())
    );
  }
}

void Server::handleLockCursorToComputerEvent(const Event &event)
{
  const auto *info = static_cast<LockCursorToComputerInfo *>(event.getData());

  // choose new state
  bool newState;
  switch (info->m_state) {
  default:
  case LockCursorToComputerInfo::kOn:
    newState = true;
    break;

  case LockCursorToComputerInfo::kOff:
    newState = false;
    break;

  case LockCursorToComputerInfo::kToggle:
    newState = !m_lockedToComputer;
    break;
  }

  // enter new state
  if (newState != m_lockedToComputer) {
    m_lockedToComputer = newState;
    LOG_INFO("cursor %s current computer", m_lockedToComputer ? "locked to" : "unlocked from");

    m_primaryClient->reconfigure(getActivePrimarySides());
    if (!isLockedToComputerServer()) {
      stopRelativeMoves();
    }
  }
}

void Server::onClipboardChanged(const BaseClientProxy *sender, ClipboardID id, uint32_t seqNum)
{
  ClipboardInfo &clipboard = m_clipboards[id];

  // ignore update if sequence number is old
  if (seqNum < clipboard.m_clipboardSeqNum) {
    LOG_INFO("ignored computer \"%s\" update of clipboard %d (mis-sequenced)", getName(sender).c_str(), id);
    return;
  }

  // should be the expected client: another computer may still send what it had, because it was not told that the
  // clipboard is not its own any more (a computer that is not told of a grab of the selection)
  const auto owner = m_clients.find(clipboard.m_clipboardOwner);
  if (owner == m_clients.end() || owner->second != sender) {
    LOG_DEBUG("ignored computer \"%s\" update of clipboard %d (not the owner)", getName(sender).c_str(), id);
    return;
  }

  // get data, and of it only what is shared: a computer that is not asked for the formats that are not does not have
  // to read them either
  clipboard.m_clipboard.setTextOnly(m_clipboardPolicy.textOnly());
  clipboard.m_clipboard.setSizeLimit(m_maximumClipboardSize * 1024);
  sender->getClipboard(id, &clipboard.m_clipboard);

  const auto decision = m_clipboardPolicy.apply(clipboard.m_clipboard);
  const bool firstTime = m_clipboardRejections[id].report(decision);
  if (decision.action == ClipboardSharePolicy::Action::Drop) {
    // every switch to another computer reads the clipboard again, so a clipboard is reported once
    const std::string name = getName(sender);
    const std::string why = decision.reason == ClipboardSharePolicy::Reason::TextTooLarge
                                ? "its text is " + std::to_string(decision.textBytes) + " bytes, only text of up to " +
                                      std::to_string(ClipboardSharePolicy::kTextOnlyMaxTextBytes) + " bytes is shared"
                                : "it has no text, only text is shared";
    // the clipboard and the selection are one copy on Windows and macOS, which are read from the same pasteboard:
    // the clipboard is the one that is reported
    if (firstTime && id == kClipboardClipboard) {
      LOG_INFO("not sharing clipboard %d of \"%s\": %s", id, name.c_str(), why.c_str());
    } else {
      LOG_DEBUG("still not sharing clipboard %d of \"%s\": %s", id, name.c_str(), why.c_str());
    }
    return;
  }

  std::string data = clipboard.m_clipboard.marshall();
  if (data.size() > m_maximumClipboardSize * 1024) {
    const auto detail = std::format("{} B exceeds the {} KB limit", data.size(), m_maximumClipboardSize);
    LOG_WARN("%s", formatSkipLine(id, clipboard.m_clipboardOwner, SkipReason::OverSize, detail).c_str());
    clipboard.m_oversize = true;
    return;
  }
  clipboard.m_oversize = false;

  // ignore if data hasn't changed
  if (data == clipboard.m_clipboardData) {
    LOG_INFO("%s", formatSkipLine(id, clipboard.m_clipboardOwner, SkipReason::IdenticalContent).c_str());
    return;
  }

  // got new data
  LOG_INFO("computer \"%s\" updated clipboard %d", clipboard.m_clipboardOwner.c_str(), id);
  clipboard.m_clipboardData = data;

  // tell all clients except the sender that the clipboard is dirty
  for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
    BaseClientProxy *client = index->second;
    if (sharesClipboardWith(client)) {
      client->setClipboardDirty(id, client != sender);
    }
  }

  // send the new clipboard to the active computer
  if (sharesClipboardWith(m_active)) {
    m_active->setClipboard(id, &clipboard.m_clipboard);
    if (m_active == m_primaryClient) {
      m_primaryClipboardWritten[id] = std::chrono::steady_clock::now();
    }
  }
}

void Server::onScreensaver(bool activated)
{
  LOG_DEBUG("onScreenSaver %s", activated ? "activated" : "deactivated");

  if (activated) {
    // save current computer and position
    m_activeSaver = m_active;
    m_xSaver = m_x;
    m_ySaver = m_y;

    // jump to primary computer
    if (m_active != m_primaryClient) {
      switchComputer(m_primaryClient, 0, 0, true);
    }
  } else {
    // jump back to previous computer and position.  we must check
    // that the position is still valid since the computer may have
    // changed resolutions while the computer saver was running.
    if (m_activeSaver != nullptr && m_activeSaver != m_primaryClient) {
      // check position
      BaseClientProxy *computer = m_activeSaver;
      int32_t x;
      int32_t y;
      int32_t w;
      int32_t h;
      computer->getShape(x, y, w, h);
      int32_t zoneSize = getJumpZoneSize(computer);
      if (m_xSaver < x + zoneSize) {
        m_xSaver = x + zoneSize;
      } else if (m_xSaver >= x + w - zoneSize) {
        m_xSaver = x + w - zoneSize - 1;
      }
      if (m_ySaver < y + zoneSize) {
        m_ySaver = y + zoneSize;
      } else if (m_ySaver >= y + h - zoneSize) {
        m_ySaver = y + h - zoneSize - 1;
      }

      // jump
      switchComputer(computer, m_xSaver, m_ySaver, false);
    }

    // reset state
    m_activeSaver = nullptr;
  }

  // send message to all clients
  for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
    BaseClientProxy *client = index->second;
    client->screensaver(activated);
  }
}

void Server::onKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang, const char *computers)
{
  LOG_VERBOSE("onKeyDown id=%d mask=0x%04x button=0x%04x lang=%s", id, mask, button, lang.c_str());
  assert(m_active != nullptr);

  // relay
  if (!m_keyboardBroadcasting && IKeyState::KeyInfo::isDefault(computers)) {
    m_active->keyDown(id, mask, button, lang);
  } else {
    if (!computers && m_keyboardBroadcasting) {
      computers = m_keyboardBroadcastingComputers.c_str();
      if (IKeyState::KeyInfo::isDefault(computers)) {
        computers = "*";
      }
    }
    for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
      if (IKeyState::KeyInfo::contains(computers, index->first)) {
        index->second->keyDown(id, mask, button, lang);
      }
    }
  }
}

void Server::onKeyUp(KeyID id, KeyModifierMask mask, KeyButton button, const char *computers)
{
  LOG_VERBOSE("onKeyUp id=%d mask=0x%04x button=0x%04x", id, mask, button);
  assert(m_active != nullptr);

  // relay
  if (!m_keyboardBroadcasting && IKeyState::KeyInfo::isDefault(computers)) {
    m_active->keyUp(id, mask, button);
  } else {
    if (!computers && m_keyboardBroadcasting) {
      computers = m_keyboardBroadcastingComputers.c_str();
      if (IKeyState::KeyInfo::isDefault(computers)) {
        computers = "*";
      }
    }
    for (ClientList::const_iterator index = m_clients.begin(); index != m_clients.end(); ++index) {
      if (IKeyState::KeyInfo::contains(computers, index->first)) {
        index->second->keyUp(id, mask, button);
      }
    }
  }
}

void Server::onKeyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang)
{
  LOG(
      (CLOG_VERBOSE "onKeyRepeat id=%d mask=0x%04x count=%d button=0x%04x lang=\"%s\"", id, mask, count, button,
       lang.c_str())
  );
  assert(m_active != nullptr);

  // relay
  m_active->keyRepeat(id, mask, count, button, lang);
}

void Server::onMouseDown(ButtonID id)
{
  LOG_VERBOSE("onMouseDown id=%d", id);
  assert(m_active != nullptr);

  // relay
  m_active->mouseDown(id);
}

void Server::onMouseUp(ButtonID id)
{
  LOG_VERBOSE("onMouseUp id=%d", id);
  assert(m_active != nullptr);

  // relay
  m_active->mouseUp(id);
}

bool Server::onMouseMovePrimary(int32_t x, int32_t y)
{
  LOG_VERBOSE("onMouseMovePrimary %d,%d", x, y);

  // mouse move on primary (server's) computer
  if (m_active != m_primaryClient) {
    // stale event -- we're actually on a secondary computer
    return false;
  }

  // save last delta
  m_xDelta2 = m_xDelta;
  m_yDelta2 = m_yDelta;

  // save current delta
  m_xDelta = x - m_x;
  m_yDelta = y - m_y;

  // save position
  m_x = x;
  m_y = y;

  // get computer shape
  int32_t ax;
  int32_t ay;
  int32_t aw;
  int32_t ah;
  m_active->getShape(ax, ay, aw, ah);
  int32_t zoneSize = getJumpZoneSize(m_active);

  using enum Direction;
  auto dirh = NoDirection;
  auto dirv = NoDirection;
  int32_t xc = x;
  int32_t yc = y;
  int32_t xh = x;
  int32_t yv = y;
  // the layout of the screen of the server is only asked for when it can be used
  if (const auto *layout = m_multiMonitor && m_useArrangement ? primaryLayout() : nullptr; layout != nullptr) {
    // The screen is not a rectangle. A pointer on an exposed edge of it, which is a pixel that has no monitor zoneSize
    // away on one side, is where the pointer leaves, whether that edge is on the bounding box or not; the computer on
    // that side is found from where the pointer would be beyond the bounding box, as it always is. The system keeps
    // the pointer on a monitor, so a pointer that is pushed against an edge is on its pixel. A position that is in a
    // hole is judged from the nearest pixel of a monitor.
    const auto box = layout->boundingBox();
    const auto pixel = layout->contains(x, y) ? MonitorPoint{x, y} : layout->nearest(x, y);
    x = xc = xh = pixel.x;
    y = yc = yv = pixel.y;
    if (!layout->contains(pixel.x - zoneSize, pixel.y)) {
      xh = box.x - zoneSize;
      dirh = Left;
    } else if (!layout->contains(pixel.x + zoneSize, pixel.y)) {
      xh = box.right() - 1 + zoneSize;
      dirh = Right;
    }
    if (!layout->contains(pixel.x, pixel.y - zoneSize)) {
      yv = box.y - zoneSize;
      dirv = Top;
    } else if (!layout->contains(pixel.x, pixel.y + zoneSize)) {
      yv = box.bottom() - 1 + zoneSize;
      dirv = Bottom;
    }
  } else {
    // clamp position to computer
    if (xc < ax + zoneSize) {
      xc = ax;
    } else if (xc >= ax + aw - zoneSize) {
      xc = ax + aw - 1;
    }
    if (yc < ay + zoneSize) {
      yc = ay;
    } else if (yc >= ay + ah - zoneSize) {
      yc = ay + ah - 1;
    }

    // see if we should change computers
    // when the cursor is in a corner, there may be a computer either
    // horizontally or vertically.  check both directions.
    if (x < ax + zoneSize) {
      xh -= zoneSize;
      dirh = Left;
    } else if (x >= ax + aw - zoneSize) {
      xh += zoneSize;
      dirh = Right;
    }
    if (y < ay + zoneSize) {
      yv -= zoneSize;
      dirv = Top;
    } else if (y >= ay + ah - zoneSize) {
      yv += zoneSize;
      dirv = Bottom;
    }
  }
  if (dirh == NoDirection && dirv == NoDirection) {
    // still on local computer
    noSwitch(x, y);
    return false;
  }

  // check both horizontally and vertically
  std::array<Direction, 2> dirs = {dirh, dirv};
  std::array<int32_t, 2> xs = {xh, x};
  std::array<int32_t, 2> ys = {y, yv};
  for (int i = 0; i < 2; ++i) {
    Direction dir = dirs.at(i);
    if (dir == NoDirection) {
      continue;
    }
    x = xs.at(i);
    y = ys.at(i);
    // get jump destination
    BaseClientProxy *newComputer = mapToNeighbor(m_active, dir, x, y);

    // should we switch or not?
    if (isSwitchOkay(newComputer, dir, x, y, xc, yc)) {
      // switch computer
      switchComputer(newComputer, x, y, false);
      return true;
    }
  }

  return false;
}

void Server::onMouseMoveSecondary(int32_t dx, int32_t dy)
{
  LOG_VERBOSE("mouse move on secondary: %+d,%+d", dx, dy);

  // TODO: move this to client side and use a qt setting or cli arg instead of env var.
  const static auto adjustEnv = "DESKFLOW_MOUSE_ADJUSTMENT";
  if (const char *envVal = std::getenv(adjustEnv); envVal) {
    try {
      double multiplier = std::stod(envVal);
      dx = static_cast<int32_t>(std::round(dx * multiplier));
      dy = static_cast<int32_t>(std::round(dy * multiplier));
      LOG_VERBOSE("adjusted mouse x %.2f: %+d,%+d", multiplier, dx, dy);
    } catch (const std::exception &e) {
      LOG_ERR("invalid %s value: %s", adjustEnv, e.what());
    }
  }

  // mouse move on secondary (client's) computer
  assert(m_active != nullptr);
  if (m_active == m_primaryClient) {
    // stale event -- we're actually on the primary computer
    return;
  }

  // if doing relative motion on secondary computers and we're locked
  // to the computer (which activates relative moves) then send a
  // relative mouse motion.  when we're doing this we pretend as if
  // the mouse isn't actually moving because we're expecting some
  // program on the secondary computer to warp the mouse on us, so we
  // have no idea where it really is.
  if (m_relativeMoves && isLockedToComputerServer()) {
    LOG_VERBOSE("relative move on %s by %d,%d", getName(m_active).c_str(), dx, dy);
    m_active->mouseRelativeMove(dx, dy);
    return;
  }

  // a computer that has a layout to map the pointer by has it moved over its monitors
  if (const auto *layout = monitorLayoutFor(m_active); layout != nullptr) {
    onMouseMoveSecondaryOnMonitors(*layout, dx, dy);
    return;
  }

  // save old position
  const int32_t xOld = m_x;
  const int32_t yOld = m_y;

  // save last delta
  m_xDelta2 = m_xDelta;
  m_yDelta2 = m_yDelta;

  // save current delta
  m_xDelta = dx;
  m_yDelta = dy;

  // accumulate motion
  m_x += dx;
  m_y += dy;

  // get computer shape
  int32_t ax;
  int32_t ay;
  int32_t aw;
  int32_t ah;
  m_active->getShape(ax, ay, aw, ah);

  // find direction of neighbor and get the neighbor
  bool jump = true;
  BaseClientProxy *newComputer;
  do {
    // clamp position to computer
    int32_t xc = m_x;
    int32_t yc = m_y;
    if (xc < ax) {
      xc = ax;
    } else if (xc >= ax + aw) {
      xc = ax + aw - 1;
    }
    if (yc < ay) {
      yc = ay;
    } else if (yc >= ay + ah) {
      yc = ay + ah - 1;
    }

    Direction dir;
    using enum Direction;
    if (m_x < ax) {
      dir = Left;
    } else if (m_x > ax + aw - 1) {
      dir = Right;
    } else if (m_y < ay) {
      dir = Top;
    } else if (m_y > ay + ah - 1) {
      dir = Bottom;
    } else {
      // we haven't left the computer
      newComputer = m_active;
      jump = false;

      // if waiting and mouse is not on the border we're waiting
      // on then stop waiting.  also if it's not on the border
      // then arm the double tap.
      if (m_switchComputer != nullptr) {
        bool clearWait;
        int32_t zoneSize = m_primaryClient->getJumpZoneSize();
        switch (m_switchDir) {
        case Left:
          clearWait = (m_x >= ax + zoneSize);
          break;

        case Right:
          clearWait = (m_x <= ax + aw - 1 - zoneSize);
          break;

        case Top:
          clearWait = (m_y >= ay + zoneSize);
          break;

        case Bottom:
          clearWait = (m_y <= ay + ah - 1 + zoneSize);
          break;

        default:
          clearWait = false;
          break;
        }
        if (clearWait) {
          // still on local computer
          noSwitch(m_x, m_y);
        }
      }

      // skip rest of block
      break;
    }

    // try to switch computer.  get the neighbor.
    newComputer = mapToNeighbor(m_active, dir, m_x, m_y);

    // see if we should switch
    if (!isSwitchOkay(newComputer, dir, m_x, m_y, xc, yc)) {
      newComputer = m_active;
      jump = false;
    }
  } while (false);

  if (jump) {
    int32_t newX = m_x;
    int32_t newY = m_y;

    // switch computers
    switchComputer(newComputer, newX, newY, false);
  } else {
    // same computer.  clamp mouse to edge.
    m_x = xOld + dx;
    m_y = yOld + dy;
    if (m_x < ax) {
      m_x = ax;
      LOG_VERBOSE("clamp to left of \"%s\"", getName(m_active).c_str());
    } else if (m_x > ax + aw - 1) {
      m_x = ax + aw - 1;
      LOG_VERBOSE("clamp to right of \"%s\"", getName(m_active).c_str());
    }
    if (m_y < ay) {
      m_y = ay;
      LOG_VERBOSE("clamp to top of \"%s\"", getName(m_active).c_str());
    } else if (m_y > ay + ah - 1) {
      m_y = ay + ah - 1;
      LOG_VERBOSE("clamp to bottom of \"%s\"", getName(m_active).c_str());
    }

    // warp cursor if it moved.
    if (m_x != xOld || m_y != yOld) {
      LOG_VERBOSE("move on %s to %d,%d", getName(m_active).c_str(), m_x, m_y);
      m_active->mouseMove(m_x, m_y);
    }
  }
}

void Server::onMouseWheel(int32_t xDelta, int32_t yDelta, ScrollSource source)
{
  LOG_VERBOSE("onMouseWheel %+d,%+d source=%d", xDelta, yDelta, static_cast<int>(source));
  assert(m_active != nullptr);

  // relay
  m_active->mouseWheel(xDelta, yDelta, source);
}

bool Server::addClient(BaseClientProxy *client)
{
  std::string name = getName(client);
  if (m_clients.contains(name)) {
    return false;
  }

  // add event handlers
  m_events->addHandler(EventTypes::ComputerShapeChanged, client->getEventTarget(), [this, client](const auto &) {
    handleShapeChanged(client);
  });
  m_events->addHandler(EventTypes::ClientProxyMonitorsReported, client->getEventTarget(), [this, client](const auto &) {
    handleMonitorsReported(client);
  });
  m_events->addHandler(EventTypes::ClipboardGrabbed, client->getEventTarget(), [this, client](const auto &e) {
    handleClipboardGrabbed(e, client);
  });
  m_events->addHandler(EventTypes::ClipboardChanged, client->getEventTarget(), [this, client](const auto &e) {
    handleClipboardChanged(e, client);
  });

  // add to list
  m_clientSet.insert(client);
  m_clients.try_emplace(name, client);

  // initialize client data
  int32_t x;
  int32_t y;
  client->getCursorPos(x, y);
  client->setJumpCursorPos(x, y);

  // tell primary client about the active sides
  m_primaryClient->reconfigure(getActivePrimarySides());

  sayMonitorGeometry(client);

  return true;
}

bool Server::removeClient(BaseClientProxy *client)
{
  using enum EventTypes;
  // return false if not in list
  ClientSet::iterator i = m_clientSet.find(client);
  if (i == m_clientSet.end()) {
    return false;
  }

  // remove event handlers
  m_events->removeHandler(ComputerShapeChanged, client->getEventTarget());
  m_events->removeHandler(ClientProxyMonitorsReported, client->getEventTarget());
  m_events->removeHandler(ClipboardGrabbed, client->getEventTarget());
  m_events->removeHandler(ClipboardChanged, client->getEventTarget());

  // remove from list
  m_monitorGeometrySaid.erase(getName(client));
  m_reportedScreenSaid.erase(getName(client));
  m_reportedMonitorsSaid.erase(getName(client));
  m_clients.erase(getName(client));
  m_clientSet.erase(i);

  return true;
}

void Server::closeClient(BaseClientProxy *client, const char *msg)
{
  assert(client != m_primaryClient);
  assert(msg != nullptr);

  // send message to client.  this message should cause the client
  // to disconnect.  we add this client to the closed client list
  // and install a timer to remove the client if it doesn't respond
  // quickly enough.  we also remove the client from the active
  // client list since we're not going to listen to it anymore.
  // note that this method also works on clients that are not in
  // the m_clients list.  adoptClient() may call us with such a
  // client.
  LOG_INFO("disconnecting client \"%s\"", getName(client).c_str());

  // send message
  // FIXME -- avoid type cast (kinda hard, though)
  auto clientProxy = static_cast<ClientProxy *>(client);
  clientProxy->close(msg);

  // install timer.  wait timeout seconds for client to close.
  double timeout = 5.0;
  EventQueueTimer *timer = m_events->newOneShotTimer(timeout, nullptr);
  m_events->addHandler(EventTypes::Timer, timer, [this, client](const auto &) { handleClientCloseTimeout(client); });

  // move client to closing list
  removeClient(client);

  m_oldClients.try_emplace(client, timer);

  // if this client is the active computer then we have to
  // jump off of it
  forceLeaveClient(client);
}

void Server::closeClients(const ServerConfig &config)
{
  // collect the clients that are connected but are being dropped
  // from the configuration (or who's canonical name is changing).
  using RemovedClients = std::set<BaseClientProxy *>;
  RemovedClients removed;
  for (auto index = m_clients.begin(); index != m_clients.end(); ++index) {
    if (!config.isCanonicalName(index->first)) {
      removed.insert(index->second);
    }
  }

  // don't close the primary client
  removed.erase(m_primaryClient);

  // now close them.  we collect the list then close in two steps
  // because closeClient() modifies the collection we iterate over.
  for (auto &client : removed) {
    closeClient(client, kMsgCClose);
  }
}

void Server::removeActiveClient(BaseClientProxy *client)
{
  if (removeClient(client)) {
    forceLeaveClient(client);
    m_events->removeHandler(EventTypes::ClientProxyDisconnected, client);
    if (m_clients.size() == 1 && m_oldClients.empty()) {
      m_events->addEvent(Event(EventTypes::ServerDisconnected, this));
    }
  }
}

void Server::removeOldClient(BaseClientProxy *client)
{
  using enum EventTypes;
  OldClients::iterator i = m_oldClients.find(client);
  if (i != m_oldClients.end()) {
    m_events->removeHandler(ClientProxyDisconnected, client);
    m_events->removeHandler(Timer, i->second);
    m_events->deleteTimer(i->second);
    m_oldClients.erase(i);
    if (m_clients.size() == 1 && m_oldClients.empty()) {
      m_events->addEvent(Event(ServerDisconnected, this));
    }
  }
}

void Server::forceLeaveClient(const BaseClientProxy *client)
{
  if (const auto *active = (m_activeSaver != nullptr) ? m_activeSaver : m_active; active == client) {
    // record new position (center of primary computer)
    m_primaryClient->getCursorCenter(m_x, m_y);

    // stop waiting to switch to this client
    if (active == m_switchComputer) {
      stopSwitch();
    }

    // don't notify active computer since it has probably already
    // disconnected.
    LOG(
        (CLOG_INFO "jump from \"%s\" to \"%s\" at %d,%d", getName(active).c_str(), getName(m_primaryClient).c_str(),
         m_x, m_y)
    );

    // cut over
    m_active = m_primaryClient;

    // enter new computer (unless we already have because of the
    // screen saver)
    if (m_activeSaver == nullptr) {
      m_primaryClient->enter(m_x, m_y, m_seqNum, m_primaryClient->getToggleMask(), false);
    }
  }

  // if this computer had the cursor when the screen saver activated
  // then we can't switch back to it when the screen saver
  // deactivates.
  if (m_activeSaver == client) {
    m_activeSaver = nullptr;
  }

  // tell primary client about the active sides
  m_primaryClient->reconfigure(getActivePrimarySides());
}
