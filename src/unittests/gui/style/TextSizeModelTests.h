/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The size of text that the look has on each OS: what the system gives, what the OS says about text size, the least
// that the OS has, and the step that the person chose. No widget is made here.
class TextSizeModelTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void minimumBodyPixels_isTheBodyOfTheSettingsOfTheOs();

  void plan_data();
  void plan();

  void plan_neverMakesTheControlsSmallerThanTheyWereDrawn_andStopsAtTheLimits();
  void plan_aSystemThatIsTooSmallIsRaised_aSystemThatIsLargeEnoughIsKept();
  void plan_thePercentIsOfTheSizeThatTheOsGave_notOfTheReference();
  void plan_anUnknownPercentIsTheDefault();

  void steps_areFive_andTheDefaultIsOneOfThem();
  void snap_keepsAStepAndTakesTheDefaultForAnythingElse();
  void step_goesOneAtATime_stopsAtTheEnds_andZeroGoesBackToTheDefault();

  void osTextScale_isAMultipleBetweenOneAndTwoAndAQuarter();
  void scaledPixels_roundsAndNeverLosesASize();
};
