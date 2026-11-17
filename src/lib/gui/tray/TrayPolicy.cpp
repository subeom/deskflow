/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayPolicy.h"

namespace deskflow::gui::tray {

CloseAction closeActionFor(bool closeToTray, bool trayAvailable)
{
  if (!closeToTray)
    return CloseAction::Quit;
  return trayAvailable ? CloseAction::HideToTray : CloseAction::Minimize;
}

bool clickTogglesWindow(bool isMac, const QString &currentDesktop)
{
  if (isMac)
    return false;
  const auto desktops = currentDesktop.split(QLatin1Char(':'), Qt::SkipEmptyParts);
  for (const auto &desktop : desktops) {
    if (desktop.compare(QLatin1String("GNOME"), Qt::CaseInsensitive) == 0)
      return false;
  }
  return true;
}

bool trayClickCanCrash(const QVersionNumber &qtVersion, int macOsMajor)
{
  return macOsMajor >= 27 && QVersionNumber::compare(qtVersion, QVersionNumber(6, 11, 3)) < 0;
}

} // namespace deskflow::gui::tray
