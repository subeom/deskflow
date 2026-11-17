/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CloudflaredLaunchTests.h"

#include "gui/tunnel/CloudflaredLaunch.h"

using namespace deskflow::tunnel;

namespace {

// made-up values that look nothing like a real token; the tests look for exactly these
const auto kClientId = QStringLiteral("TESTID-0001.access");
const auto kSecret = QStringLiteral("TESTSECRET-do-not-leak-0002");

LaunchRequest request()
{
  LaunchRequest r;
  r.program = QStringLiteral("/opt/homebrew/bin/cloudflared");
  r.hostname = QStringLiteral("mbp.example.com");
  r.localPort = 24800;
  return r;
}

ServiceToken token()
{
  return {kClientId, kSecret};
}

LaunchSpec spec()
{
  const auto built = buildLaunchSpec(request(), token(), QProcessEnvironment());
  return built.value_or(LaunchSpec{});
}

} // namespace

void CloudflaredLaunchTests::arguments_areTheAccessTcpCommandAndNameNoSecret()
{
  const auto built = spec();
  QCOMPARE(built.program, QStringLiteral("/opt/homebrew/bin/cloudflared"));
  QCOMPARE(
      built.arguments,
      (QStringList{"access", "tcp", "--hostname", "mbp.example.com", "--url", "127.0.0.1:24800", "--loglevel", "info"})
  );

  for (const auto &argument : built.arguments) {
    QVERIFY2(!argument.contains(kSecret), "the secret is in an argument");
    QVERIFY2(!argument.contains(kClientId), "the client id is in an argument");
  }
  QVERIFY(!built.program.contains(kSecret));
}

