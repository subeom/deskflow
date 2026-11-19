/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OrphanRecordTests.h"

#include "gui/tunnel/OrphanRecord.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <map>
#include <vector>

using namespace deskflow::tunnel;

namespace {

class FakeInspector : public IProcessInspector
{
public:
  std::map<qint64, ProcessInfo> processes;
  bool obeysRequest = true;
  bool obeysForce = true;
  std::vector<std::pair<qint64, bool>> asked;

  std::optional<ProcessInfo> inspect(qint64 pid) const override
  {
    const auto found = processes.find(pid);
    return found == processes.end() ? std::nullopt : std::optional<ProcessInfo>(found->second);
  }

  bool terminate(qint64 pid, bool force) override
  {
    asked.emplace_back(pid, force);
    if (force ? obeysForce : obeysRequest) {
      processes.erase(pid);
    }
    return true;
  }
};

// An inspector whose number passes to another program between the look and the end: inspect() still shows the process
// of the record, and the one step that judges and ends (terminateIf) meets the new owner of the number
class SwappingInspector : public IProcessInspector
{
public:
  ProcessInfo shown;
  ProcessInfo atTheEnd;
  int ended = 0;
  int judged = 0;

  std::optional<ProcessInfo> inspect(qint64) const override
  {
    return shown;
  }

  bool terminate(qint64, bool) override
  {
    ++ended;
    return true;
  }

  TerminateResult terminateIf(qint64, bool, const ProcessMatcher &matches) override
  {
    ++judged;
    if (!matches(atTheEnd)) {
      return TerminateResult::NotMatching;
    }
    ++ended;
    return TerminateResult::Done;
  }
};

constexpr qint64 kPid = 4242;
constexpr qint64 kStart = 1'700'000'000'000;
const QString kProgram = QStringLiteral("/opt/cloudflared/cloudflared");

OrphanRecord sampleRecord()
{
  OrphanRecord record;
  record.pid = kPid;
  record.startTimeMs = kStart;
  record.path = kProgram;
  record.hostname = QStringLiteral("mbp.example.com");
  record.port = 24800;
  return record;
}

QString fakeProgram()
{
  return QStringLiteral(FAKE_CLOUDFLARED_PATH);
}

// a long running stand-in of cloudflared that does nothing and ends by itself after a minute
std::unique_ptr<QProcess> startFake()
{
  auto process = std::make_unique<QProcess>();
  process->setProgram(fakeProgram());
  process->setArguments(
      {QStringLiteral("access"), QStringLiteral("tcp"), QStringLiteral("--hostname"),
       QStringLiteral("silent.example.com"), QStringLiteral("--url"), QStringLiteral("127.0.0.1:1")}
  );
  process->setProcessChannelMode(QProcess::ForwardedChannels);
  process->start();
  return process;
}

} // namespace

void OrphanRecordTests::aRecord_isWrittenAndReadBack()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("state/tunnel.json"));

  QVERIFY(writeOrphanRecord(file, sampleRecord()));
  const auto read = readOrphanRecord(file);
  QVERIFY(read.has_value());
  QCOMPARE(read->pid, kPid);
  QCOMPARE(read->startTimeMs, kStart);
  QCOMPARE(read->path, kProgram);
  QCOMPARE(read->hostname, QStringLiteral("mbp.example.com"));
  QCOMPARE(read->port, 24800);

  removeOrphanRecord(file);
  QVERIFY(!QFile::exists(file));
  QVERIFY(!readOrphanRecord(file).has_value());
}

void OrphanRecordTests::aFileThatIsNotARecord_isNotARecord()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));

  for (const auto &content :
       {QByteArray(), QByteArray("not json"), QByteArray("[1,2]"), QByteArray("{}"),
        QByteArray("{\"pid\":1,\"startTimeMs\":5,\"path\":\"x\"}"),
        QByteArray("{\"pid\":77,\"startTimeMs\":0,\"path\":\"x\"}")}) {
    QFile out(file);
    QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
    out.write(content);
    out.close();
    QVERIFY2(!readOrphanRecord(file).has_value(), content.constData());
  }
}

