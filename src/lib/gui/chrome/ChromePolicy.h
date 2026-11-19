/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "common/Settings.h"

#include <QString>
#include <QVersionNumber>

#include <optional>

namespace deskflow::gui::chrome {

enum class Platform
{
  Windows,
  MacOS,
  Linux,
  Other
};

//! What the frame of the main window is: the one of the system (a title bar and a menu bar), or the one of the new look
//! (a header that is part of the window, with the buttons of the window in it)
enum class FrameKind
{
  Native,
  ClientSide
};

//! Why a frame was chosen: it is told in the log, and the tests say what each rule does
enum class FrameReason
{
  //! The platform can do the frame of the new look
  Default,
  //! The environment variable DESKFLOW_WINDOW_FRAME says client
  Forced,
  //! The look of the application is the one of the system: the window is what it always was
  SystemLook,
  OptedOutByEnvironment,
  OptedOutBySetting,
  //! A high contrast theme draws the frame of the system, which is what a person who chose it expects
  HighContrast,
  //! The window manager cannot draw what the frame of the new look leaves to it (Windows without composition)
  NoComposition,
  //! The way to ask for a window without a title bar on this platform is newer than the Qt of the build
  QtTooOld,
  //! The platform plugin is not one that has a title bar to take away (offscreen, minimal, ...)
  PlatformPluginUnsupported,
  OtherSystem,
  //! Linux has the frame of the system in the first release (kLinuxClientSideByDefault): the frame of the new look is
  //! chosen with the setting or with the variable
  LinuxNativeByDefault,
  //! The setting gui/nativeWindowFrame was set to false: the person chose the frame of the new look
  ChosenBySetting
};

/*!
 * \brief What the choice of the frame depends on, as plain values: it is made without a window, so that the rules can
 * be tested. current() reads the real ones.
 */
struct ChromeEnvironment
{
  Platform platform = Platform::Other;
  //! The name of the platform plugin of Qt: windows, cocoa, xcb, wayland, offscreen ...
  QString platformPlugin;
  QVersionNumber qtVersion;
  //! The application has the look of the new design (not the look of the system)
  bool deskflowLook = false;
  //! The value of the environment variable DESKFLOW_WINDOW_FRAME: "native", "client" or nothing
  QString frameVariable;
  //! The setting gui/nativeWindowFrame: true is the frame of the system, false the frame of the new look, and nothing
  //! is that the person never chose (what the platform has by default)
  std::optional<bool> nativeFrameSetting;
  bool highContrast = false;
  //! Windows: the compositor of the desktop is on (it always is from Windows 8)
  bool compositionEnabled = true;

  static ChromeEnvironment current();
};

struct FrameDecision
{
  FrameKind kind = FrameKind::Native;
  FrameReason reason = FrameReason::SystemLook;

  bool clientSide() const
  {
    return kind == FrameKind::ClientSide;
  }
};

//! Linux has the frame of the system in the first release: the compositors differ and none of them is tried here (the
//! plan: open question 26, decided). The frame of the new look is there for whoever chooses it, with the setting or
//! with DESKFLOW_WINDOW_FRAME=client
constexpr bool kLinuxClientSideByDefault = !Settings::kLinuxNativeWindowFrameByDefault;

/*!
 * \brief The frame for an environment. The order of the rules is the order of their safety: the look of the system, the
 * ways out that a person has (the environment, the setting), what the system says (high contrast, composition), the
 * trial that forces the frame, and last what the platform can do.
 */
FrameDecision decideFrame(const ChromeEnvironment &environment);

/*!
 * \brief Whether the person wants the frame of the new look: what the setting and the variable say, and where they say
 * nothing what the platform has by default. It is what the switch of the settings shows. It is not what the window
 * gets: the system can say no (high contrast, a platform plugin that has no frame to replace), and decideFrame() says
 * what the window gets. (The look of the application is not asked: the switch is in the pages of the new look, and what
 * it says is about the frame of the new look.)
 */
bool prefersDeskflowFrame(const ChromeEnvironment &environment);

//! The reason in a few words, for the log
QString reasonName(FrameReason reason);

} // namespace deskflow::gui::chrome
