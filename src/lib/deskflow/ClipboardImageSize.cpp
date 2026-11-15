/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClipboardImageSize.h"

#include <cstring>

namespace deskflow::clipboard {

namespace {

constexpr size_t kMarshalledOverheadBytes = 12;

uint32_t read32(std::string_view data, size_t offset, bool littleEndian)
{
  const auto *bytes = reinterpret_cast<const unsigned char *>(data.data()) + offset;
  if (littleEndian) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
  }
  return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
         (static_cast<uint32_t>(bytes[2]) << 8) | static_cast<uint32_t>(bytes[3]);
}

uint16_t read16(std::string_view data, size_t offset, bool littleEndian)
{
  const auto *bytes = reinterpret_cast<const unsigned char *>(data.data()) + offset;
  if (littleEndian) {
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
  }
  return static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
}

} // namespace

std::optional<ImageSize> pngImageSize(std::string_view data)
{
  // signature (8), the length of IHDR (4), "IHDR" (4), width (4), height (4)
  static constexpr char kSignature[] = "\x89PNG\r\n\x1a\n";
  if (data.size() < 24 || std::memcmp(data.data(), kSignature, 8) != 0 ||
      std::memcmp(data.data() + 12, "IHDR", 4) != 0) {
    return std::nullopt;
  }

  const ImageSize size{read32(data, 16, false), read32(data, 20, false)};
  if (size.width == 0 || size.height == 0) {
    return std::nullopt;
  }
  return size;
}

std::optional<ImageSize> tiffImageSize(std::string_view data)
{
  if (data.size() < 8 || (std::memcmp(data.data(), "II", 2) != 0 && std::memcmp(data.data(), "MM", 2) != 0)) {
    return std::nullopt;
  }
  const bool littleEndian = data[0] == 'I';
  if (read16(data, 2, littleEndian) != 42) {
    return std::nullopt;
  }

  const size_t directory = read32(data, 4, littleEndian);
  if (directory > data.size() || data.size() - directory < 2) {
    return std::nullopt;
  }
  const size_t entries = read16(data, directory, littleEndian);
  constexpr size_t kEntryBytes = 12;
  if (entries > (data.size() - directory - 2) / kEntryBytes) {
    return std::nullopt;
  }

  constexpr uint16_t kImageWidth = 256;
  constexpr uint16_t kImageLength = 257;
  constexpr uint16_t kShort = 3;
  constexpr uint16_t kLong = 4;
  ImageSize size;
  for (size_t i = 0; i < entries; ++i) {
    const size_t entry = directory + 2 + i * kEntryBytes;
    const auto tag = read16(data, entry, littleEndian);
    if (tag != kImageWidth && tag != kImageLength) {
      continue;
    }
    const auto type = read16(data, entry + 2, littleEndian);
    if (type != kShort && type != kLong) {
      return std::nullopt;
    }
    const uint32_t value =
        type == kShort ? read16(data, entry + 8, littleEndian) : read32(data, entry + 8, littleEndian);
    (tag == kImageWidth ? size.width : size.height) = value;
  }

  if (size.width == 0 || size.height == 0) {
    return std::nullopt;
  }
  return size;
}

size_t bitmapLowerBound(ImageSize size)
{
  const uint64_t pixels = static_cast<uint64_t>(size.width) * size.height;
  if (pixels > SIZE_MAX / 4) {
    return SIZE_MAX;
  }
  return static_cast<size_t>(pixels) * 4;
}

bool surelyOverLimit(size_t formatBytes, size_t limitBytes)
{
  if (limitBytes == 0 || formatBytes == 0) {
    return false;
  }
  return formatBytes > limitBytes || limitBytes - formatBytes < kMarshalledOverheadBytes;
}

} // namespace deskflow::clipboard
