/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MacInputIndicatorController.h"

#include "InputIndicatorPolicy.h"
#include "common/Settings.h"
#include "platform/OSXInputSource.h"

#include <algorithm>
#include <utility>

namespace deskflow::gui::input {

namespace {

constexpr int kRenderFrameIntervalMs = 8;
constexpr int kFallbackFrameIntervalMs = 16;

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

} // namespace

MacInputIndicatorController::MacInputIndicatorController(QObject *parent)
    : MacInputIndicatorController(parent, Dependencies{})
{
}

MacInputIndicatorController::MacInputIndicatorController(
    QObject *parent, Dependencies dependencies
)
    : QObject(parent),
      m_renderTimer(this),
      m_fallbackTimer(this)
{
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
              return m_panel && m_panel->show(options);
            };
  m_panelMove =
      dependencies.panelMove
          ? std::move(dependencies.panelMove)
          : [this](InputIndicatorPosition position) {
              return m_panel
                  ? m_panel->move(position)
                  : MacInputIndicatorPanelMoveResult{};
            };
  m_panelHide =
      dependencies.panelHide
          ? std::move(dependencies.panelHide)
          : [this] {
              if (m_panel) {
                m_panel->hide();
              }
            };
  m_pointerMonitorStart =
      dependencies.pointerMonitorStart
          ? std::move(dependencies.pointerMonitorStart)
          : [](std::function<void()> changed) {
              return startMacInputPointerMonitor(std::move(changed));
            };
  m_diagnosticEmitter =
      dependencies.diagnosticEmitter
          ? std::move(dependencies.diagnosticEmitter)
          : [](InputIndicatorVisibilityReason reason) {
              qDebug(
                  "input indicator hidden: %s",
                  inputIndicatorVisibilityReasonText(reason)
              );
            };

  m_renderTimer.setInterval(kRenderFrameIntervalMs);
  m_renderTimer.setTimerType(Qt::PreciseTimer);
  connect(
      &m_renderTimer, &QTimer::timeout, this,
      &MacInputIndicatorController::movePointerBadge
  );
  m_fallbackTimer.setInterval(kFallbackFrameIntervalMs);
  m_fallbackTimer.setTimerType(Qt::PreciseTimer);
  connect(
      &m_fallbackTimer, &QTimer::timeout, this,
      &MacInputIndicatorController::movePointerBadge
  );
  connect(
      Settings::instance(), &Settings::settingsChanged, this,
      &MacInputIndicatorController::settingsChanged
  );
  applySettings();
}

MacInputIndicatorController::~MacInputIndicatorController()
{
  m_renderTimer.stop();
  m_fallbackTimer.stop();
  destroyMonitors();
  hide();
}

void MacInputIndicatorController::settingsChanged(const QString &key)
{
  if (key == Settings::Gui::InputIndicatorEnabled ||
      key == Settings::Gui::InputIndicatorCompanion ||
      key == Settings::Gui::InputIndicatorPosition ||
      key == Settings::Gui::InputIndicatorOpacityPercent ||
      key == Settings::Gui::InputIndicatorTone) {
    m_panelConfigured = false;
    hide();
    applySettings();
  }
}

void MacInputIndicatorController::applySettings()
{
  m_wantedEnabled =
      Settings::value(Settings::Gui::InputIndicatorEnabled).toBool();
  m_companion = inputIndicatorCompanionFromSetting(
      Settings::value(Settings::Gui::InputIndicatorCompanion).toString()
  );
  m_position = positionFromSetting(
      Settings::value(Settings::Gui::InputIndicatorPosition).toString()
  );
  m_opacityPercent = std::clamp(
      Settings::value(Settings::Gui::InputIndicatorOpacityPercent).toInt(),
      60, 100
  );
  m_tone = toneFromSetting(
      Settings::value(Settings::Gui::InputIndicatorTone).toString()
  );
  setEffectiveEnabled(m_wantedEnabled);
  if (m_effectiveEnabled) {
    refreshVisibility(true);
  }
}

void MacInputIndicatorController::setEffectiveEnabled(bool enabled)
{
  if (m_effectiveEnabled == enabled) {
    return;
  }
  m_effectiveEnabled = enabled;
  m_renderTimer.stop();
  m_fallbackTimer.stop();
  m_panelConfigured = false;
  hide();
  if (enabled) {
    createMonitors();
  } else {
    destroyMonitors();
  }
  Q_EMIT enabledChanged(enabled);
}

