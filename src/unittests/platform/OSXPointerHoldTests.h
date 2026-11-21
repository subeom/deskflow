/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

// The hold of the pointer of a Mac server while it is on a client: the policy, the log line and the controller, with a
// fake system and a fake source of notifications.  Nothing here makes an event, moves or hides the real cursor, or
// talks to the real event tap.
class OSXPointerHoldTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  // the policy
  void policy_whenNotParked_doesNothing();
  void policy_whenUnmoved_doesNothing();
  void policy_withinTheTolerance_doesNothing();
  void policy_whenMoved_holdsAtTheParkedPlace();
  void policy_aNotification_reassertsTheCaptureEvenWhenUnmoved_withoutALine();
  void policy_justAfterAWarp_doesNotWarpTwiceForTheSameMovement();
  void policy_aNotificationIsNotHeldBackBySettling();
  void policy_aDisplayReconfiguration_acceptsTheNewPlaceForAMoment();
  void policy_aTapThatIsOff_isSwitchedOnAtMostOncePerInterval();
  void policy_aTapDisabledByUserInput_isSwitchedOnEvenWhenItLooksOn();
  void policy_aShownCursor_isHiddenAgainAtMostAFewTimesPerStay();
  void policy_theLine_isWrittenAtMostOncePerInterval_andCounts();
  void policy_reparking_movesThePlace();
  void policy_arming_startsAFreshStay();

  // the line
  void line_whenMoved_namesTheComputerTheDistanceTheTriggerAndTheCount();
  void line_whenShown_saysSo();
  void line_whenTheTapWasOff_saysSo();
  void line_withoutAName_hasAQuestionMark();
  void line_theApplication_isOnlyNamedWhenItIsToldToBe();

  // the controller
  void controller_onThisMac_neverTouchesTheSystem();
  void controller_anEventAtTheParkedPlace_costsNoSystemCall();
  void controller_anOldEvent_isNotAMovement();
  void controller_aMovement_isHeldInTheOrderOfTheCalls();
  void controller_theTimer_runsOnlyWhileTheStayLasts_andHoldsTheMovedPointer();
  void controller_theLine_followsTheClockAndTheCounter();
  void controller_aNotificationFromTheSource_looksAtThePointerOnTheTapThread();
  void controller_aSilentNotification_holdsAgainWithoutALine();
  void controller_aTapDisabledByUserInput_switchesTheTapOn();
  void controller_aShownCursor_isHiddenAgain();
  void controller_theNameOfTheClient_isInTheLine();
  void controller_theServerMovingThePointer_isNotAMovement();
  void controller_detaching_stopsTheNotifications();
  void controller_destroyingIt_stopsTheTimerAndTheSource();

  // lifetime: what the system still runs after the controller is gone, and what the system throws
  void controller_workThatOutlivesTheController_doesNotTouchTheSystem();
  void controller_destroyingIt_waitsForANotificationThatIsBeingHandedOver();
  void controller_aSystemThatThrows_isContainedAndReportedOnce();
};
