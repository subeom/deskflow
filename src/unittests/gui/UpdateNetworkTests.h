/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class UpdateNetworkTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void metadataUrl_acceptsOnlyPinnedForkEndpoint();
  void redirectPolicy_rejectsApiRedirectsThirdPartyHostsAndUnsafeUrls();
  void redirectPolicy_acceptsOnlyTerminalGithubAssetHosts();
  void rateLimit_classifies429AndExhausted403ButNotOrdinary403();
  void retryPolicy_allowsOneMetadataOrChecksumTransientRetryOnly();
  void responseLimit_rejectsOversizeBeforeBodyPublication();
  void cancellation_generationInvalidatesLateCallbacks();
  void inactivityDeadline_resetsOnProgressAndHonorsTotalDeadline();
};
