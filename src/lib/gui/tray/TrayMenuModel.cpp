/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayMenuModel.h"

#include "common/PlatformInfo.h"

#include <algorithm>

namespace deskflow::gui::tray {

using core::ConnectionState;
using core::ProcessState;
using Kind = TrayMenuEntry::Kind;

namespace {

using Group = QList<TrayMenuEntry>;

bool isRunning(ProcessState process)
{
  return process == ProcessState::Starting || process == ProcessState::Started ||
         process == ProcessState::RetryPending;
}

QString commandId(TrayCommand command)
{
  switch (command) {
  case TrayCommand::Start:
    return QStringLiteral("command:start");
  case TrayCommand::Restart:
    return QStringLiteral("command:restart");
  case TrayCommand::Stop:
    return QStringLiteral("command:stop");
  case TrayCommand::TestConnection:
    return QStringLiteral("command:test-connection");
  case TrayCommand::OpenWindow:
    return QStringLiteral("command:open");
  case TrayCommand::Minimize:
    return QStringLiteral("command:minimize");
  case TrayCommand::Preferences:
    return QStringLiteral("command:preferences");
  case TrayCommand::OpenUpdate:
    return QStringLiteral("command:update");
  case TrayCommand::Quit:
    return QStringLiteral("command:quit");
  }
  return {};
}

TrayMenuEntry commandEntry(const TrayInput &input, TrayCommand command, const QString &icon)
{
  TrayMenuEntry entry;
  entry.kind = Kind::Command;
  entry.id = commandId(command);
  entry.command = command;
  entry.text = input.labels.value(command);
  entry.shortcut = input.shortcuts.value(command);
  entry.icon = icon;
  return entry;
}

// The row of a command that does not apply to the state is dimmed, or left out where that is what the OS does (true)
bool dimmedOrLeftOut(TrayMenuEntry &entry, const TrayPlatform &platform)
{
  entry.enabled = false;
  return platform.hideInapplicable;
}

TrayMenuEntry textEntry(const QString &id, const QString &text, const QString &icon)
{
  TrayMenuEntry entry;
  entry.kind = Kind::Text;
  entry.id = id;
  entry.text = TrayMenuModel::elided(text);
  if (entry.text != text)
    entry.toolTip = text;
  entry.enabled = false;
  entry.icon = icon;
  return entry;
}

// All the rows of a group have an icon, or none has one
void evenIcons(Group &group)
{
  const bool any = std::any_of(group.cbegin(), group.cend(), [](const auto &e) { return !e.icon.isEmpty(); });
  if (!any)
    return;
  for (auto &entry : group) {
    if (entry.icon.isEmpty())
      entry.icon = QStringLiteral("info");
  }
}

} // namespace

SharingMenuState sharingMenuState(const TrayInput &input, bool fingerprintAvailable)
{
  SharingMenuState state;
  const bool running = isRunning(input.process);
  state.start = !running && input.hasMode && input.canStart;
  state.restart = running;
  state.stop = running;
  state.testConnection = input.hasMode && !input.isServer && input.canTestConnection;
  state.fingerprint = fingerprintAvailable;
  state.minimize = input.windowShown;
  return state;
}

TrayPlatform TrayPlatform::windows()
{
  TrayPlatform platform;
  platform.hideInapplicable = true;
  platform.headerAsWidget = true;
  return platform;
}

TrayPlatform TrayPlatform::mac()
{
  TrayPlatform platform;
  platform.templateIcons = true;
  return platform;
}

TrayPlatform TrayPlatform::linux()
{
  TrayPlatform platform;
  platform.themeIcons = true;
  return platform;
}

TrayPlatform TrayPlatform::current()
{
  if (deskflow::platform::isWindows())
    return windows();
  if (deskflow::platform::isMac())
    return mac();
  return linux();
}

TrayMenuModel::TrayMenuModel(TrayPlatform platform) : m_platform(platform)
{
}

const TrayPlatform &TrayMenuModel::platform() const
{
  return m_platform;
}

void TrayMenuModel::setExternalRow(const QString &key, const QString &text)
{
  const auto found = std::find_if(m_external.begin(), m_external.end(), [&](const auto &r) { return r.key == key; });
  if (text.isEmpty()) {
    if (found != m_external.end())
      m_external.erase(found);
    return;
  }
  if (found == m_external.end())
    m_external.append({key, text});
  else
    found->text = text;
}

const QList<TrayExternalRow> &TrayMenuModel::externalRows() const
{
  return m_external;
}

QString TrayMenuModel::elided(const QString &text, int maxChars)
{
  if (text.size() <= maxChars)
    return text;
  return text.left(maxChars - 1).trimmed() + QChar(0x2026);
}

QString TrayMenuModel::themeIconName(const QString &icon)
{
  // the names that the rows of the menu have always asked the theme for, and for the rows that are new, the standard ones
  static const QMap<QString, QString> names{
      {QStringLiteral("play"), QStringLiteral("system-run")},
      {QStringLiteral("plugs-connected"), QStringLiteral("network-connect")},
      {QStringLiteral("stop"), QStringLiteral("process-stop")},
      {QStringLiteral("plug"), QStringLiteral("network-disconnect")},
      {QStringLiteral("arrow-clockwise"), QStringLiteral("view-refresh")},
      {QStringLiteral("network"), QStringLiteral("network-wired")},
      {QStringLiteral("desktop"), QStringLiteral("window-restore-pip")},
      {QStringLiteral("sidebar-simple"), QStringLiteral("window-minimize-pip")},
      {QStringLiteral("gear"), QStringLiteral("configure")},
      {QStringLiteral("power"), QStringLiteral("application-exit")},
      {QStringLiteral("monitor"), QStringLiteral("computer")},
      {QStringLiteral("download-simple"), QStringLiteral("software-update-available")},
      {QStringLiteral("info"), QStringLiteral("dialog-information")},
  };
  return names.value(icon);
}

TrayMenuSpec TrayMenuModel::build(const TrayInput &input) const
{
  TrayMenuSpec spec;
  spec.state = trayStateFor(input.process, input.connection, input.hasMode, input.startFailed);
  const auto &state = spec.state;
  const bool running = isRunning(input.process);

  spec.toolTip = QStringLiteral("%1: %2").arg(input.appName, input.headline);
  if (!input.updateVersion.isEmpty())
    spec.toolTip += QLatin1Char(' ') + tr("(update available)");

  // the header, and what other parts of the program tell under it
  Group header;
  {
    TrayMenuEntry entry;
    entry.kind = Kind::Header;
    entry.id = QStringLiteral("header");
    entry.enabled = false;
    entry.text = input.headline;
    entry.detail = input.detail;
    switch (state) {
    case TrayState::Connected:
      entry.chipText = tr("Running");
      entry.chipKind = StatusChip::Kind::Success;
      break;
    case TrayState::Waiting:
      entry.chipText = tr("Waiting");
      entry.chipKind = StatusChip::Kind::Info;
      break;
    case TrayState::Stopped:
      entry.chipText = tr("Stopped");
      entry.chipKind = StatusChip::Kind::Neutral;
      break;
    case TrayState::Error:
      entry.chipText = tr("Needs attention");
      entry.chipKind = StatusChip::Kind::Danger;
      break;
    case TrayState::Idle:
      break;
    }
    header.append(entry);
    for (const auto &row : m_external)
      header.append(textEntry(QStringLiteral("external:") + row.key, row.text, {}));
  }

  // the commands of the state
  Group commands;
  const QString startIcon = input.isServer ? QStringLiteral("play") : QStringLiteral("plugs-connected");
  const QString stopIcon = input.isServer ? QStringLiteral("stop") : QStringLiteral("plug");
  if (!running) {
    auto start = commandEntry(input, TrayCommand::Start, startIcon);
    start.enabled = input.canStart;
    // with no mode chosen there is nothing to start
    if (input.hasMode || !dimmedOrLeftOut(start, m_platform))
      commands.append(start);
  } else if (state == TrayState::Error) {
    if (!input.isServer) {
      auto test = commandEntry(input, TrayCommand::TestConnection, QStringLiteral("network"));
      test.enabled = input.canTestConnection;
      commands.append(test);
    }
    commands.append(commandEntry(input, TrayCommand::Stop, stopIcon));
  } else {
    commands.append(commandEntry(input, TrayCommand::Restart, QStringLiteral("arrow-clockwise")));
    commands.append(commandEntry(input, TrayCommand::Stop, stopIcon));
  }

  // the facts: an update, and the computers
  Group facts;
  if (!input.updateVersion.isEmpty()) {
    auto update = commandEntry(input, TrayCommand::OpenUpdate, QStringLiteral("download-simple"));
    update.text = tr("A new version v%1 is available").arg(input.updateVersion);
    facts.append(update);
  }
  for (const auto &name : input.computers)
    facts.append(textEntry(QStringLiteral("computer:") + name, tr("%1: Connected").arg(name), QStringLiteral("monitor")));
  if (input.isServer && input.computers.isEmpty() && state == TrayState::Waiting)
    facts.append(textEntry(QStringLiteral("computers:none"), tr("No computer has connected yet."), {}));

  // the window, the settings
  Group window;
  {
    auto open = commandEntry(input, TrayCommand::OpenWindow, QStringLiteral("desktop"));
    open.bold = true;
    window.append(open);
    auto minimize = commandEntry(input, TrayCommand::Minimize, QStringLiteral("sidebar-simple"));
    // a window that is not on the screen is not hidden again
    if (input.windowShown || !dimmedOrLeftOut(minimize, m_platform))
      window.append(minimize);
    window.append(commandEntry(input, TrayCommand::Preferences, QStringLiteral("gear")));
  }

  Group quit{commandEntry(input, TrayCommand::Quit, QStringLiteral("power"))};

  int separators = 0;
  for (auto *group : {&header, &commands, &facts, &window, &quit}) {
    // a group with no rows takes no separator
    if (group->isEmpty())
      continue;
    if (!spec.entries.isEmpty()) {
      TrayMenuEntry separator;
      separator.kind = Kind::Separator;
      separator.id = QStringLiteral("separator:%1").arg(separators++);
      spec.entries.append(separator);
    }
    // the icons are even among the rows that are there
    evenIcons(*group);
    if (!m_platform.icons)
      for (auto &entry : *group)
        entry.icon.clear();
    spec.entries.append(*group);
  }
  return spec;
}

} // namespace deskflow::gui::tray
