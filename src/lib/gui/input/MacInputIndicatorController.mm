/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacInputIndicatorController.h"

#include "InputIndicatorPolicy.h"
#include "MacInputFocusMonitor.h"
#include "common/Settings.h"
#include "platform/OSXInputSource.h"

#include <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <mach/mach_time.h>

#include <algorithm>

namespace deskflow::gui::input {

namespace {

InputIndicatorPosition positionFromSetting(const QString &value)
{
  if (value == QStringLiteral("cursorTopRight")) {
    return InputIndicatorPosition::CursorTopRight;
  }
  if (value == QStringLiteral("cursorBottomLeft")) {
    return InputIndicatorPosition::CursorBottomLeft;
  }
  if (value == QStringLiteral("cursorTopLeft")) {
    return InputIndicatorPosition::CursorTopLeft;
  }
  return InputIndicatorPosition::CursorBottomRight;
}

InputIndicatorTone toneFromSetting(const QString &value)
{
  if (value == QStringLiteral("light")) {
    return InputIndicatorTone::Light;
  }
  if (value == QStringLiteral("dark")) {
    return InputIndicatorTone::Dark;
  }
  return InputIndicatorTone::Automatic;
}

std::optional<std::chrono::milliseconds> activityAge(
    const deskflow::input::InputIndicatorActivity &activity
)
{
  mach_timebase_info_data_t timebase{};
  if (mach_timebase_info(&timebase) != KERN_SUCCESS || timebase.numer == 0) {
    return std::nullopt;
  }
  const auto now = mach_absolute_time();
  if (now < activity.eventTimestampMach) {
    return std::nullopt;
  }
  const auto elapsedNanoseconds =
      (now - activity.eventTimestampMach) * static_cast<uint64_t>(timebase.numer) /
      static_cast<uint64_t>(timebase.denom);
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::nanoseconds(elapsedNanoseconds)
  );
}

} // namespace

MacInputIndicatorController::MacInputIndicatorController(QObject *parent)
    : QObject(parent),
      m_probe(std::make_unique<MacAccessibilityProbe>(this)),
      m_panel(std::make_unique<MacInputIndicatorPanel>()),
      m_probeTimer(this),
      m_dwellTimer(this)
{
  m_probeTimer.setSingleShot(true);
  m_probeTimer.setInterval(30);
  connect(&m_probeTimer, &QTimer::timeout, this, &MacInputIndicatorController::requestAccessibility);

  m_dwellTimer.setSingleShot(true);
  connect(&m_dwellTimer, &QTimer::timeout, this, &MacInputIndicatorController::hide);

  connect(
      m_probe.get(), &MacAccessibilityProbe::completed, this, &MacInputIndicatorController::accessibilityCompleted
  );
  connect(Settings::instance(), &Settings::settingsChanged, this, &MacInputIndicatorController::settingsChanged);
  applySettings();
}

MacInputIndicatorController::~MacInputIndicatorController()
{
  destroyMonitors();
  hide();
}

void MacInputIndicatorController::settingsChanged(const QString &key)
{
  if (key == Settings::Gui::InputIndicatorEnabled || key == Settings::Gui::InputIndicatorPosition ||
      key == Settings::Gui::InputIndicatorDwellMs ||
      key == Settings::Gui::InputIndicatorOpacityPercent ||
      key == Settings::Gui::InputIndicatorTone) {
    applySettings();
  }
}

void MacInputIndicatorController::applySettings()
{
  m_wantedEnabled = Settings::value(Settings::Gui::InputIndicatorEnabled).toBool();
  m_position = positionFromSetting(Settings::value(Settings::Gui::InputIndicatorPosition).toString());
  m_dwell = std::chrono::milliseconds(
      std::clamp(Settings::value(Settings::Gui::InputIndicatorDwellMs).toInt(), 600, 3000)
  );
  m_opacityPercent = std::clamp(Settings::value(Settings::Gui::InputIndicatorOpacityPercent).toInt(), 60, 100);
  m_tone = toneFromSetting(Settings::value(Settings::Gui::InputIndicatorTone).toString());
  setEffectiveEnabled(m_wantedEnabled && AXIsProcessTrusted());
  if (m_wantedEnabled && !AXIsProcessTrusted()) {
    reportHiddenReason(InputIndicatorVisibilityReason::AccessibilityUnavailable);
  }
  if (m_effectiveEnabled) {
    trigger(InputIndicatorUiTrigger::InputSourceChanged);
  }
}

void MacInputIndicatorController::setEffectiveEnabled(bool enabled)
{
  if (m_effectiveEnabled == enabled) {
    return;
  }
  m_effectiveEnabled = enabled;
  ++m_generation;
  m_probeTimer.stop();
  m_dwellTimer.stop();
  if (enabled) {
    createMonitors();
  } else {
    destroyMonitors();
    m_coreConnected = false;
    m_isOnComputer = false;
    m_lastActivity.reset();
  }
  m_panel->hide();
  Q_EMIT enabledChanged(enabled);
}

void MacInputIndicatorController::createMonitors()
{
  if (!m_focusMonitor) {
    m_focusMonitor = std::make_unique<MacInputFocusMonitor>(
        [this] { trigger(InputIndicatorUiTrigger::FocusedElementChanged); }
    );
  }
  if (!m_sourceMonitor) {
    m_sourceMonitor = std::make_unique<deskflow::platform::OSXInputSourceMonitor>(
        [this] {
          const auto source = deskflow::platform::copyCurrentMacInputSource();
          m_source = source.source;
          if (m_source == deskflow::input::InputIndicatorSource::Unknown) {
            hide();
          }
          trigger(InputIndicatorUiTrigger::InputSourceChanged);
        }
    );
  }
  m_source = deskflow::platform::copyCurrentMacInputSource().source;
}

