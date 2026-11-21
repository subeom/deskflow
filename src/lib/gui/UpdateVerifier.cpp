/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateVerifier.h"

namespace deskflow::gui {

std::unique_ptr<IUpdateVerifier> createPlatformUpdateVerifier()
{
  return {};
}

} // namespace deskflow::gui
