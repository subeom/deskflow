/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/TunnelTestRunner.h"

#include "common/TunnelAddress.h"
#include "gui/tunnel/SecretStoreFactory.h"
#include "gui/tunnel/TunnelController.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>

namespace deskflow::tunnel {

QString maskedClientId(const QString &clientId)
{
  return clientId.left(std::min<qsizetype>(4, clientId.size() / 2)) + QChar(0x2026);
}

namespace {

struct TestTexts
{
  Q_DECLARE_TR_FUNCTIONS(TestTexts)
};

constexpr TestStage kAllStages[] = {TestStage::Cloudflared, TestStage::Credentials, TestStage::Tls,
                                    TestStage::Hostname,    TestStage::Process,     TestStage::LocalPort,
                                    TestStage::Connection};

QString stateWord(StageState state)
{
  switch (state) {
  case StageState::Pending:
    return QStringLiteral("pending");
  case StageState::Running:
    return QStringLiteral("running");
  case StageState::Ok:
    return QStringLiteral("ok");
  case StageState::Failed:
    return QStringLiteral("FAILED");
  case StageState::Skipped:
    return QStringLiteral("skipped");
  }
  return {};
}

QString stageKey(TestStage stage)
{
  switch (stage) {
  case TestStage::Cloudflared:
    return QStringLiteral("cloudflared");
  case TestStage::Credentials:
    return QStringLiteral("service token");
  case TestStage::Tls:
    return QStringLiteral("TLS");
  case TestStage::Hostname:
    return QStringLiteral("hostname");
  case TestStage::Process:
    return QStringLiteral("process");
  case TestStage::LocalPort:
    return QStringLiteral("local port");
  case TestStage::Connection:
    return QStringLiteral("connection");
  }
  return {};
}

} // namespace

QString TunnelTestReport::stageName(TestStage stage)
{
  switch (stage) {
  case TestStage::Cloudflared:
    return TestTexts::tr("cloudflared is installed");
  case TestStage::Credentials:
    return TestTexts::tr("Service Token is stored");
  case TestStage::Tls:
    return TestTexts::tr("TLS is on");
  case TestStage::Hostname:
    return TestTexts::tr("Tunnel hostname is valid");
  case TestStage::Process:
    return TestTexts::tr("cloudflared is running");
  case TestStage::LocalPort:
    return TestTexts::tr("Local port is open");
  case TestStage::Connection:
    return TestTexts::tr("Connection through the tunnel");
  }
  return {};
}

QString TunnelTestReport::summaryLine(const QDateTime &when, const QString &system) const
{
  QStringList parts;
  for (const auto &result : stages) {
    parts << QStringLiteral("%1 %2").arg(stageKey(result.stage), stateWord(result.state));
  }
  const auto verdict = ok() ? QStringLiteral("works") : QStringLiteral("does not work");
  QString line = QStringLiteral("Deskflow tunnel test %1 (%2): %3 via %4")
                     .arg(
                         when.toString(QStringLiteral("yyyy-MM-dd HH:mm")), system, verdict,
                         hostname.isEmpty() ? QStringLiteral("-") : hostname
                     );
  line += QStringLiteral("; ") + parts.join(QStringLiteral(", "));
  if (failure != TunnelFailure::None) {
    line += QStringLiteral("; reason: %1").arg(failureKey(failure));
  }
  return line;
}

TunnelTestRunner::TunnelTestRunner(Dependencies dependencies, QObject *parent)
    : QObject(parent),
      m_deps(std::move(dependencies)),
      m_timeout(new QTimer(this))
{
  if (!m_deps.store) {
    m_deps.store = createSystemSecretStore();
  }
  if (!m_deps.locate) {
    m_deps.locate = [](const QString &path) { return locateCloudflared(path); };
  }
  if (!m_deps.holdConnection) {
    m_deps.holdConnection = systemHoldConnection();
  }

  m_timeout->setSingleShot(true);
  connect(m_timeout, &QTimer::timeout, this, [this] {
    if (m_running) {
      onFailed(TunnelFailure::StartTimedOut);
    }
  });
}

TunnelTestRunner::~TunnelTestRunner()
{
  cancel();
}

StageResult &TunnelTestRunner::stage(TestStage which)
{
  return m_report.stages[static_cast<int>(which)];
}

void TunnelTestRunner::setStage(TestStage which, StageState state, const QString &detail)
{
  auto &result = stage(which);
  result.state = state;
  result.detail = detail;
  Q_EMIT changed();
}

void TunnelTestRunner::disconnectTunnel()
{
  disconnect(m_listeningConnection);
  disconnect(m_failedConnection);
  m_timeout->stop();
}

void TunnelTestRunner::start()
{
  cancel();

  ++m_generation;
  m_report = {};
  for (const auto which : kAllStages) {
    StageResult result;
    result.stage = which;
    m_report.stages.append(result);
  }
  m_running = true;
  m_startedTunnel = false;

  const auto config = m_deps.controller->config();
  m_report.hostname = config.hostname;
  m_report.localPort = config.port();
  Q_EMIT changed();

  checkPrerequisites();
}

void TunnelTestRunner::cancel()
{
  if (!m_running) {
    return;
  }
  ++m_generation;
  disconnectTunnel();
  m_running = false;
  if (m_startedTunnel) {
    m_startedTunnel = false;
    m_deps.controller->release();
  }
}

void TunnelTestRunner::checkPrerequisites()
{
  const auto config = m_deps.controller->config();
  TunnelFailure first = TunnelFailure::None;
  const auto note = [&first](TunnelFailure failure) {
    if (first == TunnelFailure::None) {
      first = failure;
    }
  };

  const auto location = m_deps.locate(config.cloudflaredPath);
  if (location.found()) {
    setStage(TestStage::Cloudflared, StageState::Ok, location.path);
  } else {
    setStage(TestStage::Cloudflared, StageState::Failed, failureText(TunnelFailure::CloudflaredMissing));
    note(TunnelFailure::CloudflaredMissing);
  }

  // only the Client ID is read here: asking whether a token is stored does not need the Secret
  StoreResult stored;
  const auto clientId = m_deps.store->storedClientId(&stored);
  if (clientId.has_value() && !clientId->isEmpty()) {
    setStage(TestStage::Credentials, StageState::Ok, TestTexts::tr("Client ID %1").arg(maskedClientId(*clientId)));
  } else {
    setStage(TestStage::Credentials, StageState::Failed, failureText(TunnelFailure::CredentialsMissing));
    note(TunnelFailure::CredentialsMissing);
  }

  if (config.tlsEnabled) {
    setStage(TestStage::Tls, StageState::Ok);
  } else {
    setStage(TestStage::Tls, StageState::Failed, failureText(TunnelFailure::TlsRequired));
    note(TunnelFailure::TlsRequired);
  }

  if (isValidHostname(config.hostname)) {
    setStage(TestStage::Hostname, StageState::Ok, config.hostname);
  } else {
    setStage(TestStage::Hostname, StageState::Failed, failureText(TunnelFailure::InvalidHostname));
    note(TunnelFailure::InvalidHostname);
  }

  if (first != TunnelFailure::None) {
    skipTheRest(TestStage::Hostname);
    fail(first);
    return;
  }

  waitForTheTunnel();
}

void TunnelTestRunner::waitForTheTunnel()
{
  setStage(TestStage::Process, StageState::Running);

  if (m_deps.controller->isListening()) {
    // the tunnel of the client that runs: it is used as it is and is not touched afterwards
    onListening();
    return;
  }

  // a tunnel that is on its way for the client (starting, or between two starts) is not the test's to let go
  const auto before = m_deps.controller->status().state;
  m_startedTunnel = before == TunnelState::Idle || before == TunnelState::Failed || before == TunnelState::Stopping;
  m_listeningConnection = connect(m_deps.controller, &ITunnelGate::listening, this, &TunnelTestRunner::onListening);
  m_failedConnection = connect(m_deps.controller, &ITunnelGate::failed, this, &TunnelTestRunner::onFailed);
  m_timeout->start(m_deps.startTimeoutMs);
  m_deps.controller->acquire();
}

void TunnelTestRunner::onListening()
{
  if (!m_running) {
    return;
  }
  disconnectTunnel();

  const auto status = m_deps.controller->status();
  const int port = status.localPort > 0 ? status.localPort : m_report.localPort;
  m_report.localPort = port;
  setStage(
      TestStage::Process, StageState::Ok,
      m_startedTunnel ? TestTexts::tr("started for this test") : TestTexts::tr("running for the client")
  );
  setStage(TestStage::LocalPort, StageState::Ok, QStringLiteral("%1:%2").arg(QLatin1String(kLoopbackHost)).arg(port));
  holdAConnection();
}

void TunnelTestRunner::onFailed(TunnelFailure failure)
{
  if (!m_running) {
    return;
  }
  disconnectTunnel();

  const auto text = failureText(failure);
  if (failure == TunnelFailure::PortBusy || failure == TunnelFailure::StartTimedOut) {
    // cloudflared started, and its local port did not open
    setStage(TestStage::Process, StageState::Ok, TestTexts::tr("started"));
    setStage(TestStage::LocalPort, StageState::Failed, text);
  } else {
    setStage(TestStage::Process, StageState::Failed, text);
    setStage(TestStage::LocalPort, StageState::Skipped);
  }
  setStage(TestStage::Connection, StageState::Skipped);
  fail(failure);
}

void TunnelTestRunner::holdAConnection()
{
  setStage(TestStage::Connection, StageState::Running);
  m_failuresBefore = m_deps.controller->supervisor().originFailureCount();

  const auto generation = m_generation;
  QPointer<TunnelTestRunner> self(this);
  m_deps.holdConnection(m_report.localPort, m_deps.holdMs, [self, generation](HoldOutcome outcome) {
    if (!self || self->m_generation != generation || !self->m_running) {
      return;
    }
    // cloudflared writes why a connection failed a moment after it closed it
    QTimer::singleShot(self->m_deps.graceMs, self.data(), [self, generation, outcome] {
      if (self && self->m_generation == generation && self->m_running) {
        self->judgeTheConnection(outcome);
      }
    });
  });
}

void TunnelTestRunner::judgeTheConnection(HoldOutcome outcome)
{
  const auto &supervisor = m_deps.controller->supervisor();
  const bool refusedByEdge = supervisor.originFailureCount() > m_failuresBefore;
  const auto reason = refusedByEdge ? supervisor.lastOriginFailure() : TunnelFailure::None;

  if (reason != TunnelFailure::None) {
    setStage(TestStage::Connection, StageState::Failed, failureText(reason));
    fail(reason);
    return;
  }

  switch (outcome) {
  case HoldOutcome::Held:
    setStage(
        TestStage::Connection, StageState::Ok,
        TestTexts::tr("a connection was carried to the server and held for %1 s").arg(m_deps.holdMs / 1000.0, 0, 'f', 1)
    );
    conclude();
    return;
  case HoldOutcome::ClosedByPeer:
    // cloudflared closed it and said nothing that is known: the server did not take the connection
    setStage(
        TestStage::Connection, StageState::Failed,
        TestTexts::tr("cloudflared closed the connection: the server or the tunnel at the server did not take it")
    );
    fail(TunnelFailure::ConnectFailed);
    return;
  case HoldOutcome::Refused:
    setStage(TestStage::Connection, StageState::Failed, TestTexts::tr("the local port refused the connection"));
    fail(TunnelFailure::ConnectFailed);
    return;
  }
}

void TunnelTestRunner::skipTheRest(TestStage after)
{
  for (const auto which : kAllStages) {
    if (static_cast<int>(which) > static_cast<int>(after)) {
      setStage(which, StageState::Skipped);
    }
  }
}

void TunnelTestRunner::fail(TunnelFailure failure)
{
  m_report.failure = failure;
  conclude();
}

void TunnelTestRunner::conclude()
{
  disconnectTunnel();
  m_running = false;
  m_report.finished = true;
  if (m_startedTunnel) {
    m_startedTunnel = false;
    // the tunnel that was started for the test goes when nothing else wants it
    m_deps.controller->release();
  }
  Q_EMIT changed();
  Q_EMIT finished();
}

TunnelTestRunner::HoldConnection TunnelTestRunner::systemHoldConnection()
{
  return [](int port, int holdMs, std::function<void(HoldOutcome)> done) {
    auto *socket = new QTcpSocket;
    auto *timer = new QTimer(socket);
    timer->setSingleShot(true);

    // the first of the events tells it, and only that one
    const auto told = std::make_shared<bool>(false);
    const auto wasConnected = std::make_shared<bool>(false);
    const auto tell = [socket, told, done = std::move(done)](HoldOutcome outcome) {
      if (*told) {
        return;
      }
      *told = true;
      socket->abort();
      socket->deleteLater();
      done(outcome);
    };

    QObject::connect(socket, &QTcpSocket::connected, socket, [timer, holdMs, wasConnected] {
      *wasConnected = true;
      timer->start(holdMs);
    });
    QObject::connect(socket, &QTcpSocket::disconnected, socket, [tell] { tell(HoldOutcome::ClosedByPeer); });
    QObject::connect(
        socket, &QAbstractSocket::errorOccurred, socket,
        [tell, wasConnected](QAbstractSocket::SocketError) {
          tell(*wasConnected ? HoldOutcome::ClosedByPeer : HoldOutcome::Refused);
        }
    );
    QObject::connect(timer, &QTimer::timeout, socket, [tell] { tell(HoldOutcome::Held); });

    socket->connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
    // a connection that does not open within the time is not held: it is refused
    QTimer::singleShot(holdMs, socket, [socket, tell] {
      if (socket->state() != QAbstractSocket::ConnectedState) {
        tell(HoldOutcome::Refused);
      }
    });
  };
}

} // namespace deskflow::tunnel
