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
  void panelPresentationRequestsFrontRegardless();
  void focusChangeInvalidatesWithoutQueueingProbe();
  void sourceTriggerWithoutActivityQueuesProbe();
  void coreTriggerWithoutActivityDoesNotQueueProbe();
  void duplicateSourceNotificationDoesNotQueueProbe();
  void sourceTransitionQueuesProbe();
  void unknownSourceTransitionHidesWithoutProbe();
  void unknownFocusCanShowAfterFreshCoreActivity();
  void secureFocusNeverShows();
  void ownApplicationNeverRequestsProbe();
  void staleProbeGenerationNeverShows();
  void mismatchedSnapshotPidNeverShows();
  void invalidActivityDoesNotCancelSourceTrigger();
#endif
  void visibilityDecisionReportsHideReason();
  void diagnosticThrottleLimitsToOnePerFiveSeconds();
  void unsafeStateHides_data();
  void unsafeStateHides();
  void staleEventHides();
  void invalidDwellHides();
};
