/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class PortProberTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void canListen_isTrueForAFreePortAndFalseForABusyOrInvalidOne();
  void canListen_leavesThePortFree();
  void findFreePort_givesAPortThatCanBeListenedOn();
  void probeOpen_tellsTrueForAPortThatListens();
  void probeOpen_tellsFalseForAPortThatDoesNotListen();
  void probeOpen_tellsOnceAndSendsNothing();
};
