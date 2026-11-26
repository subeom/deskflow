/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QTemporaryDir>

class ConnectionTestDialogTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void hintText_hasWordsForEveryHint();
  void statusText_saysWhatEachResultMeans();
  void statusText_warnsWhenTheConnectIsSlowerThanTheClientWaits();

  void runTest_withOpenTimedOutAndRefused_showsEachWithItsHints();
  void runTest_withAnOpenAndAFailedAddress_tellsToPutTheOneThatWorksFirst();
  void runTest_withANameThatDoesNotResolve_saysSoAndHasNoPort();
  void runTest_withNothingTyped_saysThereIsNothingToTest();
  void runTest_again_startsANewTest();
  void copySummary_givesTheSummaryToTheWriterOnly();
  void summary_isOneLineThatNamesEveryAddress();
  void clientConfigDialog_showsTheConnectTimeoutTheClientUses();

  void screenshots_ofTheResultPanel();

private:
  QTemporaryDir m_settingsDir;
};
