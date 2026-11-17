/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

// Fakes of what the supervisor touches, for the tests of the supervisor and of the controller: a process that does what
// the test tells it, ports that answer what the test tells them, and a system that knows the processes the test names.

#include "common/Settings.h"
#include "gui/tunnel/IPortProber.h"
#include "gui/tunnel/ITunnelProcess.h"
#include "gui/tunnel/InMemorySecretStore.h"
#include "gui/tunnel/ProcessInspector.h"
#include "gui/tunnel/TunnelController.h"

#include <QDir>
#include <QFile>
#include <QPointer>
#include <QString>

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace tunneltest {

using namespace deskflow::tunnel;

inline const QString kSecret = QStringLiteral("dummy-secret-value-0123456789");
inline const QString kClientId = QStringLiteral("dummy-client-id.access");
inline const QString kHost = QStringLiteral("mbp.example.com");
inline const QString kProgram = QStringLiteral("/opt/test/cloudflared");
inline constexpr int kPreferred = 24800;
inline constexpr int kFree = 51234;

class FakeProcess : public ITunnelProcess
{
public:
  LaunchSpec spec;
  int starts = 0;
  int terminateCalls = 0;
  int killCalls = 0;
  bool running = false;
  bool failToStart = false;
  bool obeysTerminate = true;
  bool obeysKill = true;
  qint64 pid = 5000;

  void start(const LaunchSpec &launchSpec) override
  {
    spec = launchSpec;
    ++starts;
    if (failToStart) {
      Q_EMIT startFailed(QStringLiteral("fake: cannot start"));
      return;
    }
    running = true;
    Q_EMIT started();
  }

  void terminate() override
  {
    ++terminateCalls;
    if (obeysTerminate) {
      end(-1);
    }
  }

  void kill() override
  {
    ++killCalls;
    if (obeysKill) {
      end(-1);
    }
  }

  bool isRunning() const override
  {
    return running;
  }

  qint64 processId() const override
  {
    return running ? pid : 0;
  }

  void end(int code)
  {
    if (running) {
      running = false;
      Q_EMIT finished(code);
    }
  }

  void write(const QString &line)
  {
    Q_EMIT outputLine(line);
  }
};

class FakeProber : public IPortProber
{
public:
  bool preferredFree = true;
  int freePort = kFree;
  std::vector<int> canListenAsked;
  std::vector<std::pair<int, std::function<void(bool)>>> probes;
  int probesAsked = 0;

  bool canListen(int port) override
  {
    canListenAsked.push_back(port);
    return preferredFree;
  }

  int findFreePort() override
  {
    return freePort;
  }

  void probeOpen(int port, int, std::function<void(bool)> done) override
  {
    ++probesAsked;
    probes.emplace_back(port, std::move(done));
  }

  // tells every probe that is waiting what it found
  void answer(bool open)
  {
    auto waiting = std::move(probes);
    probes.clear();
    for (auto &probe : waiting) {
      probe.second(open);
    }
  }
};

class FakeInspector : public IProcessInspector
{
public:
  std::map<qint64, ProcessInfo> processes;
  std::vector<std::pair<qint64, bool>> asked;

  std::optional<ProcessInfo> inspect(qint64 pid) const override
  {
    const auto found = processes.find(pid);
    return found == processes.end() ? std::nullopt : std::optional<ProcessInfo>(found->second);
  }

  bool terminate(qint64 pid, bool force) override
  {
    asked.emplace_back(pid, force);
    processes.erase(pid);
    return true;
  }
};

// A controller on a settings file of its own, with a supervisor made of fakes (the settings of every rig are a new file
// in
// \p base: never the ones of the user)
inline const QByteArray kConfig = "[core]\ncomputerName=test-pc\n\n[log]\nlevel=Info\n\n[security]\ntlsEnabled=true\n";

inline int g_rigCount = 0;

// A controller on a settings file of its own, with a supervisor made of fakes
struct ControllerRig
{
  QString file;
  std::shared_ptr<InMemorySecretStore::State> storeState = std::make_shared<InMemorySecretStore::State>();
  std::shared_ptr<FakeProber> prober = std::make_shared<FakeProber>();
  std::shared_ptr<FakeInspector> inspector = std::make_shared<FakeInspector>();
  std::vector<QPointer<FakeProcess>> processes;
  qint64 clock = 10'000;
  QString configuredPath;
  std::unique_ptr<TunnelController> controller;
  QStringList logs;

  explicit ControllerRig(const QString &base, const QByteArray &config = kConfig, int lingerMs = 80)
  {
    const auto dir = QStringLiteral("%1/rig%2").arg(base).arg(++g_rigCount);
    QDir().mkpath(dir);
    file = dir + QStringLiteral("/Deskflow.conf");
    QFile out(file);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      out.write(config);
    }
    out.close();
    Settings::setSettingsFile(file);
    Settings::setStateFile(dir + QStringLiteral("/Deskflow.state"));

    storeState->token = ServiceToken(kClientId, kSecret);

    TunnelSupervisor::Dependencies deps;
    deps.store = std::make_shared<InMemorySecretStore>(storeState);
    deps.locate = [this](const QString &path) {
      CloudflaredLocation location;
      location.source = CloudflaredLocation::Source::Configured;
      location.path = path.isEmpty() ? kProgram : path;
      configuredPath = path;
      return location;
    };
    deps.processFactory = [this](QObject *parent) -> ITunnelProcess * {
      auto *process = new FakeProcess;
      process->setParent(parent);
      process->pid = 6000 + static_cast<qint64>(processes.size());
      inspector->processes.emplace(process->pid, ProcessInfo{1, kProgram});
      processes.emplace_back(process);
      return process;
    };
    deps.prober = prober;
    deps.inspector = inspector;
    deps.clock = [this] { return clock; };
    deps.baseEnvironment = QProcessEnvironment();
    deps.tickIntervalMs = 0;

    TunnelController::Options options;
    options.lingerMs = lingerMs;
    controller = std::make_unique<TunnelController>(std::make_unique<TunnelSupervisor>(std::move(deps)), options);
    QObject::connect(controller.get(), &TunnelController::logLine, controller.get(), [this](const QString &line) {
      logs << line;
    });
  }

  void turnOn(const QString &hostname = QStringLiteral("mbp.example.com"))
  {
    Settings::setValue(Settings::Tunnel::Enabled, true);
    Settings::setValue(Settings::Tunnel::Hostname, hostname);
  }

  FakeProcess *process(int index = -1) const
  {
    const auto position = index < 0 ? static_cast<int>(processes.size()) + index : index;
    return position >= 0 && position < static_cast<int>(processes.size())
               ? processes.at(static_cast<size_t>(position)).data()
               : nullptr;
  }

  void advance(qint64 ms)
  {
    for (qint64 passed = 0; passed < ms; passed += 100) {
      clock += 100;
      controller->supervisor().tick();
    }
  }

  bool bringUp()
  {
    controller->acquire();
    advance(400);
    prober->answer(true);
    advance(400);
    return controller->status().state == TunnelState::Listening;
  }
};

} // namespace tunneltest
