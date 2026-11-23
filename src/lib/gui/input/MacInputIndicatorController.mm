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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wavailability"
#include <AppKit/NSRunningApplication.h>
#include <AppKit/NSWorkspace.h>
#include <ApplicationServices/ApplicationServices.h>
#pragma clang diagnostic pop
#include <mach/mach_time.h>

#include <algorithm>

#include <QCoreApplication>

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

InputIndicatorTrigger policyTrigger(InputIndicatorUiTrigger trigger)
{
  switch (trigger) {
  case InputIndicatorUiTrigger::CoreActivity:
    return InputIndicatorTrigger::CoreActivity;
  case InputIndicatorUiTrigger::InputSourceChanged:
    return InputIndicatorTrigger::SourceChanged;
  }
}

std::optional<int64_t> copyFrontmostApplicationPid()
{
  if (NSRunningApplication *frontmost =
          [[NSWorkspace sharedWorkspace] frontmostApplication]) {
    return static_cast<int64_t>(frontmost.processIdentifier);
  }
  return std::nullopt;
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
    : MacInputIndicatorController(parent, Dependencies{})
{
}

MacInputIndicatorController::MacInputIndicatorController(
    QObject *parent, Dependencies dependencies
)
    : QObject(parent),
      m_probe(std::make_unique<MacAccessibilityProbe>(this)),
      m_panel(std::make_unique<MacInputIndicatorPanel>()),
      m_probeTimer(this),
      m_dwellTimer(this)
{
  m_frontmostApplicationPid =
      dependencies.frontmostApplicationPid
          ? std::move(dependencies.frontmostApplicationPid)
          : [] { return copyFrontmostApplicationPid(); };
  m_accessibilityTrusted =
      dependencies.accessibilityTrusted
          ? std::move(dependencies.accessibilityTrusted)
          : [] { return static_cast<bool>(AXIsProcessTrusted()); };
  m_probeRequest =
      dependencies.probeRequest
          ? std::move(dependencies.probeRequest)
          : [this](
                uint64_t generation,
                std::optional<int64_t> targetPid
            ) {
              m_probe->request(generation, targetPid);
            };
  m_inputSource =
      dependencies.inputSource
          ? std::move(dependencies.inputSource)
          : [] {
              return deskflow::platform::copyCurrentMacInputSource();
            };
  m_panelShow =
      dependencies.panelShow
          ? std::move(dependencies.panelShow)
          : [this](const MacInputIndicatorPanelOptions &options) {
              return m_panel->show(options);
            };
  m_panelHide =
      dependencies.panelHide
          ? std::move(dependencies.panelHide)
          : [this] { m_panel->hide(); };
  m_diagnosticEmitter =
      dependencies.diagnosticEmitter
          ? std::move(dependencies.diagnosticEmitter)
          : [](InputIndicatorVisibilityReason reason) {
              qDebug(
                  "input indicator hidden: %s",
                  inputIndicatorVisibilityReasonText(reason)
              );
            };

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
    ++m_generation;
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    m_probeTimer.stop();
    hide();
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
  setEffectiveEnabled(m_wantedEnabled && m_accessibilityTrusted());
  if (m_wantedEnabled && !m_accessibilityTrusted()) {
    reportHiddenReason(InputIndicatorVisibilityReason::AccessibilityUnavailable);
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
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    m_visibleTrigger.reset();
  }
  m_panelHide();
  Q_EMIT enabledChanged(enabled);
}

void MacInputIndicatorController::createMonitors()
{
  m_source = m_inputSource().source;
  if (!m_focusMonitor) {
    m_focusMonitor = std::make_unique<MacInputFocusMonitor>(
        [this] { focusChanged(); }
    );
  }
  if (!m_sourceMonitor) {
    m_sourceMonitor = std::make_unique<deskflow::platform::OSXInputSourceMonitor>(
        [this] { inputSourceChanged(); }
    );
  }
}

void MacInputIndicatorController::destroyMonitors()
{
  m_focusMonitor.reset();
  m_sourceMonitor.reset();
  m_source = deskflow::input::InputIndicatorSource::Unknown;
}

void MacInputIndicatorController::focusChanged()
{
  ++m_generation;
  m_pendingTrigger.reset();
  m_pendingSource.reset();
  m_pendingTargetPid.reset();
  m_probeTimer.stop();
  hide();
}

void MacInputIndicatorController::inputSourceChanged()
{
  const auto previous = m_source;
  const auto current = m_inputSource().source;
  m_source = current;

  if (current == deskflow::input::InputIndicatorSource::Unknown) {
    ++m_generation;
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    m_probeTimer.stop();
    reportHiddenReason(InputIndicatorVisibilityReason::SourceUnknown);
    hide();
    return;
  }
  if (!deskflow::platform::shouldActivateMacInputSourceTransition(
          previous, current
      )) {
    return;
  }
  trigger(InputIndicatorUiTrigger::InputSourceChanged);
}

void MacInputIndicatorController::trigger(InputIndicatorUiTrigger trigger)
{
  if (!m_effectiveEnabled ||
      (inputIndicatorTriggerRequiresActivity(policyTrigger(trigger)) &&
       !m_lastActivity)) {
    return;
  }
  const auto source = m_inputSource().source;
  if (source == deskflow::input::InputIndicatorSource::Unknown) {
    ++m_generation;
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    m_probeTimer.stop();
    reportHiddenReason(InputIndicatorVisibilityReason::SourceUnknown);
    hide();
    return;
  }
  m_source = source;
  m_pendingTrigger = trigger;
  m_pendingSource = source;
  m_pendingTargetPid.reset();
  ++m_generation;
  hide();
  m_probeTimer.start();
}

void MacInputIndicatorController::requestAccessibility()
{
  m_probeTimer.stop();
  if (!m_effectiveEnabled) {
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    hide();
    return;
  }
  if (!m_accessibilityTrusted()) {
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    reportHiddenReason(InputIndicatorVisibilityReason::AccessibilityUnavailable);
    hide();
    return;
  }
  if (!m_coreConnected) {
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    reportHiddenReason(InputIndicatorVisibilityReason::CoreDisconnected);
    hide();
    return;
  }
  if (!m_isOnComputer) {
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    reportHiddenReason(InputIndicatorVisibilityReason::OffComputer);
    hide();
    return;
  }
  if (!m_pendingTrigger) {
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    hide();
    return;
  }
  if (!m_pendingSource ||
      *m_pendingSource == deskflow::input::InputIndicatorSource::Unknown) {
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    reportHiddenReason(InputIndicatorVisibilityReason::SourceUnknown);
    hide();
    return;
  }
  if (inputIndicatorTriggerRequiresActivity(
          policyTrigger(*m_pendingTrigger)
      ) &&
      !m_lastActivity) {
    reportHiddenReason(InputIndicatorVisibilityReason::NoRecentActivity);
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    hide();
    return;
  }
  const auto frontmostPid = m_frontmostApplicationPid();
  const auto ownPid =
      static_cast<int64_t>(QCoreApplication::applicationPid());
  const auto targetPid =
      macAccessibilityProbeTargetPid(frontmostPid, ownPid);
  if (!targetPid) {
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    reportHiddenReason(
        frontmostPid && *frontmostPid == ownPid
            ? InputIndicatorVisibilityReason::OwnApplication
            : InputIndicatorVisibilityReason::FrontmostChanged
    );
    hide();
    return;
  }
  m_pendingTargetPid = targetPid;
  m_probeRequest(m_generation, targetPid);
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
  m_pendingTrigger.reset();
  m_pendingSource.reset();
  m_pendingTargetPid.reset();
  m_probeTimer.stop();
  hide();
  if (!local) {
    m_lastActivity.reset();
    reportHiddenReason(InputIndicatorVisibilityReason::OffComputer);
    return;
  }
}

void MacInputIndicatorController::inputIndicatorActivityInvalidated()
{
  m_lastActivity.reset();
  const bool pendingActivity = m_pendingTrigger &&
      inputIndicatorTriggerRequiresActivity(
          policyTrigger(*m_pendingTrigger)
      );
  const bool visibleActivity = m_visibleTrigger &&
      inputIndicatorTriggerRequiresActivity(
          policyTrigger(*m_visibleTrigger)
      );
  if (!pendingActivity && !visibleActivity) {
    return;
  }
  ++m_generation;
  if (pendingActivity) {
    m_pendingTrigger.reset();
    m_pendingSource.reset();
    m_pendingTargetPid.reset();
    m_probeTimer.stop();
  }
  reportHiddenReason(InputIndicatorVisibilityReason::EventRejected);
  if (visibleActivity) {
    hide();
  }
}

void MacInputIndicatorController::inputIndicatorStateInvalidated()
{
  m_coreConnected = false;
  m_isOnComputer = false;
  m_lastActivity.reset();
  m_pendingTrigger.reset();
  m_pendingSource.reset();
  m_pendingTargetPid.reset();
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

  const auto pendingTrigger = m_pendingTrigger;
  const auto pendingSource = m_pendingSource;
  const auto pendingTargetPid = m_pendingTargetPid;
  m_pendingTrigger.reset();
  m_pendingSource.reset();
  m_pendingTargetPid.reset();
  if (!pendingTrigger || !pendingSource || !pendingTargetPid) {
    hide();
    return;
  }
  const auto trigger = policyTrigger(*pendingTrigger);
  std::optional<std::chrono::milliseconds> eventAge;
  if (inputIndicatorTriggerRequiresActivity(trigger)) {
    if (!m_lastActivity) {
      reportHiddenReason(InputIndicatorVisibilityReason::NoRecentActivity);
      hide();
      return;
    }
    eventAge = activityAge(*m_lastActivity);
    if (!eventAge) {
      reportHiddenReason(InputIndicatorVisibilityReason::EventRejected);
      hide();
      return;
    }
  }
  const auto frontmostPid = m_frontmostApplicationPid();
  if (!frontmostPid || *frontmostPid != *pendingTargetPid ||
      snapshot.applicationPid != *pendingTargetPid) {
    reportHiddenReason(InputIndicatorVisibilityReason::FrontmostChanged);
    hide();
    return;
  }
  const auto currentSnapshot =
      macAccessibilitySnapshotForFrontmostApplication(
          snapshot, frontmostPid
      );
  if (currentSnapshot.applicationPid <= 0) {
    reportHiddenReason(InputIndicatorVisibilityReason::FrontmostChanged);
    hide();
    return;
  }

  const auto currentSource = m_inputSource().source;
  if (currentSource != *pendingSource) {
    ++m_generation;
    reportHiddenReason(InputIndicatorVisibilityReason::GenerationMismatch);
    hide();
    return;
  }
  m_source = currentSource;
  const InputIndicatorPolicyInput input{
      .enabled = m_effectiveEnabled,
      .accessibilityTrusted = m_accessibilityTrusted(),
      .coreConnected = m_coreConnected,
      .isPrimary = true,
      .isOnComputer = m_isOnComputer,
      .screenKnown = true,
      .focus = currentSnapshot.state,
      .source = *pendingSource,
      .trigger = trigger,
      .probeGeneration = generation,
      .activityGeneration = m_generation,
      .eventAge = eventAge,
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
  if (!m_panelShow({
          .source = decision.source,
          .position = m_position,
          .opacityPercent = m_opacityPercent,
          .tone = m_tone,
      })) {
    reportHiddenReason(InputIndicatorVisibilityReason::PanelUnavailable);
    hide();
    return;
  }

  m_visibleTrigger = pendingTrigger;
  m_dwellTimer.start(static_cast<int>(decision.dwell.count()));
}

void MacInputIndicatorController::hide()
{
  m_dwellTimer.stop();
  m_visibleTrigger.reset();
  m_panelHide();
}

void MacInputIndicatorController::reportHiddenReason(
    InputIndicatorVisibilityReason reason
)
{
  if (!m_diagnosticThrottle.allow(std::chrono::steady_clock::now())) {
    return;
  }
  m_diagnosticEmitter(reason);
}

} // namespace deskflow::gui::input
