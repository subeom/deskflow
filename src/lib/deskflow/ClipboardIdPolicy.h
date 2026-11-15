/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/InputCapabilities.h"
#include "deskflow/ClipboardTypes.h"

namespace deskflow::clipboard {

//! Whether the data of clipboard \p id is exchanged with a peer on \p platform
/*!
The selection (kClipboardSelection) only means something on X11. Windows and macOS ignore the id when they set a
clipboard (they have one), so data sent for the selection to a peer on those platforms is the same bytes again and
a second write to its clipboard. Linux, other and unknown platforms keep every id.
*/
inline bool exchangesId(InputPlatform platform, ClipboardID id)
{
  if (id == kClipboardClipboard) {
    return true;
  }
  return platform != InputPlatform::Windows && platform != InputPlatform::MacOS;
}

} // namespace deskflow::clipboard
