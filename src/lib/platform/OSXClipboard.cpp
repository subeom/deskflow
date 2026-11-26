/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXClipboard.h"

#include "arch/ArchException.h"
#include "base/Log.h"
#include "deskflow/ClipboardImageSize.h"
#include "platform/OSXClipboardBMPConverter.h"
#include "platform/OSXClipboardHTMLConverter.h"
#include "platform/OSXClipboardTextConverter.h"
#include "platform/OSXClipboardUTF16Converter.h"
#include "platform/OSXClipboardUTF8Converter.h"

//
// OSXClipboard
//

OSXClipboard::OSXClipboard() : m_time(0), m_pboard(nullptr)
{
  m_converters.push_back(new OSXClipboardHTMLConverter);
  m_converters.push_back(new OSXClipboardBMPConverter);
  m_converters.push_back(new OSXClipboardUTF8Converter);
  m_converters.push_back(new OSXClipboardUTF16Converter);
  m_converters.push_back(new OSXClipboardTextConverter);

  OSStatus createErr = PasteboardCreate(kPasteboardClipboard, &m_pboard);
  if (createErr != noErr) {
    LOG_WARN("failed to create clipboard reference: error %i", createErr);
    LOG_ERR("unable to connect to pasteboard, clipboard sharing disabled", createErr);
    m_pboard = nullptr;
    return;
  }

  OSStatus syncErr = PasteboardSynchronize(m_pboard);
  if (syncErr != noErr) {
    LOG_WARN("failed to syncronize clipboard: error %i", syncErr);
  }
}

OSXClipboard::~OSXClipboard()
{
  clearConverters();
}

bool OSXClipboard::empty()
{
  LOG_DEBUG("emptying clipboard");
  if (m_pboard == nullptr)
    return false;

  // what is on it is what Deskflow writes from now on
  m_cache.invalidate();

  OSStatus err = PasteboardClear(m_pboard);
  if (err != noErr) {
    LOG_WARN("failed to clear clipboard: error %i", err);
    return false;
  }

  return true;
}

void OSXClipboard::refresh() const
{
  if (m_pboard == nullptr)
    return;

  // the flags tell what changed since the last call, so a change that is seen here is kept for synchronize()
  PasteboardSyncFlags flags = PasteboardSynchronize(m_pboard);
  LOG_VERBOSE("flags: %x", flags);

  if (flags & kPasteboardModified) {
    m_cache.invalidate();
    m_changePending = true;
  }
}

bool OSXClipboard::synchronize()
{
  if (m_pboard == nullptr)
    return false;

  refresh();
  const bool changed = m_changePending;
  m_changePending = false;
  return changed;
}

void OSXClipboard::setReadLimit(size_t bytes) const
{
  m_readLimit = bytes;
}

size_t OSXClipboard::estimateBitmapBytes(PasteboardItemID item) const
{
  // The system makes the bitmap from the image on request, and that conversion is what is costly: the size of the
  // image says what the bitmap will be, and an image is a few dozen KB to read.
  struct Source
  {
    CFStringRef flavor;
    std::optional<deskflow::clipboard::ImageSize> (*size)(std::string_view);
  };
  static const Source sources[] = {
      {CFSTR("public.png"), deskflow::clipboard::pngImageSize},
      {CFSTR("public.tiff"), deskflow::clipboard::tiffImageSize},
  };

  for (const auto &source : sources) {
    PasteboardFlavorFlags flags;
    if (PasteboardGetItemFlavorFlags(m_pboard, item, source.flavor, &flags) != noErr) {
      continue;
    }
    CFDataRef data = nullptr;
    if (PasteboardCopyItemFlavorData(m_pboard, item, source.flavor, &data) != noErr || data == nullptr) {
      continue;
    }
    const auto size = source.size(
        std::string_view(
            reinterpret_cast<const char *>(CFDataGetBytePtr(data)), static_cast<size_t>(CFDataGetLength(data))
        )
    );
    CFRelease(data);
    if (size) {
      return deskflow::clipboard::bitmapLowerBound(*size);
    }
  }
  return 0;
}

void OSXClipboard::add(Format format, const std::string &data)
{
  if (m_pboard == nullptr)
    return;

  m_cache.invalidate();

  LOG_DEBUG("add %d bytes to clipboard format: %d", data.size(), format);
  if (format == IClipboard::Format::Text) {
    LOG_DEBUG("format of data to be added to clipboard was kText");
  } else if (format == IClipboard::Format::Bitmap) {
    LOG_DEBUG("format of data to be added to clipboard was kBitmap");
  } else if (format == IClipboard::Format::HTML) {
    LOG_DEBUG("format of data to be added to clipboard was kHTML");
  }

  for (ConverterList::const_iterator index = m_converters.begin(); index != m_converters.end(); ++index) {

    IOSXClipboardConverter *converter = *index;

    // skip converters for other formats
    if (converter->getFormat() == format) {
      std::string osXData = converter->fromIClipboard(data);
      CFStringRef flavorType = converter->getOSXFormat();
      CFDataRef dataRef = CFDataCreate(kCFAllocatorDefault, (uint8_t *)osXData.data(), osXData.size());
      PasteboardItemID itemID = 0;

      if (dataRef) {
        PasteboardPutItemFlavor(m_pboard, itemID, flavorType, dataRef, kPasteboardFlavorNoFlags);

        CFRelease(dataRef);
        LOG_DEBUG("added %d bytes to clipboard format: %d", data.size(), format);
      }
    }
  }
}

