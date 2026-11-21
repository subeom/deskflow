/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/TunnelReconnect.h"

namespace deskflow::tunnel {

double reconnectSeconds(int retriesSoFar, const ReconnectPolicy &policy)
{
  if (retriesSoFar <= 0) {
    return policy.firstSeconds;
  }
  if (retriesSoFar <= policy.steadyRetries) {
    return policy.steadySeconds;
  }
  if (retriesSoFar <= policy.steadyRetries + policy.easedRetries) {
    return policy.easedSeconds;
  }
  return policy.longSeconds;
}

} // namespace deskflow::tunnel
