/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class DisplaySignatureTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void identity_isTheUuid_thenTheNumbers_thenNothing();
  void sameClass_isBuiltInResolutionAndScale();
  void sameDisplaysInAnyOrder_areTheSameSignature_data();
  void sameDisplaysInAnyOrder_areTheSameSignature();
  void aMirroredDisplay_isCountedOnce();
  void displaysThatAreNotValid_areLeftOut();
  void everyPartOfTheSituation_changesTheKey_data();
  void everyPartOfTheSituation_changesTheKey();
  void theLabel_isNotPartOfTheKey();
  void theId_isShortStableAndHidesTheHardware();
  void moreThanTheMaximum_isCutTheSameWayWhateverTheOrder();
  void monitors_areTheRectanglesOfTheDisplays();
  void summary_describesTheSituationWithoutTheHardwareIds();
  void json_roundTrips();
  void json_refusesWhatIsNotASignature_data();
  void json_refusesWhatIsNotASignature();
  void key_isTheSameWhateverTheLocale();
};
