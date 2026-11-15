/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateNetwork.h"

#include "common/Constants.h"
#include "common/VersionInfo.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>

#include <algorithm>

namespace deskflow::gui {

namespace {

constexpr auto kMaximumRedirects = 5;

} // namespace

struct QtUpdateNetwork::Request
{
  QNetworkReply *reply = nullptr;
  QTimer *timer = nullptr;
  QUrl url;
  UpdateNetworkOptions options;
  UpdateNetworkCallback callback;
  QByteArray body;
  int redirectCount = 0;
  UpdateNetworkStatus failure = UpdateNetworkStatus::Error;
  QString failureText;
};

bool isAllowedUpdateRedirect(const QUrl &url)
{
  if (url.scheme() != QStringLiteral("https") || !url.userInfo().isEmpty() || (url.port() != -1 && url.port() != 443) ||
      !url.fragment().isEmpty()) {
    return false;
  }

  const auto host = url.host();
  if (host == QStringLiteral("api.github.com")) {
    return url.path() == QStringLiteral("/repos/deskflow/deskflow/releases/latest") && url.query().isEmpty();
  }
  if (host == QStringLiteral("github.com")) {
    return url.path().startsWith(QStringLiteral("/deskflow/deskflow/releases/download/")) && url.query().isEmpty();
  }

  static const QSet<QString> redirectHosts = {
      QStringLiteral("release-assets.githubusercontent.com"),
      QStringLiteral("objects.githubusercontent.com"),
      QStringLiteral("github-releases.githubusercontent.com"),
  };
  return redirectHosts.contains(host);
}

QtUpdateNetwork::QtUpdateNetwork() : m_manager(new QNetworkAccessManager(this))
{
}

QtUpdateNetwork::~QtUpdateNetwork()
{
  cancelAll();
}

quint64 QtUpdateNetwork::get(const QUrl &url, const UpdateNetworkOptions &options, UpdateNetworkCallback callback)
{
  if (!isAllowedUpdateRedirect(url)) {
    callback({UpdateNetworkStatus::Error, 0, {}, url, QStringLiteral("update URL is not on the allowlist"), {}, 0});
    return 0;
  }

  const auto id = m_nextRequestId++;
  auto *request = new Request;
  request->url = url;
  request->options = options;
  request->callback = std::move(callback);
  m_requests.insert(id, request);
  beginRequest(id, url);
  return id;
}

void QtUpdateNetwork::cancel(quint64 requestId)
{
  auto *request = m_requests.value(requestId);
  if (request == nullptr) {
    return;
  }
  finishRequest(requestId, {UpdateNetworkStatus::Cancelled, 0, {}, request->url, QStringLiteral("cancelled"), {}, 0});
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
  if (!state->options.etag.isEmpty()) {
    request.setRawHeader("If-None-Match", state->options.etag.toUtf8());
  }

  state->body.clear();
  state->failure = UpdateNetworkStatus::Error;
  state->failureText.clear();
  state->url = url;
  state->reply = m_manager->get(request);
  state->timer = new QTimer(state->reply);
  state->timer->setSingleShot(true);
  state->timer->setInterval(state->options.timeoutMs);

  connect(state->timer, &QTimer::timeout, state->reply, [this, id] {
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
    reading->body += reading->reply->readAll();
    if (reading->body.size() > reading->options.maximumBytes) {
      reading->failure = UpdateNetworkStatus::TooLarge;
      reading->failureText = QStringLiteral("update response exceeds the configured limit");
      reading->reply->abort();
    }
  });

  connect(state->reply, &QNetworkReply::finished, state->reply, [this, id] {
    auto *completed = m_requests.value(id);
    if (completed == nullptr || completed->reply == nullptr) {
      return;
    }

    if (completed->failure == UpdateNetworkStatus::Timeout || completed->failure == UpdateNetworkStatus::TooLarge) {
      finishRequest(id, {completed->failure, 0, {}, completed->url, completed->failureText, {}, 0});
      return;
    }

    const auto status = completed->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto redirectStatuses = {301, 302, 303, 307, 308};
    if (std::ranges::find(redirectStatuses, status) != redirectStatuses.end()) {
      const auto location = QUrl::fromEncoded(completed->reply->rawHeader(QByteArrayLiteral("Location")));
      const auto redirect = location.isRelative() ? completed->reply->url().resolved(location) : location;
      if (++completed->redirectCount > kMaximumRedirects || !isAllowedUpdateRedirect(redirect)) {
        finishRequest(
            id, {UpdateNetworkStatus::Error, status, {}, redirect, QStringLiteral("blocked update redirect"), {}, 0}
        );
        return;
      }
      completed->reply->disconnect();
      completed->reply->deleteLater();
      completed->reply = nullptr;
      if (completed->timer != nullptr) {
        completed->timer->stop();
        completed->timer->deleteLater();
        completed->timer = nullptr;
      }
      beginRequest(id, redirect);
      return;
    }

    if (status == 304) {
      finishRequest(id, {UpdateNetworkStatus::NotModified, status, {}, completed->url, {}, {}, 0});
      return;
    }

    const auto retryAfterHeader = completed->reply->rawHeader(QByteArrayLiteral("Retry-After"));
    const auto remainingHeader = completed->reply->rawHeader(QByteArrayLiteral("X-RateLimit-Remaining"));
    const int retryAfter = retryAfterHeader.isEmpty() ? 0 : retryAfterHeader.toInt();
    if (status == 429 || (status == 403 && remainingHeader.trimmed() == QByteArrayLiteral("0"))) {
      finishRequest(
          id, {UpdateNetworkStatus::RateLimited,
               status,
               {},
               completed->url,
               QStringLiteral("GitHub rate limit reached"),
               {},
               retryAfter}
      );
      return;
    }

    completed->body += completed->reply->readAll();
    if (completed->reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
      finishRequest(
          id, {UpdateNetworkStatus::Error, status, {}, completed->url, completed->reply->errorString(), {}, 0}
      );
      return;
    }

    if (completed->body.size() > completed->options.maximumBytes) {
      finishRequest(
          id, {UpdateNetworkStatus::TooLarge,
               status,
               {},
               completed->url,
               QStringLiteral("update response exceeds the configured limit"),
               {},
               0}
      );
      return;
    }

    const auto etag = QString::fromUtf8(completed->reply->rawHeader(QByteArrayLiteral("ETag")));
    finishRequest(id, {UpdateNetworkStatus::Success, status, completed->body, completed->url, {}, etag, 0});
  });

  state->timer->start();
}

void QtUpdateNetwork::finishRequest(quint64 id, const UpdateNetworkResult &result)
{
  auto *state = m_requests.take(id);
  if (state == nullptr) {
    return;
  }

  if (state->timer != nullptr) {
    state->timer->stop();
    state->timer->deleteLater();
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
