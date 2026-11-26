/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/Chunk.h"
#include "deskflow/ClipboardTypes.h"
#include "deskflow/ProtocolTypes.h"

#include <cstddef>
#include <string>

constexpr static auto s_clipboardChunkMetaSize = 7;

namespace deskflow {
class IStream;
}

struct ClipboardChunkAssemblyState
{
  size_t expectedSize = 0;
  bool active = false;
};

class ClipboardChunk : public Chunk
{
public:
  explicit ClipboardChunk(size_t size);

  static ClipboardChunk *start(ClipboardID id, uint32_t sequence, const std::string &size);
  static ClipboardChunk *data(ClipboardID id, uint32_t sequence, const std::string &data);
  static ClipboardChunk *end(ClipboardID id, uint32_t sequence);

  static TransferState assemble(
      deskflow::IStream *stream, std::string &dataCached, ClipboardID &id, uint32_t &sequence,
      ClipboardChunkAssemblyState &state, size_t maxDataSize
  );

  static void send(deskflow::IStream *stream, void *data);

  //! Writes one clipboard message without building a ClipboardChunk first; the bytes equal those of send()
  static void
  write(deskflow::IStream *stream, ClipboardID id, uint32_t sequence, uint8_t mark, const std::string &data);

  //! The chunk type (ChunkType) of a ClipboardChunk passed as the data of a ClipboardSending event
  static uint8_t getMark(const void *data);

  //! The clipboard id of a ClipboardChunk passed as the data of a ClipboardSending event
  static ClipboardID getId(const void *data);

  static size_t getExpectedSize(const ClipboardChunkAssemblyState &state)
  {
    return state.expectedSize;
  }
};
