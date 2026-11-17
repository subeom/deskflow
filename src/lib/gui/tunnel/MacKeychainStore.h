/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QtGlobal>

#ifdef Q_OS_MACOS

#include "gui/tunnel/ISecretStore.h"

namespace deskflow::tunnel {

/**
 * The Service Token in the login keychain of the user, as a generic password: the service is
 * `<reverse domain name of the application>.cloudflare-access`, the account is the Client ID and the data is the
 * Secret. The application is not sandboxed and has no keychain access group, so this is the keychain that every
 * application of the user can ask for an item in; an item is readable without a prompt by the application that made it.
 * macOS ties that to the code signature, so a build with the same signing identity reads it without a prompt after an
 * update, and a build with another identity (or an ad hoc one) is asked about once.
 *
 * Reading the Client ID does not read the Secret (no data is requested), which is what never prompts.
 */
class MacKeychainStore : public ISecretStore
{
public:
  QString name() const override;
  bool isPersistent() const override;
  StoreResult save(const ServiceToken &token) override;
  std::optional<QString> storedClientId(StoreResult *result = nullptr) const override;
  std::optional<ServiceToken> load(StoreResult *result = nullptr) const override;
  StoreResult remove() override;
};

} // namespace deskflow::tunnel

#endif // Q_OS_MACOS
