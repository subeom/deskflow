/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QObject>

class ServerProxyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void handleKeepAliveAlarm_timeout_queuesDisconnectRequest();
  void handleKeepAliveAlarm_timeout_logsSilenceAndClipboardTransfer();
  void handleData_incompleteMessage_queuesDisconnectRequest();
  void parseHandshakeMessage_protocolError_queuesRefusalRequest();
  void legacyScroll_preservesDmwmValues();
  void inputCapabilities_v19_readsSourceAndWritesDestinationCapabilities();
  void f18_option_isIgnoredByTheDestination();
  void crossOSScroll_v19TransformsDmwmBeforeDelivery();
  void crossOSScroll_mouseWheelOnlyScopeFailsClosedForPlainDmwm();
  void semanticMedia_v19HandlesOnlyEligibleKeys();
  void monitorReport_serverWithoutTheBit_clientAnswersAsBeforeAndSaysNothing();
  void monitorReport_serverWithTheBit_clientAcknowledgesAndReports();
  void monitorReport_clientThatCannotListItsMonitors_doesNotAcknowledge();
  void monitorReport_afterEveryDinf_butOnlyWhenTheMonitorsChanged();
  void monitorReport_monitorsChangedButNotTheShape_sendsJustTheReport();
  void monitorReport_monitorsEventBeforeTheShapeEvent_waitsForTheDinf();
  void monitorReport_monitorsThatDoNotTileTheShape_areNotReportedAndForgotten();
  void monitorReport_matrix_data();
  void monitorReport_matrix();

private:
  Log m_log;
};