void MacInputIndicatorController::destroyMonitors()
{
  m_focusMonitor.reset();
  m_sourceMonitor.reset();
  m_source = deskflow::input::InputIndicatorSource::Unknown;
}

void MacInputIndicatorController::trigger(InputIndicatorUiTrigger)
{
  if (!m_effectiveEnabled || !m_lastActivity) {
    return;
  }
  ++m_generation;
  hide();
  m_probeTimer.start();
}

void MacInputIndicatorController::requestAccessibility()
{
  if (!m_effectiveEnabled) {
    hide();
    return;
  }
  if (!AXIsProcessTrusted()) {
    reportHiddenReason(InputIndicatorVisibilityReason::AccessibilityUnavailable);
    hide();
    return;
  }
  if (!m_coreConnected) {
    reportHiddenReason(InputIndicatorVisibilityReason::CoreDisconnected);
    hide();
    return;
  }
  if (!m_lastActivity) {
    reportHiddenReason(InputIndicatorVisibilityReason::NoRecentActivity);
    hide();
    return;
  }
  std::optional<int64_t> frontmostPid;
  if (NSRunningApplication *frontmost =
          [[NSWorkspace sharedWorkspace] frontmostApplication]) {
    frontmostPid = static_cast<int64_t>(
        frontmost.processIdentifier
    );
  }
  m_probe->request(m_generation, frontmostPid);
}

void MacInputIndicatorController::inputIndicatorActivityReceived(
    const deskflow::input::InputIndicatorActivity &activity
)
{
  if (!m_effectiveEnabled) {
    return;
  }
  m_coreConnected = true;
  m_lastActivity = activity;
  trigger(InputIndicatorUiTrigger::CoreActivity);
}

void MacInputIndicatorController::inputIndicatorLocalChanged(bool local)
{
  m_coreConnected = true;
  m_isOnComputer = local;
  ++m_generation;
  if (!local) {
    m_lastActivity.reset();
    reportHiddenReason(InputIndicatorVisibilityReason::OffComputer);
    hide();
    return;
  }
}

void MacInputIndicatorController::inputIndicatorActivityInvalidated()
{
  m_lastActivity.reset();
  ++m_generation;
  m_probeTimer.stop();
  reportHiddenReason(InputIndicatorVisibilityReason::EventRejected);
  hide();
}

void MacInputIndicatorController::inputIndicatorStateInvalidated()
{
  m_coreConnected = false;
  m_isOnComputer = false;
  m_lastActivity.reset();
  ++m_generation;
  m_probeTimer.stop();
  reportHiddenReason(InputIndicatorVisibilityReason::CoreDisconnected);
  hide();
}

void MacInputIndicatorController::accessibilityCompleted(
    uint64_t generation, const MacAccessibilitySnapshot &snapshot
)
{
  if (generation != m_generation) {
    return;
  }

  const auto eventAge = m_lastActivity ? activityAge(*m_lastActivity) : std::nullopt;
  if (!eventAge) {
    reportHiddenReason(InputIndicatorVisibilityReason::EventRejected);
    hide();
    return;
  }
  NSRunningApplication *frontmostApplication = [[NSWorkspace sharedWorkspace] frontmostApplication];
  const std::optional<int64_t> frontmostPid =
      frontmostApplication
          ? std::optional<int64_t>{
                static_cast<int64_t>(frontmostApplication.processIdentifier)
            }
          : std::nullopt;
  const auto currentSnapshot =
      macAccessibilitySnapshotForFrontmostApplication(
          snapshot, frontmostPid
      );
  if (currentSnapshot.applicationPid <= 0) {
    reportHiddenReason(InputIndicatorVisibilityReason::FrontmostChanged);
    hide();
    return;
  }

  m_source = deskflow::platform::copyCurrentMacInputSource().source;
  const InputIndicatorPolicyInput input{
      .enabled = m_effectiveEnabled,
      .accessibilityTrusted = static_cast<bool>(AXIsProcessTrusted()),
      .coreConnected = m_coreConnected,
      .isPrimary = true,
      .isOnComputer = m_isOnComputer,
      .screenKnown = true,
      .focus = currentSnapshot.state,
      .source = m_source,
      .probeGeneration = generation,
      .activityGeneration = m_generation,
      .eventAge = *eventAge,
      .maximumEventAge = std::chrono::milliseconds{
          deskflow::input::kInputIndicatorMaximumEventAgeMilliseconds
      },
  };
  const auto decision = InputIndicatorPolicy::decide(input, m_dwell);
  if (!decision.show) {
    reportHiddenReason(decision.reason);
    hide();
    return;
  }
  if (!m_panel->show({
          .source = decision.source,
          .position = m_position,
          .opacityPercent = m_opacityPercent,
          .tone = m_tone,
      })) {
    reportHiddenReason(InputIndicatorVisibilityReason::PanelUnavailable);
    hide();
    return;
  }

  m_dwellTimer.start(static_cast<int>(decision.dwell.count()));
}

void MacInputIndicatorController::hide()
{
  m_dwellTimer.stop();
  m_panel->hide();
}

void MacInputIndicatorController::reportHiddenReason(
    InputIndicatorVisibilityReason reason
)
{
  if (!m_diagnosticThrottle.allow(std::chrono::steady_clock::now())) {
    return;
  }
  qDebug(
      "input indicator hidden: %s",
      inputIndicatorVisibilityReasonText(reason)
  );
}

} // namespace deskflow::gui::input
