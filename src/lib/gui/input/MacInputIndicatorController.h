/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "InputIndicatorPolicy.h"
#include "MacInputIndicatorPanel.h"
#include "MacInputPointerMonitor.h"
#include "deskflow/ipc/InputIndicatorIpc.h"
#include "platform/OSXInputSource.h"

#include <QObject>
#include <QTimer>

#include <chrono>
#include <functional>
#include <memory>

struct MacInputIndicatorControllerTestAccess;

namespace deskflow::gui::input {

class MacInputIndicatorController : public QObject
{
  Q_OBJECT

public:
  struct Dependencies
  {
    std::function<deskflow::platform::OSXInputSourceSnapshot()>
        inputSource;
    std::function<bool(
        const MacInputIndicatorPanelOptions &options
    )> panelShow;
    std::function<MacInputIndicatorPanelMoveResult(
        InputIndicatorPosition
    )> panelMove;
    std::function<void(int)> panelSetFollowSpeed;
    std::function<void(double)> panelSetIdleGrowthMaxScale;
    std::function<void()> panelHide;
    std::function<MacInputPointerMonitorHandle(
        std::function<void()>
    )> pointerMonitorStart;
    std::function<void(InputIndicatorVisibilityReason)> diagnosticEmitter;
  };

  explicit MacInputIndicatorController(QObject *parent = nullptr);
  MacInputIndicatorController(QObject *parent, Dependencies dependencies);
  ~MacInputIndicatorController() override;

  bool isEnabled() const
  {
    return m_effectiveEnabled;
  }

public Q_SLOTS:
  void inputIndicatorActivityReceived(const deskflow::input::InputIndicatorActivity &activity);
  void inputIndicatorLocalChanged(bool local);
  void inputIndicatorActivityInvalidated();
  void inputIndicatorStateInvalidated();

Q_SIGNALS:
  void enabledChanged(bool enabled);

private Q_SLOTS:
  void settingsChanged(const QString &key);
  void movePointerBadge();

private:
  friend struct ::MacInputIndicatorControllerTestAccess;

  void applySettings();
  void setEffectiveEnabled(bool enabled);
  void createMonitors();
  void destroyMonitors();
  void inputSourceChanged();
  void pointerTargetChanged();
  void refreshVisibility(bool reconfigure);
  void hide();
  void reportHiddenReason(InputIndicatorVisibilityReason reason);

  bool m_effectiveEnabled = false;
  bool m_wantedEnabled = false;
  bool m_isOnComputer = true;
  bool m_panelConfigured = false;
  bool m_panelVisible = false;
  bool m_pointerMonitorComplete = false;
  InputIndicatorCompanion m_companion =
      InputIndicatorCompanion::Cat;
  InputIndicatorPosition m_position = InputIndicatorPosition::CursorBottomRight;
  int m_opacityPercent = macInputIndicatorPanelStyle().opacityPercent;
  int m_followSpeedPercent = 100;
  double m_idleGrowthMaxScale = kInputIndicatorIdleGrowthMaximumScale;
  InputIndicatorTone m_tone = InputIndicatorTone::Automatic;
  deskflow::platform::OSXInputSourceSnapshot m_sourceSnapshot;
  std::unique_ptr<deskflow::platform::OSXInputSourceMonitor> m_sourceMonitor;
  std::unique_ptr<MacInputIndicatorPanel> m_panel;
  QTimer m_renderTimer;
  QTimer m_fallbackTimer;
  InputIndicatorDiagnosticThrottle m_diagnosticThrottle;
  std::function<deskflow::platform::OSXInputSourceSnapshot()>
      m_inputSource;
  std::function<bool(
      const MacInputIndicatorPanelOptions &options
  )> m_panelShow;
  std::function<MacInputIndicatorPanelMoveResult(
      InputIndicatorPosition
  )> m_panelMove;
  std::function<void(int)> m_panelSetFollowSpeed;
  std::function<void(double)> m_panelSetIdleGrowthMaxScale;
  std::function<void()> m_panelHide;
  std::function<MacInputPointerMonitorHandle(
      std::function<void()>
  )> m_pointerMonitorStart;
  std::function<void()> m_pointerMonitorStop;
  std::function<void(InputIndicatorVisibilityReason)>
      m_diagnosticEmitter;

};

} // namespace deskflow::gui::input