void OrphanRecordTests::aRecordWithoutAProcess_isNotWritten()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));

  OrphanRecord record = sampleRecord();
  record.pid = 0;
  QVERIFY(!writeOrphanRecord(file, record));
  record = sampleRecord();
  record.path.clear();
  QVERIFY(!writeOrphanRecord(file, record));
  QVERIFY(!QFile::exists(file));
  QVERIFY(!writeOrphanRecord(QString(), sampleRecord()));
}

void OrphanRecordTests::withoutARecord_nothingIsDone()
{
  QTemporaryDir dir;
  FakeInspector inspector;
  const auto result = cleanUpOrphan(dir.filePath(QStringLiteral("none.json")), inspector, 100);
  QCOMPARE(result.outcome, OrphanOutcome::NoRecord);
  QVERIFY(inspector.asked.empty());
}

void OrphanRecordTests::aRecordOfAProcessThatIsGone_isRemovedAndNothingIsEnded()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));
  QVERIFY(writeOrphanRecord(file, sampleRecord()));

  FakeInspector inspector;
  const auto result = cleanUpOrphan(file, inspector, 100);
  QCOMPARE(result.outcome, OrphanOutcome::NotRunning);
  QCOMPARE(result.record.pid, kPid);
  QVERIFY(inspector.asked.empty());
  QVERIFY(!QFile::exists(file));
}

void OrphanRecordTests::aProcessWithAnotherStartTime_isLeftAlone()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));
  QVERIFY(writeOrphanRecord(file, sampleRecord()));

  // the number was given to another process after the old one ended
  FakeInspector inspector;
  inspector.processes[kPid] = {kStart + 3 * 3600 * 1000, kProgram};
  const auto result = cleanUpOrphan(file, inspector, 100);
  QCOMPARE(result.outcome, OrphanOutcome::Different);
  QVERIFY(inspector.asked.empty());
  QVERIFY(inspector.processes.contains(kPid));
}

void OrphanRecordTests::aProcessWithAnotherProgram_isLeftAlone()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));
  QVERIFY(writeOrphanRecord(file, sampleRecord()));

  FakeInspector inspector;
  inspector.processes[kPid] = {kStart, QStringLiteral("/opt/other/cloudflared")};
  QCOMPARE(cleanUpOrphan(file, inspector, 100).outcome, OrphanOutcome::Different);
  QVERIFY(inspector.asked.empty());

  // a system that does not tell the program is not trusted to be the same
  QVERIFY(writeOrphanRecord(file, sampleRecord()));
  inspector.processes[kPid] = {kStart, QString()};
  QCOMPARE(cleanUpOrphan(file, inspector, 100).outcome, OrphanOutcome::Different);
  QVERIFY(inspector.asked.empty());
}

void OrphanRecordTests::aProcessThatIsNotCloudflared_isLeftAloneEvenWhenTheRecordSaysSo()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));

  // a record that was edited by hand, or written by another program, can name any program
  auto record = sampleRecord();
  record.path = QStringLiteral("/usr/bin/ssh");
  QVERIFY(writeOrphanRecord(file, record));

  FakeInspector inspector;
  inspector.processes[kPid] = {kStart, record.path};
  QCOMPARE(cleanUpOrphan(file, inspector, 100).outcome, OrphanOutcome::Different);
  QVERIFY(inspector.asked.empty());
  QVERIFY(inspector.processes.contains(kPid));
}

