/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

#include <algorithm>

namespace deskflow::tunnel {

/**
 * A Cloudflare Access Service Token: the Client ID and the Secret that go together. It lives in the secure store of the
 * operating system and, for as long as a cloudflared is started, in memory; it is never written to the settings, to a
 * command line or to a log. The destructor overwrites the text as far as the string allows.
 */
struct ServiceToken
{
  QString clientId;
  QString secret;

  ServiceToken() = default;
  ServiceToken(QString id, QString value) : clientId(std::move(id)), secret(std::move(value))
  {
  }
  ServiceToken(const ServiceToken &) = default;
  ServiceToken(ServiceToken &&) = default;
  ServiceToken &operator=(const ServiceToken &) = default;
  ServiceToken &operator=(ServiceToken &&) = default;
  ~ServiceToken()
  {
    wipe();
  }

  bool isComplete() const
  {
    return !clientId.isEmpty() && !secret.isEmpty();
  }

  void wipe()
  {
    // writes into the buffer of the string; a copy that was made earlier is not reached
    std::ranges::fill(secret, QChar(u'\0'));
    secret.clear();
    clientId.clear();
  }
};

} // namespace deskflow::tunnel
