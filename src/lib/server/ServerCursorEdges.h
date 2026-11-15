/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>

namespace deskflow::server {

float clampCursorEdgeFraction(float position);
float getCursorEdgeFraction(int32_t cursorPosition, int32_t computerStart, int32_t computerSize);

} // namespace deskflow::server
