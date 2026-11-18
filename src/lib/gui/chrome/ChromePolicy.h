/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QVersionNumber>

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
  OtherSystem
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
  //! The setting gui/nativeWindowFrame
  bool nativeFrameSetting = false;
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

//! Linux has the frame of the new look unless this says no: the compositors differ and none of them is tried here
//! (the plan: open question 26)
constexpr bool kLinuxClientSideByDefault = true;

/*!
 * \brief The frame for an environment. The order of the rules is the order of their safety: the look of the system, the
 * ways out that a person has (the environment, the setting), what the system says (high contrast, composition), the
 * trial that forces the frame, and last what the platform can do.
 */
FrameDecision decideFrame(const ChromeEnvironment &environment);

//! The reason in a few words, for the log
QString reasonName(FrameReason reason);

} // namespace deskflow::gui::chrome