bool OSXClipboard::open(Time time) const
{
  if (m_pboard == nullptr)
    return false;

  LOG_DEBUG("opening clipboard");
  m_time = time;
  return true;
}

void OSXClipboard::close() const
{
  LOG_DEBUG("closing clipboard");
  /* not needed */
}

IClipboard::Time OSXClipboard::getTime() const
{
  return m_time;
}

bool OSXClipboard::has(Format format) const
{
  if (m_pboard == nullptr)
    return false;

  PasteboardItemID item;
  PasteboardGetItemIdentifier(m_pboard, (CFIndex)1, &item);

  for (ConverterList::const_iterator index = m_converters.begin(); index != m_converters.end(); ++index) {
    IOSXClipboardConverter *converter = *index;
    if (converter->getFormat() == format) {
      PasteboardFlavorFlags flags;
      CFStringRef type = converter->getOSXFormat();

      OSStatus res;

      if ((res = PasteboardGetItemFlavorFlags(m_pboard, item, type, &flags)) == noErr) {
        return true;
      }
    }
  }

  return false;
}

std::string OSXClipboard::get(Format format) const
{
  CFStringRef type;
  PasteboardItemID item;
  std::string result;

  if (m_pboard == nullptr)
    return result;

  // the server reads the clipboard again at every switch of computers, and nothing has changed since the last read
  refresh();
  if (const auto *cached = m_cache.find(static_cast<int>(format)); cached != nullptr) {
    LOG_DEBUG("clipboard format %d from the cache, %zu bytes", static_cast<int>(format), cached->size());
    return *cached;
  }

  PasteboardGetItemIdentifier(m_pboard, (CFIndex)1, &item);

  // find the converter for the first clipboard format we can handle
  IOSXClipboardConverter *converter = nullptr;
  for (ConverterList::const_iterator index = m_converters.begin(); index != m_converters.end(); ++index) {
    converter = *index;

    PasteboardFlavorFlags flags;
    type = converter->getOSXFormat();

    if (converter->getFormat() == format && PasteboardGetItemFlavorFlags(m_pboard, item, type, &flags) == noErr) {
      break;
    }
    converter = nullptr;
  }

  // if no converter then we don't recognize any formats
  if (converter == nullptr) {
    LOG_DEBUG("unable to find converter for data");
    return result;
  }

  // An image that cannot be shared is not converted: the conversion of a screenshot makes a bitmap of many MB, and
  // the clipboard would be dropped for its size after it was read. What stands in for the image is as large as the
  // limit, which with the 12 bytes the marshalled clipboard adds is over it, so it is dropped the same way and never
  // sent.
  constexpr size_t kLargestStandIn = 64 * 1024 * 1024;
  if (format == IClipboard::Format::Bitmap && m_readLimit != 0 && m_readLimit <= kLargestStandIn &&
      deskflow::clipboard::surelyOverLimit(estimateBitmapBytes(item), m_readLimit)) {
    LOG_DEBUG("not reading the image of the clipboard, it is larger than the limit of %zu bytes", m_readLimit);
    return std::string(m_readLimit, '\0');
  }

  // get the clipboard data.
  CFDataRef buffer = nullptr;
  try {
    OSStatus err = PasteboardCopyItemFlavorData(m_pboard, item, type, &buffer);

    if (err != noErr) {
      throw err;
    }

    result = std::string((char *)CFDataGetBytePtr(buffer), CFDataGetLength(buffer));
  } catch (OSStatus err) {
    LOG_DEBUG("exception thrown in OSXClipboard::get MacError (%d)", err);
  } catch (...) {
    LOG_DEBUG("unknown exception in OSXClipboard::get");
    RETHROW_THREADEXCEPTION
  }

  if (buffer != nullptr)
    CFRelease(buffer);

  result = converter->toIClipboard(result);
  // a read that failed is tried again next time
  if (!result.empty()) {
    m_cache.store(static_cast<int>(format), result);
  }
  return result;
}

void OSXClipboard::clearConverters()
{
  if (m_pboard == nullptr)
    return;

  for (ConverterList::iterator index = m_converters.begin(); index != m_converters.end(); ++index) {
    delete *index;
  }
  m_converters.clear();
}