void CloudflaredLaunchTests::environment_holdsTheTokenAndOnlyThere()
{
  const auto built = spec();
  QCOMPARE(built.environment.value(QStringLiteral("TUNNEL_SERVICE_TOKEN_ID")), kClientId);
  QCOMPARE(built.environment.value(QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET")), kSecret);
  QCOMPARE(QString::fromLatin1(kTokenIdVariable), QStringLiteral("TUNNEL_SERVICE_TOKEN_ID"));
  QCOMPARE(QString::fromLatin1(kTokenSecretVariable), QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET"));

  // and in no other variable and no other place of the spec
  int holding = 0;
  for (const auto &key : built.environment.keys()) {
    if (built.environment.value(key).contains(kSecret)) {
      ++holding;
      QCOMPARE(key, QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET"));
    }
  }
  QCOMPARE(holding, 1);
  QVERIFY(!built.commandLine().contains(kSecret));
}

void CloudflaredLaunchTests::environment_keepsWhatTheChildNeedsAndDropsWhatWouldChangeTheTunnel()
{
  QProcessEnvironment base;
  base.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin"));
  base.insert(QStringLiteral("HTTPS_PROXY"), QStringLiteral("http://proxy.example.com:3128"));
  base.insert(QStringLiteral("HOME"), QStringLiteral("/Users/me"));
  base.insert(QStringLiteral("TUNNEL_SERVICE_HOSTNAME"), QStringLiteral("other.example.com"));
  base.insert(QStringLiteral("TUNNEL_SERVICE_URL"), QStringLiteral("0.0.0.0:1"));
  base.insert(QStringLiteral("TUNNEL_SERVICE_DESTINATION"), QStringLiteral("elsewhere:22"));
  base.insert(QStringLiteral("TUNNEL_LOG_OUTPUT"), QStringLiteral("json"));
  base.insert(QStringLiteral("TUNNEL_MANAGEMENT_OUTPUT"), QStringLiteral("json"));
  base.insert(QStringLiteral("TUNNEL_SERVICE_TOKEN_ID"), QStringLiteral("OLD-ID"));
  base.insert(QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET"), QStringLiteral("OLD-SECRET"));

  const auto built = buildLaunchSpec(request(), token(), base);
  QVERIFY(built.has_value());
  const auto &env = built->environment;

  QCOMPARE(env.value(QStringLiteral("PATH")), QStringLiteral("/usr/bin"));
  QCOMPARE(env.value(QStringLiteral("HTTPS_PROXY")), QStringLiteral("http://proxy.example.com:3128"));
  QCOMPARE(env.value(QStringLiteral("HOME")), QStringLiteral("/Users/me"));
  for (const auto &gone :
       {"TUNNEL_SERVICE_HOSTNAME", "TUNNEL_SERVICE_URL", "TUNNEL_SERVICE_DESTINATION", "TUNNEL_LOG_OUTPUT",
        "TUNNEL_MANAGEMENT_OUTPUT"}) {
    QVERIFY2(!env.contains(QString::fromLatin1(gone)), gone);
  }
  // the token of the user, not whatever the process had
  QCOMPARE(env.value(QStringLiteral("TUNNEL_SERVICE_TOKEN_ID")), kClientId);
  QCOMPARE(env.value(QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET")), kSecret);
}

void CloudflaredLaunchTests::environment_ofThisProcessIsNotChanged()
{
  QProcessEnvironment base;
  base.insert(QStringLiteral("TUNNEL_SERVICE_URL"), QStringLiteral("0.0.0.0:1"));
  const auto before = base.toStringList();

  const auto built = buildLaunchSpec(request(), token(), base);
  QVERIFY(built.has_value());
  QCOMPARE(base.toStringList(), before);
  QVERIFY(!base.contains(QStringLiteral("TUNNEL_SERVICE_TOKEN_SECRET")));
}

void CloudflaredLaunchTests::logLevel_isInfoAndNeverDebug()
{
  QCOMPARE(QString::fromLatin1(kLogLevel), QStringLiteral("info"));
  const auto arguments = spec().arguments;
  const auto index = arguments.indexOf(QStringLiteral("--loglevel"));
  QVERIFY(index >= 0);
  QCOMPARE(arguments.at(index + 1), QStringLiteral("info"));
  QVERIFY(!arguments.contains(QStringLiteral("debug")));
}

void CloudflaredLaunchTests::commandLine_isForTheLogAndHasNoEnvironment()
{
  QCOMPARE(
      spec().commandLine(),
      QStringLiteral(
          "/opt/homebrew/bin/cloudflared access tcp --hostname mbp.example.com --url 127.0.0.1:24800 --loglevel info"
      )
  );
}

void CloudflaredLaunchTests::commandLine_quotesWhatHasBlanks()
{
  auto r = request();
  r.program = QStringLiteral("C:/Program Files (x86)/cloudflared/cloudflared.exe");
  const auto built = buildLaunchSpec(r, token(), QProcessEnvironment());
  QVERIFY(built.has_value());
  QVERIFY(
      built->commandLine().startsWith(QStringLiteral("\"C:/Program Files (x86)/cloudflared/cloudflared.exe\" access"))
  );
}

void CloudflaredLaunchTests::request_thatIsNotUsableMakesNoSpec_data()
{
  QTest::addColumn<QString>("program");
  QTest::addColumn<QString>("hostname");
  QTest::addColumn<int>("port");
  QTest::addColumn<QString>("id");
  QTest::addColumn<QString>("secret");

  const auto program = QStringLiteral("/opt/homebrew/bin/cloudflared");
  const auto host = QStringLiteral("mbp.example.com");
  QTest::newRow("no-program") << QString() << host << 24800 << kClientId << kSecret;
  QTest::newRow("blank-program") << QStringLiteral("  ") << host << 24800 << kClientId << kSecret;
  QTest::newRow("no-hostname") << program << QString() << 24800 << kClientId << kSecret;
  QTest::newRow("an-option-as-hostname") << program << QStringLiteral("--url=1.2.3.4.example.com") << 24800 << kClientId
                                         << kSecret;
  QTest::newRow("an-address-as-hostname") << program << QStringLiteral("10.0.0.5") << 24800 << kClientId << kSecret;
  QTest::newRow("port-too-low") << program << host << 80 << kClientId << kSecret;
  QTest::newRow("port-too-high") << program << host << 70000 << kClientId << kSecret;
  QTest::newRow("no-port") << program << host << 0 << kClientId << kSecret;
  QTest::newRow("no-client-id") << program << host << 24800 << QString() << kSecret;
  QTest::newRow("no-secret") << program << host << 24800 << kClientId << QString();
}

void CloudflaredLaunchTests::request_thatIsNotUsableMakesNoSpec()
{
  QFETCH(QString, program);
  QFETCH(QString, hostname);
  QFETCH(int, port);
  QFETCH(QString, id);
  QFETCH(QString, secret);

  LaunchRequest r;
  r.program = program;
  r.hostname = hostname;
  r.localPort = port;
  QVERIFY(!buildLaunchSpec(r, ServiceToken(id, secret), QProcessEnvironment()).has_value());
}

void CloudflaredLaunchTests::serviceToken_isWipedWhenAsked()
{
  ServiceToken value(kClientId, kSecret);
  QVERIFY(value.isComplete());
  value.wipe();
  QVERIFY(value.secret.isEmpty());
  QVERIFY(value.clientId.isEmpty());
  QVERIFY(!value.isComplete());
}

QTEST_MAIN(CloudflaredLaunchTests)
