/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace deskflow::clipboard {

struct ImageSize
{
  uint32_t width = 0;
  uint32_t height = 0;
};

//! The size of a PNG, from its header (the IHDR chunk that has to come first); nothing if it is not a PNG
std::optional<ImageSize> pngImageSize(std::string_view data);

//! The size of a TIFF, from the first directory of its header, in either byte order; nothing if it is not a TIFF
std::optional<ImageSize> tiffImageSize(std::string_view data);

//! A lower bound of the size of the bitmap (device independent, 32 bits a pixel) that an image of \p size becomes
/*!
Never smaller than the real size, which also has a header, so that an image over a limit by this number is over it
for certain.  A number that does not fit is the largest there is.
*/
size_t bitmapLowerBound(ImageSize size);

//! Whether a clipboard that holds a format of at least \p formatBytes is over \p limitBytes for certain
/*!
A marshalled clipboard is 4 bytes for the number of formats and 8 more for the format and the size of each, which are
counted for one format.  No limit (0) and a size that is not known (0) are never over.
*/
bool surelyOverLimit(size_t formatBytes, size_t limitBytes);

} // namespace deskflow::clipboard
