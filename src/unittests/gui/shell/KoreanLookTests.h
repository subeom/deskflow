/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTemporaryDir>
#include <QTest>

// The new look in Korean, with the translation that is shipped (the .qm that the build makes of deskflow_ko.ts): the
// screens of the new look ask for no text that the translation does not have (what a screen asks for and is not
// translated is said by name), the sidebar says what the translation says, and the window is not squeezed or cut by
// the longer words, at the narrow widths and the larger text. The log lines are not texts of the translation (they are
// for finding out why a window is what it is): they are not asked for.
class KoreanLookTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void translation_isShippedAndTranslatesTheWordsOfTheShell();

  void mainWindow_asksForNothingThatIsNotTranslated_data();
  void mainWindow_asksForNothingThatIsNotTranslated();
  void mainWindow_everyStateOfTheCoreAsksForNothingThatIsNotTranslated();
  void settingsPages_askForNothingThatIsNotTranslated();
  void dialogs_askForNothingThatIsNotTranslated();
  void tunnelTexts_everyFailureAndStateAsksForNothingThatIsNotTranslated();

  void sidebar_saysWhatTheTranslationSays();

  void mainWindow_fitsAtEveryWidthAndTextStep_inKorean_data();
  void mainWindow_fitsAtEveryWidthAndTextStep_inKorean();

private:
  QTemporaryDir m_dir;
};
