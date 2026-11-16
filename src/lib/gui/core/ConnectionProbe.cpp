/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ConnectionProbe.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QHostInfo>
#include <QMap>
#include <QNetworkProxy>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>

#include <vector>

namespace deskflow::gui {

using diagnosis::FailureKind;

namespace {

constexpr auto kMinPort = 1;
constexpr auto kMaxPort = 65535;

// a connect that is closed again as soon as it is answered: nothing is ever written to the socket
class TcpProbeSocket : public IProbeSocket
{
public:
  TcpProbeSocket()
  {
    m_socket.setParent(this);
    // the test is about the network path of this computer, not about a proxy
    m_socket.setProxy(QNetworkProxy::NoProxy);
    connect(&m_socket, &QTcpSocket::connected, this, [this] { Q_EMIT connected(); });
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
      Q_EMIT failed(failureKindFromSocketError(static_cast<int>(error)), m_socket.errorString());
    });
  }

  void connectToHost(const QString &address, quint16 port) override
  {
    m_socket.connectToHost(QHostAddress(address), port);
  }

  void abort() override
  {
    m_socket.abort();
  }

private:
  QTcpSocket m_socket;
};

QString portProblem(const QString &portText)
{
  return QCoreApplication::translate("ConnectionProbe", "the port \"%1\" is not a number from %2 to %3")
      .arg(portText)
      .arg(kMinPort)
      .arg(kMaxPort);
}

} // namespace

QList<ServerEntry> parseServerEntries(const QString &text, int defaultPort)
{
  QList<ServerEntry> entries;
  const auto parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
  for (const auto &part : parts) {
    const auto typed = part.trimmed();
    if (typed.isEmpty()) {
      continue;
    }

    ServerEntry entry;
    entry.text = typed;
    entry.port = defaultPort;
    QString portText;
    bool hasPort = false;

    if (typed.startsWith(QLatin1Char('['))) {
      const auto close = typed.indexOf(QLatin1Char(']'));
      if (close < 0) {
        entry.problem =
            QCoreApplication::translate("ConnectionProbe", "the closing bracket ] of the IPv6 address is missing");
      } else {
        entry.host = typed.mid(1, close - 1);
        const auto rest = typed.mid(close + 1);
        if (rest.startsWith(QLatin1Char(':'))) {
          hasPort = true;
          portText = rest.mid(1);
        } else if (rest.isEmpty()) {
          entry.problem = QCoreApplication::translate(
              "ConnectionProbe", "an IPv6 address in brackets needs a port after it, like [address]:24800"
          );
        } else {
          entry.problem = QCoreApplication::translate(
              "ConnectionProbe", "there is text after the closing bracket ] that is not a port"
          );
        }
      }
    } else if (typed.count(QLatin1Char(':')) == 1) {
      entry.host = typed.section(QLatin1Char(':'), 0, 0);
      hasPort = true;
      portText = typed.section(QLatin1Char(':'), 1);
    } else {
      // a name, an IPv4 address, or an IPv6 address without brackets and so without a port
      entry.host = typed;
    }

    if (entry.problem.isEmpty() && hasPort) {
      bool isNumber = false;
      const auto port = portText.trimmed().toInt(&isNumber);
      if (!isNumber || port < kMinPort || port > kMaxPort) {
        entry.problem = portProblem(portText);
      } else {
        entry.port = port;
      }
    }
    if (entry.problem.isEmpty() && entry.host.trimmed().isEmpty()) {
      entry.problem = QCoreApplication::translate("ConnectionProbe", "the address in front of the port is missing");
    }
    entry.host = entry.host.trimmed();
    entries.append(entry);
  }
  return entries;
}

FailureKind failureKindFromSocketError(int socketError)
{
  switch (static_cast<QAbstractSocket::SocketError>(socketError)) {
  case QAbstractSocket::ConnectionRefusedError:
    return FailureKind::Refused;
  case QAbstractSocket::HostNotFoundError:
    return FailureKind::DnsFailure;
  case QAbstractSocket::SocketTimeoutError:
    return FailureKind::TimedOut;
  // what the system answers to a connect that finds no way to the host or network: unreachable, no route, down
  case QAbstractSocket::NetworkError:
    return FailureKind::NoRoute;
  case QAbstractSocket::SocketAccessError:
    return FailureKind::AccessDenied;
  case QAbstractSocket::RemoteHostClosedError:
    return FailureKind::Reset;
  default:
    return FailureKind::Unknown;
  }
}

