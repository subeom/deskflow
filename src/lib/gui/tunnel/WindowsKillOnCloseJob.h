/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN

namespace deskflow::tunnel {

/**
 * Puts the process \p pid into a job that ends it when the last handle of the job is closed, which is when this program
 * ends, however it ends (a crash, a kill from the task manager). One job for the whole program. False when the process
 * or the job could not be opened, in which case the process is only ended by the code that stops it.
 */
bool assignToKillOnCloseJob(qint64 pid);

} // namespace deskflow::tunnel

#endif // Q_OS_WIN
