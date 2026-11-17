/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/ProcessInspector.h"

#include <QCoreApplication>
#include <QFile>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <libproc.h>
#include <signal.h>
#include <sys/sysctl.h>
#include <unistd.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

namespace deskflow::tunnel {

namespace {

bool isRefused(qint64 pid)
{
  return pid <= 1 || pid == static_cast<qint64>(QCoreApplication::applicationPid());
}

#if defined(Q_OS_WIN)

class WindowsInspector : public IProcessInspector
{
public:
  std::optional<ProcessInfo> inspect(qint64 pid) const override
  {
    if (pid <= 0) {
      return std::nullopt;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
      return std::nullopt;
    }

    ProcessInfo info;
    FILETIME created = {};
    FILETIME exited = {};
    FILETIME kernel = {};
    FILETIME user = {};
    DWORD exitCode = 0;
    const bool alive = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
    const bool timed = GetProcessTimes(process, &created, &exited, &kernel, &user) != FALSE;
    if (alive && timed) {
      // 100 ns steps since 1601; 11644473600 s lie between that and 1970
      ULARGE_INTEGER ticks;
      ticks.LowPart = created.dwLowDateTime;
      ticks.HighPart = created.dwHighDateTime;
      info.startTimeMs = static_cast<qint64>(ticks.QuadPart / 10000ULL) - 11644473600000LL;

      wchar_t buffer[32768];
      DWORD size = static_cast<DWORD>(std::size(buffer));
      if (QueryFullProcessImageNameW(process, 0, buffer, &size)) {
        info.path = QString::fromWCharArray(buffer, static_cast<qsizetype>(size));
      }
    }
    CloseHandle(process);

    if (!alive || !timed) {
      return std::nullopt;
    }
    return info;
  }

  bool terminate(qint64 pid, bool) override
  {
    if (isRefused(pid)) {
      return false;
    }

    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
      return false;
    }
    const BOOL ended = TerminateProcess(process, 1);
    CloseHandle(process);
    return ended != FALSE;
  }
};

#elif defined(Q_OS_MACOS)

class MacInspector : public IProcessInspector
{
public:
  std::optional<ProcessInfo> inspect(qint64 pid) const override
  {
    if (pid <= 0) {
      return std::nullopt;
    }

    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
    struct kinfo_proc kernelInfo = {};
    size_t size = sizeof(kernelInfo);
    if (sysctl(mib, 4, &kernelInfo, &size, nullptr, 0) != 0 || size == 0 || kernelInfo.kp_proc.p_pid != pid) {
      return std::nullopt;
    }
    // a process that ended and is waiting to be reaped is not there for this purpose
    if (kernelInfo.kp_proc.p_stat == SZOMB) {
      return std::nullopt;
    }

    ProcessInfo info;
    const auto &started = kernelInfo.kp_proc.p_un.__p_starttime;
    info.startTimeMs = static_cast<qint64>(started.tv_sec) * 1000 + started.tv_usec / 1000;

    char path[PROC_PIDPATHINFO_MAXSIZE] = {};
    if (proc_pidpath(static_cast<int>(pid), path, sizeof(path)) > 0) {
      info.path = QString::fromUtf8(path);
    }
    return info;
  }

  bool terminate(qint64 pid, bool force) override
  {
    return !isRefused(pid) && ::kill(static_cast<pid_t>(pid), force ? SIGKILL : SIGTERM) == 0;
  }
};

#else

class UnixInspector : public IProcessInspector
{
public:
  std::optional<ProcessInfo> inspect(qint64 pid) const override
  {
    if (pid <= 0) {
      return std::nullopt;
    }

    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly)) {
      return std::nullopt;
    }
    const auto content = QString::fromLatin1(stat.readAll());

    // the name of the program is in parentheses and may hold anything: the fields start after the last one
    const auto close = content.lastIndexOf(QLatin1Char(')'));
    if (close < 0) {
      return std::nullopt;
    }
    const auto fields = content.mid(close + 2).split(QLatin1Char(' '));
    // state is field 3 (index 0 here), starttime is field 22 (index 19)
    if (fields.size() < 20 || fields.at(0) == QLatin1String("Z")) {
      return std::nullopt;
    }

    bool ok = false;
    const auto ticks = fields.at(19).toLongLong(&ok);
    if (!ok) {
      return std::nullopt;
    }

    qint64 bootTime = 0;
    QFile global(QStringLiteral("/proc/stat"));
    if (global.open(QIODevice::ReadOnly)) {
      for (const auto &line : QString::fromLatin1(global.readAll()).split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1String("btime "))) {
          bootTime = line.mid(6).trimmed().toLongLong();
          break;
        }
      }
    }
    const long ticksPerSecond = sysconf(_SC_CLK_TCK);
    if (bootTime == 0 || ticksPerSecond <= 0) {
      return std::nullopt;
    }

    ProcessInfo info;
    info.startTimeMs = bootTime * 1000 + ticks * 1000 / ticksPerSecond;
    info.path = QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid));
    return info;
  }

  bool terminate(qint64 pid, bool force) override
  {
    return !isRefused(pid) && ::kill(static_cast<pid_t>(pid), force ? SIGKILL : SIGTERM) == 0;
  }
};

#endif

} // namespace

std::shared_ptr<IProcessInspector> createSystemProcessInspector()
{
#if defined(Q_OS_WIN)
  return std::make_shared<WindowsInspector>();
#elif defined(Q_OS_MACOS)
  return std::make_shared<MacInspector>();
#else
  return std::make_shared<UnixInspector>();
#endif
}

} // namespace deskflow::tunnel
