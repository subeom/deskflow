/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateCache.h"

#include <QString>

#include <memory>

namespace deskflow::gui {

struct UpdateInstallResult
{
  bool ok = false;
  QString error;
};

class IUpdateInstaller
{
public:
  virtual ~IUpdateInstaller() = default;

  virtual UpdateInstallResult start(const StagedUpdate &staged, bool confirmed) = 0;
};

std::unique_ptr<IUpdateInstaller> createPlatformUpdateInstaller();
UpdateInstallResult applyPlatformUpdate(const QString &manifestPath, qint64 parentProcessId);

} // namespace deskflow::gui
