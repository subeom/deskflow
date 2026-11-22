/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacInputFocusMonitor.h"

#include <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>

#include <utility>
#include <memory>

namespace deskflow::gui::input {

struct MacInputFocusMonitor::Impl
{
  struct Lifetime
  {
    Impl *owner = nullptr;
    bool active = true;
  };

  explicit Impl(std::function<void()> changedCallback) : changed(std::move(changedCallback))
  {
    lifetime = std::make_shared<Lifetime>();
    lifetime->owner = this;
  }

  ~Impl()
  {
    lifetime->active = false;
    lifetime->owner = nullptr;
    detachObserver();
    if (workspaceToken) {
      [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:workspaceToken];
      workspaceToken = nil;
    }
  }

  static void focusChanged(
      AXObserverRef, AXUIElementRef, CFStringRef, void *context
  )
  {
    const auto *self = static_cast<Impl *>(context);
    if (self && self->changed) {
      self->changed();
    }
  }

  void detachObserver()
  {
    if (observer) {
      if (applicationElement) {
        AXObserverRemoveNotification(observer, applicationElement, kAXFocusedUIElementChangedNotification);
      }
      if (auto *source = AXObserverGetRunLoopSource(observer)) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), source, kCFRunLoopDefaultMode);
      }
      CFRelease(observer);
      observer = nullptr;
    }
    if (applicationElement) {
      CFRelease(applicationElement);
      applicationElement = nullptr;
    }
  }

  void attachObserver(pid_t pid)
  {
    detachObserver();
    if (pid <= 0) {
      return;
    }

    applicationElement = AXUIElementCreateApplication(pid);
    if (!applicationElement) {
      return;
    }
    if (AXObserverCreate(pid, &focusChanged, &observer) != kAXErrorSuccess || !observer) {
      CFRelease(applicationElement);
      applicationElement = nullptr;
      return;
    }
    if (AXObserverAddNotification(
            observer, applicationElement, kAXFocusedUIElementChangedNotification, this
        ) != kAXErrorSuccess) {
      CFRelease(observer);
      observer = nullptr;
      CFRelease(applicationElement);
      applicationElement = nullptr;
      return;
    }
    CFRunLoopAddSource(CFRunLoopGetMain(), AXObserverGetRunLoopSource(observer), kCFRunLoopDefaultMode);
  }

  std::function<void()> changed;
  id workspaceToken = nil;
  AXObserverRef observer = nullptr;
  AXUIElementRef applicationElement = nullptr;
  std::shared_ptr<Lifetime> lifetime;
};

MacInputFocusMonitor::MacInputFocusMonitor(std::function<void()> changed)
    : m_impl(std::make_unique<Impl>(std::move(changed)))
{
  auto *workspace = [NSWorkspace sharedWorkspace];
  if (const auto *application = workspace.frontmostApplication) {
    m_impl->attachObserver(application.processIdentifier);
  }

  std::weak_ptr<Impl::Lifetime> lifetime = m_impl->lifetime;
  m_impl->workspaceToken = [workspace.notificationCenter
      addObserverForName:NSWorkspaceDidActivateApplicationNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification *notification) {
                const auto state = lifetime.lock();
                if (!state || !state->active || !state->owner) {
                  return;
                }
                NSRunningApplication *application = notification.userInfo[NSWorkspaceApplicationKey];
                if (application) {
                  state->owner->attachObserver(application.processIdentifier);
                  state->owner->changed();
                }
              }];
  if (m_impl->changed) {
    m_impl->changed();
  }
}

MacInputFocusMonitor::~MacInputFocusMonitor() = default;

} // namespace deskflow::gui::input
