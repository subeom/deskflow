/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Diagnostic.h"

#include "common/Settings.h"
#include "gui/tunnel/ISecretStore.h"
#include "gui/tunnel/SecretStoreFactory.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>

namespace deskflow::gui::diagnostic {

void restart()
{
  QString program = QCoreApplication::applicationFilePath();
  QStringList arguments = QCoreApplication::arguments();

  // look for and remove --reset option if found
  if (int resetIndex = arguments.indexOf("--reset"); resetIndex != -1)
    arguments.remove(resetIndex);

  qInfo("launching new process: %s", qPrintable(program));
  QProcess::startDetached(program, arguments);

  qDebug("exiting current process");
  QCoreApplication::exit();
}

bool removeStoredCredentials(std::shared_ptr<deskflow::tunnel::ISecretStore> store)
{
  if (!store) {
    store = deskflow::tunnel::createSystemSecretStore();
  }

  const auto result = store->remove();
  if (!result.ok && !result.notFound) {
    qWarning().noquote() << "the Service Token of the Cloudflare Tunnel could not be removed from" << store->name()
                         << "-" << result.detail;
    return false;
  }
  qDebug("removed the Service Token of the Cloudflare Tunnel from the secure store");
  return true;
}

void clearSettings(bool enableRestart)
{
  qDebug("clearing settings");
  // Cloudflare Tunnel: the Service Token is in the secure store, not in the settings
  removeStoredCredentials();

  Settings::proxy().clear();
  Settings::clearState();

  // save but do not emit saving signal which will prevent the current state of
  // the app config and server configs from being applied.
  Settings::save(false);

  auto profileDir = QDir(Settings::settingsPath());
  qDebug("removing profile dir: %s", qPrintable(profileDir.absolutePath()));
  profileDir.removeRecursively();

#ifdef Q_OS_WIN
  if (Settings::isPortableMode()) {
    // make a new empty portable settings file
    if (profileDir.mkpath(Settings::settingsPath())) {
      QFile file(Settings::settingsFile());
      std::ignore = file.open(QIODevice::WriteOnly);
      file.write(" ", 1);
      file.close();
    }
  }
#endif

  if (enableRestart) {
    qDebug("restarting");
    restart();
  } else {
    qDebug("skipping restart");
  }
}

} // namespace deskflow::gui::diagnostic
