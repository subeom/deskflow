/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QObject>

class MonitorReportTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void encode_oneMonitor_goldenBytes();
  void encode_negativeOriginAndTwoMonitors_goldenBytes();
  void encode_noMonitors_isTheHeaderOnly();
  void encode_neverWritesMoreThanTheCountCanSay();
  void read_roundTrips();
  void read_readsTheWholeMessageWhateverItHolds_data();
  void read_readsTheWholeMessageWhateverItHolds();
  void read_truncated_isEmptyAndDoesNotThrow_data();
  void read_truncated_isEmptyAndDoesNotThrow();
  void prepare_tilesTheShape_data();
  void prepare_tilesTheShape();
  void prepare_cannotReport_data();
  void prepare_cannotReport();
  void prepare_mirroredMonitorsAreOne();
  void prepare_keepsScaleAndPrimary();
  void prepare_sixteenMonitorsAreTheMost();

private:
  Log m_log;
};
