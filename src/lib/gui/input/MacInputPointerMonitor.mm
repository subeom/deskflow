/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacInputPointerMonitor.h"

#include <AppKit/AppKit.h>

#include <memory>
#include <utility>

namespace deskflow::gui::input {

MacInputPointerMonitorHandle startMacInputPointerMonitor(
    std::function<void()> changed
)
{
  auto callback =
      std::make_shared<std::function<void()>>(std::move(changed));
  const auto mask =
      NSEventMaskMouseMoved |
      NSEventMaskLeftMouseDragged |
      NSEventMaskRightMouseDragged |
      NSEventMaskOtherMouseDragged;

  id localToken =
      [NSEvent addLocalMonitorForEventsMatchingMask:mask
        handler:^NSEvent *(NSEvent *event) {
          (*callback)();
          return event;
        }];
  id globalToken =
      [NSEvent addGlobalMonitorForEventsMatchingMask:mask
        handler:^(NSEvent *) {
          (*callback)();
        }];
  [localToken retain];
  [globalToken retain];

  const auto stop = [localToken, globalToken] {
    if (localToken) {
      [NSEvent removeMonitor:localToken];
      [localToken release];
    }
    if (globalToken) {
      [NSEvent removeMonitor:globalToken];
      [globalToken release];
    }
  };
  return {
      .complete = localToken && globalToken,
      .stop = stop,
  };
}

} // namespace deskflow::gui::input
