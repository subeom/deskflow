/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

class VersionCheckerTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void checkLatest_deduplicatesAndUsesOfficialEndpoint();
  void checkLatest_surfacesTimeoutRateLimitAndOversizeErrors();
  void checkLatest_cancelStopsInFlightRequest();
  void redirectAllowlist_acceptsOnlyGitHubDownloadHosts();
};
