/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UpdateNetworkTests.h"

#include "common/UrlConstants.h"
#include "gui/UpdateNetwork.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <cstring>
#include <memory>

using deskflow::gui::UpdateNetworkStatus;
using deskflow::gui::UpdateRequestKind;
using deskflow::gui::appendUpdateBytes;
using deskflow::gui::isAllowedUpdateRedirect;
using deskflow::gui::isAllowedUpdateUrl;
using deskflow::gui::isCurrentUpdateGeneration;
using deskflow::gui::parseUpdateRetryAfterSeconds;
using deskflow::gui::resolveUpdateRedirectLocation;
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

QString signedAssetQuery()
{
  return QStringLiteral(
      "rsct=application%2Foctet-stream&sig=abcd%2Fefghijk&sp=r&sv=2018-03-28&sr=b"
  );
}

QString nestedPercentEncoding(const QString &escape, int levels)
{
  auto encoded = escape;
  for (int level = 0; level < levels; ++level) {
    encoded.replace(QStringLiteral("%"), QStringLiteral("%25"));
  }
  return encoded;
}

QUrl assetRedirect(
    const QString &host = QStringLiteral("release-assets.githubusercontent.com"),
    const QString &query = signedAssetQuery()
)
{
  return QUrl(
      QStringLiteral(
          "https://%1/github-production-release-asset/1/token?%2"
      ).arg(host, query)
  );
}

class FakeNetworkReply final : public QNetworkReply
{
public:
  FakeNetworkReply(
      const QNetworkRequest &request, int status, const QByteArray &location,
      const QByteArray &body, QObject *parent
  )
      : QNetworkReply(parent),
        m_body(body)
  {
    setRequest(request);
    setUrl(request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
    if (!location.isEmpty()) {
      setRawHeader(QByteArrayLiteral("Location"), location);
    }
    setHeader(QNetworkRequest::ContentLengthHeader, body.size());
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    QTimer::singleShot(0, this, [this] {
      Q_EMIT readyRead();
      setFinished(true);
      Q_EMIT finished();
    });
  }

  void abort() override
  {
  }

  qint64 bytesAvailable() const override
  {
    return m_body.size() - m_offset + QNetworkReply::bytesAvailable();
  }

protected:
  qint64 readData(char *data, qint64 maximumSize) override
  {
    const auto count = qMin<qint64>(maximumSize, m_body.size() - m_offset);
    if (count <= 0) {
      return 0;
    }
    memcpy(data, m_body.constData() + m_offset, static_cast<size_t>(count));
    m_offset += count;
    return count;
  }

private:
  QByteArray m_body;
  qint64 m_offset = 0;
};

class FakeUpdateNetworkAccessManager final : public QNetworkAccessManager
{
public:
  struct Response
  {
    int status = 200;
    QByteArray location;
    QByteArray body;
  };

  QList<QUrl> requestedUrls;
  QList<Response> responses;

protected:
  QNetworkReply *createRequest(
      Operation, const QNetworkRequest &request, QIODevice *
  ) override
  {
    requestedUrls.append(request.url());
    const auto response = responses.isEmpty() ? Response{} : responses.takeFirst();
    return new FakeNetworkReply(
        request, response.status, response.location, response.body, this
    );
  }
};

} // namespace

