/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "QtTunnelProcessTests.h"

#include "gui/tunnel/CloudflaredLaunch.h"
#include "gui/tunnel/OrphanRecord.h"
#include "gui/tunnel/OutputLineSplitter.h"
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

  // Windows reports a start that failed inside start(), Unix a moment later: the signal may be here already, and wait()
  // only waits for one more
  QVERIFY(startFailed.count() > 0 || startFailed.wait(5000));
  // once (a later duplicate would be seen in the time that the event loop is given)
  QTest::qWait(100);
  QCOMPARE(startFailed.count(), 1);
  QVERIFY(!startFailed.first().at(0).toString().isEmpty());
  QCOMPARE(started.count(), 0);
  QVERIFY(!process.isRunning());
  QCOMPARE(process.processId(), qint64(0));
}

// Without a program QProcess reports the failure inside start() on every system (a start that fails in CreateProcess
// does the same on Windows). Whoever uses the process has to be ready for a failure that is told before start()
// returns.
void QtTunnelProcessTests::aStartThatFailsInsideStart_isReportedBeforeStartReturns()
{
  LaunchSpec spec = fakeSpec(QStringLiteral("ok"), 0);
  spec.program.clear();

  QtTunnelProcess process;
  QSignalSpy started(&process, &ITunnelProcess::started);
  QSignalSpy startFailed(&process, &ITunnelProcess::startFailed);
  process.start(spec);

  QCOMPARE(startFailed.count(), 1);
  QVERIFY(!startFailed.first().at(0).toString().isEmpty());
  QCOMPARE(started.count(), 0);
  QVERIFY(!process.isRunning());
  QTest::qWait(100);
  QCOMPARE(startFailed.count(), 1);
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

void QtTunnelProcessTests::aLongLineWithTheTokenAcrossTheLimit_isSkippedAndNoPartOfTheTokenIsGiven()
{
  Output output;
  auto process = run(fakeSpec(QStringLiteral("long"), 0), output);
  QTRY_VERIFY_WITH_TIMEOUT(output.contains(QStringLiteral("after the long line")), 15000);

  // the line of 200 KB is not given, and no part of the token that it holds is given: the mask looks for the whole
  // token
  const auto secret = kToken.secret;
  for (const auto &line : output.lines) {
    QVERIFY2(!line.contains(secret.left(12)), "the first part of the token is in a line");
    QVERIFY2(!line.contains(secret.right(12)), "the last part of the token is in a line");
    QVERIFY(line.size() < 64 * 1024);
  }
  QCOMPARE(
      static_cast<int>(std::ranges::count(output.lines, QString::fromLatin1(OutputLineSplitter::kTooLongNotice))), 1
  );

  QSignalSpy finished(process.get(), &ITunnelProcess::finished);
  process->kill();
  QVERIFY(finished.wait(10000));
}

void QtTunnelProcessTests::aTokenCutBetweenTwoWrites_arrivesWholeInOneLine()
{
  Output output;
  auto process = run(fakeSpec(QStringLiteral("split"), 0), output);
  QTRY_VERIFY_WITH_TIMEOUT(output.contains(QStringLiteral("end of the line")), 10000);

  const auto found = std::ranges::find_if(output.lines, [](const QString &line) {
    return line.contains(QStringLiteral("token cut in two"));
  });
  QVERIFY(found != output.lines.end());
  // one line, with the whole token: whoever masks it sees all of it
  QVERIFY2(found->contains(kToken.secret), qPrintable(*found));

  QSignalSpy finished(process.get(), &ITunnelProcess::finished);
  process->kill();
  QVERIFY(finished.wait(10000));
}

void QtTunnelProcessTests::carriageReturnAlone_endsALine()
{
  Output output;
  auto process = run(fakeSpec(QStringLiteral("cr"), 0), output);
  QTRY_VERIFY_WITH_TIMEOUT(output.contains(QStringLiteral("cr three")), 10000);

  QVERIFY(output.lines.contains(QStringLiteral("cr one")));
  QVERIFY(output.lines.contains(QStringLiteral("cr two")));
  QVERIFY(output.lines.contains(QStringLiteral("cr three")));

  QSignalSpy finished(process.get(), &ITunnelProcess::finished);
  process->kill();
  QVERIFY(finished.wait(10000));
}

void QtTunnelProcessTests::theLastLineWithoutAnEnd_isGivenWhenTheProcessEnds()
{
  Output output;
  auto process = run(fakeSpec(QStringLiteral("noend"), 0), output);
  QSignalSpy finished(process.get(), &ITunnelProcess::finished);
  QVERIFY(finished.wait(10000));
  QCOMPARE(finished.takeFirst().at(0).toInt(), 3);
  // it is given as it is (the mask is applied by whoever reads it, like for every line)
  QVERIFY(output.contains(QStringLiteral("ERR the last words, with the secret")));
}

QTEST_MAIN(QtTunnelProcessTests)
