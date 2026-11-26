/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/InputCapabilities.h"

// A layout capability travels in the capability mask of DIPC and CIPC like the clipboard ones, in a bit of its own
// above them (bits 0-3 are input, 8-9 clipboard). A peer that predates the bit ignores it.

//! In DIPC: the server uses the monitors that this computer reports. In CIPC: this computer reports its monitors
/*!
The server sets it only while the experimental multi-monitor mapping is on, and a client sets it only when it was set
in the DIPC it got and it can list its monitors. A DMON is sent, and expected, only on a link where both have it.
*/
inline constexpr InputCapabilityMask kLayoutCapabilityMonitors = 1u << 12;
