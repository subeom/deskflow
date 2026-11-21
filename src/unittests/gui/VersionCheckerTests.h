/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class VersionCheckerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void checkLatest_deduplicatesInFlightAndUsesPinnedEndpoint();
  void checkLatest_upToDateUsesOnlyBoundedCachedSnapshotOn304();
  void checkLatest_rateLimitPersistsRetryAndMakesNoImmediateRetry();
  void checkLatest_timeoutMakesOnlyOneMetadataRetry();
  void checkLatest_manualPlatformsNeverDownload();
  void checkLatest_directMacDownloadsChecksumsThenDmgThenVerifies();
  void checkLatest_cancelInvalidatesLateNetworkCallbacks();
  void checkLatest_discardsWrongIntegrityAndPlatformVerification();
  void checkLatest_missingExactArchitectureNeverFallsBack();
  void checkLatest_reusesOnlyAReverifiedStage();
  void checkLatest_unsupportedEnvironmentDoesNotRequest();
};
