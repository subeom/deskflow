/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/TunnelTypes.h"
#include "gui/widgets/StatusChip.h"

#include <QString>

namespace deskflow::gui {

struct TunnelChipSpec
{
  QString text;
  StatusChip::Kind kind = StatusChip::Kind::Neutral;
};

/*!
 * \brief The chip that says how the Cloudflare Tunnel is, beside the state of sharing in the main window of the new
 * look.
 *
 * A chip is as wide as its text and does not cut it, so the text is a few words (the whole line, with the failure and
 * the link to the server, is under the address of the server): "Tunnel: connected", "Tunnel: failed". It is in the
 * language of the application. The kind says how it is: a problem or a failure is not the colour of a tunnel that
 * works.
 */
TunnelChipSpec tunnelChip(const deskflow::tunnel::TunnelStatus &status, deskflow::tunnel::ServerLink link);

} // namespace deskflow::gui
