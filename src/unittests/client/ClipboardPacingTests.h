/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

class ClipboardPacingTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void handshake_bothSidesAdvertiseTheClipboardCapability();
  void serverToClient_notNegotiated_keepsTheLegacyBytes_data();
  void serverToClient_notNegotiated_keepsTheLegacyBytes();
  void serverToClient_negotiated_isPaced();
  void serverToClient_pacedStreamIsAcceptedByTheRealServerProxy();
  void serverToClient_pacedStreamWithInputInBetweenStillArrivesIntact();
  void clientToServer_notNegotiated_keepsTheLegacyBytes();
  void clientToServer_negotiated_isPaced();
  void clientToServer_pacedStreamIsAcceptedByTheRealClientProxy();
  void grabClipboard_dropsTheRunningTransferOfThatId();
  void proofOfLife_negotiated_anyReceivedDataResetsTheAlarm();
  void proofOfLife_notNegotiated_onlyKeepAlivesResetTheAlarm();
  void proofOfLife_peerWithoutTheCapability_isUnchanged();

private:
  Log m_log;
  QTemporaryDir m_settingsDir;
};
