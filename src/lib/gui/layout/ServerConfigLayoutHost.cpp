/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/ServerConfigLayoutHost.h"

#include "common/Settings.h"
#include "gui/layout/ServerConfigLayout.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>

namespace deskflow::gui::layout {

namespace {

QString readRunningConfig()
{
  QFile file(Settings::serverConfigFile());
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};
  return QString::fromUtf8(file.readAll());
}

} // namespace

ServerConfigLayoutHost::ServerConfigLayoutHost(
    ServerConfig &config, std::unique_ptr<IDisplayProvider> displays, HostHooks hooks
)
    : m_config(config),
      m_displays(std::move(displays)),
      m_hooks(std::move(hooks))
{
  if (!m_hooks.serverMode)
    m_hooks.serverMode = [] { return true; };
  if (!m_hooks.coreRunning)
    m_hooks.coreRunning = [] { return false; };
  if (!m_hooks.restartCore)
    m_hooks.restartCore = [] {};
  if (!m_hooks.dialogOpen)
    m_hooks.dialogOpen = [] { return QApplication::activeModalWidget() != nullptr; };
  if (!m_hooks.notify)
    m_hooks.notify = [](const Notice &) {};
  if (!m_hooks.runningConfigText)
    m_hooks.runningConfigText = readRunningConfig;
  if (!m_hooks.wallClockMs)
    m_hooks.wallClockMs = [] { return static_cast<int64_t>(QDateTime::currentMSecsSinceEpoch()); };
  if (!m_hooks.log) {
    m_hooks.log = [](QtMsgType type, const QString &line) {
      switch (type) {
      case QtWarningMsg:
        qWarning().noquote() << line;
        break;
      case QtInfoMsg:
        qInfo().noquote() << line;
        break;
      default:
        qDebug().noquote() << line;
        break;
      }
    };
  }
}

bool ServerConfigLayoutHost::enabled() const
{
  // The switch is only for something that the arrangement makes: the experiment and its use are needed. A configuration
  // file of the user's own is not made from the arrangement, and only a server has an arrangement.
  return Settings::value(Settings::Server::RememberArrangements).toBool() &&
         Settings::value(Settings::Server::ExperimentalMultiMonitor).toBool() &&
         Settings::value(Settings::Server::UseArrangement).toBool() &&
         !Settings::value(Settings::Server::ExternalConfig).toBool() && m_hooks.serverMode();
}

bool ServerConfigLayoutHost::canApplyNow() const
{
  return !m_hooks.dialogOpen();
}

std::vector<DisplayInfo> ServerConfigLayoutHost::displays()
{
  return m_displays ? m_displays->displays() : std::vector<DisplayInfo>{};
}

Layout ServerConfigLayoutHost::currentLayout() const
{
  return captureLayout(m_config);
}

ApplyOutcome ServerConfigLayoutHost::applyLayout(const Layout &layout)
{
  auto outcome = deskflow::gui::layout::applyLayout(m_config, layout);
  // the settings are written when something was changed, and only then
  if (outcome.applied && outcome.changed)
    m_config.commit();
  return outcome;
}

ReattachOutcome ServerConfigLayoutHost::reattach(const DisplaySignature &from, const DisplaySignature &to)
{
  return reattachLayout(m_config, from, to);
}

bool ServerConfigLayoutHost::coreRunning() const
{
  return m_hooks.coreRunning();
}

bool ServerConfigLayoutHost::coreConfigDiffers() const
{
  const auto running = m_hooks.runningConfigText();
  // a configuration that cannot be read is not known to be the one that is wanted
  if (running.isEmpty())
    return true;
  return running != generatedConfigText(m_config);
}

void ServerConfigLayoutHost::restartCore()
{
  m_hooks.restartCore();
}

int64_t ServerConfigLayoutHost::wallClockMs() const
{
  return m_hooks.wallClockMs();
}

void ServerConfigLayoutHost::debug(const QString &line)
{
  m_hooks.log(QtDebugMsg, line);
}

void ServerConfigLayoutHost::info(const QString &line)
{
  m_hooks.log(QtInfoMsg, line);
}

void ServerConfigLayoutHost::warn(const QString &line)
{
  m_hooks.log(QtWarningMsg, line);
}

void ServerConfigLayoutHost::notify(const Notice &notice)
{
  m_hooks.notify(notice);
}

} // namespace deskflow::gui::layout
