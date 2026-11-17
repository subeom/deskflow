/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class StatusModelTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void headline_saysTheStateOfTheCore_data();
  void headline_saysTheStateOfTheCore();
  void sharingWith_countsTheComputersThatAreConnected();
  void detail_isTheSentenceThatWasGiven();
  void securityChip_isShownWhileThereIsAnEncryptionToTellOf();
  void externalChips_comeAfterTheChipsOfTheCore_byTheirOwnKey();
  void error_winsUntilTheCoreComesToAnotherState();
  void changed_isOnlySaidWhenWhatIsShownChanges();
};
