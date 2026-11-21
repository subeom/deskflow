/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateNetwork.h"

#include "common/Constants.h"
#include "common/UrlConstants.h"
#include "common/VersionInfo.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>

#include <limits>

namespace deskflow::gui {

namespace {

constexpr qint64 kMaximumUrlBytes = 8LL * 1024;
constexpr int kMaximumRedirects = 3;
constexpr int kMaximumRateLimitWaitSeconds = 24 * 60 * 60;
constexpr int kMaximumRequestTimeoutMs = 30'000;
constexpr int kMaximumMetadataTotalTimeoutMs = 30'000;
constexpr int kMaximumInstallerTotalTimeoutMs = 5 * 60'000;
constexpr qint64 kMaximumMetadataBytes = 2LL * 1024 * 1024;
constexpr qint64 kMaximumInstallerBytes = 512LL * 1024 * 1024;

bool isSafeHttpsUrl(const QUrl &url)
{
  if (!url.isValid() || url.scheme() != QStringLiteral("https") || !url.userInfo().isEmpty() ||
      (url.port() != -1 && url.port() != 443) || !url.fragment().isEmpty() || url.host().isEmpty() ||
      url.toEncoded(QUrl::FullyEncoded).size() > kMaximumUrlBytes) {
    return false;
  }

  const auto encoded = QString::fromLatin1(url.toEncoded(QUrl::FullyEncoded)).toLower();
  return !encoded.contains(QStringLiteral("%2f")) && !encoded.contains(QStringLiteral("%5c")) &&
         !encoded.contains(QStringLiteral("%00"));
}

bool hasSafePath(const QUrl &url)
{
  const auto path = url.path(QUrl::FullyDecoded);
  for (const auto &part : path.split(QLatin1Char('/'))) {
    if (part == QStringLiteral(".") || part == QStringLiteral("..")) {
      return false;
    }
  }
  return !path.isEmpty() && path != QStringLiteral("/");
}

bool isTerminalAssetHost(const QString &host)
{
  static const QSet<QString> hosts{
      QStringLiteral("release-assets.githubusercontent.com"),
      QStringLiteral("objects.githubusercontent.com"),
      QStringLiteral("github-releases.githubusercontent.com"),
  };
  return hosts.contains(host);
}

bool isExactLatestUrl(const QUrl &url)
{
  return isSafeHttpsUrl(url) && url.query().isEmpty() &&
         url.toEncoded(QUrl::FullyEncoded) == kUrlUpdateCheck.toUtf8();
}

bool isExactAssetUrl(const QUrl &url)
{
  if (!isSafeHttpsUrl(url) || url.host() != QStringLiteral("github.com") || !url.query().isEmpty() ||
      !hasSafePath(url)) {
    return false;
  }

  static const QRegularExpression pattern(
      QStringLiteral(
          R"(^/subeom/deskflow/releases/download/v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)/([A-Za-z0-9][A-Za-z0-9._+-]*)$)"
      )
  );
  return pattern.match(url.path(QUrl::FullyEncoded)).hasMatch();
}

QString networkHttpError(int status)
{
  return QStringLiteral("GitHub returned HTTP %1").arg(status);
}

bool hasAllowedLimits(const UpdateNetworkOptions &options)
{
  const auto maximumBytes =
      options.kind == UpdateRequestKind::Installer ? kMaximumInstallerBytes : kMaximumMetadataBytes;
  const auto maximumTotal =
      options.kind == UpdateRequestKind::Installer ? kMaximumInstallerTotalTimeoutMs
                                                   : kMaximumMetadataTotalTimeoutMs;
  return options.maximumBytes > 0 && options.maximumBytes <= maximumBytes && options.timeoutMs > 0 &&
         options.timeoutMs <= kMaximumRequestTimeoutMs && options.maximumTotalMs > 0 &&
         options.maximumTotalMs <= maximumTotal;
}

} // namespace

struct QtUpdateNetwork::Request
{
  QNetworkReply *reply = nullptr;
  QTimer *timeoutTimer = nullptr;
  QTimer *totalTimer = nullptr;
  QUrl url;
  UpdateNetworkOptions options;
  UpdateNetworkCallback callback;
  QByteArray body;
  int redirectCount = 0;
  UpdateNetworkStatus failure = UpdateNetworkStatus::HttpError;
  QString failureText;
};

bool isAllowedUpdateUrl(const QUrl &url, UpdateRequestKind kind)
{
  if (kind == UpdateRequestKind::Metadata) {
    return isExactLatestUrl(url);
  }
  return isExactAssetUrl(url);
}

bool isAllowedUpdateRedirect(
    const QUrl &source, const QUrl &redirect, UpdateRequestKind kind, int redirectCount
)
{
  if ((kind != UpdateRequestKind::Checksum && kind != UpdateRequestKind::Installer) ||
      redirectCount < 1 || redirectCount > kMaximumRedirects || !isExactAssetUrl(source) ||
      !isSafeHttpsUrl(redirect) || !isTerminalAssetHost(redirect.host()) || !hasSafePath(redirect)) {
    return false;
  }
  return redirect.query(QUrl::FullyEncoded).toUtf8().size() <= kMaximumUrlBytes;
}

bool isUpdateRateLimited(int httpStatus, const QByteArray &remainingHeader)
{
  return httpStatus == 429 ||
         (httpStatus == 403 && remainingHeader.trimmed() == QByteArrayLiteral("0"));
}

int parseUpdateRetryAfterSeconds(
    const QByteArray &retryAfter, const QByteArray &rateLimitReset, qint64 nowEpochSeconds
)
{
  bool valid = false;
  const auto retry = retryAfter.trimmed().toLongLong(&valid);
  if (valid && retry >= 0) {
    return static_cast<int>(qMin<qint64>(retry, kMaximumRateLimitWaitSeconds));
  }

  valid = false;
  const auto reset = rateLimitReset.trimmed().toLongLong(&valid);
  if (!valid || reset <= nowEpochSeconds) {
    return 0;
  }
  return static_cast<int>(qMin<qint64>(reset - nowEpochSeconds, kMaximumRateLimitWaitSeconds));
}

bool shouldRetryUpdateRequest(
    UpdateRequestKind kind, UpdateNetworkStatus status, int attempt, int httpStatus
)
{
  if (attempt != 0 ||
      (kind != UpdateRequestKind::Metadata && kind != UpdateRequestKind::Checksum)) {
    return false;
  }
  if (status == UpdateNetworkStatus::Timeout || status == UpdateNetworkStatus::NetworkError) {
    return true;
  }
  return status == UpdateNetworkStatus::HttpError &&
         (httpStatus == 408 || (httpStatus >= 500 && httpStatus <= 599));
}

bool appendUpdateBytes(QByteArray &body, const QByteArray &next, qint64 maximumBytes)
{
  if (maximumBytes <= 0 || next.size() > maximumBytes - body.size()) {
    return false;
  }
  body += next;
  return true;
}

bool isCurrentUpdateGeneration(quint64 currentGeneration, quint64 callbackGeneration)
{
  return currentGeneration != 0 && currentGeneration == callbackGeneration;
}

QtUpdateNetwork::QtUpdateNetwork() : m_manager(new QNetworkAccessManager(this))
{
}

QtUpdateNetwork::~QtUpdateNetwork()
{
  cancelAll();
}

quint64
QtUpdateNetwork::get(const QUrl &url, const UpdateNetworkOptions &options, UpdateNetworkCallback callback)
{
  if (!callback) {
    return 0;
  }
  if (!isAllowedUpdateUrl(url, options.kind) || !hasAllowedLimits(options)) {
    callback(
        {UpdateNetworkStatus::PolicyError, 0, {}, url, QStringLiteral("update URL or limit is not allowed"), {}, 0}
    );
    return 0;
  }

  const auto id = m_nextRequestId++;
  if (m_nextRequestId == 0) {
    m_nextRequestId = 1;
  }
  auto *state = new Request;
  state->url = url;
  state->options = options;
  state->callback = std::move(callback);
  state->totalTimer = new QTimer;
  state->totalTimer->setSingleShot(true);
  state->totalTimer->setInterval(options.maximumTotalMs);
  connect(state->totalTimer, &QTimer::timeout, this, [this, id] {
    auto *timedOut = m_requests.value(id);
    if (timedOut == nullptr) {
      return;
    }
    timedOut->failure = UpdateNetworkStatus::Timeout;
    timedOut->failureText = QStringLiteral("update request exceeded its total timeout");
    if (timedOut->reply != nullptr) {
      timedOut->reply->abort();
    } else {
      finishRequest(id, {UpdateNetworkStatus::Timeout, 0, {}, timedOut->url, timedOut->failureText, {}, 0});
    }
  });
  m_requests.insert(id, state);
  state->totalTimer->start();
  beginRequest(id, url);
  return id;
}

void QtUpdateNetwork::cancel(quint64 requestId)
{
  auto *state = m_requests.value(requestId);
  if (state == nullptr) {
    return;
  }
  finishRequest(
      requestId, {UpdateNetworkStatus::Cancelled, 0, {}, state->url, QStringLiteral("update request cancelled"), {}, 0}
  );
}

void QtUpdateNetwork::cancelAll()
{
  const auto ids = m_requests.keys();
  for (const auto id : ids) {
    cancel(id);
  }
}

void QtUpdateNetwork::beginRequest(quint64 id, const QUrl &url)
{
  auto *state = m_requests.value(id);
  if (state == nullptr) {
    return;
  }

  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
  request.setRawHeader("User-Agent", QStringLiteral("%1/%2").arg(kAppName, kVersion).toUtf8());
  if (!state->options.etag.isEmpty() && state->options.kind == UpdateRequestKind::Metadata) {
    request.setRawHeader("If-None-Match", state->options.etag.toUtf8());
  }

  state->body.clear();
  state->failure = UpdateNetworkStatus::HttpError;
  state->failureText.clear();
  state->url = url;
  state->reply = m_manager->get(request);
  state->timeoutTimer = new QTimer(state->reply);
  state->timeoutTimer->setSingleShot(true);
  state->timeoutTimer->setInterval(state->options.timeoutMs);

  connect(state->timeoutTimer, &QTimer::timeout, state->reply, [this, id] {
    auto *timedOut = m_requests.value(id);
    if (timedOut == nullptr || timedOut->reply == nullptr) {
      return;
    }
    timedOut->failure = UpdateNetworkStatus::Timeout;
    timedOut->failureText = QStringLiteral("update request timed out");
    timedOut->reply->abort();
  });

  connect(state->reply, &QNetworkReply::readyRead, state->reply, [this, id] {
    auto *reading = m_requests.value(id);
    if (reading == nullptr || reading->reply == nullptr) {
      return;
    }
    if (!appendUpdateBytes(
            reading->body, reading->reply->readAll(), reading->options.maximumBytes
        )) {
      reading->failure = UpdateNetworkStatus::TooLarge;
      reading->failureText = QStringLiteral("update response exceeds its configured limit");
      reading->reply->abort();
    }
  });

  connect(state->reply, &QNetworkReply::finished, state->reply, [this, id] {
    auto *completed = m_requests.value(id);
    if (completed == nullptr || completed->reply == nullptr) {
      return;
    }

    if (completed->failure == UpdateNetworkStatus::Timeout ||
        completed->failure == UpdateNetworkStatus::TooLarge) {
      finishRequest(id, {completed->failure, 0, {}, completed->url, completed->failureText, {}, 0});
      return;
    }

    const auto status =
        completed->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    static const QSet<int> redirectStatuses{301, 302, 303, 307, 308};
    if (redirectStatuses.contains(status)) {
      const auto location =
          QUrl::fromEncoded(completed->reply->rawHeader(QByteArrayLiteral("Location")));
      const auto redirect =
          location.isRelative() ? completed->reply->url().resolved(location) : location;
      ++completed->redirectCount;
      if (!isAllowedUpdateRedirect(
              completed->url, redirect, completed->options.kind, completed->redirectCount
          )) {
        finishRequest(
            id,
            {UpdateNetworkStatus::PolicyError,
             status,
             {},
             completed->url,
             QStringLiteral("blocked update redirect"),
             {},
             0}
        );
        return;
      }

      completed->reply->disconnect();
      completed->reply->deleteLater();
      completed->reply = nullptr;
      if (completed->timeoutTimer != nullptr) {
        completed->timeoutTimer->stop();
        completed->timeoutTimer->deleteLater();
        completed->timeoutTimer = nullptr;
      }
      beginRequest(id, redirect);
      return;
    }

    if (status == 304) {
      finishRequest(
          id,
          {UpdateNetworkStatus::NotModified,
           status,
           {},
           completed->url,
           {},
           QString::fromUtf8(completed->reply->rawHeader(QByteArrayLiteral("ETag"))),
           0}
      );
      return;
    }

    if (!appendUpdateBytes(
            completed->body, completed->reply->readAll(), completed->options.maximumBytes
        )) {
      finishRequest(
          id,
          {UpdateNetworkStatus::TooLarge,
           status,
           {},
           completed->url,
           QStringLiteral("update response exceeds its configured limit"),
           {},
           0}
      );
      return;
    }

    const auto remaining =
        completed->reply->rawHeader(QByteArrayLiteral("X-RateLimit-Remaining"));
    if (isUpdateRateLimited(status, remaining)) {
      finishRequest(
          id,
          {UpdateNetworkStatus::RateLimited,
           status,
           {},
           completed->url,
           QStringLiteral("GitHub rate limit reached"),
           {},
           parseUpdateRetryAfterSeconds(
               completed->reply->rawHeader(QByteArrayLiteral("Retry-After")),
               completed->reply->rawHeader(QByteArrayLiteral("X-RateLimit-Reset")),
               QDateTime::currentSecsSinceEpoch()
           )}
      );
      return;
    }

    if (completed->reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
      const auto resultStatus =
          status == 0 ? UpdateNetworkStatus::NetworkError : UpdateNetworkStatus::HttpError;
      finishRequest(
          id,
          {resultStatus,
           status,
           {},
           completed->url,
           status == 0 ? QStringLiteral("update network request failed")
                       : networkHttpError(status),
           {},
           0}
      );
      return;
    }

    finishRequest(
        id,
        {UpdateNetworkStatus::Success,
         status,
         completed->body,
         completed->url,
         {},
         QString::fromUtf8(completed->reply->rawHeader(QByteArrayLiteral("ETag"))),
         0}
    );
  });

  state->timeoutTimer->start();
}

void QtUpdateNetwork::finishRequest(quint64 id, const UpdateNetworkResult &result)
{
  auto *state = m_requests.take(id);
  if (state == nullptr) {
    return;
  }

  if (state->timeoutTimer != nullptr) {
    state->timeoutTimer->stop();
    state->timeoutTimer->deleteLater();
  }
  if (state->totalTimer != nullptr) {
    state->totalTimer->stop();
    state->totalTimer->disconnect();
    state->totalTimer->deleteLater();
  }
  if (state->reply != nullptr) {
    state->reply->disconnect();
    if (state->reply->isRunning()) {
      state->reply->abort();
    }
    state->reply->deleteLater();
  }

  const auto callback = std::move(state->callback);
  delete state;
  callback(result);
}

} // namespace deskflow::gui
