/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <QTemporaryDir>
#include <QTest>

// The group is made with a secret store in memory, a locator that does not look at the file system and a TLS setting
// that the test sets: no keychain, no Credential Manager, no file of the user and no message box is used.
class TunnelSettingsGroupTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void init();
  void cleanup();

  void defaults_areOffAndEmptyAndTheSecretIsNeverRead();
  void tlsOff_blocksEnablingAndSaysWhy();
  void tlsOff_stillLetsAnEnabledTunnelBeTurnedOff();
  void save_withNothingTouched_writesNothingToTheConfiguration();
  void save_writesOnlyWhatChangedAndNormalisesTheHostname();
  void save_thatChangesTheConfigurationForgetsTheActivePort();
  void save_storesTheTokenAndNeverShowsTheSecretAgain();
  void validation_saysWhatIsMissing_data();
  void validation_saysWhatIsMissing();
  void removeCredentials_asksAndThenRemoves();
  void aStoreThatRefuses_savesNothingAndKeepsWhatWasTyped();
  void aStoreThatCannotBeRead_isSaidAndNothingBreaks();
  void cloudflaredLine_followsTheLocatorAndTheTypedPath();
  void resetToDefault_dropsTheEntriesAndKeepsTheToken();
  void isModifiedAndIsDefault_followTheEntries();
  void credentialLine_saysWhereAPersistentTokenIsAndThatAMemoryOneIsForgotten();
  void notWritable_disablesEveryEntry();
  void saveInTheOtherDirection_aSavedTunnelIsShownAgain();

  void openingWithTheTunnelOff_makesNoCallToTheSecureStore();
  void openingWithTheTunnelOn_looksUpOnce();
  void turningTheTunnelOn_looksUpOnceAndTheAnswerIsKept();
  void goingIntoTheFieldsOfTheToken_looksUpOnce();
  void typingInAField_looksUpOnceAndDoesNotOverwriteWhatWasTyped();
  void savingWithoutTouchingTheToken_makesNoCallToTheSecureStore();
  void savingATypedToken_storesItAndShowsWhatIsStored();
};
