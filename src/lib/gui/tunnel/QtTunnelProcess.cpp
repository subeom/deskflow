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
    // the last line, which had no end, is whole now: nothing follows it
    for (const auto &line : m_splitter.finish()) {
      Q_EMIT outputLine(line);
    }
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
  m_splitter = OutputLineSplitter();
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
  // only whole lines are given: a token that is cut between two reads is whole in the line that holds it, and a line
  // that is too long is skipped (see OutputLineSplitter)
  for (const auto &line : m_splitter.feed(m_process->readAllStandardOutput())) {
    Q_EMIT outputLine(line);
  }
}

TunnelProcessFactory QtTunnelProcess::factory()
{
  return [](QObject *parent) -> ITunnelProcess * { return new QtTunnelProcess(parent); };
}

} // namespace deskflow::tunnel
