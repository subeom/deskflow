/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/SecretStoreFactory.h"

#include "gui/tunnel/InMemorySecretStore.h"
#include "gui/tunnel/MacKeychainStore.h"
#include "gui/tunnel/WindowsCredentialStore.h"

#include <QString>

namespace deskflow::tunnel {

std::shared_ptr<ISecretStore> sessionSecretStore()
{
  static const std::shared_ptr<ISecretStore> store = std::make_shared<InMemorySecretStore>();
  return store;
}

std::shared_ptr<ISecretStore> createSystemSecretStore()
{
  if (qEnvironmentVariable(kSecretStoreVariable).compare(QStringLiteral("memory"), Qt::CaseInsensitive) == 0) {
    return sessionSecretStore();
  }

#if defined(Q_OS_WIN)
  return std::make_shared<WindowsCredentialStore>();
#elif defined(Q_OS_MACOS)
  return std::make_shared<MacKeychainStore>();
#else
  return sessionSecretStore();
#endif
}

} // namespace deskflow::tunnel
