/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The rules of the native event filter of Windows (what is answered how, and where the answer is written), run with a
// made-up window on every system. The crash that the seventh run of the checks of Windows found (a click on the button
// of maximizing wrote through a null pointer: Qt does not give the place of the answer to a message that it takes from
// the queue) is here as a test that cannot be made to pass by a null pointer, which is what a Mac would never meet.
class FrameMessagesTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void delivery_isWhatQtDoes_withTheMessagesOfTheQueueAndTheOnesThatAreSent();
  void sent_areAnsweredInTheirPlace_andTheHostDoesItsPart();
  void sent_withNoPlaceForTheAnswer_areLeftToWindows_andTheHostIsNotAsked();
  void clickOnTheMaximizeButton_fromTheQueue_isTaken_andWritesNothing();
  void clickOnTheMaximizeButton_withAPlace_isAnsweredWithZero();
  void clickThatIsNotOnTheMaximizeButton_isLeftToWindows();
  void theMouseOverTheNonClientArea_isNoted_andNeverHandled();
  void otherMessages_areNotHandled_withAPlaceOrNot();
  void everyMessageWithEveryPlace_neverWritesWhereThereIsNoPlace_andNeverCrashes();
  void source_aNativeEventFilter_goesThroughTheRules_andWritesNoAnswerItself();
  void source_theNumbersOfTheMessages_areCheckedAgainstTheHeadersOfWindows();
};
