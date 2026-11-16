/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ConnectionDiagnosisTests.h"

#include "common/ConnectionDiagnosis.h"

#include <QString>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace deskflow::diagnosis;

namespace {

Failure failureOf(FailureKind kind, Phase phase = Phase::Tcp)
{
  Failure failure;
  failure.kind = kind;
  failure.phase = phase;
  failure.host = "10.0.0.5";
  failure.port = 24800;
  return failure;
}

QString textOf(const std::string &text)
{
  return QString::fromStdString(text);
}

bool hasHint(const Diagnosis &diagnosis, Hint hint)
{
  return std::ranges::find(diagnosis.hints, hint) != diagnosis.hints.end();
}

} // namespace

void ConnectionDiagnosisTests::formatTarget_namesTheAddressUsedAndBracketsIpv6()
{
  QCOMPARE(textOf(formatTarget("10.0.0.5", "", 24800)), QStringLiteral("10.0.0.5:24800"));
  QCOMPARE(textOf(formatTarget("10.0.0.5", "10.0.0.5", 24800)), QStringLiteral("10.0.0.5:24800"));
  QCOMPARE(textOf(formatTarget("desk", "10.0.0.5", 24800)), QStringLiteral("desk (10.0.0.5):24800"));
  QCOMPARE(textOf(formatTarget("fe80::1", "", 24800)), QStringLiteral("[fe80::1]:24800"));
  QCOMPARE(textOf(formatTarget("desk", "fe80::1", 24800)), QStringLiteral("desk ([fe80::1]):24800"));
  QCOMPARE(textOf(formatTarget("desk", "", 0)), QStringLiteral("desk"));
}

void ConnectionDiagnosisTests::everyKind_namesTheServerAndHasWordsAndHints_data()
{
  QTest::addColumn<int>("kind");
  QTest::addColumn<int>("phase");
  QTest::addColumn<QString>("headlineWord");
  QTest::addColumn<int>("expectedHint");

  const auto row = [](const char *name, FailureKind kind, Phase phase, const char *word, Hint hint) {
    QTest::newRow(name) << static_cast<int>(kind) << static_cast<int>(phase) << QString::fromLatin1(word)
                        << static_cast<int>(hint);
  };
  row("dns", FailureKind::DnsFailure, Phase::Resolving, "could not be resolved", Hint::CheckName);
  row("timeout-tcp", FailureKind::TimedOut, Phase::Tcp, "timed out", Hint::ServerFirewall);
  row("refused", FailureKind::Refused, Phase::Tcp, "refused", Hint::StartServer);
  row("no-route", FailureKind::NoRoute, Phase::Tcp, "no route", Hint::NetworkIsolation);
  row("reset", FailureKind::Reset, Phase::Tcp, "reset", Hint::InspectionProduct);
  row("denied", FailureKind::AccessDenied, Phase::Tcp, "not allowed", Hint::InspectionProduct);
  row("tls", FailureKind::TlsHandshakeFailed, Phase::Tls, "secure handshake", Hint::TlsSetting);
  row("fingerprint", FailureKind::FingerprintRejected, Phase::Tls, "fingerprint", Hint::TrustFingerprint);
  row("not-deskflow", FailureKind::NotDeskflowServer, Phase::Hello, "not a Deskflow server", Hint::OtherService);
  row("version", FailureKind::VersionMismatch, Phase::Hello, "version", Hint::UpdateDeskflow);
  row("unknown-name", FailureKind::UnknownClientName, Phase::Hello, "does not know", Hint::AddScreen);
  row("already", FailureKind::AlreadyConnected, Phase::Hello, "already connected", Hint::DuplicateName);
  row("unknown", FailureKind::Unknown, Phase::Tcp, "could not connect", Hint::CheckAddress);
}

