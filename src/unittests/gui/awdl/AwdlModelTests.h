/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

// The rules of the option that turns off the wireless link of AirDrop while sharing, as plain values: the reading of
// the state of awdl0, the one command that needs the password, and when the program asks for it. Nothing here runs a
// program or touches the system.
class AwdlModelTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void parseIfconfig_readsTheStateOfTheInterface_data();
  void parseIfconfig_readsTheStateOfTheInterface();
  void linkStateId_isAWordForEveryState();

  void parseAdminResult_tellsDoneFromCancelledFromFailed_data();
  void parseAdminResult_tellsDoneFromCancelledFromFailed();

  void appleScriptString_cannotLeaveItsQuotes_data();
  void appleScriptString_cannotLeaveItsQuotes();
  void adminArguments_areTheConstantCommandAndTheTextOfTheDialogOnly();
  void readArguments_askForTheInterfaceAndNothingElse();

  void parseRoute_readsTheGatewayAndTheInterface_data();
  void parseRoute_readsTheGatewayAndTheInterface();
  void isLocalGateway_isTheLocalNetworkOnly_data();
  void isLocalGateway_isTheLocalNetworkOnly();
  void canProbe_isWifiWithALocalGatewayAndAPlainDeviceOnly_data();
  void canProbe_isWifiWithALocalGatewayAndAPlainDeviceOnly();
  void pingArguments_haveTheGatewayAsTheOnlyAddress();
  void pingArguments_areNoneForARouteThatMayNotBeMeasured();
  void parsePing_readsAnswersAndLosses();
  void parsePing_saysNothingForWhatIsNotPing();
  void analyzeBurst_findsTheSpikesOfAwdl_andNothingElse_data();
  void analyzeBurst_findsTheSpikesOfAwdl_andNothingElse();
  void analyzeBurst_hasTheMedianAndTheWorst();
  void shouldProbe_isOnlyWithTheOptionTheCoreAndTheLink_data();
  void shouldProbe_isOnlyWithTheOptionTheCoreAndTheLink();
  void summarize_saysWhatTheStatusLineSays_data();
  void summarize_saysWhatTheStatusLineSays();

  void decide_followsTheOptionTheCoreAndTheLink_data();
  void decide_followsTheOptionTheCoreAndTheLink();
  void decide_asksOnceForARun_andNeverTwiceByItself();
  void decide_whenTheLinkComesBackItIsAHintOnly();
  void decide_aStoppedCoreForgetsTheRun_andNeverRestores();
  void decide_theHintNeedsTheSpikes_theLinkBeingUpIsNotEnough();
  void decide_anAnswerThatIsStillOpenSaysNothing();
  void decide_whatThePersonRestoredIsNotTakenBack();
  void decide_isNothingWithTheOptionOffOrOffAMac_forEveryInput();
};
