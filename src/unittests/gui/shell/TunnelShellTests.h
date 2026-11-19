/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The Cloudflare Tunnel in the main window of the new look: where it is said (the card of the connection, the chip of
// the state, the tray) and that nothing of it is there while it is not used. The core, cloudflared and the secure store
// of the system are never started or called (the store is the one in memory).
class TunnelShellTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void tunnelChip_isAFewWordsAndSaysHowTheTunnelIs_data();
  void tunnelChip_isAFewWordsAndSaysHowTheTunnelIs();

  void tunnelOff_nothingIsAddedToTheShell();
  void aServer_hasNoLineOfTheTunnel_whateverTheSettingsSay();
  void tunnelOn_theAddressIsTheLineOfThePath_andTheStateIsInTheCardTheChipAndTheTray();
  void tunnelOn_theLinesFollowTheChoiceThatIsMadeAndUnmade();
  void tunnelOn_aFailureIsTheColourOfAProblem_inTheLineAndInTheChip();
  void tunnelOn_theLinkToTheServerIsInTheChip();
  void tunnelOn_theWindowIsNotWiderThanItPromises_withALongHostnameAndLargerText();
  void systemLook_hasTheLinesWhereTheyAlwaysWere();
  void tunnelStatusWidget_saysThePathLineToo();

private:
  QTemporaryDir m_dir;
};
