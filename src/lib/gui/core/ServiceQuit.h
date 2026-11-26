/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

namespace deskflow::gui {

namespace ipc {
class DaemonIpcClient;
}

//! What came of the stop that is sent to the service when the program ends
enum class StopDelivery
{
  Delivered,    //!< written to the service
  NotConnected, //!< the service could not be reached in the time that was given
  NotFlushed    //!< sent, but not written in time
};

/**
 * Tells the service to stop the core, at the end of the program. A stop that is only queued behind a connection would
 * never be sent (the program is gone before the connection is made), so this connects when it has to and waits for it,
 * up to
 * \p connectWaitMs, then sends the stop and waits up to \p flushMs for it to be written. It runs an event loop while it
 * waits. Whatever comes of it, it returns: the end of the program is not held back for ever.
 */
StopDelivery deliverStopToService(ipc::DaemonIpcClient &client, int connectWaitMs, int flushMs);

//! The warning for the log when the stop was not delivered: what keeps running and what to do; empty for Delivered
QString stopDeliveryWarning(StopDelivery result);

} // namespace deskflow::gui
