/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/ConnectFailureKind.h"

namespace deskflow::net {

using diagnosis::FailureKind;

FailureKind failureKindOf(const ArchNetworkException &error)
{
  // the arch layers (unix and windows) turn their error numbers into these types, so this holds on both
  if (dynamic_cast<const ArchNetworkConnectionRefusedException *>(&error) != nullptr) {
    return FailureKind::Refused;
  }
  if (dynamic_cast<const ArchNetworkTimedOutException *>(&error) != nullptr) {
    return FailureKind::TimedOut;
  }
  if (dynamic_cast<const ArchNetworkNoRouteException *>(&error) != nullptr) {
    return FailureKind::NoRoute;
  }
  if (dynamic_cast<const ArchNetworkDisconnectedException *>(&error) != nullptr) {
    return FailureKind::Reset;
  }
  if (dynamic_cast<const ArchNetworkAccessException *>(&error) != nullptr) {
    return FailureKind::AccessDenied;
  }
  return FailureKind::Unknown;
}

FailureKind failureKindOf(const SocketAddressException &error)
{
  using Error = SocketAddressException::SocketError;
  switch (error.getError()) {
  case Error::NotFound:
  case Error::NoAddress:
  case Error::Unsupported:
    return FailureKind::DnsFailure;
  default:
    return FailureKind::Unknown;
  }
}

} // namespace deskflow::net
