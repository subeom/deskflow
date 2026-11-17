/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class QtTunnelProcessTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void theTokenIsInTheEnvironmentOnly_andNoOtherTunnelVariableIsPassed();
  void theOutputComesAsLines();
  void aProgramThatIsNotThere_reportsItAndNeverStarts();
  void aProcessThatStopsOnItsOwn_reportsItsExitCode();
  void terminate_endsTheProcess();
  void kill_endsTheProcess();
  void aProcessThatIsStillThereWhenTheObjectGoes_isEnded();
};
