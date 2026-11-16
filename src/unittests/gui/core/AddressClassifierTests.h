/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class AddressClassifierTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void classifyAddress_onMacOsInterfaces_data();
  void classifyAddress_onMacOsInterfaces();
  void classifyAddress_onWindowsInterfaces_data();
  void classifyAddress_onWindowsInterfaces();
  void classifyAddress_onLinuxInterfaces_data();
  void classifyAddress_onLinuxInterfaces();
  void classifyAddress_byRangeAndFlags_data();
  void classifyAddress_byRangeAndFlags();

  void usableIpv4Addresses_ofAMacWithTwoNetworkCardsAndTunnels_listsEachWithItsKind();
  void usableIpv4Addresses_ofAWindowsComputerWithVirtualAdapters_putsTheLanFirst();
  void usableIpv4Addresses_ofALinuxServerWithDocker_listsTheBridgeAsVirtual();
  void usableIpv4Addresses_onlyLinkLocal_isKeptForACableBetweenTwoComputers();
  void usableIpv4Addresses_leavesOutWhatCannotBeReached();
  void usableIpv4Addresses_withOneAddress_isThatAddress();

  void labels_haveWordsForEveryKind();
  void addressListToolTip_putsEachAddressOnALineWithItsInterfaceAndKind();
  void addressListHtml_hasATableAnExplanationPerKindAndTheTip();
};
