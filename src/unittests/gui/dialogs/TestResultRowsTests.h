/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The results of the tests of the connection as rows (the new look): what each row says, what is hidden until it is
// asked for, that the system look keeps its text, and the rows of a report of the Cloudflare Tunnel. Nothing opens a
// connection: the probe is a made-up network.
class TestResultRowsTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void cleanup();

  void rows_haveAChipAndASentence_andWhatToTryIsHiddenUntilItIsAskedFor();
  void rows_keepWhatWasOpened_whileTheTestGoesOn();
  void connectionDialog_newLook_showsRowsInThePlaceOfTheText();
  void connectionDialog_systemLook_keepsItsText();
  void connectionDialog_aSlowConnect_isAWarning();
  void connectionDialog_noAddress_saysWhatToDo();
  void tunnelDialog_rowsFollowTheStagesAndGiveWhatToTryToTheOneThatFailed();
  void tunnelDialog_aFailureNoStageNames_isUnderTheRows();

private:
  QTemporaryDir m_dir;
};
