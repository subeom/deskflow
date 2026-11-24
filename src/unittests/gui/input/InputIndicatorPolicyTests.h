/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class InputIndicatorPolicyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void safeStateShowsKnownSource();
  void unknownAndNotEditableFocusCanShow_data();
  void unknownAndNotEditableFocusCanShow();
  void sourceTriggerWithoutActivityShows();
  void coreTriggerWithoutActivityHidesWithReason();
  void nonCoreTriggerSafetyHides_data();
  void nonCoreTriggerSafetyHides();
  void accessibilityProbeRejectsOwnApplication();
  void accessibilityProbeAcceptsExternalApplication();
#ifdef Q_OS_MACOS
  void accessibilityCandidateUsesFrontmostWhenSystemLookupFails();
  void accessibilityCandidatesAvoidDuplicatePids();
  void accessibilityCandidatesIgnoreStaleSystemPid();
  void accessibilityCandidatesRequireFrontmostPid();
  void accessibilityUnavailableMetadataKeepsTargetIdentity();
  void accessibilitySnapshotRejectsStalePid();
  void accessibilitySnapshotRequiresFrontmostPid();
  void accessibilitySnapshotAcceptsMatchingFrontmostPid();
  void panelPresentationNeverRequestsCursorMutation();
  void panelPresentationNeededAfterActiveSpaceChange();
  void persistentPolicyUsesOnlyEnabledLocalAndKnownSource();
  void persistentKnownSourceShowsWithoutConnectivityOrAx();
  void persistentOffComputerHides();
  void persistentLocalAndDisconnectedTransitionsShow();
  void persistentUnknownSourceHidesWithoutStaleLabel();
  void persistentPointerTicksMoveWithoutReconfigure();
  void persistentDuplicateSourceDoesNotReconfigure();
  void persistentSourceChangesUpdateLabel();
  void persistentCompanionChoiceUpdatesPanel();
  void persistentFeatureOffStopsAndHides();
  void persistentGeometryOrPanelFailureHides();
  void persistentActivityDoesNotGateVisibility();
  void completePointerMonitorUsesEventRendering();
  void incompletePointerMonitorKeepsFallbackTimer();
  void pointerEventsCoalesceWhileRenderTimerIsActive();
  void persistentIdleCompanionUsesLowDutyTimer();
  void persistentWalkAndRunUseRenderTimer();
  void persistentHiddenAndReducedMotionStopTimers();
#endif
  void visibilityDecisionReportsHideReason();
  void diagnosticThrottleLimitsToOnePerFiveSeconds();
  void unsafeStateHides_data();
  void unsafeStateHides();
  void staleEventHides();
  void invalidDwellHides();
};
