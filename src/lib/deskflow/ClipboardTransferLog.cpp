/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClipboardTransferLog.h"

#include "deskflow/IClipboard.h"

#include <format>

namespace deskflow::clipboard {

namespace {

uint32_t readUInt32(std::string_view data, size_t offset)
{
  const auto *bytes = reinterpret_cast<const unsigned char *>(data.data()) + offset;
  return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
         (static_cast<uint32_t>(bytes[2]) << 8) | static_cast<uint32_t>(bytes[3]);
}

const char *formatName(uint32_t format)
{
  switch (static_cast<IClipboard::Format>(format)) {
  case IClipboard::Format::Text:
    return "Text";
  case IClipboard::Format::HTML:
    return "HTML";
  case IClipboard::Format::Bitmap:
    return "Bitmap";
  default:
    return "format";
  }
}

long long toMs(ClipboardTransferLog::Clock::time_point from, ClipboardTransferLog::Clock::time_point to)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count();
}

} // namespace

std::string describeFormats(std::string_view marshalled)
{
  if (marshalled.size() < 4) {
    return "unreadable";
  }

  const auto count = readUInt32(marshalled, 0);
  size_t offset = 4;
  std::string result;
  for (uint32_t i = 0; i < count; ++i) {
    if (marshalled.size() - offset < 8) {
      return "unreadable";
    }
    const auto format = readUInt32(marshalled, offset);
    const auto size = readUInt32(marshalled, offset + 4);
    offset += 8;
    if (size > marshalled.size() - offset) {
      return "unreadable";
    }
    offset += size;

    if (!result.empty()) {
      result += ", ";
    }
    result += std::format("{} {} B", formatName(format), size);
  }
  return result.empty() ? "no data" : result;
}

size_t transferChunkCount(size_t dataBytes, size_t chunkSize)
{
  const size_t dataChunks = chunkSize == 0 ? 0 : (dataBytes + chunkSize - 1) / chunkSize;
  return dataChunks + 2;
}

std::string formatSkipLine(ClipboardID id, std::string_view peer, SkipReason reason, std::string_view detail)
{
  const char *text = "";
  switch (reason) {
  case SkipReason::OverSize:
    text = "over size";
    break;
  case SkipReason::SharingDisabled:
    text = "clipboard sharing is disabled";
    break;
  case SkipReason::DisabledForComputer:
    text = "disabled for this computer";
    break;
  case SkipReason::IdenticalContent:
    text = "identical content";
    break;
  case SkipReason::SelectionNotShared:
    text = "the selection is not shared with a peer that ignores the clipboard id";
    break;
  }

  auto line = std::format("clipboard {} skipped for \"{}\": {}", static_cast<int>(id), peer, text);
  if (!detail.empty()) {
    line += std::format(" ({})", detail);
  }
  return line;
}

std::string joinActivity(std::string_view first, std::string_view second)
{
  if (first == "none") {
    return std::string(second);
  }
  if (second == "none") {
    return std::string(first);
  }
  return std::format("{}, {}", first, second);
}

std::string formatKeepAliveTimeout(
    std::string_view who, std::chrono::milliseconds sinceKeepAlive, std::chrono::milliseconds sinceData,
    std::string_view transfers
)
{
  return std::format(
      "{} is dead: no keepalive for {} ms, last data {} ms ago, clipboard transfer in flight: {}", who,
      sinceKeepAlive.count(), sinceData.count(), transfers
  );
}

std::array<ClipboardTransferLog::Slot, kClipboardEnd> &ClipboardTransferLog::slots(TransferDirection direction)
{
  return direction == TransferDirection::Sent ? m_sent : m_received;
}

const std::array<ClipboardTransferLog::Slot, kClipboardEnd> &
ClipboardTransferLog::slots(TransferDirection direction) const
{
  return direction == TransferDirection::Sent ? m_sent : m_received;
}

void ClipboardTransferLog::begin(
    TransferDirection direction, ClipboardID id, std::string_view peer, std::string_view formats, size_t bytes,
    size_t chunks, bool paced, Clock::time_point now
)
{
  if (id >= kClipboardEnd) {
    return;
  }
  if (direction == TransferDirection::Received) {
    // a receiver has one assembly state, so a start chunk ends a transfer of another id that was abandoned
    for (auto &slot : m_received) {
      slot.active = false;
    }
  }
  slots(direction)[id] = Slot{true, std::string(peer), std::string(formats), bytes, chunks, paced, now};
}

void ClipboardTransferLog::addChunk(TransferDirection direction, ClipboardID id)
{
  if (id < kClipboardEnd && slots(direction)[id].active) {
    ++slots(direction)[id].chunks;
  }
}

std::optional<std::string> ClipboardTransferLog::finish(
    TransferDirection direction, ClipboardID id, Clock::time_point now, std::string_view formats
)
{
  if (id >= kClipboardEnd || !slots(direction)[id].active) {
    return std::nullopt;
  }

  auto &slot = slots(direction)[id];
  slot.active = false;
  const auto &shownFormats = formats.empty() ? std::string_view(slot.formats) : formats;
  const auto *verb = direction == TransferDirection::Sent ? "sent to" : "received from";
  // the receiver cannot tell whether the sender paced the transfer
  const auto *pacing = direction == TransferDirection::Sent ? (slot.paced ? ", paced" : ", unpaced") : "";
  return std::format(
      "clipboard {} {} \"{}\": {}; {} B in {} chunks, {} ms{}", static_cast<int>(id), verb, slot.peer, shownFormats,
      slot.bytes, slot.chunks, toMs(slot.start, now), pacing
  );
}

void ClipboardTransferLog::cancel(TransferDirection direction, ClipboardID id)
{
  if (id < kClipboardEnd) {
    slots(direction)[id].active = false;
  }
}

bool ClipboardTransferLog::isActive(TransferDirection direction) const
{
  for (const auto &slot : slots(direction)) {
    if (slot.active) {
      return true;
    }
  }
  return false;
}

std::string ClipboardTransferLog::describeActive(Clock::time_point now) const
{
  std::string result;
  const auto describe = [&](const std::array<Slot, kClipboardEnd> &list, const char *verb, bool showChunks) {
    for (ClipboardID id = 0; id < kClipboardEnd; ++id) {
      const auto &slot = list[id];
      if (!slot.active) {
        continue;
      }
      if (!result.empty()) {
        result += ", ";
      }
      result += std::format("{} clipboard {} ({} B", verb, static_cast<int>(id), slot.bytes);
      if (showChunks) {
        result += std::format(", {} chunks so far", slot.chunks);
      }
      result += std::format(", {} ms ago)", toMs(slot.start, now));
    }
  };
  describe(m_received, "receiving", true);
  describe(m_sent, "sending", false);
  return result.empty() ? "none" : result;
}

} // namespace deskflow::clipboard
