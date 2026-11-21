/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlSetup.h"

#include "AwdlController.h"
#include "AwdlPlatform.h"
#include "AwdlTexts.h"
#include "common/Settings.h"

#include <QDebug>

namespace deskflow::awdl {

AwdlController *makeController(IAwdlSystem *system, QObject *parent)
{
  AwdlController::Options options;
  options.optionOn = [] { return Settings::value(Settings::Network::MacAwdlOff).toBool(); };
  options.prompt = [](Direction direction) { return prompt(direction); };
  options.permissionPrompt = [](PermissionAction action) { return permissionPrompt(action); };
  options.supported = platformSupported();
  auto *controller = new AwdlController(system, options, parent);

  QObject::connect(Settings::instance(), &Settings::settingsChanged, controller, [controller](const QString &key) {
    if (key == Settings::Network::MacAwdlOff)
      controller->optionChanged();
  });
  QObject::connect(controller, &AwdlController::logLine, controller, [](const QString &line) {
    qInfo().noquote() << "awdl:" << line;
  });
  return controller;
}

} // namespace deskflow::awdl
