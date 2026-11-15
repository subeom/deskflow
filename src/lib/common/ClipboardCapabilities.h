/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/InputCapabilities.h"

// Clipboard capabilities travel in the capability mask of DIPC and CIPC, the only per peer field of the handshake,
// so they sit in the bits above the input capabilities (bits 0-3 of InputCapabilities.h). A peer that predates a bit
// ignores it: it only ever tests the bits it knows.

//! The peer sends clipboards as a paced, low priority stream and counts any data it receives as proof of life
inline constexpr InputCapabilityMask kClipboardCapabilityPaced = 1u << 8;

//! The peer takes the data of clipboard id 0 from a Windows or macOS sender as the selection (id 1) too, so the
//! sender does not have to send the selection data
inline constexpr InputCapabilityMask kClipboardCapabilitySingleId = 1u << 9;
