/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class LayoutMatcherTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void equalSignatures_areExact();
  void theSameDisplaysInAnotherArrangement_areSimilar();
  void aDifferentMainDisplay_costsAFactor();
  void aStandInOfTheSameKind_isSimilar();
  void aDifferentMonitorOfAnotherKind_isNotSimilar();
  void theSameMonitorInAnotherMode_isNearlyTheSame();
  void aSubsetOrASupersetCostsWhatItLacks_data();
  void aSubsetOrASupersetCostsWhatItLacks();
  void theThresholdIsInclusive();
  void anonymousDisplaysAreComparedByTheirKind();
  void nothingIsNeverSimilar();
  void bestMatch_takesTheExactOneEvenWhenASimilarOneIsNewer();
  void bestMatch_tiesGoToTheNewestUse_thenTheNewestMake_thenTheSmallestId();
  void bestMatch_aScoreBelowTheThresholdIsNothing();
  void bestMatch_doesNotDependOnTheOrderOfTheCandidates();
  void property_scoreIsBetweenZeroAndOne_andOneOnlyForEqualKeys();
  void property_aSituationIsExactlyLikeItself_andTheMatchIsDeterministic();
};
