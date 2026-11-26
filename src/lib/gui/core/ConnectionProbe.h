/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/ConnectionDiagnosis.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace deskflow::gui {

/**
 * One address of the list in the client's "Connect to" field, split the way the core reads it: `host`, `host:port`,
 * `[ipv6]:port` or a bare IPv6 address (which has no port).
 */
struct ServerEntry
{
  QString text;    //!< as typed, trimmed
  QString host;    //!< without brackets
  int port = 0;    //!< the default port when the entry has none
  QString problem; //!< why the entry cannot be tested (a bad port); empty when it can
  bool isValid() const
  {
    return problem.isEmpty();
  }
};

//! Splits the comma separated list the client is given. Empty parts are skipped.
QList<ServerEntry> parseServerEntries(const QString &text, int defaultPort);

//! What a connect to one address of one entry came to
struct ProbeResult
{
  QString entry;   //!< the entry as typed
  QString host;    //!< the name or address of the entry, without brackets
  QString address; //!< the numeric address that was connected to; empty when the name did not resolve
  int port = 0;
  bool open = false;
  diagnosis::FailureKind kind = diagnosis::FailureKind::Unknown; //!< what went wrong, when it is not open
  QString detail;                                                //!< the system's own text
  QString problem;                                               //!< an entry that was not tested at all, and why
  qint64 elapsedMs = 0;
};

//! `QAbstractSocket::SocketError` (as int) of a failed connect to what it means
diagnosis::FailureKind failureKindFromSocketError(int socketError);

//! One TCP connect that is closed again at once: no byte of the Deskflow protocol is ever sent
class IProbeSocket : public QObject
{
  Q_OBJECT
public:
  using QObject::QObject;
  virtual void connectToHost(const QString &address, quint16 port) = 0;
  virtual void abort() = 0;

Q_SIGNALS:
  void connected();
  void failed(deskflow::diagnosis::FailureKind kind, const QString &detail);
};

/**
 * Tries to open a TCP connection to every address of a list of server entries, all at once, without blocking, and
 * says per address what came of it. A name is resolved first; every address it resolves to is tried, the way the
 * client tries them one after the other.
 */
class ConnectionProbe : public QObject
{
  Q_OBJECT
public:
  using ResolveDone = std::function<void(const QStringList &addresses, const QString &error)>;
  //! Resolves a name; calls \p done, at once or later, with the addresses or with the reason there are none
  using Resolver = std::function<void(const QString &host, ResolveDone done)>;
  //! Makes a socket; the probe owns it
  using SocketFactory = std::function<IProbeSocket *()>;

  static constexpr int kDefaultTimeoutMs = 3000;

  ConnectionProbe(Resolver resolver, SocketFactory socketFactory, int timeoutMs, QObject *parent = nullptr);
  ~ConnectionProbe() override;

  //! A probe that resolves with QHostInfo and connects with QTcpSocket
  static ConnectionProbe *createSystem(QObject *parent = nullptr, int timeoutMs = kDefaultTimeoutMs);

  //! Starts testing \p entries; a test that runs is dropped without a result
  void start(const QList<ServerEntry> &entries);
  //! Drops what is running; no result and no finished signal follow
  void cancel();
  bool isRunning() const;
  int timeoutMs() const;

Q_SIGNALS:
  //! One address is done
  void resultReady(const deskflow::gui::ProbeResult &result);
  //! Every address is done; the results are in the order of the entries, then of the addresses of a name
  void finished(const QList<deskflow::gui::ProbeResult> &results);

private:
  struct Run;

  void begin(const std::shared_ptr<Run> &run, int entryIndex, const ServerEntry &entry);
  void tryAddress(
      const std::shared_ptr<Run> &run, int entryIndex, int slot, const ServerEntry &entry, const QString &address
  );
  void complete(const std::shared_ptr<Run> &run, int entryIndex, int slot, ProbeResult result);

  Resolver m_resolver;
  SocketFactory m_socketFactory;
  int m_timeoutMs;
  std::shared_ptr<Run> m_run;
};

//! The result as a short phrase, for the one line summary: "open", "timed out", ...
QString outcomeWord(const ProbeResult &result);

/**
 * A line a person can paste into a message to somebody who looks after the network, such as
 * `Deskflow connection test 2026-11-16 22:40 (macOS 15): 10.0.0.5:24800 timed out after 3000 ms; ...`.
 * In English on purpose: it is read by somebody else.
 */
QString summaryLine(const QList<ProbeResult> &results, int timeoutMs, const QDateTime &when, const QString &system);

} // namespace deskflow::gui

Q_DECLARE_METATYPE(deskflow::gui::ProbeResult)
Q_DECLARE_METATYPE(QList<deskflow::gui::ProbeResult>)
