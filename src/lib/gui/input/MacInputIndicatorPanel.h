/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "InputIndicatorGeometry.h"
#include "deskflow/ipc/InputIndicatorIpc.h"

#include <memory>

namespace deskflow::gui::input {

enum class InputIndicatorTone
{
  Automatic,
  Light,
  Dark
};

struct MacInputIndicatorPanelOptions
{
  deskflow::input::InputIndicatorSource source = deskflow::input::InputIndicatorSource::Unknown;
  InputIndicatorPosition position = InputIndicatorPosition::CursorBottomRight;
  int opacityPercent = 85;
  InputIndicatorTone tone = InputIndicatorTone::Automatic;
};

enum class MacInputIndicatorPanelOrderCommand
{
  FrontRegardless
};

template <typename Dispatch>
void requestMacInputIndicatorPanelPresentation(Dispatch dispatch)
{
  dispatch(MacInputIndicatorPanelOrderCommand::FrontRegardless);
}

class MacInputIndicatorPanel
{
public:
  MacInputIndicatorPanel();
  ~MacInputIndicatorPanel();

  MacInputIndicatorPanel(const MacInputIndicatorPanel &) = delete;
  MacInputIndicatorPanel &operator=(const MacInputIndicatorPanel &) = delete;

  bool show(const MacInputIndicatorPanelOptions &options);
  void hide();

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace deskflow::gui::input