void UpdateNetworkTests::metadataUrl_acceptsOnlyPinnedForkEndpoint()
{
  QCOMPARE(
      kUrlUpdateCheck,
      QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest")
  );
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

  const QStringList unsafeQueries{
      QStringLiteral("sig=a%00b"),
      QStringLiteral("sig=a%0Ab"),
      QStringLiteral("sig=a%0Db"),
      QStringLiteral("sig=a%5Cb"),
      QStringLiteral("sig=a%255Cb"),
  };
  for (const auto &query : unsafeQueries) {
    QVERIFY2(
        !isAllowedUpdateRedirect(
            assetUrl(), assetRedirect(
                            QStringLiteral("release-assets.githubusercontent.com"),
                            query
                        ),
            UpdateRequestKind::Installer, 1
        ),
        qPrintable(query)
    );
  }
  for (const auto &path : {
           QStringLiteral("a%2Fb.dmg"),
           QStringLiteral("a%252Fb.dmg"),
           QStringLiteral("%2e%2e/file.dmg"),
           QStringLiteral("%252e%252e/file.dmg"),
           QStringLiteral("a%5Cb.dmg"),
           QStringLiteral("a%00b.dmg"),
           QStringLiteral("a%0Ab.dmg"),
           QStringLiteral("a%0Db.dmg")}) {
    QVERIFY2(
        !isAllowedUpdateRedirect(
            assetUrl(),
            QUrl(
                QStringLiteral("https://release-assets.githubusercontent.com/%1?%2")
                    .arg(path, signedAssetQuery())
            ),
            UpdateRequestKind::Installer, 1
        ),
        qPrintable(path)
    );
  }
  for (int depth = 1; depth <= 10; ++depth) {
    const auto nestedSlash = nestedPercentEncoding(QStringLiteral("%2F"), depth);
    const auto path = QStringLiteral("%1/file.dmg").arg(
        nestedSlash
    );
    QVERIFY2(
        !isAllowedUpdateRedirect(
            assetUrl(),
            QUrl(
                QStringLiteral("https://release-assets.githubusercontent.com/%1?%2")
                    .arg(path, signedAssetQuery())
            ),
            UpdateRequestKind::Installer, 1
        ),
        qPrintable(path)
    );
    const auto nestedSlashQuery =
          QStringLiteral("sig=%1").arg(nestedSlash);
    QVERIFY2(
          !isAllowedUpdateRedirect(
              assetUrl(),
              assetRedirect(
                  QStringLiteral("release-assets.githubusercontent.com"),
                  nestedSlashQuery
              ),
              UpdateRequestKind::Installer, 1
          ),
          qPrintable(nestedSlashQuery)
    );
    for (const auto &escape :
         {QStringLiteral("%5C"), QStringLiteral("%00"), QStringLiteral("%0A"), QStringLiteral("%0D")}) {
        const auto nestedPath = QStringLiteral("%1/file.dmg").arg(
            nestedPercentEncoding(escape, depth)
        );
        QVERIFY2(
            !isAllowedUpdateRedirect(
                assetUrl(),
                QUrl(
                    QStringLiteral("https://release-assets.githubusercontent.com/%1?%2")
                        .arg(nestedPath, signedAssetQuery())
                ),
                UpdateRequestKind::Installer, 1
            ),
            qPrintable(nestedPath)
        );
      const auto query =
          QStringLiteral("sig=%1").arg(nestedPercentEncoding(escape, depth));
      QVERIFY2(
          !isAllowedUpdateRedirect(
              assetUrl(),
              assetRedirect(
                  QStringLiteral("release-assets.githubusercontent.com"),
                  query
              ),
              UpdateRequestKind::Installer, 1
          ),
          qPrintable(query)
      );
    }
  }
  QVERIFY(!isAllowedUpdateRedirect(
      QUrl(QStringLiteral("%1?x=1").arg(assetUrl().toString())),
      assetRedirect(), UpdateRequestKind::Installer, 1
  ));
  QVERIFY(!isAllowedUpdateUrl(
      QUrl(QStringLiteral("%1?").arg(assetUrl().toString())),
      UpdateRequestKind::Installer
  ));
  QVERIFY(!isAllowedUpdateRedirect(
      assetUrl(),
      assetRedirect(
          QStringLiteral("release-assets.githubusercontent.com"),
          QStringLiteral("sig=%1").arg(QString(9 * 1024, QLatin1Char('a')))
      ),
      UpdateRequestKind::Installer, 1
  ));
}

void UpdateNetworkTests::redirectPolicy_handlesSignedGithubCdnRedirectsInNetworkPath()
{
  FakeUpdateNetworkAccessManager manager;
  const auto signedRedirect = assetRedirect();
  manager.responses = {
      {302, signedRedirect.toEncoded(QUrl::FullyEncoded), {}},
      {200, {}, QByteArrayLiteral("signed checksums")},
  };
  deskflow::gui::QtUpdateNetwork network(&manager);
  deskflow::gui::UpdateNetworkOptions options;
  options.kind = UpdateRequestKind::Checksum;
  options.maximumBytes = 1024;
  options.timeoutMs = 1000;
  options.maximumTotalMs = 3000;
  int completed = 0;
  deskflow::gui::UpdateNetworkResult result;
  QVERIFY(network.get(
      assetUrl(), options,
      [&completed, &result](const auto &value) {
        ++completed;
        result = value;
      }
  ) != 0);
  QTRY_COMPARE(completed, 1);
  QCOMPARE(result.status, UpdateNetworkStatus::Success);
  QCOMPARE(result.data, QByteArrayLiteral("signed checksums"));
  QCOMPARE(result.finalUrl, signedRedirect);
  QCOMPARE(manager.requestedUrls, (QList<QUrl>{assetUrl(), signedRedirect}));

  FakeUpdateNetworkAccessManager blockedManager;
  blockedManager.responses = {
      {302,
       QByteArray(
           "https://release-assets.githubusercontent.com/github-production-release-asset/1/token?sig=a%00b"
       ),
       {}},
  };
  deskflow::gui::QtUpdateNetwork blockedNetwork(&blockedManager);
  completed = 0;
  QVERIFY(blockedNetwork.get(
      assetUrl(), options,
      [&completed, &result](const auto &value) {
        ++completed;
        result = value;
      }
  ) != 0);
  QTRY_COMPARE(completed, 1);
  QCOMPARE(result.status, UpdateNetworkStatus::PolicyError);
  QCOMPARE(result.httpStatus, 302);
  QCOMPARE(blockedManager.requestedUrls, (QList<QUrl>{assetUrl()}));
}

