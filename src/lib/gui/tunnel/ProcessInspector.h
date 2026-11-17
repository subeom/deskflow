/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QtGlobal>

#include <functional>
#include <memory>
#include <optional>

namespace deskflow::tunnel {

struct ProcessInfo
{
  qint64 startTimeMs = 0; //!< when the process started, in milliseconds since 1970
  QString path;           //!< the program that it runs as the system tells it; empty when the system does not say
};

//! Whether the process that has been found is the one that is meant
using ProcessMatcher = std::function<bool(const ProcessInfo &)>;

enum class TerminateResult
{
  Done,        //!< the process was asked to go, or ended
  NotRunning,  //!< there is no such process
  NotMatching, //!< there is a process with the number, and the matcher did not accept it: it was left alone
  Failed       //!< the process matched, and could not be asked or ended (or the number is refused)
};

/**
 * What is asked of a process by its number. It is only ever used for the one process that this program started earlier
 * and recorded (see OrphanRecord): there is no way to look for a process by its name.
 */
class IProcessInspector
{
public:
  virtual ~IProcessInspector() = default;

  //! Nothing when there is no such process
  virtual std::optional<ProcessInfo> inspect(qint64 pid) const = 0;
  //! Asks the process to go, or with \p force ends it. False when it could not be done; the number of this program and
  //! numbers that are not those of a process of a user (0 and 1) are refused.
  virtual bool terminate(qint64 pid, bool force) = 0;

  /**
   * Asks the process to go, or with \p force ends it, only when \p matches accepts what the system tells about it.
   *
   * Windows overrides this: the process is opened once, and what is judged and what is ended is that one process, so a
   * process number that passes to another program in the meantime cannot be ended by mistake. This default looks and
   * then ends in two calls (kill(2) gives no handle for a process): in the time between them the process could end and
   * its number pass to another program that is also a cloudflared with the same start time within the tolerance, which
   * is not a thing that happens in practice, and the worst that comes of it is a cloudflared that is not ours being
   * ended.
   */
  virtual TerminateResult terminateIf(qint64 pid, bool force, const ProcessMatcher &matches)
  {
    const auto info = inspect(pid);
    if (!info.has_value()) {
      return TerminateResult::NotRunning;
    }
    if (!matches(*info)) {
      return TerminateResult::NotMatching;
    }
    return terminate(pid, force) ? TerminateResult::Done : TerminateResult::Failed;
  }
};

//! The inspector of this system
std::shared_ptr<IProcessInspector> createSystemProcessInspector();

} // namespace deskflow::tunnel
