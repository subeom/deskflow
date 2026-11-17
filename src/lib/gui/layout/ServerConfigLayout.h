/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/config/ServerConfig.h"
#include "gui/layout/LayoutMemory.h"

namespace deskflow::gui::layout {

//! Where the computers are now, as far as a change of the displays changes it; nothing when there is no arrangement
Layout captureLayout(const ServerConfig &config);

//! Puts the computers of \p layout where it says: all of it, or none of it
/*!
The result is worked out first and checked (no overlap, no wrong place); computers of the configuration that the layout
does not know keep their place, or are put beside the others when that would overlap. Then the places are set, and the
zoom of a computer that has another one (through \c ServerConfig::setComputerZoom, so that its monitor scales follow). The
settings are not written: the caller commits.
*/
ApplyOutcome applyLayout(ServerConfig &config, const Layout &layout);

//! The arrangement that has the computers against the server again, for the displays \p to after \p from
ReattachOutcome reattachLayout(const ServerConfig &config, const DisplaySignature &from, const DisplaySignature &to);

//! The server configuration that the core would be given now, as it is written to its file
QString generatedConfigText(const ServerConfig &config);

} // namespace deskflow::gui::layout
