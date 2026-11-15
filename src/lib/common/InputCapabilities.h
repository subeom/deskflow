/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>

enum class InputPlatform : uint8_t
{
  Unknown = 0,
  Windows = 1,
  MacOS = 2,
  Linux = 3,
  Other = 4
};

using InputCapabilityMask = uint32_t;

inline constexpr InputCapabilityMask kInputCapabilityProtocolV19 = 1u << 0;

struct InputCapabilities
{
  InputPlatform platform = InputPlatform::Unknown;
  InputCapabilityMask capabilities = 0;
};

inline bool isKnownInputPlatform(InputPlatform platform)
{
  return platform == InputPlatform::Windows || platform == InputPlatform::MacOS || platform == InputPlatform::Linux ||
         platform == InputPlatform::Other;
}
