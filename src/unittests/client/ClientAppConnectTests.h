/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "arch/Arch.h"
#include "base/Log.h"

#include <QObject>
#include <QTemporaryDir>

class ClientAppConnectTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void init();

  void parseArgs_listWithSpacesAndEmptyParts_keepsTheAddressesInOrder();
  void parseArgs_listWrittenByHandWithoutQuotes_isRead();
  void parseArgs_singleAddress_isOneAddress();

  void connect_firstAddressRefuses_triesTheSecondAtOnceAfterTheRetryDelay();
  void connect_firstAddressNeverGreets_triesTheSecondAfterTheTimeLimitAndTheRetryDelay();
  void connect_afterAFullRoundOfFailures_startsAgainWithTheFirstAddress();

private:
  Arch m_arch;
  Log m_log;
  QTemporaryDir m_dir;
};
