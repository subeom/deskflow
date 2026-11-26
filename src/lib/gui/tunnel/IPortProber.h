/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <functional>

namespace deskflow::tunnel {

/**
 * What the supervisor asks of the local ports. Only the loopback address (127.0.0.1) is ever touched.
 */
class IPortProber
{
public:
  virtual ~IPortProber() = default;

  //! Whether a listener can be opened on 127.0.0.1:\p port now (it is closed again at once)
  virtual bool canListen(int port) = 0;
  //! A port that nothing listens on, from the range of ports that the system hands out; 0 when there is none
  virtual int findFreePort() = 0;
  //! Opens a connection to 127.0.0.1:\p port and closes it again at once (no byte is sent); \p done is told whether it
  //! was opened, once, at the latest after \p timeoutMs
  virtual void probeOpen(int port, int timeoutMs, std::function<void(bool open)> done) = 0;
};

} // namespace deskflow::tunnel
