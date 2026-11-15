/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateRelease.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace deskflow::gui {

struct UpdateIntegrityResult
{
  bool ok = false;
  QString error;
};

UpdateIntegrityResult verifyUpdatePayload(
    const UpdateAsset &asset, const QByteArray &payload, const std::optional<QByteArray> &sums = std::nullopt
);

} // namespace deskflow::gui
