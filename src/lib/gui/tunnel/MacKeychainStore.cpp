/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/MacKeychainStore.h"

#ifdef Q_OS_MACOS

#include "common/Constants.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <QByteArray>
#include <QCoreApplication>

#include <cstring>

namespace deskflow::tunnel {

namespace {

struct KeychainTexts
{
  Q_DECLARE_TR_FUNCTIONS(KeychainTexts)
};

// An owned Core Foundation object, released on every path
template <typename T> class Owned
{
public:
  Owned() = default;
  explicit Owned(T ref) : m_ref(ref)
  {
  }
  Owned(const Owned &) = delete;
  Owned &operator=(const Owned &) = delete;
  Owned(Owned &&other) noexcept : m_ref(other.m_ref)
  {
    other.m_ref = nullptr;
  }
  ~Owned()
  {
    if (m_ref != nullptr) {
      CFRelease(m_ref);
    }
  }

  T get() const
  {
    return m_ref;
  }

  explicit operator bool() const
  {
    return m_ref != nullptr;
  }

private:
  T m_ref = nullptr;
};

Owned<CFStringRef> cfString(const QString &text)
{
  const auto utf8 = text.toUtf8();
  return Owned<CFStringRef>(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(utf8.constData()), utf8.size(), kCFStringEncodingUTF8, false
  ));
}

QString toQString(CFStringRef string)
{
  if (string == nullptr) {
    return {};
  }
  const auto length = CFStringGetLength(string);
  const auto capacity = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  QByteArray buffer(static_cast<qsizetype>(capacity), '\0');
  if (!CFStringGetCString(string, buffer.data(), capacity, kCFStringEncodingUTF8)) {
    return {};
  }
  return QString::fromUtf8(buffer.constData());
}

QString serviceName()
{
  return QStringLiteral("%1.cloudflare-access").arg(QString::fromLatin1(kRevFqdnName));
}

Owned<CFMutableDictionaryRef> newDictionary()
{
  return Owned<CFMutableDictionaryRef>(CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks
  ));
}

// the items of this program: a generic password of the service, of any account
Owned<CFMutableDictionaryRef> serviceQuery()
{
  auto query = newDictionary();
  const auto service = cfString(serviceName());
  CFDictionarySetValue(query.get(), kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query.get(), kSecAttrService, service.get());
  return query;
}

QString statusText(OSStatus status)
{
  const Owned<CFStringRef> message(SecCopyErrorMessageString(status, nullptr));
  const auto text = toQString(message.get());
  return text.isEmpty() ? KeychainTexts::tr("Keychain error %1").arg(status)
                        : KeychainTexts::tr("%1 (keychain error %2)").arg(text).arg(status);
}

StoreResult resultOf(OSStatus status)
{
  if (status == errSecSuccess) {
    return StoreResult::success();
  }
  if (status == errSecItemNotFound) {
    return StoreResult::missing();
  }
  return StoreResult::failure(statusText(status));
}

// the bytes of the secret in a Core Foundation object whose memory is wiped when it is given back
class SecretData
{
public:
  explicit SecretData(const QString &secret)
  {
    auto utf8 = secret.toUtf8();
    m_data = CFDataCreateMutable(kCFAllocatorDefault, utf8.size());
    if (m_data != nullptr) {
      CFDataAppendBytes(m_data, reinterpret_cast<const UInt8 *>(utf8.constData()), utf8.size());
    }
    // the copy that this function made
    utf8.fill('\0');
  }
  SecretData(const SecretData &) = delete;
  SecretData &operator=(const SecretData &) = delete;
  ~SecretData()
  {
    if (m_data != nullptr) {
      std::memset(CFDataGetMutableBytePtr(m_data), 0, static_cast<size_t>(CFDataGetLength(m_data)));
      CFRelease(m_data);
    }
  }

  CFDataRef get() const
  {
    return m_data;
  }

private:
  CFMutableDataRef m_data = nullptr;
};

} // namespace

QString MacKeychainStore::name() const
{
  return KeychainTexts::tr("macOS Keychain");
}

bool MacKeychainStore::isPersistent() const
{
  return true;
}

