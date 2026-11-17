/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/tunnel/OrphanRecord.h"

#include "common/Constants.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

#include <cstdlib>

namespace deskflow::tunnel {

namespace {

bool samePath(const QString &a, const QString &b)
{
#if defined(Q_OS_WIN)
  return QDir::fromNativeSeparators(a).compare(QDir::fromNativeSeparators(b), Qt::CaseInsensitive) == 0;
#else
  return a == b;
#endif
}

bool looksLikeCloudflared(const QString &path)
{
  return QFileInfo(path).completeBaseName().startsWith(QLatin1String("cloudflared"), Qt::CaseInsensitive);
}

// whether the process that was found is the process of the record: its start time, its program and its name
bool isTheRecordedProcess(const ProcessInfo &info, const OrphanRecord &record)
{
  return std::llabs(info.startTimeMs - record.startTimeMs) <= kStartTimeToleranceMs && !info.path.isEmpty() &&
         samePath(info.path, record.path) && looksLikeCloudflared(info.path);
}

// whether the process of the record is still there
bool stillThere(const IProcessInspector &inspector, const OrphanRecord &record)
{
  const auto info = inspector.inspect(record.pid);
  return info.has_value() && isTheRecordedProcess(*info, record);
}

bool waitUntilGone(const IProcessInspector &inspector, const OrphanRecord &record, int waitMs)
{
  constexpr int kStepMs = 50;
  for (int waited = 0; waited <= waitMs; waited += kStepMs) {
    if (!stillThere(inspector, record)) {
      return true;
    }
    QThread::msleep(kStepMs);
  }
  return !stillThere(inspector, record);
}

} // namespace

QString defaultOrphanRecordPath()
{
  const auto xdgStateHome = qEnvironmentVariable("XDG_STATE_HOME");
  const auto base = !xdgStateHome.isEmpty()
                        ? xdgStateHome
                        : QStandardPaths::standardLocations(QStandardPaths::GenericStateLocation).value(0);
  return QStringLiteral("%1/%2-tunnel.json").arg(base, QString::fromLatin1(kAppName));
}

std::optional<OrphanRecord> readOrphanRecord(const QString &file)
{
  QFile in(file);
  if (!in.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }

  const auto document = QJsonDocument::fromJson(in.readAll());
  if (!document.isObject()) {
    return std::nullopt;
  }

  const auto object = document.object();
  OrphanRecord record;
  record.pid = static_cast<qint64>(object.value(QStringLiteral("pid")).toDouble());
  record.startTimeMs = static_cast<qint64>(object.value(QStringLiteral("startTimeMs")).toDouble());
  record.path = object.value(QStringLiteral("path")).toString();
  record.hostname = object.value(QStringLiteral("hostname")).toString();
  record.port = object.value(QStringLiteral("port")).toInt();
  return record.isValid() ? std::optional<OrphanRecord>(record) : std::nullopt;
}

bool writeOrphanRecord(const QString &file, const OrphanRecord &record)
{
  if (!record.isValid() || file.isEmpty()) {
    return false;
  }

  QDir().mkpath(QFileInfo(file).absolutePath());
  QSaveFile out(file);
  if (!out.open(QIODevice::WriteOnly)) {
    return false;
  }

  QJsonObject object;
  object.insert(QStringLiteral("pid"), static_cast<double>(record.pid));
  object.insert(QStringLiteral("startTimeMs"), static_cast<double>(record.startTimeMs));
  object.insert(QStringLiteral("path"), record.path);
  object.insert(QStringLiteral("hostname"), record.hostname);
  object.insert(QStringLiteral("port"), record.port);
  out.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
  return out.commit();
}

void removeOrphanRecord(const QString &file)
{
  if (!file.isEmpty()) {
    QFile::remove(file);
  }
}

OrphanCleanup cleanUpOrphan(const QString &recordFile, IProcessInspector &inspector, int waitMs)
{
  OrphanCleanup result;
  const auto record = readOrphanRecord(recordFile);
  if (!record.has_value()) {
    // a file that is not a record is not left to confuse the next run
    removeOrphanRecord(recordFile);
    return result;
  }
  result.record = *record;

  // the process is judged and asked to go in one step (see IProcessInspector::terminateIf)
  const auto matches = [&record](const ProcessInfo &info) { return isTheRecordedProcess(info, *record); };
  const auto asked = inspector.terminateIf(record->pid, false, matches);
  if (asked == TerminateResult::NotRunning) {
    removeOrphanRecord(recordFile);
    result.outcome = OrphanOutcome::NotRunning;
    return result;
  }
  if (asked == TerminateResult::NotMatching) {
    removeOrphanRecord(recordFile);
    result.outcome = OrphanOutcome::Different;
    return result;
  }

  if (!waitUntilGone(inspector, *record, waitMs)) {
    inspector.terminateIf(record->pid, true, matches);
    waitUntilGone(inspector, *record, waitMs);
  }

  removeOrphanRecord(recordFile);
  result.outcome = stillThere(inspector, *record) ? OrphanOutcome::EndFailed : OrphanOutcome::Ended;
  return result;
}

} // namespace deskflow::tunnel
