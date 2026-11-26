/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QObject>

#include "base/Log.h"

#include <memory>

class QLocalSocket;

namespace deskflow::core::ipc {
class CoreIpcServer;
}

class CoreIpcServerSecurityTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void cleanup();

  void rejectsMissingRegistrationAndWrongCapability();
  void exactCapabilityAuthorizesAndDisconnectRemovesIt();
  void malformedRawSubscriptionNeverAuthorizes();
  void malformedRawMessageRemovesExistingAuthorization();
  void ordinaryPayloadsContainingIndicatorNamesStayConnected();
  void subscriptionExpiresAndCanRenew();
  void disconnectFromOnePeerKeepsOtherSubscription();

private:
  std::unique_ptr<QLocalSocket> connectClient(bool registered = true);
  QByteArray send(std::unique_ptr<QLocalSocket> &client, const QByteArray &message);

  deskflow::core::ipc::CoreIpcServer *m_server = nullptr;
  QString m_capability;
  QString m_socketName;
  Log m_log;
};
