/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "QtTunnelProcessTests.h"

#include "gui/tunnel/CloudflaredLaunch.h"
#include "gui/tunnel/OrphanRecord.h"
#include "gui/tunnel/QtPortProber.h"
#include "gui/tunnel/QtTunnelProcess.h"

#include <QProcessEnvironment>
#include <QSignalSpy>

#include <memory>

using namespace deskflow::tunnel;

namespace {

// a made up token: the fake reports whether it received it and never the value
const ServiceToken kToken(QStringLiteral("dummy-client-id.access"), QStringLiteral("dummy-secret-value-0123456789"));

LaunchSpec
fakeSpec(const QString &mode, int port, const QProcessEnvironment &base = QProcessEnvironment::systemEnvironment())
{
  LaunchRequest request;
  request.program = QStringLiteral(FAKE_CLOUDFLARED_PATH);
  request.hostname = QStringLiteral("%1.example.com").arg(mode);
  // the request needs a port that is valid, also for a mode of the fake that never listens on it
  request.localPort = port > 0 ? port : QtPortProber().findFreePort();
  const auto spec = buildLaunchSpec(request, kToken, base);
  return spec.value_or(LaunchSpec());
}

struct Output
{
  QStringList lines;

  bool contains(const QString &part) const
  {
    return std::ranges::any_of(lines, [&](const QString &line) { return line.contains(part); });
  }
};

std::unique_ptr<QtTunnelProcess> run(const LaunchSpec &spec, Output &output)
{
  auto process = std::make_unique<QtTunnelProcess>();
  QObject::connect(process.get(), &ITunnelProcess::outputLine, process.get(), [&output](const QString &line) {
    output.lines << line;
  });
  process->start(spec);
  return process;
}

} // namespace

void QtTunnelProcessTests::theTokenIsInTheEnvironmentOnly_andNoOtherTunnelVariableIsPassed()
{
  auto base = QProcessEnvironment::systemEnvironment();
  // variables of the environment of the user that would change what `access tcp` does
  base.insert(QStringLiteral("TUNNEL_SERVICE_HOSTNAME"), QStringLiteral("inherited.example.com"));
  base.insert(QStringLiteral("TUNNEL_SERVICE_URL"), QStringLiteral("127.0.0.1:1"));
  base.insert(QStringLiteral("TUNNEL_LOG_OUTPUT"), QStringLiteral("inherited.log"));

  Output output;
  const auto spec = fakeSpec(QStringLiteral("exit"), 0, base);
  QVERIFY(!spec.program.isEmpty());
  auto process = run(spec, output);
  QSignalSpy finished(process.get(), &ITunnelProcess::finished);
  QVERIFY(finished.wait(10000));

  QVERIFY2(
      output.contains(QStringLiteral("tokenId=1 tokenSecret=1 tokenInArguments=0 strayTunnelVariables=0")),
      qPrintable(output.lines.join(QLatin1Char('\n')))
  );
  // what was written never holds the secret (only the leak mode of the fake does, on purpose)
  QVERIFY(!output.contains(kToken.secret));
}

void QtTunnelProcessTests::theOutputComesAsLines()
{
  QtPortProber prober;
  Output output;
  auto process = run(fakeSpec(QStringLiteral("ok"), prober.findFreePort()), output);

  QTRY_VERIFY_WITH_TIMEOUT(output.contains(QStringLiteral("Start Websocket listener")), 10000);
  QVERIFY(process->isRunning());
  QVERIFY(process->processId() > 0);
  for (const auto &line : output.lines) {
    QVERIFY(!line.contains(QLatin1Char('\n')));
    QVERIFY(!line.isEmpty());
    QCOMPARE(line, line.trimmed());
  }

  QSignalSpy finished(process.get(), &ITunnelProcess::finished);
  process->kill();
  QVERIFY(finished.wait(10000));
  QVERIFY(!process->isRunning());
  QCOMPARE(process->processId(), qint64(0));
}

void QtTunnelProcessTests::aProgramThatIsNotThere_reportsItAndNeverStarts()
{
  LaunchSpec spec = fakeSpec(QStringLiteral("ok"), 0);
  spec.program = QStringLiteral("/nonexistent/directory/cloudflared");

  QtTunnelProcess process;
  QSignalSpy started(&process, &ITunnelProcess::started);
  QSignalSpy startFailed(&process, &ITunnelProcess::startFailed);
  process.start(spec);

  QVERIFY(startFailed.wait(5000));
  QCOMPARE(started.count(), 0);
  QVERIFY(!process.isRunning());
}

void QtTunnelProcessTests::aProcessThatStopsOnItsOwn_reportsItsExitCode()
{
  Output output;
  auto process = run(fakeSpec(QStringLiteral("exit"), 0), output);
  QSignalSpy finished(process.get(), &ITunnelProcess::finished);
  QVERIFY(finished.wait(10000));
  QCOMPARE(finished.takeFirst().at(0).toInt(), 3);
  // the last line of the process is not lost when it stops at once
  QVERIFY(output.contains(QStringLiteral("the connection to the edge failed")));
}

void QtTunnelProcessTests::terminate_endsTheProcess()
{
  Output output;
  auto process = run(fakeSpec(QStringLiteral("silent"), 0), output);
  QTRY_VERIFY_WITH_TIMEOUT(output.contains(QStringLiteral("fake:")), 10000);

  QSignalSpy finished(process.get(), &ITunnelProcess::finished);
  process->terminate();
  QVERIFY(finished.wait(10000));
  QVERIFY(!process->isRunning());
}

void QtTunnelProcessTests::kill_endsTheProcess()
{
  Output output;
  auto process = run(fakeSpec(QStringLiteral("silent"), 0), output);
  QTRY_VERIFY_WITH_TIMEOUT(output.contains(QStringLiteral("fake:")), 10000);

  QSignalSpy finished(process.get(), &ITunnelProcess::finished);
  process->kill();
  QVERIFY(finished.wait(10000));
  QCOMPARE(finished.takeFirst().at(0).toInt(), -1);
}

void QtTunnelProcessTests::aProcessThatIsStillThereWhenTheObjectGoes_isEnded()
{
  Output output;
  auto process = run(fakeSpec(QStringLiteral("silent"), 0), output);
  QTRY_VERIFY_WITH_TIMEOUT(output.contains(QStringLiteral("fake:")), 10000);
  const auto pid = process->processId();
  QVERIFY(pid > 0);

  const auto inspector = createSystemProcessInspector();
  QVERIFY(inspector->inspect(pid).has_value());

  process.reset();
  QTRY_VERIFY_WITH_TIMEOUT(!inspector->inspect(pid).has_value(), 5000);
}

QTEST_MAIN(QtTunnelProcessTests)
