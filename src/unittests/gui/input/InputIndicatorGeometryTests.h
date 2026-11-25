/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class InputIndicatorGeometryTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void bottomRightOffsetsCursor();
  void positionsFlipInsideScreen();
  void choosesScreenContainingCursor();
  void everyPositionIsPointerRelative_data();
  void everyPositionIsPointerRelative();
  void unknownOrInvalidGeometryHides_data();
  void unknownOrInvalidGeometryHides();
  void compactPanelStyleUsesApprovedMetrics();
  void companionDisplayModelScalesAllLogicalGeometry();
  void companionFacingTurnCompressesAndSwitchesOnce();
  void companionRenderModelUsesUnifiedSilhouetteAndInternalLabel();
  void catRenderModelUsesSideProfileAndPersistentFacing();
  void catTorsoContourExpandsWithoutPadPrimitive();
  void catArtworkFramesFollowApprovedPoseSheet();
  void catArtworkMetricsMatchApprovedSource();
  void catLabelLayoutFitsEveryArtworkPoseAndSource();
  void coreTextInkBoundsExposeGlyphOrigin();
  void coreTextCatGlyphPixelsStayInsideSilhouette();
  void catPoseUsesDistinctSeatedWalkRunGeometry();
  void companionPaletteUsesNeutralBodyAndReadableGlyph();
  void companionHaloPulsesStayBoundedAndReducedMotionSafe();
  void companionRenderAlphaFollowsSettingAndBoundsDetail();
  void catTailUsesContinuousPhaseLaggedCurve();
  void catIdleTailMotionsCoverFiveFinitePatterns();
  void catIdleTailMotionGatesMovementAndReducedMotion();
  void catIdleTailMotionIsSeededAndFacingDeterministic();
  void companionSettingDefaultsToCat();
  void motionOffsetClampsAndRespectsReducedMotion();
  void transitionProgressEasesAndRespectsReducedMotion();
  void companionMotionStateClassifiesIdleWalkRun();
  void companionMotionStateKeepsMovingForSettleDelay();
  void companionMotionPhaseWrapsPerState();
  void companionMotionPoseHonorsBounds();
  void companionMotionFrameIntervalsStayLowDuty();
  void followerCatchesUpWithoutOvershoot();
  void followerCapsTrailingLag();
  void followerRejectsInvalidInput();
};