//
// ConnectionProbe
//

// one test of a list of entries, shared with the callbacks that may come after the test was dropped
struct ConnectionProbe::Run
{
  QList<ServerEntry> entries;
  // per entry, the result of each of its addresses by their place in the list the name resolved to
  QList<QMap<int, ProbeResult>> results;
  // addresses that are being tried plus names that are being resolved
  int pending = 0;
  bool dropped = false;
  QList<QPointer<IProbeSocket>> sockets;
};

ConnectionProbe::ConnectionProbe(Resolver resolver, SocketFactory socketFactory, int timeoutMs, QObject *parent)
    : QObject(parent),
      m_resolver(std::move(resolver)),
      m_socketFactory(std::move(socketFactory)),
      m_timeoutMs(timeoutMs)
{
}

ConnectionProbe::~ConnectionProbe()
{
  cancel();
}

ConnectionProbe *ConnectionProbe::createSystem(QObject *parent, int timeoutMs)
{
  auto *probe = new ConnectionProbe({}, [] { return new TcpProbeSocket; }, timeoutMs, parent);
  probe->m_resolver = [probe](const QString &host, const ResolveDone &done) {
    QHostInfo::lookupHost(host, probe, [done](const QHostInfo &info) {
      QStringList addresses;
      for (const auto &address : info.addresses()) {
        addresses << address.toString();
      }
      done(addresses, info.error() == QHostInfo::NoError ? QString() : info.errorString());
    });
  };
  return probe;
}

bool ConnectionProbe::isRunning() const
{
  return m_run != nullptr;
}

int ConnectionProbe::timeoutMs() const
{
  return m_timeoutMs;
}

void ConnectionProbe::cancel()
{
  if (!m_run) {
    return;
  }
  m_run->dropped = true;
  for (const auto &socket : std::as_const(m_run->sockets)) {
    if (socket) {
      socket->disconnect(this);
      socket->abort();
      socket->deleteLater();
    }
  }
  m_run.reset();
}

void ConnectionProbe::start(const QList<ServerEntry> &entries)
{
  cancel();

  auto run = std::make_shared<Run>();
  run->entries = entries;
  run->results.resize(entries.size());
  run->pending = static_cast<int>(entries.size());
  m_run = run;

  if (entries.isEmpty()) {
    m_run.reset();
    Q_EMIT finished({});
    return;
  }

  for (int index = 0; index < entries.size() && !run->dropped; ++index) {
    begin(run, index, entries.at(index));
  }
}

void ConnectionProbe::begin(const std::shared_ptr<Run> &run, int entryIndex, const ServerEntry &entry)
{
  ProbeResult base;
  base.entry = entry.text;
  base.host = entry.host;
  base.port = entry.port;

  if (!entry.isValid()) {
    base.problem = entry.problem;
    complete(run, entryIndex, 0, base);
    return;
  }

  // a number needs no lookup, and the lookup of a name that is not found must not be mistaken for a refusal
  QHostAddress literal;
  if (literal.setAddress(entry.host)) {
    tryAddress(run, entryIndex, 0, entry, entry.host);
    return;
  }

  QPointer<ConnectionProbe> self(this);
  m_resolver(entry.host, [self, run, entryIndex, entry, base](const QStringList &addresses, const QString &error) {
    if (!self || run->dropped) {
      return;
    }
    QStringList unique;
    for (const auto &address : addresses) {
      if (!unique.contains(address)) {
        unique << address;
      }
    }
    if (unique.isEmpty()) {
      auto result = base;
      result.kind = FailureKind::DnsFailure;
      result.detail = error;
      self->complete(run, entryIndex, 0, result);
      return;
    }
    // the name was one thing to wait for, each of its addresses is one now
    run->pending += static_cast<int>(unique.size()) - 1;
    for (int slot = 0; slot < unique.size() && !run->dropped; ++slot) {
      self->tryAddress(run, entryIndex, slot, entry, unique.at(slot));
    }
  });
}

