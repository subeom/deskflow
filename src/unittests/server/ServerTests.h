/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

namespace deskflow::core::ipc {
class CoreIpcServer;
}

class ServerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();
  void SwitchToComputerInfo_alloc_computer();
  void KeyboardBroadcastInfo_alloc_stateAndComputers();
  void WheelInfo_alloc_defaultsToUnknownSource();
  void WheelInfo_alloc_keepsExplicitSource_data();
  void WheelInfo_alloc_keepsExplicitSource();
  void wheelEvent_relaysScrollSourceToActiveClient_data();
  void wheelEvent_relaysScrollSourceToActiveClient();
  void edgeOvershoot_clampsRouteFraction();
  void namedTopology_topLeftOvershoot_selectsLeftComputer();
  void neighborTraversal_disconnectedCycleStops();
  void clipboardOff_sharesEveryFormatOfThePrimary();
  void clipboardOff_theSizeLimitStillApplies();
  void clipboardTextOnly_primaryToClient_sharesTextUpToTheLimit_data();
  void clipboardTextOnly_primaryToClient_sharesTextUpToTheLimit();
  void clipboardTextOnly_primaryToClient_sharesNothingElse_data();
  void clipboardTextOnly_primaryToClient_sharesNothingElse();
  void clipboardTextOnly_primaryToClient_stripsOtherFormats();
  void clipboardTextOnly_primaryToClient_neverReadsHtmlOrBitmap();
  void clipboardOff_primaryToClient_readsEveryFormat();
  void clipboardTextOnly_clientToPrimary_stripsOtherFormats();
  void clipboardTextOnly_clientToPrimary_sharesNothingElse_data();
  void clipboardTextOnly_clientToPrimary_sharesNothingElse();
  void clipboardTextOnly_clientToClient_forwardsTheTextOnly();
  void clipboardTextOnly_clientToClient_forwardsNothingElse_data();
  void clipboardTextOnly_clientToClient_forwardsNothingElse();
  void clipboardTextOnly_turnedOnLater_stripsWhatIsCached();

private:
  Log m_log;
  QTemporaryDir m_settingsDir;
  // The Server tells the GUI about its connections through the core IPC server, which has to exist. It never listens:
  // that would take over the socket of a Deskflow that is running.
  deskflow::core::ipc::CoreIpcServer *m_ipcServer = nullptr;
};
