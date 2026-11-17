/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QtGlobal>

#include <memory>
#include <optional>

namespace deskflow::tunnel {

struct ProcessInfo
{
  qint64 startTimeMs = 0; //!< when the process started, in milliseconds since 1970
  QString path;           //!< the program that it runs as the system tells it; empty when the system does not say
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
};

//! The inspector of this system
std::shared_ptr<IProcessInspector> createSystemProcessInspector();

} // namespace deskflow::tunnel
