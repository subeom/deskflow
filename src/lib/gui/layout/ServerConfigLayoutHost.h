/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/config/ServerConfig.h"
#include "gui/layout/DisplayProvider.h"
#include "gui/layout/LayoutMemory.h"

#include <QtGlobal>

#include <functional>
#include <memory>

namespace deskflow::gui::layout {

//! What the host needs of the program around it; every one has a default that works without a program (for tests)
struct HostHooks
{
  //! The program is in server mode (default: yes)
  std::function<bool()> serverMode;
  //! The core process runs (default: no)
  std::function<bool()> coreRunning;
  //! Restarts the core process (default: nothing)
  std::function<void()> restartCore;
  //! A dialog is open (default: a modal widget of the application is)
  std::function<bool()> dialogOpen;
  //! The user is told (default: nothing)
  std::function<void(const Notice &)> notify;
  //! The text of the configuration that the core was started with (default: its file)
  std::function<QString()> runningConfigText;
  //! Milliseconds since the epoch (default: the clock of the computer)
  std::function<int64_t()> wallClockMs;
  //! A line of the log (default: qDebug, qInfo and qWarning)
  std::function<void(QtMsgType, const QString &)> log;
};

//! The host of the layout memory in the GUI: the arrangement of the settings, the displays of this computer, the core
/*!
Nothing here is new behaviour of its own: it reads and writes through \c ServerConfig, the settings, and the hooks.
*/
class ServerConfigLayoutHost : public ILayoutHost
{
public:
  ServerConfigLayoutHost(ServerConfig &config, std::unique_ptr<IDisplayProvider> displays, HostHooks hooks);

  //! Whether the settings and the mode of the program ask for the feature: it is on, with both experiments, with the
  //! configuration of the GUI, in server mode
  bool enabled() const override;
  bool canApplyNow() const override;
  std::vector<DisplayInfo> displays() override;
  Layout currentLayout() const override;
  ApplyOutcome applyLayout(const Layout &layout) override;
  ReattachOutcome reattach(const DisplaySignature &from, const DisplaySignature &to) override;
  bool coreRunning() const override;
  bool coreConfigDiffers() const override;
  void restartCore() override;
  int64_t wallClockMs() const override;
  void debug(const QString &line) override;
  void info(const QString &line) override;
  void warn(const QString &line) override;
  void notify(const Notice &notice) override;

private:
  ServerConfig &m_config;
  std::unique_ptr<IDisplayProvider> m_displays;
  HostHooks m_hooks;
};

} // namespace deskflow::gui::layout
