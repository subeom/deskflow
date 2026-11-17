/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/InMemorySecretStore.h"

#include <QCoreApplication>

namespace deskflow::tunnel {

namespace {

struct StoreTexts
{
  Q_DECLARE_TR_FUNCTIONS(StoreTexts)
};

} // namespace

InMemorySecretStore::InMemorySecretStore(std::shared_ptr<State> state) : m_state(std::move(state))
{
}

QString InMemorySecretStore::name() const
{
  return StoreTexts::tr("Session memory");
}

bool InMemorySecretStore::isPersistent() const
{
  return false;
}

StoreResult InMemorySecretStore::save(const ServiceToken &token)
{
  ++m_state->saveCount;
  if (!m_state->failWith.isEmpty()) {
    return StoreResult::failure(m_state->failWith);
  }
  if (!token.isComplete()) {
    return StoreResult::failure(StoreTexts::tr("The Client ID and the Secret are both needed."));
  }
  m_state->token = token;
  return StoreResult::success();
}

std::optional<QString> InMemorySecretStore::storedClientId(StoreResult *result) const
{
  ++m_state->idReadCount;
  if (!m_state->failWith.isEmpty()) {
    if (result != nullptr) {
      *result = StoreResult::failure(m_state->failWith);
    }
    return std::nullopt;
  }
  if (!m_state->token.has_value()) {
    if (result != nullptr) {
      *result = StoreResult::missing();
    }
    return std::nullopt;
  }
  if (result != nullptr) {
    *result = StoreResult::success();
  }
  return m_state->token->clientId;
}

std::optional<ServiceToken> InMemorySecretStore::load(StoreResult *result) const
{
  ++m_state->loadCount;
  if (!m_state->failWith.isEmpty()) {
    if (result != nullptr) {
      *result = StoreResult::failure(m_state->failWith);
    }
    return std::nullopt;
  }
  if (!m_state->token.has_value()) {
    if (result != nullptr) {
      *result = StoreResult::missing();
    }
    return std::nullopt;
  }
  if (result != nullptr) {
    *result = StoreResult::success();
  }
  return m_state->token;
}

StoreResult InMemorySecretStore::remove()
{
  ++m_state->removeCount;
  if (!m_state->failWith.isEmpty()) {
    return StoreResult::failure(m_state->failWith);
  }
  if (m_state->token.has_value()) {
    m_state->token->wipe();
  }
  m_state->token.reset();
  return StoreResult::success();
}

} // namespace deskflow::tunnel
