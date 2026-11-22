/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class InputIndicatorIpcTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void activityRoundTrips();
  void malformedActivityIsRejected_data();
  void malformedActivityIsRejected();
  void staleAndFutureActivityIsRejected();
  void newerSequenceWins();
  void booleanPayloadsAreStrict();
  void subscriptionCapabilityIsExactAndRedacted();
  void rawIndicatorMessagesAreStrict();
  void ordinaryPayloadsContainingIndicatorNamesStayOrdinary();
  void subscriptionStateIsProcessLocalAndClearable();
};
