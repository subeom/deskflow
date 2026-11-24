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
  void companionRenderModelUsesUnifiedSilhouetteAndInternalLabel();
  void catRenderModelUsesSideProfileAndPersistentFacing();
  void companionRenderAlphaFollowsSettingAndBoundsDetail();
  void catTailUsesContinuousPhaseLaggedCurve();
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
