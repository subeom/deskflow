/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2013 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ClipboardTypes.h"

#include <cstddef>
#include <string_view>

class IEventQueue;

//! Size of the data chunks of a clipboard transfer that is not paced
inline constexpr size_t kClipboardChunkSize = 512 * 1024;

class StreamChunker
{
public:
  static void sendClipboard(
      const std::string_view &data, size_t size, ClipboardID id, uint32_t sequence, IEventQueue *events,
      void *eventTarget
  );
};
