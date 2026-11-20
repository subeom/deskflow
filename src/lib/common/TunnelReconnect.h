/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

namespace deskflow::tunnel {

/**
 * How long the client waits before it connects again while the Cloudflare Tunnel is used. A direct link does not use
 * this: it retries every second, whatever happened (ClientApp::retryTime).
 *
 * The tunnel loses a connection now and then (the server computer changes its network, its connections to Cloudflare
 * are made again within seconds). The first try after a session ended does not wait for a second: the other connections
 * of the tunnel are most often up and take the new one at once. A tunnel that stays down is not asked about every
 * second for minutes: every try is a new connection through Cloudflare and a line in the log, so the interval grows,
 * and stays short enough that the client is back within a few seconds after the path is.
 */
struct ReconnectPolicy
{
  double firstSeconds = 0.25; //!< the first retry after a session that worked ended, or after the first try failed
  double steadySeconds = 1.0; //!< as for a direct link
  int steadyRetries = 15;     //!< retries (after the first) at the steady interval
  double easedSeconds = 2.0;
  int easedRetries = 30;    //!< retries at the eased interval, after the steady ones
  double longSeconds = 4.0; //!< every retry after that, for as long as the path is down
};

/**
 * The wait before the next try, when \p retriesSoFar retries were already made since the last connection that was made
 * (the hello was answered): 0 for the retry right after a session that worked ended.
 */
double reconnectSeconds(int retriesSoFar, const ReconnectPolicy &policy = {});

} // namespace deskflow::tunnel
