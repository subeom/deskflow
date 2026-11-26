/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/NativeMonitors.h"

#include <vector>

namespace deskflow::platform {

//! The monitors of the virtual screen of this Windows PC, as Windows lists them
/*!
One entry for each monitor that \c EnumDisplayMonitors reports: the rectangle (\c rcMonitor, the whole monitor and
not only its work area), whether it is the primary monitor, and the effective DPI of the monitor (\c GetDpiForMonitor
of \c shcore.dll, loaded when it is first asked for; 96, which is 100 %, when Windows does not have it or does not
answer).

The coordinates are those of the calling thread, whose DPI awareness is not changed: they are the same space as
\c GetSystemMetrics(SM_XVIRTUALSCREEN ...) that makes the shape that this computer reports (DINF) when it is asked from
the same thread. Whether they are the same numbers, which they are for a process that is aware of the DPI of each
monitor, is checked by whoever reports them (\c prepareMonitors compares their box with the shape); nothing is
guessed here. Empty when Windows lists no monitor.
*/
std::vector<deskflow::monitors::NativeMonitor> listWindowsMonitors();

} // namespace deskflow::platform
