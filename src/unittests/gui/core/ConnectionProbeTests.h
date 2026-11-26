/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class ConnectionProbeTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();

  void parseServerEntries_splitsTheListTheWayTheCoreReadsIt();
  void parseServerEntries_namesWhatIsWrongWithAPort_data();
  void parseServerEntries_namesWhatIsWrongWithAPort();

  void failureKindFromSocketError_saysWhatEachSocketErrorMeans_data();
  void failureKindFromSocketError_saysWhatEachSocketErrorMeans();

  void probe_openAddress_isReportedOpenAndClosedAtOnce();
  void probe_refusedAddress_isReportedRefused();
  void probe_noRoute_isReportedAsNoRoute();
  void probe_silentAddress_timesOutAfterTheTimeLimit();
  void probe_nameThatDoesNotResolve_isReportedAsDnsFailure();
  void probe_nameWithTwoAddresses_triesBothAndKeepsTheOrder();
  void probe_numericAddress_doesNotAskTheResolver();
  void probe_resolverThatAnswersAtOnce_isHandled();
  void probe_severalEntries_runAtTheSameTimeAndAreReportedInOrder();
  void probe_entryWithABadPort_isReportedWithoutBeingTested();
  void probe_emptyList_finishesAtOnce();
  void probe_cancel_dropsTheTest();
  void probe_start_dropsATestThatRuns();
  void probe_neverSendsData();

  void summaryLine_namesEveryAddressAndWhatCameOfIt();
  void summaryLine_withoutResults_saysNothingWasTested();

  void system_openLocalPort_isOpen();
  void system_closedLocalPort_isRefused();
  void system_localhostName_isResolvedAndTried();
};
