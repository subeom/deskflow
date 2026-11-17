/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

// The stop of the service core that goes with the quit of the program, with a stand-in of the client of the service (no
// daemon, no socket)
class ServiceQuitTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aConnectedClient_sendsTheStopAndFlushesIt();
  void aClientThatConnectsWithinTheWait_sendsTheStopAfterTheConnection();
  void aClientThatNeverConnects_givesUpAfterTheWait_andSendsNothing();
  void aConnectionThatFailsAtOnce_doesNotWaitForTheWholeTime();
  void aStopThatCannotBeFlushed_isReported();
  void theWarning_namesWhatRemains();
};
