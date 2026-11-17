/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class TrayPolicyTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void closeAction_hidesToTheTrayOnlyWhereThereIsOne_data();
  void closeAction_hidesToTheTrayOnlyWhereThereIsOne();
  void click_togglesTheWindowExceptOnMacAndGnome_data();
  void click_togglesTheWindowExceptOnMacAndGnome();
  void trayClick_canCrashOnMacOS27WithQtBefore6113_data();
  void trayClick_canCrashOnMacOS27WithQtBefore6113();
};
