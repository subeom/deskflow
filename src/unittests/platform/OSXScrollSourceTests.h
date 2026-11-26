/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class OSXScrollSourceTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void classifyPhases_matrix_data();
  void classifyPhases_matrix();
  void classifyEvent_syntheticEvents_data();
  void classifyEvent_syntheticEvents();
  void readFields_reportsRawValues();
  void update_reportsOnlyTransitions();
  void update_nullEventIsUnknown();
};