void ConnectionDiagnosisTests::everyKind_namesTheServerAndHasWordsAndHints()
{
  QFETCH(int, kind);
  QFETCH(int, phase);
  QFETCH(QString, headlineWord);
  QFETCH(int, expectedHint);

  const auto diagnosis = diagnose(failureOf(static_cast<FailureKind>(kind), static_cast<Phase>(phase)));

  QCOMPARE(static_cast<int>(diagnosis.kind), kind);
  QVERIFY2(textOf(diagnosis.headline).contains(QStringLiteral("10.0.0.5")), qPrintable(textOf(diagnosis.headline)));
  QVERIFY2(textOf(diagnosis.headline).contains(headlineWord), qPrintable(textOf(diagnosis.headline)));
  QVERIFY(!diagnosis.cause.empty());
  QVERIFY(hasHint(diagnosis, static_cast<Hint>(expectedHint)));
  for (const auto hint : diagnosis.hints) {
    QVERIFY(!hintText(hint, 24800).empty());
  }
}

void ConnectionDiagnosisTests::timeout_saysWhichPhaseWasReached()
{
  const auto tcp = diagnose(failureOf(FailureKind::TimedOut, Phase::Tcp));
  const auto tls = diagnose(failureOf(FailureKind::TimedOut, Phase::Tls));
  const auto hello = diagnose(failureOf(FailureKind::TimedOut, Phase::Hello));

  QVERIFY(textOf(tcp.headline).contains(QStringLiteral("while connecting (TCP)")));
  QVERIFY(!textOf(tcp.headline).contains(QStringLiteral("secure handshake")));
  QVERIFY(textOf(tls.headline).contains(QStringLiteral("while the secure handshake (TLS)")));
  QVERIFY(textOf(tls.headline).contains(QStringLiteral("TCP connection to")));
  QVERIFY(textOf(tls.headline).contains(QStringLiteral("was made")));
  QVERIFY(textOf(hello.headline).contains(QStringLiteral("greeting")));
  QVERIFY(textOf(hello.headline).contains(QStringLiteral("was made")));

  // a silent drop is a network matter, a stuck handshake is not
  QVERIFY(hasHint(tcp, Hint::ServerFirewall));
  QVERIFY(hasHint(tcp, Hint::NetworkIsolation));
  QVERIFY(!hasHint(tls, Hint::ServerFirewall));
  QVERIFY(hasHint(tls, Hint::TlsSetting));
  QVERIFY(hasHint(tls, Hint::SlowNetwork));
}

void ConnectionDiagnosisTests::timeout_statesTheLimitThatWasUsed()
{
  auto failure = failureOf(FailureKind::TimedOut);
  QVERIFY(textOf(diagnose(failure).headline).contains(QStringLiteral("within 2 s")));

  failure.timeoutSeconds = 7.5;
  QVERIFY(textOf(diagnose(failure).headline).contains(QStringLiteral("within 7.5 s")));
  QVERIFY(textOf(hintText(Hint::SlowNetwork, 24800, 7.5)).contains(QStringLiteral("client/connectTimeout")));
  QVERIFY(textOf(hintText(Hint::SlowNetwork, 24800, 7.5)).contains(QStringLiteral("7.5")));
}

void ConnectionDiagnosisTests::macLocalNetworkHint_isOnlyForMacOs()
{
  for (const auto kind : {FailureKind::NoRoute, FailureKind::TimedOut}) {
    auto failure = failureOf(kind);

    failure.platform = InputPlatform::MacOS;
    const auto onMac = diagnose(failure);
    QVERIFY(hasHint(onMac, Hint::MacLocalNetwork));
    QVERIFY(textOf(onMac.cause).contains(QStringLiteral("Local Network")));
    QVERIFY(
        textOf(hintText(Hint::MacLocalNetwork, 24800)).contains(QStringLiteral("Privacy & Security > Local Network"))
    );

    for (const auto platform : {InputPlatform::Windows, InputPlatform::Linux, InputPlatform::Other}) {
      failure.platform = platform;
      const auto elsewhere = diagnose(failure);
      QVERIFY(!hasHint(elsewhere, Hint::MacLocalNetwork));
      QVERIFY(!textOf(elsewhere.cause).contains(QStringLiteral("Local Network")));
    }
  }

  // a refused connection reached the server computer: the permission is not the reason
  auto refused = failureOf(FailureKind::Refused);
  refused.platform = InputPlatform::MacOS;
  QVERIFY(!hasHint(diagnose(refused), Hint::MacLocalNetwork));
}

void ConnectionDiagnosisTests::serverFirewallHint_namesThePort()
{
  const auto text = textOf(hintText(Hint::ServerFirewall, 24801));
  QVERIFY(text.contains(QStringLiteral("24801")));
  QVERIFY(text.contains(QStringLiteral("server")));
}

