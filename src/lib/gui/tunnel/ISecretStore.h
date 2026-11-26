/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/ServiceToken.h"

#include <QString>

#include <optional>

namespace deskflow::tunnel {

//! What an operation of a secret store came to
struct StoreResult
{
  bool ok = false;
  bool notFound = false; //!< nothing is stored (not an error for a remove, an answer for a read)
  QString detail;        //!< the system's own words, for a message to the user; never a secret

  static StoreResult success()
  {
    StoreResult result;
    result.ok = true;
    return result;
  }

  static StoreResult missing()
  {
    StoreResult result;
    result.notFound = true;
    return result;
  }

  static StoreResult failure(QString text)
  {
    StoreResult result;
    result.detail = std::move(text);
    return result;
  }
};

/**
 * Where the Service Token is kept between runs: the secure store of the operating system, never the settings file.
 * One token is kept; saving replaces it. The interface is what the rest of the program sees, so everything above it is
 * tested with an in-memory one and no real keychain is touched by a test.
 */
class ISecretStore
{
public:
  virtual ~ISecretStore() = default;

  //! In words for the user: "Windows Credential Manager", "macOS Keychain", "Session memory"
  virtual QString name() const = 0;

  //! The token is still there after Deskflow ends. When it is not, the user is told so.
  virtual bool isPersistent() const = 0;

  //! Stores \p token, replacing the one that is stored
  virtual StoreResult save(const ServiceToken &token) = 0;

  /**
   * The Client ID of the stored token, which is not a secret: what the dialog shows to say that there is a token. It
   * does not read the Secret where the system lets it be left unread, so asking never prompts for access to it.
   * Empty when nothing is stored or it cannot be read; \p result tells which.
   */
  virtual std::optional<QString> storedClientId(StoreResult *result = nullptr) const = 0;

  //! The stored token, to start a cloudflared with; reads the Secret. Only that start asks for it.
  virtual std::optional<ServiceToken> load(StoreResult *result = nullptr) const = 0;

  //! Deletes the stored token; nothing stored is not an error
  virtual StoreResult remove() = 0;
};

} // namespace deskflow::tunnel
