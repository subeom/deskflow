/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "arch/Arch.h"
#include "base/Log.h"

#include <QObject>

class ConnectFailureTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void failureKindOf_archErrors_data();
  void failureKindOf_archErrors();
  void failureKindOf_addressErrors_data();
  void failureKindOf_addressErrors();
  void socketConnectException_carriesKindAndKeepsTheMessage();
  void tcpSocket_closedLocalPort_reportsRefused();

private:
  Arch m_arch;
  Log m_log;
};
