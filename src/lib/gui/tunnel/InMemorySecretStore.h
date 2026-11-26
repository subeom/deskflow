/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/ISecretStore.h"

#include <memory>

namespace deskflow::tunnel {

/**
 * A store in memory: the token lives as long as the program. It is what systems without a secure store of their own
 * get (the user is told so), and what every test uses in place of a real keychain. Copies made with the same state
 * share the token, so a dialog that is opened twice sees what the first one stored.
 */
class InMemorySecretStore : public ISecretStore
{
public:
  struct State
  {
    std::optional<ServiceToken> token;
    int saveCount = 0;
    int idReadCount = 0;
    int loadCount = 0; //!< reads of the Secret
    int removeCount = 0;
    //! Makes the next operations fail the way a locked keychain does (for the tests of the error paths)
    QString failWith;
  };

  explicit InMemorySecretStore(std::shared_ptr<State> state = std::make_shared<State>());

  QString name() const override;
  bool isPersistent() const override;
  StoreResult save(const ServiceToken &token) override;
  std::optional<QString> storedClientId(StoreResult *result = nullptr) const override;
  std::optional<ServiceToken> load(StoreResult *result = nullptr) const override;
  StoreResult remove() override;

  const std::shared_ptr<State> &state() const
  {
    return m_state;
  }

private:
  std::shared_ptr<State> m_state;
};

} // namespace deskflow::tunnel
