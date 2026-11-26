/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/tray/TrayMenuModel.h"

#include <QKeySequence>

namespace tray_test {

using namespace deskflow::gui::tray;

// What the main window gives the menu for a core in a state: the texts that its actions have (in English)
inline TrayInput makeInput(
    deskflow::core::ProcessState process, deskflow::core::ConnectionState connection, bool server = true,
    bool windowShown = true
)
{
  TrayInput input;
  input.process = process;
  input.connection = connection;
  input.isServer = server;
  input.windowShown = windowShown;
  input.appName = QStringLiteral("Deskflow");
  input.headline = QStringLiteral("Headline");
  input.detail = QStringLiteral("The sentence behind it");
  input.labels = {
      {TrayCommand::Start, server ? QStringLiteral("Start") : QStringLiteral("Connect")},
      {TrayCommand::Restart, QStringLiteral("Rest&art")},
      {TrayCommand::Stop, server ? QStringLiteral("Stop") : QStringLiteral("Disconnect")},
      {TrayCommand::TestConnection, QStringLiteral("Test connection")},
      {TrayCommand::OpenWindow, QStringLiteral("&Open Deskflow")},
      {TrayCommand::Minimize, QStringLiteral("&Minimize to tray")},
      {TrayCommand::Preferences, QStringLiteral("&Preferences")},
      {TrayCommand::Quit, QStringLiteral("&Quit")},
  };
  input.shortcuts = {
      {TrayCommand::Start, QKeySequence(QStringLiteral("Ctrl+S"))},
      {TrayCommand::Restart, QKeySequence(QStringLiteral("Ctrl+S"))},
      {TrayCommand::Stop, QKeySequence(QStringLiteral("Ctrl+T"))},
      {TrayCommand::Quit, QKeySequence(QStringLiteral("Ctrl+Q"))},
  };
  return input;
}

} // namespace tray_test
