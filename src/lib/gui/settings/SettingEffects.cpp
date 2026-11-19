/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingEffects.h"

#include "common/Settings.h"

#include <QHash>

namespace deskflow::gui::settings {

namespace {

const QHash<QString, Effect> &table()
{
  using S = Settings;
  static const QHash<QString, Effect> effects = [] {
    QHash<QString, Effect> map;
    auto live = [&map](const QString &key) { map.insert(key, Effect::Live); };
    auto restart = [&map](const QString &key) { map.insert(key, Effect::RestartSharing); };

    // the window and the program: followed at once, or read at the next start or close of the window
    live(S::Core::Language);
    live(S::Gui::Autohide);
    live(S::Gui::CloseToTray);
    live(S::Gui::SymbolicTrayIcon);
    live(S::Gui::ShowVersionInTitle);
    live(S::Gui::AutoUpdateCheck);
    // the look is chosen when the application starts
    live(S::Gui::Appearance);
    // the frame of the window is chosen when the window is made, at the start of the application
    live(S::Gui::NativeWindowFrame);
    // the size of text follows at once (TextSize listens to the setting)
    live(S::Gui::TextSize);
    // the GUI logger listens for it
    live(S::Log::GuiDebug);
    // the running core is told (CoreProcess::applyLogLevel)
    live(S::Log::Level);

    // the core reads these when it starts
    restart(S::Core::Port);
    restart(S::Core::Interface);
    restart(S::Core::PreventSleep);
    restart(S::Core::ProcessMode);
    restart(S::Core::EnableEnterCommand);
    restart(S::Core::ComputerEnterCommand);
    restart(S::Core::EnableExitCommand);
    restart(S::Core::ComputerExitCommand);
    restart(S::Daemon::Elevate);
    restart(S::Log::ToFile);
    restart(S::Log::File);
    restart(S::Security::TlsEnabled);
    restart(S::Security::Certificate);
    restart(S::Security::KeySize);
    restart(S::Security::CheckPeers);

    // the client reads these when it starts (the dialog of the client restarts the core that runs, the page says it
    // with the banner)
    restart(S::Client::DynamicConnectionRetry);
    restart(S::Client::LanguageSync);
    restart(S::Client::ConnectTimeout);
    restart(S::Client::InvertYScroll);
    restart(S::Client::YScrollScale);
    restart(S::Client::InvertXScroll);
    restart(S::Client::XScrollScale);
    // the Cloudflare Tunnel: the core is started behind it (see TunnelStartGuard)
    restart(S::Tunnel::Enabled);
    restart(S::Tunnel::Hostname);
    restart(S::Tunnel::LocalPort);
    restart(S::Tunnel::CloudflaredPath);

    // the server reads its configuration when it starts
    restart(S::Server::ClipboardSize);
    restart(S::Server::ClipboardTextOnly);
    restart(S::Server::DefaultLockToComputerState);
    restart(S::Server::DisableLockToComputer);
    restart(S::Server::EnableClipboard);
    restart(S::Server::EnableHeartbeat);
    restart(S::Server::Heartbeat);
    restart(S::Server::EnableSwitchDelay);
    restart(S::Server::SwitchDelay);
    restart(S::Server::EnableSwitchDoubleTap);
    restart(S::Server::SwitchDoubleTap);
    restart(S::Server::ExperimentalMultiMonitor);
    restart(S::Server::ExternalConfig);
    restart(S::Server::ExternalConfigFile);
    restart(S::Server::Protocol);
    restart(S::Server::RelativeMouseMoves);
    restart(S::Server::Win32KeepForeground);
    return map;
  }();
  return effects;
}

} // namespace

std::optional<Effect> effectOf(const QString &key)
{
  const auto found = table().constFind(key);
  if (found == table().constEnd())
    return std::nullopt;
  return *found;
}

QStringList keysWith(Effect effect)
{
  QStringList keys;
  for (auto it = table().constBegin(); it != table().constEnd(); ++it) {
    if (it.value() == effect)
      keys << it.key();
  }
  keys.sort();
  return keys;
}

} // namespace deskflow::gui::settings
