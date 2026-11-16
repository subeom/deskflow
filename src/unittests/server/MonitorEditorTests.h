/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class MonitorEditorTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void validation_data();
  void validation();
  void presets_andAspects();
  void withScale_keepsThePixels_andTheSeamThatCanBeKept();
  void withResolution_andRotated_keepTheEdgesThatTouch();
  void addedAndRemoved_andMain();

  void move_snapsToEdgesAndMiddles_data();
  void move_snapsToEdgesAndMiddles();
  void move_neverGoesOverAnotherMonitor();
  void resize_everyHandle_keepsTheOppositeEdges_data();
  void resize_everyHandle_keepsTheOppositeEdges();
  void resize_edgesSnapToTheEdgesOfTheOthers();
  void resize_sidesSnapToTheCommonRatios();
  void resize_sizesSnapToThePresetsAndTheTarget();
  void resize_aPresetSnapOfASingleEdge_movesOnlyThatSide_data();
  void resize_aPresetSnapOfASingleEdge_movesOnlyThatSide();
  void resize_withTheAspectLocked_theOtherSideFollows();
  void resize_neverSmallerThanTheMinimum_andNeverOverAnother();
  void resize_aSingleRectangleAmongObstacles();

  void realise_cases_data();
  void realise_cases();
  void realise_aShapeThatDiffers_isSaid_andCanBeStretched();
  void realise_keepsTheDrawingWhereItWas();
  void realise_aLayoutThatIsNotValid_isNotMade();
  void realise_aZoomThatCannotBeUsed_isNotMade();
  void fromStored_usesTheScalesThatAreStored_andTheZoomForTheRest();

  void property_fitToTheReportedBox_isExact_andHasNoGapAndNoOverlap();
  void property_realise_isIdempotent_andRoundTrips();
  void property_editing_neverCreatesAnOverlap();

  void history_goesBackAndForward();
};