void OrphanRecordTests::aStartTimeWithinTheTolerance_isTheSameProcess()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));

  for (const qint64 offset : {-kStartTimeToleranceMs, qint64(-1), qint64(0), qint64(1), kStartTimeToleranceMs}) {
    QVERIFY(writeOrphanRecord(file, sampleRecord()));
    FakeInspector inspector;
    inspector.processes[kPid] = {kStart + offset, kProgram};
    QCOMPARE(cleanUpOrphan(file, inspector, 100).outcome, OrphanOutcome::Ended);
  }

  QVERIFY(writeOrphanRecord(file, sampleRecord()));
  FakeInspector inspector;
  inspector.processes[kPid] = {kStart + kStartTimeToleranceMs + 1, kProgram};
  QCOMPARE(cleanUpOrphan(file, inspector, 100).outcome, OrphanOutcome::Different);
}

// Where the system tells the start time of a process in exact steps and the same at every read (Windows, macOS), the
// tolerance is small; only where it is derived from a boot time that can move (Linux) is it large
void OrphanRecordTests::theToleranceIsSmallWhereTheSystemIsExact()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
  QVERIFY(kStartTimeToleranceMs <= 100);
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));
  QVERIFY(writeOrphanRecord(file, sampleRecord()));
  FakeInspector inspector;
  inspector.processes[kPid] = {kStart + 1000, kProgram};
  QCOMPARE(cleanUpOrphan(file, inspector, 100).outcome, OrphanOutcome::Different);
  QVERIFY(inspector.asked.empty());
#else
  QVERIFY(kStartTimeToleranceMs >= 1000);
#endif
}

// the process is judged and ended in one step, so that the process that was judged is the process that is ended
void OrphanRecordTests::theProcessThatIsJudged_isTheProcessThatIsEnded()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));
  QVERIFY(writeOrphanRecord(file, sampleRecord()));

  SwappingInspector inspector;
  inspector.shown = {kStart, kProgram};
  inspector.atTheEnd = {kStart + 60'000, QStringLiteral("/usr/bin/other")};
  QCOMPARE(cleanUpOrphan(file, inspector, 100).outcome, OrphanOutcome::Different);
  QVERIFY(inspector.judged > 0);
  QCOMPARE(inspector.ended, 0);
  QVERIFY(!QFile::exists(file));

  // the same program, but not the same process: the start time decides
  QVERIFY(writeOrphanRecord(file, sampleRecord()));
  inspector.atTheEnd = {kStart + 60'000, kProgram};
  QCOMPARE(cleanUpOrphan(file, inspector, 100).outcome, OrphanOutcome::Different);
  QCOMPARE(inspector.ended, 0);
}

void OrphanRecordTests::theDefaultOfTerminateIf_looksFirstAndEndsOnlyWhatMatches()
{
  FakeInspector inspector;
  const auto always = [](const ProcessInfo &) { return true; };
  const auto never = [](const ProcessInfo &) { return false; };

  QCOMPARE(inspector.terminateIf(kPid, false, always), TerminateResult::NotRunning);
  QVERIFY(inspector.asked.empty());

  inspector.processes[kPid] = {kStart, kProgram};
  QCOMPARE(inspector.terminateIf(kPid, false, never), TerminateResult::NotMatching);
  QVERIFY(inspector.asked.empty());
  QVERIFY(inspector.processes.contains(kPid));

  ProcessInfo seen;
  const auto remember = [&seen](const ProcessInfo &info) {
    seen = info;
    return true;
  };
  QCOMPARE(inspector.terminateIf(kPid, true, remember), TerminateResult::Done);
  QCOMPARE(seen.startTimeMs, kStart);
  QCOMPARE(seen.path, kProgram);
  QCOMPARE(inspector.asked.size(), size_t(1));
  QCOMPARE(inspector.asked.front().second, true);
}

void OrphanRecordTests::theSameProcess_isAskedToGoFirstAndEndedOnlyWhenItStays()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));

  QVERIFY(writeOrphanRecord(file, sampleRecord()));
  FakeInspector polite;
  polite.processes[kPid] = {kStart, kProgram};
  QCOMPARE(cleanUpOrphan(file, polite, 100).outcome, OrphanOutcome::Ended);
  QCOMPARE(polite.asked.size(), size_t(1));
  QCOMPARE(polite.asked.front().second, false);

  QVERIFY(writeOrphanRecord(file, sampleRecord()));
  FakeInspector stubborn;
  stubborn.obeysRequest = false;
  stubborn.processes[kPid] = {kStart, kProgram};
  QCOMPARE(cleanUpOrphan(file, stubborn, 100).outcome, OrphanOutcome::Ended);
  QCOMPARE(stubborn.asked.size(), size_t(2));
  QCOMPARE(stubborn.asked.at(0).second, false);
  QCOMPARE(stubborn.asked.at(1).second, true);
  QCOMPARE(stubborn.asked.at(1).first, kPid);
}

