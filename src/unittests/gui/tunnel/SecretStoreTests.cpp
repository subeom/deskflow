/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SecretStoreTests.h"

#include "gui/Diagnostic.h"
#include "gui/tunnel/InMemorySecretStore.h"
#include "gui/tunnel/SecretStoreFactory.h"

using namespace deskflow::tunnel;
namespace diagnostic = deskflow::gui::diagnostic;

namespace {

// made-up values that look nothing like a real token
const auto kId = QStringLiteral("TESTID-0001.access");
const auto kSecret = QStringLiteral("TESTSECRET-do-not-leak-0002");

} // namespace

void SecretStoreTests::aStoreThatIsEmpty_saysSoWithoutAnError()
{
  InMemorySecretStore store;
  StoreResult result;
  QVERIFY(!store.storedClientId(&result).has_value());
  QVERIFY(result.notFound);
  QVERIFY(!result.ok);
  QVERIFY(result.detail.isEmpty());
  QVERIFY(!store.load(&result).has_value());
  QVERIFY(result.notFound);
}

void SecretStoreTests::save_storesTheTokenAndReplacesTheOneThatWasThere()
{
  InMemorySecretStore store;
  QVERIFY(store.save(ServiceToken(kId, kSecret)).ok);
  QCOMPARE(store.storedClientId().value_or(QString()), kId);

  QVERIFY(store.save(ServiceToken(QStringLiteral("OTHER.access"), QStringLiteral("OTHER-SECRET-VALUE"))).ok);
  QCOMPARE(store.storedClientId().value_or(QString()), QStringLiteral("OTHER.access"));
  QCOMPARE(store.load()->secret, QStringLiteral("OTHER-SECRET-VALUE"));
}

void SecretStoreTests::storedClientId_doesNotReadTheSecret()
{
  InMemorySecretStore store;
  QVERIFY(store.save(ServiceToken(kId, kSecret)).ok);
  for (int i = 0; i < 3; ++i) {
    QCOMPARE(store.storedClientId().value_or(QString()), kId);
  }
  QCOMPARE(store.state()->idReadCount, 3);
  QCOMPARE(store.state()->loadCount, 0);
}

void SecretStoreTests::load_givesTheWholeToken()
{
  InMemorySecretStore store;
  QVERIFY(store.save(ServiceToken(kId, kSecret)).ok);
  StoreResult result;
  const auto token = store.load(&result);
  QVERIFY(result.ok);
  QVERIFY(token.has_value());
  QCOMPARE(token->clientId, kId);
  QCOMPARE(token->secret, kSecret);
  QCOMPARE(store.state()->loadCount, 1);
}

void SecretStoreTests::remove_forgetsTheTokenAndIsNotAnErrorWhenThereIsNone()
{
  InMemorySecretStore store;
  QVERIFY(store.remove().ok);

  QVERIFY(store.save(ServiceToken(kId, kSecret)).ok);
  QVERIFY(store.remove().ok);
  QVERIFY(!store.storedClientId().has_value());
  QVERIFY(!store.load().has_value());
  QVERIFY(store.remove().ok);
}

void SecretStoreTests::save_refusesATokenThatIsNotWhole_data()
{
  QTest::addColumn<QString>("id");
  QTest::addColumn<QString>("secret");
  QTest::newRow("no-id") << QString() << kSecret;
  QTest::newRow("no-secret") << kId << QString();
  QTest::newRow("nothing") << QString() << QString();
}

void SecretStoreTests::save_refusesATokenThatIsNotWhole()
{
  QFETCH(QString, id);
  QFETCH(QString, secret);

  InMemorySecretStore store;
  const auto result = store.save(ServiceToken(id, secret));
  QVERIFY(!result.ok);
  QVERIFY(!result.detail.isEmpty());
  QVERIFY(!store.storedClientId().has_value());
}

