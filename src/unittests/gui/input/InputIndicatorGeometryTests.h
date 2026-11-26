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
  void followApproachUsesExactRectangleSupportAtEightHeadings();
  void followApproachFallbackAndFixedModesStayCompatible();
  void followApproachClampsEveryScreenEdgeAndCorner();
  void followApproachUsesLastHeadingWhileIdle();
  void followApproachHeadingChangeTraversesIntermediatePositions();
  void followApproachFirstShowUsesBottomRight();
  void followApproachSettlesOppositeCommandedTravel();
  void followApproachPreservesStateAcrossLiveChanges();
  void followApproachCompanionSpeedRasterMatrix();
  void unknownOrInvalidGeometryHides_data();
  void unknownOrInvalidGeometryHides();
  void compactPanelStyleUsesApprovedMetrics();
  void companionDisplayModelScalesAllLogicalGeometry();
  void companionFacingTurnCompressesAndSwitchesOnce();
  void companionVisualLeanStaysUprightAtEightHeadings();
  void companionRasterStaysUprightAtEightHeadings();
  void companionRenderModelUsesUnifiedSilhouetteAndInternalLabel();
  void catRenderModelUsesSideProfileAndPersistentFacing();
  void catTorsoContourExpandsWithoutPadPrimitive();
  void catArtworkFramesFollowApprovedPoseSheet();
  void catArtworkMetricsMatchApprovedSource();
  void catLabelLayoutFitsEveryArtworkPoseAndSource();
  void coreTextInkBoundsExposeGlyphOrigin();
  void coreTextCatGlyphPixelsStayInsideSilhouette();
  void catStartleRenderKeepsLabelAndCanvasStable();
  void catIdleTailMotionChangesOnlyTailPixels();
  void catPanelIdleTailFrameRepaints();
  void catPoseUsesDistinctSeatedWalkRunGeometry();
  void companionPaletteUsesNeutralBodyAndReadableGlyph();
  void companionRenderAlphaFollowsSettingAndBoundsDetail();
  void catTailUsesContinuousPhaseLaggedCurve();
  void catIdleTailMotionsCoverFiveFinitePatterns();
  void catIdleTailMotionGatesMovementAndReducedMotion();
  void catIdleTailMotionIsSeededAndFacingDeterministic();
  void catSourceStartleTimelineUsesFiniteNeutralEndpoints();
  void catSourceStartleCoalescesLatestSourceAtFiftyMilliseconds();
  void catSourceStartleRejectsNonEventsAndReducedMotion();
  void catSourceStartleWarpsOnlyApprovedDorsalRegion();
  void catPanelSourceChangesTriggerOnlyOneStartle();
  void catStartleReplacesHaloAndResumesIdleTail();
  void dogAndTurtleStartleOnSourceChange();
  void catPursuitBoundsLargeJumpAndNeverTeleports();
  void companionPursuitScalesCapsWithoutChangingGates();
  void catPursuitWalksNearAndSettlesWithoutOvershoot();
  void companionModelUsesFiveSharedIdlePatterns();
  void companionMotionPoseDistinguishesDogAndTurtleGaits();
  void dogAndTurtleReactionPointsReturnToNeutral();
  void companionHeadingTransformsBodyButNotGlyphBounds();
  void companionArtworkUsesContinuousSpeciesSilhouettes();
  void dogAndTurtleIdleAndGaitPixelsChangeAtOneAndTwoX();
  void dogAndTurtleReactionAndHaloPixelsUseOneLayer();
  void allCompanionCanvasContainsEightHeadings();
  void dogAndTurtleIdlePatternsRepaint();
  void dogAndTurtleReactionCoalescesAndEnds();
  void allCompanionPanelsHonorHeadingAndGlyphCoverage();
  void catPursuitHandlesEightHeadingsDeterministically();
  void catPursuitRejectsInvalidAndHonorsReducedMotion();
  void catPursuitTreatsPointerAndCaretTargetsIdentically();
  void catPursuitPreservesInitializedStateAcrossRepresentationChanges();
  void catPursuitPanelTraversesLargeTargetJump();
  void catDefaultFixedModeHonorsPursuitHeadingTurnLimit();
  void companionPursuitPanelHonorsFollowSpeed();
  void companionPursuitPanelSpeedChangePreservesPosition();
  void companionPursuitPanelPreservesPositionOnSourceAndSpaceChange();
  void catPursuitPanelSettlesWithinWholePointOfFractionalTargets();
  void catPursuitPanelPreservesPositionOnSourceAndSpaceChange();
  void catPursuitRotatesSilhouetteButKeepsGlyphUpright();
  void catPursuitCanvasContainsEightHeadings();
  void catPursuitCaretTargetFollowsSamePath();
  void catStartleFurReachesTwoPointFourWithoutChangingTiming();
  void catStartleGlowUsesOneBoundedEnvelope();
  void catStartleLatestSourceSelectsGlowColor();
  void catStartleFurAndGlowRespectReducedMotion();
  void catStartleFurIsVisiblyStrongerThanOldPeak();
  void catStartleGlowIsSingleColoredAndBehindBody();
  void catStartleFurAndGlowStayInsideRotatingCanvas();
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
