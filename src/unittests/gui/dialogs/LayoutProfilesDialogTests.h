/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>
#include <QTemporaryDir>

class LayoutProfilesDialogTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  // the notice bar
  void noticeBar_isHiddenUntilItHasSomethingToSay();
  void noticeBar_showsTheTextWithUndoOnlyWhenItCanBeUndone();
  void noticeBar_cutsALongTextToItsWidthAndKeepsItAllInTheToolTip();
  void noticeBar_tellsWhatTheUserClicked();

  // the list of the remembered layouts
  void dialog_listsTheLayouts_theNewestUseFirst_andMarksTheOneThatIsCurrent();
  void dialog_renamesForgetsAndSaves_asAtOnce();
  void dialog_whenTheFeatureIsOff_isEmptyAndCannotChangeAnything();
  void dialog_saysWhatTheDisplaysAre();
  void dialog_readsTheListAgainWhenTheLayoutsChange();

  // the arrangement panel with the service of the program
  void panel_showsWhatWasDone_andUndoTakesItBackInTheCopyOfTheDialog();
  void panel_dismissHidesTheNoticeForGood();
  void panel_hasNoNoticeAndNoListWithoutAService();

private:
  QTemporaryDir m_dir;
};
