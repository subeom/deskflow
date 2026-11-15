/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QObject>

class ClientProxyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void keyDown_data();
  void keyDown();
  void keyRepeat_data();
  void keyRepeat();
  void keyUp_data();
  void keyUp();
  void inputCapabilities_v19_sendsSourcePlatformAndCapabilities();
  void inputCapabilities_v18_doesNotSendCapabilityMessage();
  void inputCapabilities_v19_parsesDestinationCapabilities();
  void inputCapabilities_v19_waitsForCapabilitiesBeforeReady();
  void mouseWheel_data();
  void mouseWheel();
  void rightCommand_hangulPolicy_preservesSuperRAndSuppressesRepeat();
  void rightCommand_sameOrDisabledPolicy_preservesLegacyLeftSuper();
  void rightCommand_leftSuperHeld_keepsSuperOnOtherKeys();
  void rightCommand_v18_preservesLegacyMacWireValue();
  void rightCommand_v19_preservesLegacyAggregateState();
  void rightCommand_leave_clearsMappedAndLegacyState();

private:
  Log m_log;
};