void OrphanRecordTests::aProcessThatCannotBeEnded_isReported()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));
  QVERIFY(writeOrphanRecord(file, sampleRecord()));

  FakeInspector inspector;
  inspector.obeysRequest = false;
  inspector.obeysForce = false;
  inspector.processes[kPid] = {kStart, kProgram};
  QCOMPARE(cleanUpOrphan(file, inspector, 100).outcome, OrphanOutcome::EndFailed);
}

void OrphanRecordTests::theRecordIsRemovedInEveryCase()
{
  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));

  struct Case
  {
    std::optional<ProcessInfo> process;
    bool obeys;
  };
  const std::vector<Case> cases = {
      {std::nullopt, true},
      {ProcessInfo{kStart, kProgram}, true},
      {ProcessInfo{kStart, kProgram}, false},
      {ProcessInfo{kStart + 99999, kProgram}, true},
  };
  for (const auto &testCase : cases) {
    QVERIFY(writeOrphanRecord(file, sampleRecord()));
    FakeInspector inspector;
    inspector.obeysRequest = testCase.obeys;
    inspector.obeysForce = testCase.obeys;
    if (testCase.process) {
      inspector.processes[kPid] = *testCase.process;
    }
    cleanUpOrphan(file, inspector, 100);
    QVERIFY(!QFile::exists(file));
  }
}

void OrphanRecordTests::theInspector_knowsTheProcessOfThisProgram()
{
  const auto inspector = createSystemProcessInspector();
  const auto info = inspector->inspect(QCoreApplication::applicationPid());
  QVERIFY(info.has_value());

  const auto now = QDateTime::currentMSecsSinceEpoch();
  QVERIFY(info->startTimeMs > 0);
  QVERIFY(info->startTimeMs <= now + 2000);
  QVERIFY(info->startTimeMs > now - 24LL * 3600 * 1000);
  QCOMPARE(
      QFileInfo(info->path).canonicalFilePath(), QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath()
  );
}

void OrphanRecordTests::theInspector_doesNotKnowAProcessThatIsGone()
{
  const auto inspector = createSystemProcessInspector();
  QVERIFY(!inspector->inspect(0).has_value());
  QVERIFY(!inspector->inspect(-5).has_value());

  QProcess process;
  process.setProgram(fakeProgram());
  process.setArguments({QStringLiteral("--hostname"), QStringLiteral("exit.example.com")});
  process.start();
  QVERIFY(process.waitForStarted(5000));
  const auto pid = static_cast<qint64>(process.processId());
  QVERIFY(process.waitForFinished(10000));
  QVERIFY(!inspector->inspect(pid).has_value());
}

void OrphanRecordTests::theInspector_refusesToEndThisProgramAndTheSystemProcesses()
{
  const auto inspector = createSystemProcessInspector();
  QVERIFY(!inspector->terminate(QCoreApplication::applicationPid(), false));
  QVERIFY(!inspector->terminate(QCoreApplication::applicationPid(), true));
  QVERIFY(!inspector->terminate(0, true));
  QVERIFY(!inspector->terminate(1, true));
  QVERIFY(!inspector->terminate(-1, true));
}