void ConnectionDiagnosisTests::detail_isKeptForTheKindsThatCarryAnOsMessage()
{
  auto failure = failureOf(FailureKind::NoRoute);
  failure.detail = "No route to host";
  QVERIFY(textOf(diagnose(failure).headline).contains(QStringLiteral("(No route to host)")));

  failure = failureOf(FailureKind::Unknown);
  failure.detail = "weird failure";
  QVERIFY(textOf(diagnose(failure).headline).contains(QStringLiteral("weird failure")));

  failure = failureOf(FailureKind::TimedOut);
  failure.detail = "Operation timed out";
  QVERIFY(!textOf(diagnose(failure).headline).contains(QStringLiteral("Operation timed out")));
}

void ConnectionDiagnosisTests::line_endsWithThePointerToTheDocs()
{
  const auto line = textOf(diagnose(failureOf(FailureKind::TimedOut)).line());
  QVERIFY2(line.endsWith(QStringLiteral("See docs/connecting-across-networks.md")), qPrintable(line));
  QVERIFY(
      line.startsWith(QStringLiteral("cannot reach the server 10.0.0.5:24800: the connection timed out (no answer"))
  );
  QVERIFY(line.contains(QStringLiteral("A firewall on the server computer")));
  QVERIFY(!line.contains(QLatin1Char('\n')));
}

void ConnectionDiagnosisTests::connectTimeout_unsetIsTheDefaultOfTwoSeconds()
{
  QCOMPARE(kDefaultConnectTimeoutSeconds, 2.0);
  for (const auto &unset : {QVariant(), QVariant(QString())}) {
    const auto timeout = connectTimeoutFromSetting(unset);
    QCOMPARE(timeout.seconds, 2.0);
    QVERIFY(timeout.valid);
  }
}

void ConnectionDiagnosisTests::connectTimeout_acceptsOneToSixtySeconds_data()
{
  QTest::addColumn<QVariant>("raw");
  QTest::addColumn<double>("seconds");
  QTest::newRow("lower-bound") << QVariant(1.0) << 1.0;
  QTest::newRow("default-as-double") << QVariant(2.0) << 2.0;
  QTest::newRow("fraction") << QVariant(2.5) << 2.5;
  QTest::newRow("text-from-conf") << QVariant(QStringLiteral("10")) << 10.0;
  QTest::newRow("text-fraction") << QVariant(QStringLiteral("2.5")) << 2.5;
  QTest::newRow("int") << QVariant(30) << 30.0;
  QTest::newRow("upper-bound") << QVariant(60) << 60.0;
}

void ConnectionDiagnosisTests::connectTimeout_acceptsOneToSixtySeconds()
{
  QFETCH(QVariant, raw);
  QFETCH(double, seconds);

  const auto timeout = connectTimeoutFromSetting(raw);
  QVERIFY(timeout.valid);
  QCOMPARE(timeout.seconds, seconds);
}

void ConnectionDiagnosisTests::connectTimeout_invalidFallsBackToTheDefault_data()
{
  QTest::addColumn<QVariant>("raw");
  QTest::newRow("below-range") << QVariant(0.5);
  QTest::newRow("zero") << QVariant(0);
  QTest::newRow("negative") << QVariant(-3);
  QTest::newRow("above-range") << QVariant(60.5);
  QTest::newRow("huge") << QVariant(1000000);
  QTest::newRow("text") << QVariant(QStringLiteral("soon"));
  QTest::newRow("text-with-unit") << QVariant(QStringLiteral("5s"));
  QTest::newRow("nan") << QVariant(std::numeric_limits<double>::quiet_NaN());
  QTest::newRow("infinity") << QVariant(std::numeric_limits<double>::infinity());
  QTest::newRow("list-from-comma") << QVariant(QStringList{QStringLiteral("5"), QStringLiteral("6")});
}

void ConnectionDiagnosisTests::connectTimeout_invalidFallsBackToTheDefault()
{
  QFETCH(QVariant, raw);

  const auto timeout = connectTimeoutFromSetting(raw);
  QVERIFY(!timeout.valid);
  QCOMPARE(timeout.seconds, 2.0);
}

QTEST_MAIN(ConnectionDiagnosisTests)
