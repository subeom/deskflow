/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace deskflow::gui::settings {

/*!
 * What it takes for a changed setting to be in use. The settings pages of the main window apply a change as soon as
 * it is made (the settings file is written at once), and this says what else is needed:
 *  - Live: nothing (the window, the tray icon, the language and the log level of the running core follow at once, and
 *    the settings that the window reads at its next start or close need nothing now)
 *  - RestartSharing: the core reads it when it starts, so a running core has to be restarted ("Restart sharing")
 *
 * The dialogs of the system look restart a running core after every Save. The list of keys that need it is the list of
 * the settings that the core process reads, found in the code of the core (lib/server, lib/deskflow, lib/platform,
 * lib/client, apps/deskflow-daemon).
 */
enum class Effect
{
  Live,
  RestartSharing
};

//! The effect of a setting; none when the key is not one that a page binds (a page cannot bind a key without one)
std::optional<Effect> effectOf(const QString &key);

//! All the keys that have the effect, sorted
QStringList keysWith(Effect effect);

} // namespace deskflow::gui::settings
