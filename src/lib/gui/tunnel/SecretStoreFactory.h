/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/ISecretStore.h"

#include <memory>

namespace deskflow::tunnel {

//! Setting this variable to `memory` makes the system store the in-memory one: for tests and for a machine where the
//! keychain must not be touched. Nothing else about the token changes.
inline constexpr auto kSecretStoreVariable = "DESKFLOW_SECRET_STORE";

//! The in-memory store that every user of it in this process shares
std::shared_ptr<ISecretStore> sessionSecretStore();

/**
 * The secret store of this system: the Credential Manager on Windows, the login keychain on macOS and the in-memory
 * store (shared, so that every dialog sees the same token) elsewhere. Nothing is read or written by making it.
 */
std::shared_ptr<ISecretStore> createSystemSecretStore();

} // namespace deskflow::tunnel
