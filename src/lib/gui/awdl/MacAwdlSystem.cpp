/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacAwdlSystem.h"

#include <QNetworkInterface>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

namespace deskflow::awdl {

namespace {

// the messages of the programs are read in one language
QProcessEnvironment plainEnvironment()
{
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  return environment;
}

} // namespace

MacAwdlSystem::MacAwdlSystem(QObject *parent) : IAwdlSystem(parent)
{
}

MacAwdlSystem::MacAwdlSystem(const Programs &programs, QObject *parent) : IAwdlSystem(parent), m_programs(programs)
{
}

MacAwdlSystem::~MacAwdlSystem()
{
  // a program that is still under way is not left behind (it is a child of this object: ended with it)
  for (const auto &process : {m_read, m_admin, m_route, m_ping}) {
    if (process && process->state() != QProcess::NotRunning) {
      process->disconnect(this);
      process->kill();
      process->waitForFinished(500);
    }
  }
}

void MacAwdlSystem::readState()
{
  if (m_read && m_read->state() != QProcess::NotRunning)
    return;

  auto *process = new QProcess(this);
  m_read = process;
  process->setProcessEnvironment(plainEnvironment());
  auto *timeout = new QTimer(process);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, process, &QProcess::kill);
  connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
    // a program that is not there says nothing else: the state is not known
    if (error == QProcess::FailedToStart) {
      process->deleteLater();
      Q_EMIT stateRead(LinkState::Unknown);
    }
  });
  connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
    const auto state = status == QProcess::NormalExit
                           ? parseIfconfig(
                                 exitCode, QString::fromUtf8(process->readAllStandardOutput()),
                                 QString::fromUtf8(process->readAllStandardError())
                             )
                           : LinkState::Unknown;
    process->deleteLater();
    Q_EMIT stateRead(state);
  });
  process->start(m_programs.ifconfig, readArguments());
  timeout->start(kReadTimeoutMs);
}

void MacAwdlSystem::setLink(Direction direction, const QString &prompt)
{
  if (m_admin && m_admin->state() != QProcess::NotRunning)
    return;

  auto *process = new QProcess(this);
  m_admin = process;
  process->setProcessEnvironment(plainEnvironment());
  connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      process->deleteLater();
      Q_EMIT linkSet(parseAdminResult(false, -1, QString()));
    }
  });
  connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
    const auto error = QString::fromUtf8(process->readAllStandardError());
    const auto result =
        parseAdminResult(status == QProcess::NormalExit, status == QProcess::NormalExit ? exitCode : -1, error);
    process->deleteLater();
    Q_EMIT linkSet(result);
  });
  // the dialog of the system stays as long as the person takes: there is no timeout
  process->start(m_programs.osascript, adminArguments(direction, prompt));
}

LinkKind MacAwdlSystem::kindOfInterface(const QString &name)
{
  const auto device = QNetworkInterface::interfaceFromName(name);
  if (!device.isValid())
    return LinkKind::Unknown;
  switch (device.type()) {
  case QNetworkInterface::Wifi:
    return LinkKind::WiFi;
  case QNetworkInterface::Ethernet:
    return LinkKind::Wired;
  default:
    break;
  }
  return LinkKind::Other;
}

void MacAwdlSystem::findRoute()
{
  if (m_route && m_route->state() != QProcess::NotRunning)
    return;

  auto *process = new QProcess(this);
  m_route = process;
  process->setProcessEnvironment(plainEnvironment());
  auto *timeout = new QTimer(process);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, process, &QProcess::kill);
  connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      process->deleteLater();
      Q_EMIT routeFound(Route());
    }
  });
  connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
    Route route;
    if (status == QProcess::NormalExit && exitCode == 0)
      route = parseRoute(QString::fromUtf8(process->readAllStandardOutput()));
    process->deleteLater();
    if (!route.interfaceName.isEmpty())
      route.kind = m_programs.kindOf ? m_programs.kindOf(route.interfaceName) : kindOfInterface(route.interfaceName);
    Q_EMIT routeFound(route);
  });
  process->start(m_programs.route, {QStringLiteral("-n"), QStringLiteral("get"), QStringLiteral("default")});
  timeout->start(kReadTimeoutMs);
}

void MacAwdlSystem::measure(const Route &route)
{
  if (m_ping && m_ping->state() != QProcess::NotRunning)
    return;

  // a route that may not be measured sends nothing: the answer is an empty burst, from the event loop like the others
  const auto arguments = pingArguments(route);
  if (arguments.isEmpty()) {
    QTimer::singleShot(0, this, [this] { Q_EMIT measured({}); });
    return;
  }

  auto *process = new QProcess(this);
  m_ping = process;
  process->setProcessEnvironment(plainEnvironment());
  auto *timeout = new QTimer(process);
  timeout->setSingleShot(true);
  connect(timeout, &QTimer::timeout, process, &QProcess::kill);
  connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      process->deleteLater();
      Q_EMIT measured({});
    }
  });
  connect(process, &QProcess::finished, this, [this, process] {
    // what ping wrote is read, also from a ping that was ended for taking too long
    const auto samples = parsePing(QString::fromUtf8(process->readAllStandardOutput()));
    process->deleteLater();
    Q_EMIT measured(samples);
  });
  process->start(m_programs.ping, arguments);
  // the probes take count x interval, and a wait for the last answer
  timeout->start(kBurstCount * kBurstIntervalMs + 5000);
}

} // namespace deskflow::awdl
