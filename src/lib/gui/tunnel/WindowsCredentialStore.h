/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN

#include "gui/tunnel/ISecretStore.h"

namespace deskflow::tunnel {

/**
 * The Service Token in the Credential Manager of Windows, as a generic credential of the user who runs Deskflow:
 * `Deskflow/CloudflareAccess`, the Client ID as its user name and the Secret as its secret. It persists for every later
 * logon of the user on this computer (CRED_PERSIST_LOCAL_MACHINE); it is never the roaming kind, so the Secret does not
 * leave the computer. The credentials of a user are not visible to a service that runs as another account: the tunnel
 * is run by the GUI of the user for that reason.
 */
class WindowsCredentialStore : public ISecretStore
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

#endif // Q_OS_WIN
