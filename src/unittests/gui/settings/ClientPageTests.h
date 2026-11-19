/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The page of the client in the settings of the new look: the settings that the dialog "Client Configuration" has, and
// its group of the Cloudflare Tunnel. The secure store is one in memory: none of these tests reaches the keychain or
// the Credential Manager, runs cloudflared or opens a connection.
class ClientPageTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void page_hasNoEntryOfItsOwn_andTheNetworkPageAsksForIt();
  void page_isAfterTheNetworkPage_andBindsTheKeysOfTheDialog();
  void page_writesWhatTheDialogWritesForTheSameChanges();
  void page_isForAClient_aServerSeesItDimmedWithANote();
  void page_clearsTheListOfServers();
  void tunnel_theGroupIsMadeWhenThePageIsShown_andTheStoreIsNotAskedForIt();
  void tunnel_isSavedWithTheButton_asTheDialogSavesIt();
  void tunnel_whatCannotBeSavedWritesNothingAndSaysWhy();
  void tunnel_resetTakesTheStoredValues_andRestoreDefaultsTheDefaults();
  void tunnel_theButtonsFollowWhatIsEntered();

private:
  QTemporaryDir m_dir;
};