StoreResult MacKeychainStore::save(const ServiceToken &token)
{
  if (!token.isComplete()) {
    return StoreResult::failure(KeychainTexts::tr("The Client ID and the Secret are both needed."));
  }

  // one item is kept: a token of another account is replaced, not added to
  if (const auto existing = storedClientId(); existing.has_value() && *existing != token.clientId) {
    if (const auto removed = remove(); !removed.ok) {
      return removed;
    }
  }

  const auto account = cfString(token.clientId);
  const auto label = cfString(KeychainTexts::tr("Deskflow Cloudflare Access Service Token"));
  const SecretData data(token.secret);
  if (!account || !label || data.get() == nullptr) {
    return StoreResult::failure(KeychainTexts::tr("The token could not be prepared for the keychain."));
  }

  auto add = serviceQuery();
  CFDictionarySetValue(add.get(), kSecAttrAccount, account.get());
  CFDictionarySetValue(add.get(), kSecAttrLabel, label.get());
  CFDictionarySetValue(add.get(), kSecValueData, data.get());
  OSStatus status = SecItemAdd(add.get(), nullptr);

  if (status == errSecDuplicateItem) {
    auto find = serviceQuery();
    CFDictionarySetValue(find.get(), kSecAttrAccount, account.get());
    auto change = newDictionary();
    CFDictionarySetValue(change.get(), kSecValueData, data.get());
    status = SecItemUpdate(find.get(), change.get());
  }

  return resultOf(status);
}

std::optional<QString> MacKeychainStore::storedClientId(StoreResult *result) const
{
  // attributes only: no data is requested, so the Secret is not read and no access prompt can come of it
  auto query = serviceQuery();
  CFDictionarySetValue(query.get(), kSecReturnAttributes, kCFBooleanTrue);
  CFDictionarySetValue(query.get(), kSecMatchLimit, kSecMatchLimitOne);

  CFTypeRef found = nullptr;
  const OSStatus status = SecItemCopyMatching(query.get(), &found);
  const Owned<CFTypeRef> owned(found);
  if (result != nullptr) {
    *result = resultOf(status);
  }
  if (status != errSecSuccess || found == nullptr || CFGetTypeID(found) != CFDictionaryGetTypeID()) {
    return std::nullopt;
  }

  const auto account =
      static_cast<CFStringRef>(CFDictionaryGetValue(static_cast<CFDictionaryRef>(found), kSecAttrAccount));
  return toQString(account);
}

std::optional<ServiceToken> MacKeychainStore::load(StoreResult *result) const
{
  auto query = serviceQuery();
  CFDictionarySetValue(query.get(), kSecReturnAttributes, kCFBooleanTrue);
  CFDictionarySetValue(query.get(), kSecReturnData, kCFBooleanTrue);
  CFDictionarySetValue(query.get(), kSecMatchLimit, kSecMatchLimitOne);

  CFTypeRef found = nullptr;
  const OSStatus status = SecItemCopyMatching(query.get(), &found);
  const Owned<CFTypeRef> owned(found);
  if (result != nullptr) {
    *result = resultOf(status);
  }
  if (status != errSecSuccess || found == nullptr || CFGetTypeID(found) != CFDictionaryGetTypeID()) {
    return std::nullopt;
  }

  const auto item = static_cast<CFDictionaryRef>(found);
  const auto data = static_cast<CFDataRef>(CFDictionaryGetValue(item, kSecValueData));
  if (data == nullptr) {
    if (result != nullptr) {
      *result = StoreResult::failure(KeychainTexts::tr("The keychain did not return the Secret."));
    }
    return std::nullopt;
  }

  ServiceToken token;
  token.clientId = toQString(static_cast<CFStringRef>(CFDictionaryGetValue(item, kSecAttrAccount)));
  token.secret = QString::fromUtf8(reinterpret_cast<const char *>(CFDataGetBytePtr(data)), CFDataGetLength(data));
  return token;
}

StoreResult MacKeychainStore::remove()
{
  // an item per call: a delete of a query that matches more than one takes only the first on this system
  for (int round = 0; round < 10; ++round) {
    const auto query = serviceQuery();
    const OSStatus status = SecItemDelete(query.get());
    if (status == errSecItemNotFound) {
      return StoreResult::success();
    }
    if (status != errSecSuccess) {
      return StoreResult::failure(statusText(status));
    }
  }
  return StoreResult::success();
}

} // namespace deskflow::tunnel

#endif // Q_OS_MACOS
