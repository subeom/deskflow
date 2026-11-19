/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

// The Cloudflare Tunnel group inside the dialog that configures the client, with a store in memory and nothing of the
// system: what the dialog writes when the tunnel is not used is what it always wrote.
class ClientConfigDialogTunnelTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void save_withTheTunnelUntouched_writesExactlyWhatTheDialogWroteBefore();
  void save_withTheTunnelUntouched_leavesTheSavedAddressesAndTheTunnelKeysAlone();
  void save_withATunnelThatCannotBeSaved_staysOpenAndWritesNothing();
  void save_withATunnel_writesTheGroupAndAcceptsTheDialog();
  void save_thatTheStoreRefuses_staysOpen();
  void restoreDefaults_resetsTheGroupToo();
  void theGroup_isModifiedMakesTheDialogSaveable();
  void openingTheDialogWithTheTunnelOff_andAcceptingItWithoutTheToken_neverAsksTheSecureStore();
};
