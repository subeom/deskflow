/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXInputSource.h"

#include "platform/OSXAutoTypes.h"

#include <Carbon/Carbon.h>

#include <mutex>

namespace deskflow::platform {

deskflow::input::InputIndicatorSource classifyMacInputSourceId(std::string_view identifier)
{
  if (identifier == kMacABCInputSourceId) {
    return deskflow::input::InputIndicatorSource::LatinABC;
  }
  if (identifier == kMacKorean2SetInputSourceId) {
    return deskflow::input::InputIndicatorSource::Korean2Set;
  }
  return deskflow::input::InputIndicatorSource::Unknown;
}

bool shouldActivateMacInputSourceTransition(
    deskflow::input::InputIndicatorSource previous,
    deskflow::input::InputIndicatorSource current
)
{
  return previous != current &&
         current != deskflow::input::InputIndicatorSource::Unknown;
}

OSXInputSourceSnapshot makeMacInputSourceSnapshot(std::string identifier)
{
  const auto source = classifyMacInputSourceId(identifier);
  return {.identifier = std::move(identifier), .source = source};
}

OSXInputSourceSnapshot copyCurrentMacInputSource()
{
  std::string identifier;
  {
    std::lock_guard<std::mutex> lock(g_tisMutex);
    AutoTISInputSourceRef source(TISCopyCurrentKeyboardInputSource(), CFRelease);
    if (source) {
      const auto value =
          static_cast<CFStringRef>(TISGetInputSourceProperty(source.get(), kTISPropertyInputSourceID));
      char buffer[256] = {};
      if (value && CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        identifier.assign(buffer);
      }
    }
  }
  return makeMacInputSourceSnapshot(std::move(identifier));
}

struct OSXInputSourceMonitor::Impl
{
  explicit Impl(std::function<void()> changedCallback) : changed(std::move(changedCallback))
  {
  }

  static void inputSourceChanged(
      CFNotificationCenterRef, void *observer, CFNotificationName, const void *, CFDictionaryRef
  )
  {
    const auto *self = static_cast<Impl *>(observer);
    if (self && self->changed) {
      self->changed();
    }
  }

  std::function<void()> changed;
};

OSXInputSourceMonitor::OSXInputSourceMonitor(std::function<void()> changed)
    : m_impl(std::make_unique<Impl>(std::move(changed)))
{
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(),
      m_impl.get(),
      &Impl::inputSourceChanged,
      kTISNotifySelectedKeyboardInputSourceChanged,
      nullptr,
      CFNotificationSuspensionBehaviorCoalesce
  );
}

OSXInputSourceMonitor::~OSXInputSourceMonitor()
{
  if (m_impl) {
    CFNotificationCenterRemoveObserver(
        CFNotificationCenterGetDistributedCenter(), m_impl.get(), kTISNotifySelectedKeyboardInputSourceChanged, nullptr
    );
  }
}

} // namespace deskflow::platform
