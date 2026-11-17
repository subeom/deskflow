/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

class LayoutStoreTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void init();

  // the layout of the computers
  void layout_roundTripsThroughJson();
  void layout_find_ignoresTheCase();
  void layout_sameAs_ignoresTheOrder_andAMillionth();
  void layout_refusesWhatIsNotALayout_data();
  void layout_refusesWhatIsNotALayout();

  // the profiles
  void store_aMissingFile_isAnEmptyStore();
  void upsert_makesOneProfileForEachSignature();
  void upsert_changesTheLayoutOnlyWhenItIsAnother();
  void upsert_refusesWhatIsNotValid();
  void upsert_namesAProfile_orSuggestsTheName();
  void suggestName_listsTheDisplays();
  void rename_trimsCapsAndRefuses();
  void remove_removesOnlyThatProfile();
  void touch_marksTheUse();
  void candidates_listTheProfilesForTheMatcher();
  void ids_areNeverUsedTwice();

  // the file
  void save_and_load_roundTrip();
  void active_isKeptInTheFile();
  void load_aFileThatIsNotAStore_isDamagedAndMovedAside_data();
  void load_aFileThatIsNotAStore_isDamagedAndMovedAside();
  void load_aNewerFile_isInertAndLeftAlone();
  void load_invalidEntries_areDroppedAndTheFileIsKept();
  void load_duplicateSignatures_keepTheNewest();
  void load_moreThanTheMaximum_keepsTheNewest();
  void load_aFileThatIsTooBig_isDamaged();
  void save_replacesTheFileAtomically();
  void save_failsWhenTheFolderCannotBeWritten();

  // too many profiles
  void evict_removesTheLeastRecentlyUsedOneThatTheUserDidNotName();
  void evict_removesNamedOnesOnlyWhenThereIsNothingElse();
  void evict_neverRemovesTheActiveOrTheNewProfile();
  void evict_followsTheUse();

  void fuzz_aDamagedFileNeverBreaksTheStore();

private:
  QString path() const
  {
    return m_dir.filePath(QStringLiteral("layout-profiles.json"));
  }
  QTemporaryDir m_dir;
};
