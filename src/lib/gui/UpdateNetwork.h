/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;

namespace deskflow::gui {

enum class UpdateRequestKind
{
  Metadata,
  Checksum,
  Installer
};

enum class UpdateNetworkStatus
{
  Success,
  NotModified,
  Cancelled,
  Timeout,
  TooLarge,
  RateLimited,
  NetworkError,
  HttpError,
  PolicyError
};

struct UpdateNetworkOptions
{
  qint64 maximumBytes = 2LL * 1024 * 1024;
  int timeoutMs = 30'000;
  int maximumTotalMs = 30'000;
  QString etag;
  UpdateRequestKind kind = UpdateRequestKind::Metadata;
};

struct UpdateNetworkResult
{
  UpdateNetworkStatus status = UpdateNetworkStatus::HttpError;
  int httpStatus = 0;
  QByteArray data;
  QUrl finalUrl;
  QString error;
  QString etag;
  int retryAfterSeconds = 0;
};

class UpdateInactivityDeadline
{
public:
  void start(qint64 nowMs, qint64 inactivityMs, qint64 totalMs);
  void markProgress(qint64 nowMs);
  bool expired(qint64 nowMs) const;

private:
  qint64 m_startedMs = 0;
  qint64 m_lastProgressMs = 0;
  qint64 m_inactivityMs = 0;
  qint64 m_totalMs = 0;
};

using UpdateNetworkCallback = std::function<void(const UpdateNetworkResult &)>;

class IUpdateNetwork
{
public:
  virtual ~IUpdateNetwork() = default;

  virtual quint64
  get(const QUrl &url, const UpdateNetworkOptions &options, UpdateNetworkCallback callback) = 0;
  virtual void cancel(quint64 requestId) = 0;
  virtual void cancelAll() = 0;
};

bool isAllowedUpdateUrl(const QUrl &url, UpdateRequestKind kind);
bool isAllowedUpdateRedirect(
    const QUrl &source, const QUrl &redirect, UpdateRequestKind kind, int redirectCount
);
bool resolveUpdateRedirectLocation(
    const QByteArray &rawLocation, const QUrl &base, QUrl &resolved
);
bool isUpdateRateLimited(int httpStatus, const QByteArray &remainingHeader);
int parseUpdateRetryAfterSeconds(
    const QByteArray &retryAfter, const QByteArray &rateLimitReset, qint64 nowEpochSeconds
);
bool shouldRetryUpdateRequest(
    UpdateRequestKind kind, UpdateNetworkStatus status, int attempt, int httpStatus = 0
);
bool appendUpdateBytes(QByteArray &body, const QByteArray &next, qint64 maximumBytes);
bool isCurrentUpdateGeneration(quint64 currentGeneration, quint64 callbackGeneration);

class QtUpdateNetwork final : public QObject, public IUpdateNetwork
{
public:
  QtUpdateNetwork();
  explicit QtUpdateNetwork(QNetworkAccessManager *manager);
  ~QtUpdateNetwork() override;

  quint64 get(
      const QUrl &url, const UpdateNetworkOptions &options, UpdateNetworkCallback callback
  ) override;
  void cancel(quint64 requestId) override;
  void cancelAll() override;

private:
  struct Request;

  void beginRequest(quint64 id, const QUrl &url);
  void finishRequest(quint64 id, const UpdateNetworkResult &result);

  QNetworkAccessManager *m_manager = nullptr;
  QHash<quint64, Request *> m_requests;
  quint64 m_nextRequestId = 1;
};

} // namespace deskflow::gui
