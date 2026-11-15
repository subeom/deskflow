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
  void setClipboardDirty_dropsTheRunningTransferOfThatId();
  void clientGrab_fromTheServerDropsTheRunningUpload();
  void idPolicy_onlyXWindowsStylePeersExchangeTheSelection_data();
  void idPolicy_onlyXWindowsStylePeersExchangeTheSelection();
  void serverToClient_nonX11Peer_getsOnlyClipboardIdZeroData_data();
  void serverToClient_nonX11Peer_getsOnlyClipboardIdZeroData();
  void serverToClient_x11OrUnknownPeer_getsBothIds_data();
  void serverToClient_x11OrUnknownPeer_getsBothIds();
  void serverToClient_nonX11Peer_stillGetsTheGrabOfBothIds();
  void serverToClient_beforeTheCapabilities_keepsBothIds();
  void clientToServer_nonX11ServerThatMirrors_getsOnlyClipboardIdZeroData();
  void clientToServer_serverWithoutTheCapabilityOrX11Server_getsBothIds_data();
  void clientToServer_serverWithoutTheCapabilityOrX11Server_getsBothIds();
  void clientToServer_skippedIdIsReportedByTheProxy();
  void mirror_nonX11ClientThatSendsOneId_isAlsoTheSelection();
  void mirror_peerWithoutTheCapabilityOrX11Peer_isNotMirrored_data();
  void mirror_peerWithoutTheCapabilityOrX11Peer_isNotMirrored();
  void proofOfLife_negotiated_anyReceivedDataResetsTheAlarm();
  void proofOfLife_notNegotiated_onlyKeepAlivesResetTheAlarm();
  void proofOfLife_peerWithoutTheCapability_isUnchanged();

private:
  Log m_log;
  QTemporaryDir m_settingsDir;
};