void ConnectionProbe::tryAddress(
    const std::shared_ptr<Run> &run, int entryIndex, int slot, const ServerEntry &entry, const QString &address
)
{
  auto *socket = m_socketFactory();
  socket->setParent(this);
  run->sockets.append(socket);

  auto *timer = new QTimer(socket);
  timer->setSingleShot(true);
  auto clock = std::make_shared<QElapsedTimer>();
  auto finished = std::make_shared<bool>(false);

  ProbeResult base;
  base.entry = entry.text;
  base.host = entry.host;
  base.address = address;
  base.port = entry.port;

  QPointer<ConnectionProbe> self(this);
  QPointer<IProbeSocket> guarded(socket);
  const auto finish = [self, run, entryIndex, slot, base, clock, finished, guarded,
                       timer](bool open, FailureKind kind, const QString &detail) {
    if (*finished || !self || run->dropped || !guarded) {
      return;
    }
    *finished = true;
    timer->stop();
    auto result = base;
    result.open = open;
    result.kind = kind;
    result.detail = detail;
    result.elapsedMs = clock->elapsed();
    // closing is all that is done with a connection that opened
    guarded->abort();
    guarded->disconnect(self.data());
    guarded->deleteLater();
    self->complete(run, entryIndex, slot, result);
  };

  connect(socket, &IProbeSocket::connected, this, [finish] { finish(true, FailureKind::Unknown, {}); });
  connect(socket, &IProbeSocket::failed, this, [finish](FailureKind kind, const QString &detail) {
    finish(false, kind, detail);
  });
  connect(timer, &QTimer::timeout, this, [finish] { finish(false, FailureKind::TimedOut, {}); });

  clock->start();
  timer->start(m_timeoutMs);
  socket->connectToHost(address, static_cast<quint16>(entry.port));
}

void ConnectionProbe::complete(const std::shared_ptr<Run> &run, int entryIndex, int slot, ProbeResult result)
{
  if (run != m_run || run->dropped) {
    return;
  }
  run->results[entryIndex].insert(slot, result);
  Q_EMIT resultReady(result);
  if (run != m_run || --run->pending > 0) {
    return;
  }

  QList<ProbeResult> all;
  for (const auto &perEntry : std::as_const(run->results)) {
    all.append(perEntry.values());
  }
  m_run.reset();
  Q_EMIT finished(all);
}

//
// words
//

QString outcomeWord(const ProbeResult &result)
{
  if (!result.problem.isEmpty()) {
    return QStringLiteral("not tested");
  }
  if (result.open) {
    return QStringLiteral("open");
  }
  switch (result.kind) {
  case FailureKind::TimedOut:
    return QStringLiteral("timed out");
  case FailureKind::Refused:
    return QStringLiteral("refused");
  case FailureKind::NoRoute:
    return QStringLiteral("no route");
  case FailureKind::DnsFailure:
    return QStringLiteral("name not found");
  case FailureKind::Reset:
    return QStringLiteral("reset");
  case FailureKind::AccessDenied:
    return QStringLiteral("blocked on this computer");
  default:
    return QStringLiteral("failed");
  }
}

QString summaryLine(const QList<ProbeResult> &results, int timeoutMs, const QDateTime &when, const QString &system)
{
  const auto prefix = QStringLiteral("Deskflow connection test %1 (%2): ")
                          .arg(when.toString(QStringLiteral("yyyy-MM-dd HH:mm")), system);
  if (results.isEmpty()) {
    return prefix + QStringLiteral("no address was tested");
  }

  QStringList parts;
  for (const auto &result : results) {
    if (!result.problem.isEmpty()) {
      parts << QStringLiteral("%1 %2 (%3)").arg(result.entry, outcomeWord(result), result.problem);
      continue;
    }

    const auto target = QString::fromStdString(
        diagnosis::formatTarget(result.host.toStdString(), result.address.toStdString(), result.port)
    );
    QString text = QStringLiteral("%1 %2").arg(target, outcomeWord(result));
    if (result.open) {
      text += QStringLiteral(" in %1 ms").arg(result.elapsedMs);
    } else if (result.kind == FailureKind::TimedOut) {
      text += QStringLiteral(" (no answer in %1 ms)").arg(timeoutMs);
    } else if (result.kind == FailureKind::Refused) {
      text += QStringLiteral(" (nothing listens there)");
    } else if (result.kind != FailureKind::DnsFailure && !result.detail.isEmpty()) {
      text += QStringLiteral(" (%1)").arg(result.detail);
    }
    parts << text;
  }
  return prefix + parts.join(QStringLiteral("; "));
}

} // namespace deskflow::gui
