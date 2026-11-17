/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/WindowsCredentialStore.h"

#ifdef Q_OS_WIN

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// after windows.h: it needs the types of it
#include <wincred.h>

#include <QByteArray>
#include <QCoreApplication>

#include <string>

namespace deskflow::tunnel {

namespace {

struct WindowsStoreTexts
{
  Q_DECLARE_TR_FUNCTIONS(WindowsStoreTexts)
};

// The name the credential is stored under. Fixed: a change of it would lose the credentials of every user.
const std::wstring kTargetName = L"Deskflow/CloudflareAccess";

QString windowsErrorText(DWORD error)
{
  return WindowsStoreTexts::tr("Windows error %1").arg(error);
}

// A credential that was read, freed on every path
class ReadCredential
{
public:
  ~ReadCredential()
  {
    if (m_credential != nullptr) {
      // the Secret is in the memory that is given back: wipe it first
      if (m_credential->CredentialBlob != nullptr && m_credential->CredentialBlobSize > 0) {
        SecureZeroMemory(m_credential->CredentialBlob, m_credential->CredentialBlobSize);
      }
      CredFree(m_credential);
    }
  }

  // 0 on success, otherwise the Windows error (ERROR_NOT_FOUND when nothing is stored)
  DWORD read()
  {
    if (!CredReadW(kTargetName.c_str(), CRED_TYPE_GENERIC, 0, &m_credential)) {
      m_credential = nullptr;
      return GetLastError();
    }
    return 0;
  }

  const CREDENTIALW *get() const
  {
    return m_credential;
  }

private:
  PCREDENTIALW m_credential = nullptr;
};

} // namespace

QString WindowsCredentialStore::name() const
{
  return WindowsStoreTexts::tr("Windows Credential Manager");
}

bool WindowsCredentialStore::isPersistent() const
{
  return true;
}

StoreResult WindowsCredentialStore::save(const ServiceToken &token)
{
  if (!token.isComplete()) {
    return StoreResult::failure(WindowsStoreTexts::tr("The Client ID and the Secret are both needed."));
  }

  QByteArray secret = token.secret.toUtf8();
  if (secret.size() > static_cast<qsizetype>(CRED_MAX_CREDENTIAL_BLOB_SIZE)) {
    SecureZeroMemory(secret.data(), static_cast<SIZE_T>(secret.size()));
    return StoreResult::failure(WindowsStoreTexts::tr("The Secret is too long for the Credential Manager."));
  }

  std::wstring userName = token.clientId.toStdWString();
  std::wstring target = kTargetName;

  CREDENTIALW credential = {};
  credential.Type = CRED_TYPE_GENERIC;
  credential.TargetName = target.data();
  credential.UserName = userName.data();
  credential.CredentialBlobSize = static_cast<DWORD>(secret.size());
  credential.CredentialBlob = reinterpret_cast<LPBYTE>(secret.data());
  credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

  const BOOL written = CredWriteW(&credential, 0);
  const DWORD error = written ? 0 : GetLastError();
  SecureZeroMemory(secret.data(), static_cast<SIZE_T>(secret.size()));

  return written ? StoreResult::success() : StoreResult::failure(windowsErrorText(error));
}

std::optional<QString> WindowsCredentialStore::storedClientId(StoreResult *result) const
{
  // CredReadW returns the secret with the rest: it is only the name that is used, and the secret is wiped when freed
  ReadCredential credential;
  const DWORD error = credential.read();
  if (error != 0) {
    if (result != nullptr) {
      *result = error == ERROR_NOT_FOUND ? StoreResult::missing() : StoreResult::failure(windowsErrorText(error));
    }
    return std::nullopt;
  }

  if (result != nullptr) {
    *result = StoreResult::success();
  }
  return QString::fromWCharArray(credential.get()->UserName != nullptr ? credential.get()->UserName : L"");
}

std::optional<ServiceToken> WindowsCredentialStore::load(StoreResult *result) const
{
  ReadCredential credential;
  const DWORD error = credential.read();
  if (error != 0) {
    if (result != nullptr) {
      *result = error == ERROR_NOT_FOUND ? StoreResult::missing() : StoreResult::failure(windowsErrorText(error));
    }
    return std::nullopt;
  }

  const CREDENTIALW *read = credential.get();
  ServiceToken token;
  token.clientId = QString::fromWCharArray(read->UserName != nullptr ? read->UserName : L"");
  token.secret = QString::fromUtf8(reinterpret_cast<const char *>(read->CredentialBlob), read->CredentialBlobSize);

  if (result != nullptr) {
    *result = StoreResult::success();
  }
  return token;
}

StoreResult WindowsCredentialStore::remove()
{
  if (CredDeleteW(kTargetName.c_str(), CRED_TYPE_GENERIC, 0)) {
    return StoreResult::success();
  }

  const DWORD error = GetLastError();
  return error == ERROR_NOT_FOUND ? StoreResult::success() : StoreResult::failure(windowsErrorText(error));
}

} // namespace deskflow::tunnel

#endif // Q_OS_WIN
