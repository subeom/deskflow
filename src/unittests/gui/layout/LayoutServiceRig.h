/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "LayoutTestData.h"
#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/layout/DisplayProvider.h"
#include "gui/layout/LayoutMemoryService.h"
#include "gui/layout/ServerConfigLayout.h"

#include <QList>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

// A program in miniature for the tests of the windows: a configuration with the T and the computers around it, a service
// that remembers layouts, displays that the test changes, and a core that is only counted. Nothing real is read or changed.

namespace layout_rig {

using namespace layout_test;
using namespace deskflow::gui::layout;

inline const QStringList kNames{"srv", "PC1", "PC2", "MINI"};

inline void writeGrid()
{
  Settings::setValue(Settings::Core::ComputerName, QStringLiteral("srv"));
  auto &settings = Settings::proxy();
  settings.beginGroup(QStringLiteral("internalConfig"));
  settings.remove(QString());
  settings.beginWriteArray(QStringLiteral("screens"));
  for (int i = 0; i < 15; ++i) {
    settings.setArrayIndex(i);
    settings.setValue(
        QStringLiteral("name"), i == 6   ? QStringLiteral("PC1")
                                : i == 7 ? QStringLiteral("srv")
                                : i == 8 ? QStringLiteral("PC2")
                                : i == 9 ? QStringLiteral("MINI")
                                         : QString()
    );
  }
  settings.endArray();
  settings.endGroup();
}

inline void place(const QString &name, double x, double y)
{
  Settings::setValue(Settings::Computer::ArrangementX.arg(name), x);
  Settings::setValue(Settings::Computer::ArrangementY.arg(name), y);
  Settings::setValue(Settings::Computer::ArrangementWidth.arg(name), 1920);
  Settings::setValue(Settings::Computer::ArrangementHeight.arg(name), 1080);
}

// the user's T with the computers around it, the experiments on and the use of the arrangement on
inline void arrangeT()
{
  writeGrid();
  ServerConfig::overrideServerMonitors(sig(tShape()).monitors());
  place("srv", 0, 0);
  place("PC1", -1920, 0);
  place("PC2", 3440, 0);
  place("MINI", 964, 2422);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
}

inline void forgetEverything()
{
  for (const auto &name : kNames) {
    for (const auto &key :
         {Settings::Computer::ArrangementX, Settings::Computer::ArrangementY, Settings::Computer::ArrangementZoom,
          Settings::Computer::ArrangementWidth, Settings::Computer::ArrangementHeight,
          Settings::Computer::MonitorLayout, Settings::Computer::MonitorScales})
      Settings::setValue(key.arg(name));
  }
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  Settings::setValue(Settings::Server::UseArrangement);
  Settings::setValue(Settings::Server::RememberArrangements);
  ServerConfig::overrideServerMonitors(std::nullopt);
}

class FixedProvider : public IDisplayProvider
{
public:
  std::vector<DisplayInfo> shown = tShape();
  std::vector<DisplayInfo> displays() const override
  {
    return shown;
  }
};

struct ServiceRig
{
  Millis clock = 0;
  int restarts = 0;
  bool dialogOpen = false;
  QString runningText;
  FixedProvider *provider = nullptr;
  std::unique_ptr<ServerConfig> config = std::make_unique<ServerConfig>();
  std::unique_ptr<LayoutMemoryService> service;

  explicit ServiceRig(const QString &storePath)
  {
    runningText = generatedConfigText(*config);
    auto fixed = std::make_unique<FixedProvider>();
    provider = fixed.get();
    HostHooks hooks;
    hooks.serverMode = [] { return true; };
    hooks.coreRunning = [] { return true; };
    hooks.restartCore = [this] {
      ++restarts;
      runningText = generatedConfigText(*config);
    };
    hooks.dialogOpen = [this] { return dialogOpen; };
    hooks.runningConfigText = [this] { return runningText; };
    hooks.wallClockMs = [this] { return int64_t(1'700'000'000'000) + clock; };
    hooks.log = [](QtMsgType, const QString &) {};
    LayoutMemoryService::Options options;
    options.storePath = storePath;
    options.useTimers = false;
    options.clock = [this] { return clock; };
    service = std::make_unique<LayoutMemoryService>(*config, std::move(fixed), hooks, options);
  }

  void turnOn()
  {
    Settings::setValue(Settings::Server::RememberArrangements, true);
    service->settingsChanged();
  }

  void setDisplays(const std::vector<DisplayInfo> &displays)
  {
    provider->shown = displays;
    ServerConfig::overrideServerMonitors(sig(displays).monitors());
  }

  // the displays changed at \p time, and the time goes on until the memory looks
  void changeAt(Millis time)
  {
    clock = time;
    service->displaysMayHaveChanged();
    clock = time + kQuietMs;
    service->pump();
  }

  // the external monitor is unplugged: the laptop is alone and the computers are put against it
  void unplug(Millis time = 1000)
  {
    setDisplays(laptopOnly());
    changeAt(time);
  }
};

} // namespace layout_rig
