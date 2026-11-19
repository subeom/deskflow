/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <memory>

namespace deskflow::tunnel {
class ISecretStore;
}

namespace deskflow::gui::diagnostic {

void clearSettings(bool enableRestart);

/**
 * Deletes the Service Token of the Cloudflare Tunnel from the secure store (the store of the system when \p store is
 * null). It is not in the settings that clearSettings() removes, so the reset of the settings does it with this. A
 * store that has nothing is not an error; false when the store could not delete (the user is told by the log).
 */
bool removeStoredCredentials(std::shared_ptr<deskflow::tunnel::ISecretStore> store = nullptr);

} // namespace deskflow::gui::diagnostic
