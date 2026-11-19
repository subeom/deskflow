/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class TunnelTestDialogTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void everyStageIsShownAsItHappens();
  void aFailure_isMarkedWithWhatToDoAboutIt();
  void aTunnelThatWorks_saysSoAndAllowsTheSummaryToBeCopied();
  void copy_writesTheSummaryToTheWriterOnly();
  void runAgain_startsANewTest();
  void everyFailureHasAHint();
  void theDialog_asksForARoomThatFitsAScreen_data();
  void theDialog_asksForARoomThatFitsAScreen();
  void newLook_showsTheStagesAsRows_andTheSystemLookKeepsItsText();

private:
  QString m_dir;
};
