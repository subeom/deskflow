/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ClipboardTypes.h"

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace deskflow::clipboard {

enum class TransferDirection
{
  Sent,
  Received
};

enum class SkipReason
{
  OverSize,
  SharingDisabled,
  DisabledForComputer,
  IdenticalContent,
  SelectionNotShared
};

//! Lists the formats of a marshalled clipboard with their sizes, e.g. "Text 5 B, HTML 9 B"
std::string describeFormats(std::string_view marshalled);

//! The number of chunks a transfer of \p dataBytes takes: the start and end chunks plus the data chunks
size_t transferChunkCount(size_t dataBytes, size_t chunkSize);

//! One INFO line saying why a clipboard was not transferred; \p detail is appended in parentheses
std::string formatSkipLine(ClipboardID id, std::string_view peer, SkipReason reason, std::string_view detail = {});

//! Joins two descriptions from ClipboardTransferLog::describeActive(), leaving out the one that is "none"
std::string joinActivity(std::string_view first, std::string_view second);

//! The INFO line for a keepalive that did not arrive in time
std::string formatKeepAliveTimeout(
    std::string_view who, std::chrono::milliseconds sinceKeepAlive, std::chrono::milliseconds sinceData,
    std::string_view transfers
);

//! Tracks the clipboard transfers of one connection to produce one summary line per transfer.
/*!
At most one transfer per direction and clipboard id is tracked. The caller supplies the clock so the
durations are testable, and logs the returned lines itself.
*/
class ClipboardTransferLog
{
public:
  using Clock = std::chrono::steady_clock;

  void begin(
      TransferDirection direction, ClipboardID id, std::string_view peer, std::string_view formats, size_t bytes,
      size_t chunks, bool paced, Clock::time_point now
  );

  //! Counts a received chunk
  void addChunk(TransferDirection direction, ClipboardID id);

  //! Ends the transfer and returns its summary line; \p formats replaces the formats given to begin() if not empty
  std::optional<std::string>
  finish(TransferDirection direction, ClipboardID id, Clock::time_point now, std::string_view formats = {});

  void cancel(TransferDirection direction, ClipboardID id);
  bool isActive(TransferDirection direction) const;

  //! "none" or the transfers in flight, with how long they have been running
  std::string describeActive(Clock::time_point now) const;

private:
  struct Slot
  {
    bool active = false;
    std::string peer;
    std::string formats;
    size_t bytes = 0;
    size_t chunks = 0;
    bool paced = false;
    Clock::time_point start;
  };

  std::array<Slot, kClipboardEnd> &slots(TransferDirection direction);
  const std::array<Slot, kClipboardEnd> &slots(TransferDirection direction) const;

  std::array<Slot, kClipboardEnd> m_sent;
  std::array<Slot, kClipboardEnd> m_received;
};

} // namespace deskflow::clipboard