void UpdateNetworkTests::redirectLocation_rejectsRawMalformedAndOverlongReferences()
{
  QUrl resolved;
  const QByteArray valid(assetRedirect().toEncoded(QUrl::FullyEncoded));
  QVERIFY(resolveUpdateRedirectLocation(valid, assetUrl(), resolved));
  QCOMPARE(resolved, assetRedirect());

  QVERIFY(resolveUpdateRedirectLocation(
      QByteArrayLiteral("//release-assets.githubusercontent.com/github-production-release-asset/1/token?sig=x"),
      assetUrl(), resolved
  ));
  QCOMPARE(resolved.host(), QStringLiteral("release-assets.githubusercontent.com"));
  QVERIFY(resolveUpdateRedirectLocation(
      QByteArrayLiteral("//evil.example/path"), assetUrl(), resolved
  ));
  QVERIFY(!isAllowedUpdateRedirect(
      assetUrl(), resolved, UpdateRequestKind::Installer, 1
  ));

  const QByteArray encodedSeparator(
      "https://release-assets.githubusercontent.com/path%5cfile"
  );
  QVERIFY(resolveUpdateRedirectLocation(
      encodedSeparator, assetUrl(), resolved
  ));
  QVERIFY(!isAllowedUpdateRedirect(
      assetUrl(), resolved, UpdateRequestKind::Installer, 1
  ));

  auto nul = QByteArrayLiteral("https://release-assets.githubusercontent.com/path");
  nul.append('\0');
  nul.append('x');
  const QList<QByteArray> invalid{
      {},
      QByteArrayLiteral("https://release-assets.githubusercontent.com/%ZZ"),
      QByteArrayLiteral("https://release-assets.githubusercontent.com/%2"),
      QByteArrayLiteral("https://release-assets.githubusercontent.com:bad/path"),
      QByteArrayLiteral("https://[invalid]/path"),
      QByteArrayLiteral("\\\\release-assets.githubusercontent.com\\path"),
      QByteArrayLiteral("https://release-assets.githubusercontent.com/path\r\nX-Evil: yes"),
      nul,
  };
  for (const auto &raw : invalid) {
    QVERIFY2(
        !resolveUpdateRedirectLocation(raw, assetUrl(), resolved),
        raw.constData()
    );
  }

  const auto overlongRelative =
      QStringLiteral("./").repeated(5 * 1024).toUtf8();
  QVERIFY(overlongRelative.size() > 8 * 1024);
  QVERIFY(!resolveUpdateRedirectLocation(overlongRelative, assetUrl(), resolved));

  QVERIFY(resolveUpdateRedirectLocation(
      QByteArrayLiteral("../relative/target"), assetUrl(), resolved
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

void UpdateNetworkTests::inactivityDeadline_resetsOnProgressAndHonorsTotalDeadline()
{
  deskflow::gui::UpdateInactivityDeadline deadline;
  deadline.start(1'000, 30'000, 300'000);
  QVERIFY(!deadline.expired(30'999));
  deadline.markProgress(30'000);
  QVERIFY(!deadline.expired(59'999));
  QVERIFY(deadline.expired(60'000));

  deskflow::gui::UpdateInactivityDeadline largeTransfer;
  largeTransfer.start(0, 30'000, 300'000);
  for (qint64 now = 29'000; now < 300'000; now += 29'000) {
    largeTransfer.markProgress(now);
    QVERIFY(!largeTransfer.expired(now));
  }
  QVERIFY(largeTransfer.expired(300'000));
}

QTEST_MAIN(UpdateNetworkTests)
