/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "arch/ArchException.h"
#include "common/ConnectionDiagnosis.h"
#include "net/SocketException.h"

namespace deskflow::net {

//! What a failed connect() says about the connection, from the typed error the arch layer threw
diagnosis::FailureKind failureKindOf(const ArchNetworkException &error);

//! A name that did not resolve is a DNS failure, a bad port is not one
diagnosis::FailureKind failureKindOf(const SocketAddressException &error);

} // namespace deskflow::net
