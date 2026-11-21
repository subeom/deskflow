/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateNetworkTests.h"

#include "gui/UpdateNetwork.h"

using deskflow::gui::UpdateNetworkStatus;
using deskflow::gui::UpdateRequestKind;
using deskflow::gui::appendUpdateBytes;
using deskflow::gui::isAllowedUpdateRedirect;
using deskflow::gui::isAllowedUpdateUrl;
using deskflow::gui::isCurrentUpdateGeneration;
using deskflow::gui::parseUpdateRetryAfterSeconds;
using deskflow::gui::shouldRetryUpdateRequest;

namespace {

QUrl latestUrl()
{
  return QUrl(QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest"));
}

QUrl assetUrl()
{
  return QUrl(
      QStringLiteral("https://github.com/subeom/deskflow/releases/download/v2.0.6/"
                     "deskflow-2.0.6-macos-arm64.dmg")
  );
}

QUrl assetRedirect(const QString &host = QStringLiteral("release-assets.githubusercontent.com"))
{
  return QUrl(QStringLiteral("https://%1/github-production-release-asset/1/token?x=private").arg(host));
}

} // namespace

void UpdateNetworkTests::metadataUrl_acceptsOnlyPinnedForkEndpoint()
{
  QVERIFY(isAllowedUpdateUrl(latestUrl(), UpdateRequestKind::Metadata));

  const QStringList invalid{
      QStringLiteral("http://api.github.com/repos/subeom/deskflow/releases/latest"),
      QStringLiteral("https://api.github.com/repos/deskflow/deskflow/releases/latest"),
      QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest?draft=false"),
      QStringLiteral("https://user@api.github.com/repos/subeom/deskflow/releases/latest"),
      QStringLiteral("https://api.github.com:8443/repos/subeom/deskflow/releases/latest"),
      QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest#fragment"),
      QStringLiteral("https://127.0.0.1/repos/subeom/deskflow/releases/latest"),
      QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest/"),
  };
  for (const auto &url : invalid)
    QVERIFY2(!isAllowedUpdateUrl(QUrl(url), UpdateRequestKind::Metadata), qPrintable(url));
}

void UpdateNetworkTests::redirectPolicy_rejectsApiRedirectsThirdPartyHostsAndUnsafeUrls()
{
  const auto redirect = assetRedirect();
  QVERIFY(!isAllowedUpdateRedirect(latestUrl(), redirect, UpdateRequestKind::Metadata, 1));
  QVERIFY(!isAllowedUpdateRedirect(assetUrl(), QUrl(QStringLiteral("https://example.com/file.dmg")), UpdateRequestKind::Installer, 1));
  QVERIFY(!isAllowedUpdateRedirect(
      assetUrl(), QUrl(QStringLiteral("http://release-assets.githubusercontent.com/file.dmg")),
      UpdateRequestKind::Installer, 1
  ));
  QVERIFY(!isAllowedUpdateRedirect(
      assetUrl(), QUrl(QStringLiteral("https://user@release-assets.githubusercontent.com/file.dmg")),
      UpdateRequestKind::Installer, 1
  ));
  QVERIFY(!isAllowedUpdateRedirect(
      assetUrl(), QUrl(QStringLiteral("https://release-assets.githubusercontent.com/file.dmg#part")),
      UpdateRequestKind::Installer, 1
  ));
  QVERIFY(!isAllowedUpdateRedirect(assetUrl(), redirect, UpdateRequestKind::Installer, 4));
}

void UpdateNetworkTests::redirectPolicy_acceptsOnlyTerminalGithubAssetHosts()
{
  for (const auto host : {
           QStringLiteral("release-assets.githubusercontent.com"),
           QStringLiteral("objects.githubusercontent.com"),
           QStringLiteral("github-releases.githubusercontent.com")}) {
    const auto redirect = assetRedirect(host);
    QVERIFY(isAllowedUpdateRedirect(assetUrl(), redirect, UpdateRequestKind::Checksum, 1));
    QVERIFY(isAllowedUpdateRedirect(assetUrl(), redirect, UpdateRequestKind::Installer, 3));
    QVERIFY(!isAllowedUpdateRedirect(
        redirect, assetRedirect(host), UpdateRequestKind::Installer, 2
    ));
  }

  QVERIFY(!isAllowedUpdateUrl(assetRedirect(), UpdateRequestKind::Installer));
  QVERIFY(!isAllowedUpdateRedirect(
      assetUrl(), QUrl(QStringLiteral("https://release-assets.githubusercontent.com/a%2Fb.dmg")),
      UpdateRequestKind::Installer, 1
  ));
}

void UpdateNetworkTests::rateLimit_classifies429AndExhausted403ButNotOrdinary403()
{
  QVERIFY(deskflow::gui::isUpdateRateLimited(429, QByteArrayLiteral("10")));
  QVERIFY(deskflow::gui::isUpdateRateLimited(403, QByteArrayLiteral("0")));
  QVERIFY(!deskflow::gui::isUpdateRateLimited(403, QByteArrayLiteral("1")));
  QVERIFY(!deskflow::gui::isUpdateRateLimited(503, QByteArrayLiteral("0")));

  QCOMPARE(parseUpdateRetryAfterSeconds(QByteArrayLiteral("120"), QByteArrayLiteral(""), 100), 120);
  QCOMPARE(parseUpdateRetryAfterSeconds(QByteArrayLiteral("999999"), QByteArrayLiteral(""), 100), 86400);
  QCOMPARE(parseUpdateRetryAfterSeconds(QByteArrayLiteral("invalid"), QByteArrayLiteral("110"), 100), 10);
  QCOMPARE(parseUpdateRetryAfterSeconds(QByteArrayLiteral(""), QByteArrayLiteral("999999999"), 100), 86400);
}

void UpdateNetworkTests::retryPolicy_allowsOneMetadataOrChecksumTransientRetryOnly()
{
  QVERIFY(shouldRetryUpdateRequest(UpdateRequestKind::Metadata, UpdateNetworkStatus::Timeout, 0));
  QVERIFY(shouldRetryUpdateRequest(UpdateRequestKind::Checksum, UpdateNetworkStatus::NetworkError, 0));
  QVERIFY(shouldRetryUpdateRequest(UpdateRequestKind::Metadata, UpdateNetworkStatus::HttpError, 0, 503));
  QVERIFY(shouldRetryUpdateRequest(UpdateRequestKind::Metadata, UpdateNetworkStatus::HttpError, 0, 408));
  QVERIFY(!shouldRetryUpdateRequest(UpdateRequestKind::Metadata, UpdateNetworkStatus::Timeout, 1));
  QVERIFY(!shouldRetryUpdateRequest(UpdateRequestKind::Installer, UpdateNetworkStatus::Timeout, 0));
  QVERIFY(!shouldRetryUpdateRequest(UpdateRequestKind::Checksum, UpdateNetworkStatus::RateLimited, 0));
  QVERIFY(!shouldRetryUpdateRequest(UpdateRequestKind::Checksum, UpdateNetworkStatus::TooLarge, 0));
  QVERIFY(!shouldRetryUpdateRequest(UpdateRequestKind::Metadata, UpdateNetworkStatus::HttpError, 0, 404));
}

void UpdateNetworkTests::responseLimit_rejectsOversizeBeforeBodyPublication()
{
  QByteArray body(QStringLiteral("abc").toUtf8());
  QVERIFY(appendUpdateBytes(body, QByteArrayLiteral("de"), 5));
  QCOMPARE(body, QByteArrayLiteral("abcde"));
  QVERIFY(!appendUpdateBytes(body, QByteArrayLiteral("fg"), 5));
  QCOMPARE(body, QByteArrayLiteral("abcde"));
}

void UpdateNetworkTests::cancellation_generationInvalidatesLateCallbacks()
{
  QVERIFY(isCurrentUpdateGeneration(42, 42));
  QVERIFY(!isCurrentUpdateGeneration(43, 42));
  QVERIFY(!isCurrentUpdateGeneration(0, 1));
}

QTEST_MAIN(UpdateNetworkTests)
