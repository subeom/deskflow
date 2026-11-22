/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacAccessibilityProbe.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>

#include <QCoreApplication>
#include <QPointer>
#include <QThreadPool>

#include <optional>
#include <utility>

namespace deskflow::gui::input {

namespace {

template <typename T> class OwnedCF
{
public:
  explicit OwnedCF(T value = nullptr) : m_value(value)
  {
  }

  ~OwnedCF()
  {
    if (m_value) {
      CFRelease(m_value);
    }
  }

  OwnedCF(const OwnedCF &) = delete;
  OwnedCF &operator=(const OwnedCF &) = delete;

  OwnedCF(OwnedCF &&other) noexcept : m_value(std::exchange(other.m_value, nullptr))
  {
  }

  OwnedCF &operator=(OwnedCF &&other) noexcept
  {
    if (this != &other) {
      if (m_value) {
        CFRelease(m_value);
      }
      m_value = std::exchange(other.m_value, nullptr);
    }
    return *this;
  }

  T get() const
  {
    return m_value;
  }

private:
  T m_value;
};

CFTypeRef copyAttribute(AXUIElementRef element, CFStringRef attribute)
{
  CFTypeRef value = nullptr;
  return AXUIElementCopyAttributeValue(element, attribute, &value) == kAXErrorSuccess ? value : nullptr;
}

bool cfStringEquals(CFTypeRef value, CFStringRef expected)
{
  return value && CFGetTypeID(value) == CFStringGetTypeID() &&
         CFStringCompare(reinterpret_cast<CFStringRef>(value), expected, 0) == kCFCompareEqualTo;
}

std::optional<bool> cfBoolean(CFTypeRef value)
{
  if (!value || CFGetTypeID(value) != CFBooleanGetTypeID()) {
    return std::nullopt;
  }
  return CFBooleanGetValue(static_cast<CFBooleanRef>(value));
}

MacAccessibilitySnapshot readFocusedElement()
{
  if (!AXIsProcessTrusted()) {
    return {};
  }

  OwnedCF<AXUIElementRef> system(AXUIElementCreateSystemWide());
  if (!system.get()) {
    return {};
  }

  OwnedCF<CFTypeRef> application(copyAttribute(system.get(), kAXFocusedApplicationAttribute));
  OwnedCF<CFTypeRef> focused;
  if (application.get() && CFGetTypeID(application.get()) == AXUIElementGetTypeID()) {
    focused = OwnedCF<CFTypeRef>(
        copyAttribute(reinterpret_cast<AXUIElementRef>(application.get()), kAXFocusedUIElementAttribute)
    );
  }
  if (!focused.get() || CFGetTypeID(focused.get()) != AXUIElementGetTypeID()) {
    return {};
  }

  const auto element = reinterpret_cast<AXUIElementRef>(focused.get());
  pid_t pid = 0;
  if (AXUIElementGetPid(element, &pid) != kAXErrorSuccess || pid <= 0) {
    return {};
  }

  OwnedCF<CFTypeRef> role(copyAttribute(element, kAXRoleAttribute));
  if (!role.get() || (!cfStringEquals(role.get(), kAXTextFieldRole) &&
                      !cfStringEquals(role.get(), kAXTextAreaRole))) {
    return {.state = EditableFocusState::NotEditable, .applicationPid = pid};
  }

  OwnedCF<CFTypeRef> subrole(copyAttribute(element, kAXSubroleAttribute));
  if (cfStringEquals(subrole.get(), kAXSecureTextFieldSubrole)) {
    return {.state = EditableFocusState::SecureOrProtected, .applicationPid = pid};
  }

  OwnedCF<CFTypeRef> enabledValue(copyAttribute(element, kAXEnabledAttribute));
  OwnedCF<CFTypeRef> focusedValue(copyAttribute(element, kAXFocusedAttribute));
  const auto enabled = cfBoolean(enabledValue.get());
  const auto focusedState = cfBoolean(focusedValue.get());
  if (!enabled || !focusedState) {
    return {.state = EditableFocusState::Unknown, .applicationPid = pid};
  }
  if (!*enabled || !*focusedState) {
    return {.state = EditableFocusState::NotEditable, .applicationPid = pid};
  }

  Boolean settable = false;
  if (AXUIElementIsAttributeSettable(element, kAXValueAttribute, &settable) != kAXErrorSuccess || !settable) {
    return {.state = EditableFocusState::Unknown, .applicationPid = pid};
  }

  return {.state = EditableFocusState::SafeEditable, .applicationPid = pid};
}

} // namespace

MacAccessibilityProbe::MacAccessibilityProbe(QObject *parent) : QObject(parent)
{
}

void MacAccessibilityProbe::request(uint64_t generation)
{
  m_requestedGeneration = generation;
  if (m_running) {
    return;
  }
  m_running = true;

  QPointer<MacAccessibilityProbe> self(this);
  QThreadPool::globalInstance()->start([self, generation] {
    const auto snapshot = readFocusedElement();
    auto *application = QCoreApplication::instance();
    if (!application) {
      return;
    }
    QMetaObject::invokeMethod(
        application,
        [self, generation, snapshot] {
          if (self) {
            self->finish(generation, snapshot);
          }
        },
        Qt::QueuedConnection
    );
  });
}

void MacAccessibilityProbe::finish(uint64_t generation, const MacAccessibilitySnapshot &snapshot)
{
  m_running = false;
  if (generation != m_requestedGeneration) {
    request(m_requestedGeneration);
    return;
  }
  Q_EMIT completed(generation, snapshot);
}

} // namespace deskflow::gui::input
