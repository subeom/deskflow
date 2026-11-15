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
  void scrollSource_legacyProxiesIgnoreSource_data();
  void scrollSource_legacyProxiesIgnoreSource();
  void scrollScope_all_forwardsOptionsAndKeepsLegacyWheelBytes();
  void scrollScope_mouseWheelOnly_invertsOnlyMouseEvents();
  void scrollScope_mouseWheelOnly_invertsOnlyConfiguredAxes();
  void scrollScope_mouseWheelOnly_withholdsInvertOptionsFromDestination();
  void scrollScope_resetOptions_restoresDestinationInversion();
  void scrollScope_ineligibleLinks_keepLegacyBytes_data();
  void scrollScope_ineligibleLinks_keepLegacyBytes();
  void rightCommand_bytesMatchOriginalServer_data();
  void rightCommand_bytesMatchOriginalServer();
  void f18_macToWindows_sendsHangulWithSourceButtonAndModifiers();
  void f18_consecutivePresses_eachToggleOnce();
  void f18_heldKey_togglesOnceAndSuppressesRepeat();
  void f18_holdWithAutoRepeat_thenRelease_thenNewPress();
  void f18_releaseIsMatchedByButton_data();
  void f18_releaseIsMatchedByButton();
  void f18_optionTurnedOffMidHold_data();
  void f18_optionTurnedOffMidHold();
  void f18_optionTurnedOnMidHold_releaseStaysPlain();
  void f18_leaveMidHold_forgetsTheHeldKey();
  void f18_otherKeysWhileHeld_doNotDisturbTheHeldKey();
  void f18_ineligibleLinks_passThrough_data();
  void f18_ineligibleLinks_passThrough();
  void f18_legacyLinks_passThrough_data();
  void f18_legacyLinks_passThrough();
  void f18_otherKeys_passThrough_data();
  void f18_otherKeys_passThrough();
  void f18_optionOffInvalidOrReset_passesThrough();
  void semanticMedia_v19EnabledSuppressesRepeat();
  void semanticMedia_v18AndDisabledPreserveWireValues();

private:
  Log m_log;
};
