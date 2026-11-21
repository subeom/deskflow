/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/UpdateCache.h"

#include <QString>

#include <memory>

namespace deskflow::gui {

struct UpdateVerifierResult
{
  bool ok = false;
  QString error;
};

class IUpdateVerifier
{
public:
  virtual ~IUpdateVerifier() = default;
  virtual UpdateVerifierResult verify(const StagedUpdate &staged) = 0;
};

std::unique_ptr<IUpdateVerifier> createPlatformUpdateVerifier();

} // namespace deskflow::gui
