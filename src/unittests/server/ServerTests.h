/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

class ServerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
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

private:
  Log m_log;
  QTemporaryDir m_settingsDir;
};
