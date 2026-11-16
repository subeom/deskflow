/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class ConnectionDiagnosisTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void formatTarget_namesTheAddressUsedAndBracketsIpv6();
  void everyKind_namesTheServerAndHasWordsAndHints_data();
  void everyKind_namesTheServerAndHasWordsAndHints();
  void timeout_saysWhichPhaseWasReached();
  void timeout_statesTheLimitThatWasUsed();
  void macLocalNetworkHint_isOnlyForMacOs();
  void serverFirewallHint_namesThePort();
  void detail_isKeptForTheKindsThatCarryAnOsMessage();
  void line_endsWithThePointerToTheDocs();
  void connectTimeout_unsetIsTheDefaultOfTwoSeconds();
  void connectTimeout_acceptsOneToSixtySeconds_data();
  void connectTimeout_acceptsOneToSixtySeconds();
  void connectTimeout_invalidFallsBackToTheDefault_data();
  void connectTimeout_invalidFallsBackToTheDefault();
};
