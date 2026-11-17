/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/ProcessInspector.h"

#include <QString>

#include <optional>

namespace deskflow::tunnel {

//! The two start times are the same when they are this close (the clocks of the systems tell it in different steps)
inline constexpr qint64 kStartTimeToleranceMs = 2000;

/**
 * The cloudflared that this program started last, written to a file while it runs and removed when it is gone. A
 * cloudflared that outlives this program (a crash, a kill) is found again by it: only the process with this number,
 * this start time and this program is ended, never a process that is found by its name.
 */
struct OrphanRecord
{
  qint64 pid = 0;
  qint64 startTimeMs = 0;
  QString path;     //!< the program as the system told it when the process was started
  QString hostname; //!< for the message only
  int port = 0;     //!< for the message only

  bool isValid() const
  {
    return pid > 1 && startTimeMs > 0 && !path.isEmpty();
  }
};

//! The file in the state directory of the user (the same directory as the state of the settings)
QString defaultOrphanRecordPath();

std::optional<OrphanRecord> readOrphanRecord(const QString &file);
bool writeOrphanRecord(const QString &file, const OrphanRecord &record);
void removeOrphanRecord(const QString &file);

enum class OrphanOutcome
{
  NoRecord,   //!< no record: nothing was left behind
  NotRunning, //!< the process of the record is gone
  Different,  //!< the number belongs to another process now (another start time or another program): left alone
  Ended,      //!< the process was asked to go, or ended, and is gone
  EndFailed   //!< the process could not be ended
};

struct OrphanCleanup
{
  OrphanOutcome outcome = OrphanOutcome::NoRecord;
  OrphanRecord record;
};

/**
 * Ends the cloudflared of the record when it is still there and still the same process (the same number, the same start
 * time within the tolerance, the same program, and a program that is called cloudflared). The record is removed in
 * every case. It waits up to \p waitMs for the process to go after the request, then ends it, then waits as long again.
 */
OrphanCleanup cleanUpOrphan(const QString &recordFile, IProcessInspector &inspector, int waitMs = 1500);

} // namespace deskflow::tunnel
