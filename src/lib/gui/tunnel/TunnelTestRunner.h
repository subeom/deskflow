/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tunnel/CloudflaredLocator.h"
#include "gui/tunnel/ISecretStore.h"
#include "gui/tunnel/TunnelTypes.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class QTimer;

namespace deskflow::tunnel {

class TunnelController;

enum class TestStage
{
  Cloudflared, //!< cloudflared is found
  Credentials, //!< a Service Token is stored (the Client ID is read; the Secret is not)
  Tls,         //!< Deskflow's TLS is on
  Hostname,    //!< the tunnel hostname is a host name
  Process,     //!< cloudflared runs
  LocalPort,   //!< its local port is open
  Connection   //!< a connection through the tunnel reaches the server
};

enum class StageState
{
  Pending,
  Running,
  Ok,
  Failed,
  Skipped //!< not tried because an earlier stage failed
};

//! The start of a Client ID, for the screen: the first characters (never more than half of it) and an ellipsis. The
//! Secret is never shown.
QString maskedClientId(const QString &clientId);

struct StageResult
{
  TestStage stage = TestStage::Cloudflared;
  StageState state = StageState::Pending;
  QString detail; //!< what was found, or what is wrong
};

struct TunnelTestReport
{
  QList<StageResult> stages;
  //! why the test failed (the first stage that did); None when it did not
  TunnelFailure failure = TunnelFailure::None;
  bool finished = false;
  QString hostname;
  int localPort = 0;

  bool ok() const
  {
    return finished && failure == TunnelFailure::None;
  }

  //! The stage as a name, in the language of the application
  static QString stageName(TestStage stage);
  //! A line to paste into a message to somebody who looks after the server: English on purpose, no token, no Client ID
  QString summaryLine(const QDateTime &when, const QString &system) const;
};

//! What came of one connection to the local port that is held for a while (nothing is written to it)
enum class HoldOutcome
{
  Held,         //!< connected and still open when the time was over
  ClosedByPeer, //!< connected, and cloudflared closed it before the time was over
  Refused       //!< could not be connected
};

/**
 * Tests the whole way that the client takes to its server through the tunnel, stage by stage, and says which stage is
 * the first that does not work: cloudflared, the Service Token, TLS, the hostname, the process, the local port, and a
 * connection through the tunnel (the connection is opened and held for a few seconds; cloudflared closes it, and says
 * so in its log, when the edge or the server refuses it).
 *
 * No Deskflow data is sent and no TLS handshake is made: what the test shows is that a TCP connection can be carried to
 * the server. That the server's certificate is the one that is trusted is shown by the real connection of the client.
 *
 * The tunnel is started for the test when it does not run (the controller is asked as the core is) and let go
 * afterwards; a tunnel that runs for the core is used as it is and left running.
 */
class TunnelTestRunner : public QObject
{
  Q_OBJECT

public:
  //! Opens a connection to 127.0.0.1:\p port, writes nothing, and tells what happened to it after \p holdMs at the
  //! latest
  using HoldConnection = std::function<void(int port, int holdMs, std::function<void(HoldOutcome)> done)>;

  struct Dependencies
  {
    TunnelController *controller = nullptr;
    std::shared_ptr<ISecretStore> store; //!< null: the secure store of the system
    //! null: the real file system and PATH
    std::function<CloudflaredLocation(const QString &configuredPath)> locate;
    HoldConnection holdConnection; //!< null: a QTcpSocket
    int holdMs = 3000;
    //! A tunnel that does not listen in this time is given up (the supervisor has its own, shorter time out)
    int startTimeoutMs = 20000;
    //! After the connection was held or closed, cloudflared is given this long to write why it failed
    int graceMs = 400;
  };

  explicit TunnelTestRunner(Dependencies dependencies, QObject *parent = nullptr);
  ~TunnelTestRunner() override;

  //! Starts the test; a test that runs is dropped
  void start();
  //! Drops a test that runs (no result follows); the tunnel that was started for it is let go
  void cancel();
  bool isRunning() const
  {
    return m_running;
  }
  const TunnelTestReport &report() const
  {
    return m_report;
  }

  //! The system's way to hold a connection
  static HoldConnection systemHoldConnection();

Q_SIGNALS:
  //! A stage changed; the report holds all of them
  void changed();
  void finished();

private:
  StageResult &stage(TestStage stage);
  void setStage(TestStage which, StageState state, const QString &detail = {});
  void checkPrerequisites();
  void waitForTheTunnel();
  void onListening();
  void onFailed(TunnelFailure failure);
  void holdAConnection();
  void judgeTheConnection(HoldOutcome outcome);
  void skipTheRest(TestStage after);
  void fail(TunnelFailure failure);
  void conclude();
  void disconnectTunnel();

  Dependencies m_deps;
  TunnelTestReport m_report;
  QTimer *m_timeout;
  bool m_running = false;
  bool m_startedTunnel = false;
  quint64 m_generation = 0;
  quint64 m_failuresBefore = 0;
  QMetaObject::Connection m_listeningConnection;
  QMetaObject::Connection m_failedConnection;
};

} // namespace deskflow::tunnel
