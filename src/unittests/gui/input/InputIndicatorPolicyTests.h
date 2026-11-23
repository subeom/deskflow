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
  void focusTriggerWithoutActivityShows();
  void sourceTriggerWithoutActivityShows();
  void coreTriggerWithoutActivityHidesWithReason();
  void focusTriggerGenerationMismatchHides();
  void nonCoreTriggerSafetyHides_data();
  void nonCoreTriggerSafetyHides();
#ifdef Q_OS_MACOS
  void accessibilityCandidateUsesFrontmostWhenSystemLookupFails();
  void accessibilityCandidatesAvoidDuplicatePids();
  void accessibilityCandidatesIgnoreStaleSystemPid();
  void accessibilityCandidatesRequireFrontmostPid();
  void accessibilitySnapshotRejectsStalePid();
  void accessibilitySnapshotRequiresFrontmostPid();
  void accessibilitySnapshotAcceptsMatchingFrontmostPid();
  void panelPresentationRequestsFrontRegardless();
  void focusTriggerWithoutActivityQueuesProbe();
  void sourceTriggerWithoutActivityQueuesProbe();
  void coreTriggerWithoutActivityDoesNotQueueProbe();
  void invalidActivityDoesNotCancelFocusTrigger();
#endif
  void visibilityDecisionReportsHideReason();
  void diagnosticThrottleLimitsToOnePerFiveSeconds();
  void unsafeStateHides_data();
  void unsafeStateHides();
  void staleEventHides();
  void invalidDwellHides();
};