void OrphanRecordTests::theInspector_endsOnlyAProcessThatTheMatcherAccepts()
{
  auto process = startFake();
  QVERIFY(process->waitForStarted(5000));
  const auto inspector = createSystemProcessInspector();
  const auto pid = static_cast<qint64>(process->processId());
  QTRY_VERIFY_WITH_TIMEOUT(inspector->inspect(pid).has_value(), 5000);
  const auto info = inspector->inspect(pid);
  QVERIFY(info.has_value());

  QCOMPARE(inspector->terminateIf(pid, true, [](const ProcessInfo &) { return false; }), TerminateResult::NotMatching);
  QCOMPARE(process->state(), QProcess::Running);

  ProcessInfo judged;
  const auto result = inspector->terminateIf(pid, true, [&judged](const ProcessInfo &seen) {
    judged = seen;
    return true;
  });
  QCOMPARE(result, TerminateResult::Done);
  QCOMPARE(judged.startTimeMs, info->startTimeMs);
  QCOMPARE(judged.path, info->path);
  QVERIFY(process->waitForFinished(5000));

  QCOMPARE(inspector->terminateIf(pid, true, [](const ProcessInfo &) { return true; }), TerminateResult::NotRunning);
}

void OrphanRecordTests::theInspector_neverEndsThisProgramThroughTerminateIf()
{
  const auto inspector = createSystemProcessInspector();
  const auto always = [](const ProcessInfo &) { return true; };
  QVERIFY(inspector->terminateIf(QCoreApplication::applicationPid(), true, always) != TerminateResult::Done);
  QVERIFY(inspector->terminateIf(0, true, always) != TerminateResult::Done);
  QVERIFY(inspector->terminateIf(1, true, always) != TerminateResult::Done);
  QVERIFY(inspector->terminateIf(-1, true, always) != TerminateResult::Done);
}

void OrphanRecordTests::aRealLeftoverProcess_isEnded()
{
  auto process = startFake();
  QVERIFY(process->waitForStarted(5000));
  const auto inspector = createSystemProcessInspector();
  const auto pid = static_cast<qint64>(process->processId());

  QTRY_VERIFY_WITH_TIMEOUT(inspector->inspect(pid).has_value(), 5000);
  const auto info = inspector->inspect(pid);
  QVERIFY(info.has_value());

  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));
  OrphanRecord record;
  record.pid = pid;
  record.startTimeMs = info->startTimeMs;
  record.path = info->path;
  QVERIFY(writeOrphanRecord(file, record));

  const auto result = cleanUpOrphan(file, *inspector, 3000);
  QCOMPARE(result.outcome, OrphanOutcome::Ended);
  QVERIFY(process->waitForFinished(5000));
  QVERIFY(!QFile::exists(file));
}

void OrphanRecordTests::aRealProcessWithAnotherStartTime_isNotTouched()
{
  auto process = startFake();
  QVERIFY(process->waitForStarted(5000));
  const auto inspector = createSystemProcessInspector();
  const auto pid = static_cast<qint64>(process->processId());

  QTRY_VERIFY_WITH_TIMEOUT(inspector->inspect(pid).has_value(), 5000);
  const auto info = inspector->inspect(pid);
  QVERIFY(info.has_value());

  QTemporaryDir dir;
  const auto file = dir.filePath(QStringLiteral("tunnel.json"));
  OrphanRecord record;
  record.pid = pid;
  record.startTimeMs = info->startTimeMs - 3600 * 1000;
  record.path = info->path;
  QVERIFY(writeOrphanRecord(file, record));

  QCOMPARE(cleanUpOrphan(file, *inspector, 300).outcome, OrphanOutcome::Different);
  QCOMPARE(process->state(), QProcess::Running);
  QVERIFY(inspector->inspect(pid).has_value());

  process->kill();
  QVERIFY(process->waitForFinished(5000));
}

QTEST_MAIN(OrphanRecordTests)
