/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacAccessibilityProbe.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#include <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>
#pragma clang diagnostic pop

#include <QCoreApplication>
#include <QPointer>
#include <QThreadPool>

#include <limits>
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

std::optional<int64_t> applicationPidOfElement(AXUIElementRef element)
{
  pid_t pid = 0;
  if (!element ||
      AXUIElementGetPid(element, &pid) != kAXErrorSuccess ||
      pid <= 0) {
    return std::nullopt;
  }
  return static_cast<int64_t>(pid);
}

MacAccessibilitySnapshot readFocusedElementForApplication(int64_t applicationPid)
{
  if (applicationPid <= 0 ||
      applicationPid > std::numeric_limits<pid_t>::max()) {
    return {};
  }

  OwnedCF<AXUIElementRef> application(
      AXUIElementCreateApplication(static_cast<pid_t>(applicationPid))
  );
  if (!application.get()) {
    return {};
  }
  OwnedCF<CFTypeRef> focused(
      copyAttribute(application.get(), kAXFocusedUIElementAttribute)
  );
  if (!focused.get() || CFGetTypeID(focused.get()) != AXUIElementGetTypeID()) {
    return {};
  }

  const auto element = reinterpret_cast<AXUIElementRef>(focused.get());
  const auto elementPid = applicationPidOfElement(element);
  if (!elementPid || *elementPid != applicationPid) {
    return {};
  }

  OwnedCF<CFTypeRef> role(copyAttribute(element, kAXRoleAttribute));
  if (!role.get() || (!cfStringEquals(role.get(), kAXTextFieldRole) &&
                      !cfStringEquals(role.get(), kAXTextAreaRole))) {
    return {
        .state = EditableFocusState::NotEditable,
        .applicationPid = *elementPid
    };
  }

  OwnedCF<CFTypeRef> subrole(copyAttribute(element, kAXSubroleAttribute));
  if (cfStringEquals(subrole.get(), kAXSecureTextFieldSubrole)) {
    return {
        .state = EditableFocusState::SecureOrProtected,
        .applicationPid = *elementPid
    };
  }

  OwnedCF<CFTypeRef> enabledValue(copyAttribute(element, kAXEnabledAttribute));
  OwnedCF<CFTypeRef> focusedValue(copyAttribute(element, kAXFocusedAttribute));
  const auto enabled = cfBoolean(enabledValue.get());
  const auto focusedState = cfBoolean(focusedValue.get());
  if (!enabled || !focusedState) {
    return {.state = EditableFocusState::Unknown, .applicationPid = *elementPid};
  }
  if (!*enabled || !*focusedState) {
    return {
        .state = EditableFocusState::NotEditable,
        .applicationPid = *elementPid
    };
  }

  Boolean settable = false;
  if (AXUIElementIsAttributeSettable(element, kAXValueAttribute, &settable) != kAXErrorSuccess || !settable) {
    return {.state = EditableFocusState::Unknown, .applicationPid = *elementPid};
  }

  return {
      .state = EditableFocusState::SafeEditable,
      .applicationPid = *elementPid
  };
}

MacAccessibilitySnapshot readFocusedElement(
    std::optional<int64_t> frontmostPid
)
{
  if (!AXIsProcessTrusted()) {
    return {};
  }

  std::optional<int64_t> systemWideApplicationPid;
  OwnedCF<AXUIElementRef> system(AXUIElementCreateSystemWide());
  if (system.get()) {
    OwnedCF<CFTypeRef> application(
        copyAttribute(system.get(), kAXFocusedApplicationAttribute)
    );
    if (application.get() &&
        CFGetTypeID(application.get()) == AXUIElementGetTypeID()) {
      systemWideApplicationPid = applicationPidOfElement(
          reinterpret_cast<AXUIElementRef>(application.get())
      );
    }
  }

  for (const auto pid :
       macAccessibilityApplicationPidCandidates(
           systemWideApplicationPid, frontmostPid
       )) {
    const auto snapshot = readFocusedElementForApplication(pid);
    if (snapshot.applicationPid > 0) {
      return snapshot;
    }
  }
  return {};
}

} // namespace

std::vector<int64_t> macAccessibilityApplicationPidCandidates(
    std::optional<int64_t> systemWideApplicationPid,
    std::optional<int64_t> frontmostApplicationPid
)
{
  if (!frontmostApplicationPid || *frontmostApplicationPid <= 0) {
    return {};
  }
  if (systemWideApplicationPid &&
      *systemWideApplicationPid == *frontmostApplicationPid) {
    return {*systemWideApplicationPid};
  }
  return {*frontmostApplicationPid};
}

MacAccessibilitySnapshot macAccessibilitySnapshotForFrontmostApplication(
    const MacAccessibilitySnapshot &snapshot,
    std::optional<int64_t> frontmostApplicationPid
)
{
  if (!frontmostApplicationPid ||
      *frontmostApplicationPid <= 0 ||
      snapshot.applicationPid != *frontmostApplicationPid) {
    return {};
  }
  return snapshot;
}

MacAccessibilityProbe::MacAccessibilityProbe(QObject *parent) : QObject(parent)
{
}

void MacAccessibilityProbe::request(
    uint64_t generation,
    std::optional<int64_t> frontmostApplicationPid
)
{
  m_requestedGeneration = generation;
  m_requestedFrontmostApplicationPid = frontmostApplicationPid;
  if (m_running) {
    return;
  }
  m_running = true;

  QPointer<MacAccessibilityProbe> self(this);
  QThreadPool::globalInstance()->start([self, generation, frontmostApplicationPid] {
    const auto snapshot = readFocusedElement(frontmostApplicationPid);
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
    request(m_requestedGeneration, m_requestedFrontmostApplicationPid);
    return;
  }
  Q_EMIT completed(generation, snapshot);
}

} // namespace deskflow::gui::input
