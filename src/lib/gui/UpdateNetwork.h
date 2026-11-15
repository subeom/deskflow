/*
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

enum class UpdateNetworkStatus
{
  Success,
  NotModified,
  Cancelled,
  Timeout,
  TooLarge,
  RateLimited,
  Error
};

struct UpdateNetworkOptions
{
  qint64 maximumBytes = 2 * 1024 * 1024;
  int timeoutMs = 30'000;
  QString etag;
};

struct UpdateNetworkResult
{
  UpdateNetworkStatus status = UpdateNetworkStatus::Error;
  int httpStatus = 0;
  QByteArray data;
  QUrl finalUrl;
  QString error;
  QString etag;
  int retryAfterSeconds = 0;
};

using UpdateNetworkCallback = std::function<void(const UpdateNetworkResult &)>;

class IUpdateNetwork
{
public:
  virtual ~IUpdateNetwork() = default;

  virtual quint64 get(const QUrl &url, const UpdateNetworkOptions &options, UpdateNetworkCallback callback) = 0;
  virtual void cancel(quint64 requestId) = 0;
  virtual void cancelAll() = 0;
};

bool isAllowedUpdateRedirect(const QUrl &url);

class QtUpdateNetwork final : public QObject, public IUpdateNetwork
{
public:
  QtUpdateNetwork();
  ~QtUpdateNetwork() override;

  quint64 get(const QUrl &url, const UpdateNetworkOptions &options, UpdateNetworkCallback callback) override;
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
