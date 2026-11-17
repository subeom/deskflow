/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/QtTunnelProcess.h"

#include "gui/tunnel/WindowsKillOnCloseJob.h"

#include <QProcess>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <signal.h>
#include <sys/prctl.h>
#endif

namespace deskflow::tunnel {

namespace {

// a line that never ends is not kept for ever
constexpr qsizetype kMaxPendingBytes = 64 * 1024;

} // namespace

QtTunnelProcess::QtTunnelProcess(QObject *parent) : ITunnelProcess(parent), m_process(new QProcess(this))
{
  connect(m_process, &QProcess::started, this, [this] {
#if defined(Q_OS_WIN)
    // ends with this program, whatever ends it
    assignToKillOnCloseJob(m_process->processId());
#endif
    Q_EMIT started();
  });

  connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      Q_EMIT startFailed(m_process->errorString());
    }
  });

  connect(m_process, &QProcess::readyReadStandardOutput, this, &QtTunnelProcess::readOutput);

  connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
    readOutput();
    emitLines(true);
    Q_EMIT finished(status == QProcess::CrashExit ? -1 : exitCode);
  });
}

QtTunnelProcess::~QtTunnelProcess()
{
  // a process that is still there when its owner goes is ended: it must not outlive the tunnel that wanted it
  if (m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(2000);
  }
}

void QtTunnelProcess::start(const LaunchSpec &spec)
{
  m_buffer.clear();
  m_process->setProgram(spec.program);
  m_process->setArguments(spec.arguments);
  m_process->setProcessEnvironment(spec.environment);
  m_process->setProcessChannelMode(QProcess::MergedChannels);
  m_process->setStandardInputFile(QProcess::nullDevice());

#if defined(Q_OS_WIN)
  m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
    // no console window, not even a flash of one
    arguments->flags |= CREATE_NO_WINDOW;
    arguments->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
    arguments->startupInfo->wShowWindow = SW_HIDE;
  });
#elif defined(Q_OS_LINUX)
  m_process->setChildProcessModifier([] {
    // the process is left over when this program ends abruptly (kill -9): ask the system to end it then
    prctl(PR_SET_PDEATHSIG, SIGTERM);
  });
#endif

  m_process->start();
}

void QtTunnelProcess::terminate()
{
#if defined(Q_OS_WIN)
  // a console program without a window does not get the request that terminate() makes
  m_process->kill();
#else
  m_process->terminate();
#endif
}

void QtTunnelProcess::kill()
{
  m_process->kill();
}

bool QtTunnelProcess::isRunning() const
{
  return m_process->state() != QProcess::NotRunning;
}

qint64 QtTunnelProcess::processId() const
{
  return m_process->state() == QProcess::NotRunning ? 0 : static_cast<qint64>(m_process->processId());
}

void QtTunnelProcess::readOutput()
{
  m_buffer.append(m_process->readAllStandardOutput());
  emitLines(false);
}

void QtTunnelProcess::emitLines(bool flush)
{
  for (;;) {
    const auto end = m_buffer.indexOf('\n');
    const auto carriage = m_buffer.indexOf('\r');
    const auto cut = end < 0 ? carriage : (carriage < 0 ? end : qMin(end, carriage));
    if (cut < 0) {
      break;
    }

    const auto line = QString::fromUtf8(m_buffer.left(cut)).trimmed();
    m_buffer.remove(0, cut + 1);
    if (!line.isEmpty()) {
      Q_EMIT outputLine(line);
    }
  }

  if ((flush || m_buffer.size() > kMaxPendingBytes) && !m_buffer.isEmpty()) {
    const auto line = QString::fromUtf8(m_buffer).trimmed();
    m_buffer.clear();
    if (!line.isEmpty()) {
      Q_EMIT outputLine(line);
    }
  }
}

TunnelProcessFactory QtTunnelProcess::factory()
{
  return [](QObject *parent) -> ITunnelProcess * { return new QtTunnelProcess(parent); };
}

} // namespace deskflow::tunnel
