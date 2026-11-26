/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The fonts that the new look has for the letters that the font of the system does not have (Hangul): which ones, for
// which OS and which language, what the person can say with DESKFLOW_FONT_FAMILIES, and which weight a role gets from a
// font that has only some of them. No widget is made here.
class FontPolicyTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void parseFamilyList_isTheFamiliesInTheirOrder_withoutSpacesQuotesEmptyOnesAndRepeats_data();
  void parseFamilyList_isTheFamiliesInTheirOrder_withoutSpacesQuotesEmptyOnesAndRepeats();

  void chooseFonts_followsThePlatformTheLanguageAndTheVariable_data();
  void chooseFonts_followsThePlatformTheLanguageAndTheVariable();
  void chooseFonts_neverTouchesEnglishOrTheOtherLanguages_whateverThePlatform();
  void languageFontCandidates_areTheFontsThatTheOsShips_notOnesThatNeedAnInstall();

  void snapWeight_isTheNearestWeightThatTheFontHas_data();
  void snapWeight_isTheNearestWeightThatTheFontHas();
  void fontWeight_isWhatIsAskedUntilTheFontsAreChosen();
};
