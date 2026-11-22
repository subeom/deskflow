/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "MacAccessibilityProbe.h"
#include "MacInputIndicatorPanel.h"
#include "deskflow/ipc/InputIndicatorIpc.h"

#include <QObject>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

namespace deskflow::platform {
class OSXInputSourceMonitor;
}

namespace deskflow::gui::input {

class MacInputFocusMonitor;

enum class InputIndicatorUiTrigger
{
  CoreActivity,
  FocusedElementChanged,
  InputSourceChanged
};

class MacInputIndicatorController : public QObject
{
  Q_OBJECT

public:
  explicit MacInputIndicatorController(QObject *parent = nullptr);
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
  void accessibilityCompleted(uint64_t generation, const MacAccessibilitySnapshot &snapshot);
  void requestAccessibility();

private:
  void applySettings();
  void setEffectiveEnabled(bool enabled);
  void createMonitors();
  void destroyMonitors();
  void trigger(InputIndicatorUiTrigger trigger);
  void hide();

  bool m_effectiveEnabled = false;
  bool m_wantedEnabled = false;
  bool m_coreConnected = false;
  bool m_isOnComputer = false;
  uint64_t m_generation = 0;
  std::chrono::milliseconds m_dwell{1500};
  InputIndicatorPosition m_position = InputIndicatorPosition::CursorBottomRight;
  int m_opacityPercent = 85;
  InputIndicatorTone m_tone = InputIndicatorTone::Automatic;
  deskflow::input::InputIndicatorSource m_source = deskflow::input::InputIndicatorSource::Unknown;
  std::optional<deskflow::input::InputIndicatorActivity> m_lastActivity;

  std::unique_ptr<MacAccessibilityProbe> m_probe;
  std::unique_ptr<MacInputFocusMonitor> m_focusMonitor;
  std::unique_ptr<deskflow::platform::OSXInputSourceMonitor> m_sourceMonitor;
  std::unique_ptr<MacInputIndicatorPanel> m_panel;
  QTimer m_probeTimer;
  QTimer m_dwellTimer;
};

} // namespace deskflow::gui::input
