/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/UpdateVerifier.h"

#if defined(Q_OS_MACOS)
#include "gui/MacUpdateVerifier.h"
#endif

namespace deskflow::gui {

std::unique_ptr<IUpdateVerifier> createPlatformUpdateVerifier()
{
#if defined(Q_OS_MACOS)
  return std::make_unique<MacUpdateVerifier>();
#else
  return {};
#endif
}

} // namespace deskflow::gui
