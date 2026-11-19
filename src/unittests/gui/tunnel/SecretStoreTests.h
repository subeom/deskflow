/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTest>

// Only the store in memory is used here. The stores of Windows and macOS are not called by any test: they are checked
// by hand on those systems, where a call can ask the user for a password.
class SecretStoreTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void aStoreThatIsEmpty_saysSoWithoutAnError();
  void save_storesTheTokenAndReplacesTheOneThatWasThere();
  void storedClientId_doesNotReadTheSecret();
  void load_givesTheWholeToken();
  void remove_forgetsTheTokenAndIsNotAnErrorWhenThereIsNone();
  void save_refusesATokenThatIsNotWhole_data();
  void save_refusesATokenThatIsNotWhole();
  void aStoreThatFails_saysWhyOnEveryOperation();
  void storesOfTheSameStateSeeTheSameToken();
  void sessionStore_isOneForTheWholeProgram();
  void systemStore_isTheMemoryOneWhenTheVariableSaysMemory();
  void systemStore_isPersistentOnWindowsAndMacOnly();

  void theResetOfTheSettings_removesTheStoredToken();
  void theResetOfTheSettings_isNotAnErrorWhenNothingIsStored();
  void theResetOfTheSettings_saysWhenTheStoreFails();
  void theResetOfTheSettings_usesTheSystemStoreByDefault();
};
