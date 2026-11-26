/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/WindowsKillOnCloseJob.h"

#ifdef Q_OS_WIN

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace deskflow::tunnel {

namespace {

HANDLE createJob()
{
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (job == nullptr) {
    return nullptr;
  }

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
    CloseHandle(job);
    return nullptr;
  }
  return job;
}

} // namespace

bool assignToKillOnCloseJob(qint64 pid)
{
  // never closed by this program: the system closes it when the program ends, and that ends what is in it
  static const HANDLE job = createJob();
  if (job == nullptr || pid <= 0) {
    return false;
  }

  HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
  if (process == nullptr) {
    return false;
  }

  const BOOL assigned = AssignProcessToJobObject(job, process);
  CloseHandle(process);
  return assigned != FALSE;
}

} // namespace deskflow::tunnel

#endif // Q_OS_WIN
