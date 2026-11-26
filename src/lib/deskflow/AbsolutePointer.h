/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>

namespace deskflow::pointer {

//! The value that Windows `SendInput` takes as an absolute position on one axis
/*!
Absolute mouse input is normalised to 0..65535 over \p extent pixels of the primary monitor. A position right of or
below the primary monitor gives more than 65535, one left of or above it a negative value, and the caller passes the
32 bits of the result.

The value is what `(DWORD)((65535.0f * x) / (w - 1) + 0.5f)` gave on x64: the float is truncated toward zero through a
signed 64-bit integer and the low 32 bits are kept, so a negative value keeps its two's complement pattern. Writing the
cast of a negative float to an unsigned type directly is undefined, and native ARM64 saturates it to 0, which would
send the pointer to the edge of the primary monitor for every monitor on its left or above it. A value that no integer
holds (an axis of one pixel divides by zero) is 0, as the x64 conversion gives.
*/
inline uint32_t absoluteAxisValue(int32_t position, int32_t extent)
{
  const float scaled = (65535.0f * static_cast<float>(position)) / static_cast<float>(extent - 1) + 0.5f;
  constexpr float kInt64Bound = 9223372036854775808.0f; // 2^63
  if (!(scaled > -kInt64Bound && scaled < kInt64Bound)) {
    return 0;
  }
  return static_cast<uint32_t>(static_cast<int64_t>(scaled));
}

} // namespace deskflow::pointer