void MacInputIndicatorController::createMonitors()
{
  if (!m_panel) {
    m_panel = std::make_unique<MacInputIndicatorPanel>(
        [this] { pointerTargetChanged(); }
    );
  }
  m_sourceSnapshot = m_inputSource();
  if (!m_sourceMonitor) {
    m_sourceMonitor =
        std::make_unique<deskflow::platform::OSXInputSourceMonitor>(
            [this] { inputSourceChanged(); }
        );
  }
  if (!m_pointerMonitorStop) {
    const auto handle = m_pointerMonitorStart(
        [this] { pointerTargetChanged(); }
    );
    m_pointerMonitorComplete = handle.complete;
    m_pointerMonitorStop = handle.stop;
  }
}

void MacInputIndicatorController::destroyMonitors()
{
  m_renderTimer.stop();
  m_fallbackTimer.stop();
  if (m_pointerMonitorStop) {
    m_pointerMonitorStop();
    m_pointerMonitorStop = {};
  }
  m_pointerMonitorComplete = false;
  m_sourceMonitor.reset();
  m_sourceSnapshot = {};
  m_panel.reset();
}

void MacInputIndicatorController::inputSourceChanged()
{
  if (!m_effectiveEnabled) {
    return;
  }
  auto current = m_inputSource();
  if (current.identifier == m_sourceSnapshot.identifier &&
      current.source == m_sourceSnapshot.source) {
    return;
  }
  m_sourceSnapshot = std::move(current);
  m_panelConfigured = false;
  refreshVisibility(true);
}

void MacInputIndicatorController::refreshVisibility(bool reconfigure)
{
  if (!m_effectiveEnabled) {
    m_renderTimer.stop();
    m_fallbackTimer.stop();
    hide();
    return;
  }

  const auto decision = InputIndicatorPolicy::decidePersistent({
      .enabled = m_effectiveEnabled,
      .isOnComputer = m_isOnComputer,
      .source = m_sourceSnapshot.source,
  });
  if (!decision.show) {
    m_renderTimer.stop();
    m_fallbackTimer.stop();
    hide();
    reportHiddenReason(decision.reason);
    return;
  }

  if (m_pointerMonitorComplete) {
    m_fallbackTimer.stop();
  } else if (!m_fallbackTimer.isActive()) {
    m_fallbackTimer.start(kFallbackFrameIntervalMs);
  }
  if (reconfigure || !m_panelConfigured) {
    const bool presented = m_panelShow({
        .source = decision.source,
        .companion = m_companion,
        .position = m_position,
        .opacityPercent = m_opacityPercent,
        .tone = m_tone,
    });
    m_panelConfigured = true;
    m_panelVisible = presented;
    if (!presented) {
      hide();
      reportHiddenReason(
          InputIndicatorVisibilityReason::PanelUnavailable
      );
      return;
    }
  }
  pointerTargetChanged();
}

void MacInputIndicatorController::pointerTargetChanged()
{
  if (!m_effectiveEnabled ||
      !m_isOnComputer ||
      m_sourceSnapshot.source ==
          deskflow::input::InputIndicatorSource::Unknown) {
    return;
  }
  if (m_renderTimer.isActive()) {
    return;
  }
  m_renderTimer.start(kRenderFrameIntervalMs);
  movePointerBadge();
}

void MacInputIndicatorController::movePointerBadge()
{
  if (!m_effectiveEnabled ||
      !m_isOnComputer ||
      m_sourceSnapshot.source ==
          deskflow::input::InputIndicatorSource::Unknown) {
    return;
  }
  if (!m_panelConfigured) {
    refreshVisibility(true);
    return;
  }
  const auto result = m_panelMove(m_position);
  if (!result.visible) {
    if (m_panelVisible) {
      hide();
    }
    m_renderTimer.stop();
    reportHiddenReason(InputIndicatorVisibilityReason::ScreenUnknown);
    return;
  }
  m_panelVisible = true;
  if (result.animating) {
    if (!m_renderTimer.isActive()) {
      m_renderTimer.start(kRenderFrameIntervalMs);
    }
  } else {
    m_renderTimer.stop();
  }
}

void MacInputIndicatorController::inputIndicatorActivityReceived(
    const deskflow::input::InputIndicatorActivity &
)
{
  // Legacy activity IPC is not a trigger for persistent visibility.
}

void MacInputIndicatorController::inputIndicatorLocalChanged(bool local)
{
  m_isOnComputer = local;
  refreshVisibility(false);
}

void MacInputIndicatorController::inputIndicatorActivityInvalidated()
{
  // Stale legacy activity cannot invalidate a persistent badge.
}

void MacInputIndicatorController::inputIndicatorStateInvalidated()
{
  m_isOnComputer = true;
  refreshVisibility(false);
}

void MacInputIndicatorController::hide()
{
  m_panelHide();
  m_panelVisible = false;
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
