/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/core/ConnectionProbe.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QTimer>

// A network that is a table: which address answers how, so that tests of the probe and of what shows its results do not
// need a real network. Nothing here is ever written to a socket: IProbeSocket has no way to do that.
namespace probetest {

// what a fake address does when it is connected to
struct Script
{
  enum class Kind
  {
    Open,
    Fail,
    Silent
  };
  Kind kind = Kind::Silent;
  deskflow::diagnosis::FailureKind failure = deskflow::diagnosis::FailureKind::Unknown;
  QString detail;
  int delayMs = 0;
};

// the whole fake network: which address does what, which were connected to, which were closed
struct FakeNetwork
{
  QMap<QString, Script> scripts;
  QMap<QString, QStringList> names;
  QMap<QString, QString> resolveErrors;
  bool resolveAtOnce = false;

  QStringList connected;
  QStringList aborted;
  QStringList resolved;
};

class FakeProbeSocket : public deskflow::gui::IProbeSocket
{
public:
  explicit FakeProbeSocket(FakeNetwork *network) : m_network(network)
  {
  }

  void connectToHost(const QString &address, quint16) override
  {
    m_address = address;
    m_network->connected << address;
    const auto script = m_network->scripts.value(address);
    if (script.kind == Script::Kind::Silent) {
      return;
    }
    QTimer::singleShot(script.delayMs, this, [this, script] {
      if (m_aborted) {
        return;
      }
      if (script.kind == Script::Kind::Open) {
        Q_EMIT connected();
      } else {
        Q_EMIT failed(script.failure, script.detail);
      }
    });
  }

  void abort() override
  {
    m_aborted = true;
    m_network->aborted << m_address;
  }

private:
  FakeNetwork *m_network;
  QString m_address;
  bool m_aborted = false;
};

inline deskflow::gui::ConnectionProbe *makeProbe(FakeNetwork &network, int timeoutMs = 60)
{
  auto resolver = [&network](const QString &host, const deskflow::gui::ConnectionProbe::ResolveDone &done) {
    network.resolved << host;
    const auto addresses = network.names.value(host);
    const auto error = network.resolveErrors.value(host);
    if (network.resolveAtOnce) {
      done(addresses, error);
    } else {
      QTimer::singleShot(0, [done, addresses, error] { done(addresses, error); });
    }
  };
  return new deskflow::gui::ConnectionProbe(resolver, [&network] { return new FakeProbeSocket(&network); }, timeoutMs);
}

inline Script open(int delayMs = 0)
{
  return {Script::Kind::Open, deskflow::diagnosis::FailureKind::Unknown, {}, delayMs};
}

inline Script fail(deskflow::diagnosis::FailureKind kind, const QString &detail, int delayMs = 0)
{
  return {Script::Kind::Fail, kind, detail, delayMs};
}

} // namespace probetest
