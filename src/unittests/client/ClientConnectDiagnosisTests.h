/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "arch/Arch.h"
#include "base/Log.h"

#include <QObject>

class ClientConnectDiagnosisTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void connect_withoutTheSetting_waitsTwoSeconds();
  void connect_withTheSetting_waitsThatLong();
  void connect_withInvalidSetting_waitsTwoSecondsAndSaysSoOnce();

  void timeout_whileConnectingTcp_namesTheTcpPhaseAndKeepsTheOldMessage();
  void timeout_afterTcpConnectedWithTls_namesTheHandshake();
  void timeout_afterTheSecureConnectionWithoutHello_namesTheGreeting();
  void timeout_afterAnEarlierTlsFailure_reportsTheTlsFailure();
  void timeout_afterAnEarlierFingerprintRejection_reportsTheFingerprint();
  void timeout_withoutTls_namesTheGreetingAfterTcp();

  void connectionFailed_refused_namesTheRefusal();
  void connectionFailed_noRoute_namesTheMissingRoute();
  void connectThrows_noRoute_namesTheMissingRouteAndKeepsTheOldMessage();
  void connectThrows_unresolvedName_namesTheName();

  void hello_fromAnotherProgram_saysItIsNotDeskflow();
  void hello_withIncompatibleMajorVersion_namesTheVersion();
  void refuseConnection_unknownName_namesTheScreenName();
  void refuseConnection_alreadyConnected_namesTheDuplicate();
  void connect_toAClosedLocalPort_logsTheRefusalOfTheRealSocket();
  void connect_withTlsToAPeerThatNeverAnswers_namesTheHandshakeAndNotTcp();

private:
  Arch m_arch;
  Log m_log;
};
