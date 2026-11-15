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
  void handleData_incompleteMessage_queuesDisconnectRequest();
  void parseHandshakeMessage_protocolError_queuesRefusalRequest();
  void legacyScroll_preservesDmwmValues();
  void inputCapabilities_v19_readsSourceAndWritesDestinationCapabilities();
  void rightCommand_hangulPolicyTransformsSuperR();
  void rightCommand_missingHangulCapability_preservesLegacy();
  void crossOSScroll_v19TransformsDmwmBeforeDelivery();
  void semanticMedia_v19HandlesOnlyEligibleKeys();

private:
  Log m_log;
};
