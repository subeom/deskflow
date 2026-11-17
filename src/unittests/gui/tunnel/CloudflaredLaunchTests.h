/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

class CloudflaredLaunchTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void arguments_areTheAccessTcpCommandAndNameNoSecret();
  void environment_holdsTheTokenAndOnlyThere();
  void environment_keepsWhatTheChildNeedsAndDropsWhatWouldChangeTheTunnel();
  void environment_ofThisProcessIsNotChanged();
  void logLevel_isInfoAndNeverDebug();
  void commandLine_isForTheLogAndHasNoEnvironment();
  void commandLine_quotesWhatHasBlanks();
  void request_thatIsNotUsableMakesNoSpec_data();
  void request_thatIsNotUsableMakesNoSpec();
  void serviceToken_isWipedWhenAsked();
};
