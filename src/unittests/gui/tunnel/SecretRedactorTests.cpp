/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SecretRedactorTests.h"

#include "gui/tunnel/SecretRedactor.h"

using namespace deskflow::tunnel;

namespace {

const auto kSecret = QStringLiteral("TESTSECRET-do-not-leak-0002");
const auto kId = QStringLiteral("TESTID-0001.access");
const auto kMask = QLatin1String(SecretRedactor::kMask);

} // namespace

void SecretRedactorTests::aRegisteredSecret_isMaskedWhereverItAppears()
{
  SecretRedactor redactor;
  redactor.addSecret(kSecret);

  const auto line = QStringLiteral("token %1 and again %1.").arg(kSecret);
  const auto redacted = redactor.redact(line);
  QVERIFY(!redacted.contains(kSecret));
  QCOMPARE(redacted, QStringLiteral("token %1 and again %1.").arg(kMask));
}

void SecretRedactorTests::aRegisteredSecret_isMaskedInTheFormsOfAHeaderDump()
{
  SecretRedactor redactor;
  const auto secret = QStringLiteral("ab/cd+ef=\"gh\"\\ij");
  redactor.addSecret(secret);

  // as it is, percent encoded (a query), and with the escapes of JSON
  QVERIFY(!redactor.redact(QStringLiteral("x %1 y").arg(secret)).contains(secret));
  QVERIFY(!redactor.redact(QStringLiteral("x ab%2Fcd%2Bef%3D%22gh%22%5Cij y")).contains(QStringLiteral("%2Fcd")));
  QVERIFY(!redactor.redact(QStringLiteral("{\"k\":\"ab/cd+ef=\\\"gh\\\"\\\\ij\"}")).contains(QStringLiteral("cd+ef")));
}

void SecretRedactorTests::shortValues_areNotRegistered()
{
  SecretRedactor redactor;
  redactor.addSecret(QString());
  redactor.addSecret(QStringLiteral("a"));
  redactor.addSecret(QStringLiteral("abc"));
  const auto text = QStringLiteral("a normal line about abc");
  QCOMPARE(redactor.redact(text), text);
}

void SecretRedactorTests::headers_ofCloudflareAccessAreMaskedWithoutKnowingTheValue_data()
{
  QTest::addColumn<QString>("line");
  QTest::addColumn<QString>("leak");

  QTest::newRow("header") << QStringLiteral("Cf-Access-Client-Secret: 0123456789abcdef0123456789abcdef\r\nHost: x")
                          << QStringLiteral("0123456789abcdef");
  QTest::newRow("header-lowercase") << QStringLiteral("cf-access-client-secret: 0123456789abcdef")
                                    << QStringLiteral("0123456789abcdef");
  QTest::newRow("client-id") << QStringLiteral("Cf-Access-Client-Id: 99999999.access") << QStringLiteral("99999999");
  QTest::newRow("json-array") << QStringLiteral("{\"Cf-Access-Client-Secret\":[\"0123456789abcdef\"],\"Host\":[\"x\"]}")
                              << QStringLiteral("0123456789abcdef");
  QTest::newRow("json-string") << QStringLiteral("{\"cf-access-client-secret\": \"0123456789abcdef\"}")
                               << QStringLiteral("0123456789abcdef");
  QTest::newRow("environment") << QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET=0123456789abcdef")
                               << QStringLiteral("0123456789abcdef");
  QTest::newRow("environment-id") << QStringLiteral("TUNNEL_SERVICE_TOKEN_ID=99999999.access")
                                  << QStringLiteral("99999999");
  QTest::newRow("access-token") << QStringLiteral("Cf-Access-Token: abcdefghij0123456789")
                                << QStringLiteral("abcdefghij");
  QTest::newRow("cookie") << QStringLiteral("Cookie: CF_Authorization=abcdefghij0123456789; other=1")
                          << QStringLiteral("abcdefghij");
}

void SecretRedactorTests::headers_ofCloudflareAccessAreMaskedWithoutKnowingTheValue()
{
  QFETCH(QString, line);
  QFETCH(QString, leak);

  const auto redacted = SecretRedactor().redact(line);
  QVERIFY2(!redacted.contains(leak), qPrintable(redacted));
  QVERIFY2(redacted.contains(kMask), qPrintable(redacted));
  // the same without an instance
  QCOMPARE(SecretRedactor::redactKnownShapes(line), redacted);
}

void SecretRedactorTests::aJsonWebToken_isMasked()
{
  const auto jwt = QStringLiteral("eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJ4In0.c2lnbmF0dXJl");
  const auto redacted = SecretRedactor().redact(QStringLiteral("token=%1 end").arg(jwt));
  QVERIFY(!redacted.contains(QStringLiteral("eyJhbGci")));
  QVERIFY(!redacted.contains(QStringLiteral("c2lnbmF0")));
  QVERIFY(redacted.endsWith(QStringLiteral("end")));
}

void SecretRedactorTests::ordinaryLines_areNotChanged_data()
{
  QTest::addColumn<QString>("line");

  QTest::newRow("listener") << QStringLiteral("2026-11-17T04:00:00Z INF Start Websocket listener host=127.0.0.1:24800");
  QTest::newRow("handshake") << QStringLiteral("ERR failed to connect to origin error=\"websocket: bad handshake\"");
  QTest::newRow("mentions-a-secret-without-a-value") << QStringLiteral("the secret is stored in the keychain");
  QTest::newRow("empty") << QString();
  QTest::newRow("a-url") << QStringLiteral("https://login.example.com/cdn-cgi/access/cli?aud=abc");
}

void SecretRedactorTests::ordinaryLines_areNotChanged()
{
  QFETCH(QString, line);
  QCOMPARE(SecretRedactor().redact(line), line);
}

void SecretRedactorTests::clear_forgetsTheSecrets()
{
  SecretRedactor redactor;
  redactor.addSecret(kSecret);
  redactor.addSecret(kId);
  QVERIFY(!redactor.redact(kId).contains(kId));
  redactor.clear();
  QCOMPARE(redactor.redact(kSecret), kSecret);
}

QTEST_MAIN(SecretRedactorTests)
