/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class DesignTokensTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void contrast_blackOnWhiteIs21_andARatioNeverDependsOnTheOrder();
  void contrast_aKnownPair_isTheWcagValue();
  void over_mixesTheLayerWithWhatIsBehindIt();
  void ensureContrast_changesAColourOnlyAsFarAsItNeedsTo();
  void osColors_areTheOnesOfThePaletteAndEverythingElseIsReadable_data();
  void osColors_areTheOnesOfThePaletteAndEverythingElseIsReadable();

  void textPairs_meetTheContrastOfWcagAa_data();
  void textPairs_meetTheContrastOfWcagAa();
  void controlBorder_meetsThreeToOneOnADensePanel_data();
  void controlBorder_meetsThreeToOneOnADensePanel();
  void tints_carryTheirTextAtAaContrast_data();
  void tints_carryTheirTextAtAaContrast();

  void opaqueLooks_haveNoGradientNoAlphaAndTheTextStaysReadable_data();
  void opaqueLooks_haveNoGradientNoAlphaAndTheTextStaysReadable();
  void highContrast_takesTheColoursOfTheOs();
  void motion_isInstantWhenTheOsAsksForLess();

  void palette_hasEveryRoleOfTheTokens_data();
  void palette_hasEveryRoleOfTheTokens();
  void palette_textIsReadableInEveryGroup_data();
  void palette_textIsReadableInEveryGroup();
};
