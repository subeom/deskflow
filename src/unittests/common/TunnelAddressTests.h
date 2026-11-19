/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class TunnelAddressTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  void localPort_isLimitedToTheUnprivilegedRange();
  void localPortFromSetting_takesWholeNumbersInTheRange_data();
  void localPortFromSetting_takesWholeNumbersInTheRange();
  void normalizedHostname_trimsLowersAndDropsTheFinalDot();
  void hostname_isADnsNameThatCloudflaredCannotTakeForAnOption_data();
  void hostname_isADnsNameThatCloudflaredCannotTakeForAnOption();

  void isActive_needsTheSwitchTlsAndAUsableHostname_data();
  void isActive_needsTheSwitchTlsAndAUsableHostname();
  void port_prefersTheActiveOneThenTheLocalOneThenTheDefault();
  void localAddress_isTheLoopbackWithThePort();

  void effectiveRemoteHost_isTheSavedAddressUnchangedWhenTheTunnelIsNotUsed_data();
  void effectiveRemoteHost_isTheSavedAddressUnchangedWhenTheTunnelIsNotUsed();
  void effectiveRemoteHost_isNothingWhenTheTunnelIsOnAndNotUsable_data();
  void effectiveRemoteHost_isNothingWhenTheTunnelIsOnAndNotUsable();
  void remoteHostBlock_isNothingWhenTheTunnelIsOffOrUsable();
  void effectiveRemoteHost_isTheLocalAddressAndNothingElseWhenTheTunnelIsUsed();

  void displayedTunnelHost_isTheHostnameOnlyWhileTheTunnelIsUsed();

  void settings_keysAreKnownAndOffByDefault();
  void settings_aConfigFromBeforeTheTunnelIsReadAsOffAndIsNotChangedByReading();
  void settings_valuesOfTheTunnelAreKeptAndUnknownKeysOfItsGroupAreRemoved();
  void settings_fromSettingsFallsBackToTheDefaultsForUnusableValues();
  void settings_effectiveRemoteHostFollowsTheSavedAddressWhenOffAndNeverChangesIt();

private:
  QString m_dir;
};