void SecretStoreTests::aStoreThatFails_saysWhyOnEveryOperation()
{
  InMemorySecretStore store;
  QVERIFY(store.save(ServiceToken(kId, kSecret)).ok);
  store.state()->failWith = QStringLiteral("the keychain is locked");

  StoreResult result;
  QVERIFY(!store.storedClientId(&result).has_value());
  QVERIFY(!result.ok && !result.notFound);
  QCOMPARE(result.detail, QStringLiteral("the keychain is locked"));
  QVERIFY(!store.load(&result).has_value());
  QCOMPARE(result.detail, QStringLiteral("the keychain is locked"));
  QVERIFY(!store.save(ServiceToken(kId, kSecret)).ok);
  QVERIFY(!store.remove().ok);

  // and it is still there when the keychain is open again
  store.state()->failWith.clear();
  QCOMPARE(store.storedClientId().value_or(QString()), kId);
}

void SecretStoreTests::storesOfTheSameStateSeeTheSameToken()
{
  InMemorySecretStore first;
  InMemorySecretStore second(first.state());
  QVERIFY(first.save(ServiceToken(kId, kSecret)).ok);
  QCOMPARE(second.storedClientId().value_or(QString()), kId);

  InMemorySecretStore apart;
  QVERIFY(!apart.storedClientId().has_value());
}

void SecretStoreTests::sessionStore_isOneForTheWholeProgram()
{
  const auto first = sessionSecretStore();
  const auto second = sessionSecretStore();
  QCOMPARE(first.get(), second.get());
  QVERIFY(!first->isPersistent());
  QVERIFY(!first->name().isEmpty());
}

void SecretStoreTests::systemStore_isTheMemoryOneWhenTheVariableSaysMemory()
{
  const auto before = qgetenv(kSecretStoreVariable);
  qputenv(kSecretStoreVariable, "memory");
  const auto store = createSystemSecretStore();
  QCOMPARE(store.get(), sessionSecretStore().get());

  qputenv(kSecretStoreVariable, "MEMORY");
  QCOMPARE(createSystemSecretStore().get(), sessionSecretStore().get());

  if (before.isNull()) {
    qunsetenv(kSecretStoreVariable);
  } else {
    qputenv(kSecretStoreVariable, before);
  }
}

void SecretStoreTests::systemStore_isPersistentOnWindowsAndMacOnly()
{
  // asking for the store and for its name touches nothing: no call of the keychain or of the Credential Manager is made
  const auto before = qgetenv(kSecretStoreVariable);
  qunsetenv(kSecretStoreVariable);
  const auto store = createSystemSecretStore();
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
  QVERIFY(store->isPersistent());
  QVERIFY(store.get() != sessionSecretStore().get());
#else
  QVERIFY(!store->isPersistent());
  QCOMPARE(store.get(), sessionSecretStore().get());
#endif
  QVERIFY(!store->name().isEmpty());

  if (!before.isNull()) {
    qputenv(kSecretStoreVariable, before);
  }
}

void SecretStoreTests::theResetOfTheSettings_removesTheStoredToken()
{
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);

  QVERIFY(diagnostic::removeStoredCredentials(store));
  StoreResult result;
  QVERIFY(!store->storedClientId(&result).has_value());
  QVERIFY(result.notFound);
  QVERIFY(!store->load().has_value());
  QCOMPARE(store->state()->removeCount, 1);
}

void SecretStoreTests::theResetOfTheSettings_isNotAnErrorWhenNothingIsStored()
{
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(diagnostic::removeStoredCredentials(store));
}

void SecretStoreTests::theResetOfTheSettings_saysWhenTheStoreFails()
{
  const auto store = std::make_shared<InMemorySecretStore>();
  QVERIFY(store->save(ServiceToken(kId, kSecret)).ok);
  store->state()->failWith = QStringLiteral("the keychain is locked");

  QVERIFY(!diagnostic::removeStoredCredentials(store));
}

void SecretStoreTests::theResetOfTheSettings_usesTheSystemStoreByDefault()
{
  // never the keychain or the Credential Manager of this computer: the store in memory
  qputenv(kSecretStoreVariable, "memory");
  QVERIFY(!createSystemSecretStore()->isPersistent());

  const auto shared = sessionSecretStore();
  QVERIFY(shared->save(ServiceToken(kId, kSecret)).ok);
  QVERIFY(diagnostic::removeStoredCredentials());
  QVERIFY(!shared->load().has_value());
}

QTEST_MAIN(SecretStoreTests)
