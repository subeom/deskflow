/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QtGlobal>

#include <cstdint>

namespace deskflow::gui::chrome {

/*!
 * \brief The messages of Windows that the frame of the new look answers, and the rules for answering them.
 *
 * This is the part of the native event filter of Windows that does not need Windows: which message is answered how, and
 * where the answer is written. It is plain code on every system, so that a test on any system can run it with a made-up
 * window (see FrameMessagesTests) and the Windows file has only what asks the system.
 *
 * Why the place of the answer is a rule of its own (the seventh run of the checks of Windows found it: a click on the
 * button of maximizing ended the program with 0xC0000005): Qt 6 gives a native event filter a pointer to the answer,
 * and the pointer is not always there (Qt 6.9.3, qtbase):
 *  - a message that Windows sends to the window procedure of Qt (WM_NCCALCSIZE, WM_GETMINMAXINFO, WM_ERASEBKGND,
 *    WM_NCHITTEST) is filtered by QWindowsContext::windowsProc (qwindowscontext.cpp) with the place of the answer that
 *    Windows waits for: it is there;
 *  - a message that is taken from the queue of messages is filtered by QEventDispatcherWin32::processEvents
 *    (qeventdispatcher_win.cpp, "filterNativeEvent(\"windows_generic_MSG\", &msg, 0)") with a null pointer. The input
 *    messages are these (QWindowsContext::isInputMessage, QTBUG-67095: windowsProc does not filter them again): the
 *    messages of the mouse in the non client area (WM_NCMOUSEMOVE to WM_NCXBUTTONDBLCLK, WM_NCMOUSELEAVE), of the mouse
 *    and the keyboard, WM_PAINT, WM_SYSCOMMAND and others. A filter that returns true for such a message has taken it:
 *    Qt does not hand it to the window procedure, so DefWindowProc does not see it either.
 * So nothing here writes through the pointer without looking at it, and a message that needs an answer is left to
 * Windows when there is no place for it (it comes again through the window procedure, with a place).
 */

//! The numbers are those of winuser.h; WindowChromeWin.cpp checks them with static_assert
enum class FrameMessage : std::uint32_t
{
  EraseBackground = 0x0014,  // WM_ERASEBKGND
  GetMinMaxInfo = 0x0024,    // WM_GETMINMAXINFO
  NcCalcSize = 0x0083,       // WM_NCCALCSIZE
  NcHitTest = 0x0084,        // WM_NCHITTEST
  NcMouseMove = 0x00A0,      // WM_NCMOUSEMOVE
  NcLeftButtonDown = 0x00A1, // WM_NCLBUTTONDOWN
  NcLeftButtonUp = 0x00A2,   // WM_NCLBUTTONUP
  NcMouseLeave = 0x02A2      // WM_NCMOUSELEAVE
};

//! The value of HTMAXBUTTON, the part of the window that is the button of maximizing
inline constexpr std::uintptr_t kHitMaximizeButton = 9;

//! How Qt gives a message to a native event filter
enum class Delivery
{
  //! Windows sends it to the window procedure of Qt, which filters it with a place for the answer
  Sent,
  //! Qt takes it from the queue of messages, and filters it with no place for the answer (a null pointer)
  Queued
};

constexpr Delivery deliveryOf(FrameMessage message)
{
  switch (message) {
  case FrameMessage::EraseBackground:
  case FrameMessage::GetMinMaxInfo:
  case FrameMessage::NcCalcSize:
  case FrameMessage::NcHitTest:
    return Delivery::Sent;
  case FrameMessage::NcMouseMove:
  case FrameMessage::NcLeftButtonDown:
  case FrameMessage::NcLeftButtonUp:
  case FrameMessage::NcMouseLeave:
    return Delivery::Queued;
  }
  return Delivery::Queued;
}

//! The answer is written where there is a place for it
inline void giveAnswer(qintptr *place, qintptr answer)
{
  if (place)
    *place = answer;
}

/*!
 * Does with a message what the frame does, and says whether it is handled (the value that the native event filter
 * returns). \p result is where Qt wants the answer, and is null for a message that comes from the queue.
 *
 * \p Host is what answers for the window; it has
 *  - calculateClientArea(wParam, lParam), limitSizes(lParam), paintBackground(wParam): do their part
 *  - qintptr hitCodeAt(lParam): what part of the window the point of the message is (the value of HT...)
 *  - noteMouseOverNonClient(hit), noteMouseLeftNonClient(): where the mouse is (the program draws the button of
 *    maximizing, which the system owns for the flyout of the snap layouts)
 *  - bool pressesMaximizeButton(hit), bool releasesMaximizeButton(hit): the click on the button of maximizing; true
 * when the program takes the click
 */
template <class Host>
bool routeFrameMessage(Host &host, std::uint32_t message, std::uintptr_t wParam, std::intptr_t lParam, qintptr *result)
{
  const auto kind = static_cast<FrameMessage>(message);
  // an answer that Windows waits for cannot be given where there is no place for it: it is left to Windows, and comes
  // again through the window procedure
  if (deliveryOf(kind) == Delivery::Sent && !result)
    return false;

  switch (kind) {
  case FrameMessage::NcCalcSize:
    host.calculateClientArea(wParam, lParam);
    giveAnswer(result, 0);
    return true;

  case FrameMessage::GetMinMaxInfo:
    host.limitSizes(lParam);
    giveAnswer(result, 0);
    return true;

  case FrameMessage::EraseBackground:
    host.paintBackground(wParam);
    giveAnswer(result, 1);
    return true;

  case FrameMessage::NcHitTest:
    giveAnswer(result, host.hitCodeAt(lParam));
    return true;

  case FrameMessage::NcMouseMove:
    host.noteMouseOverNonClient(wParam);
    return false;

  case FrameMessage::NcMouseLeave:
    host.noteMouseLeftNonClient();
    return false;

  // The click on the button of maximizing is the program's: the system would run its own loop of the button and ask for
  // the maximizing itself. The message is taken (true), so it does not go on to the system, and it comes from the queue
  // with no place for an answer
  case FrameMessage::NcLeftButtonDown:
    if (!host.pressesMaximizeButton(wParam))
      return false;
    giveAnswer(result, 0);
    return true;

  case FrameMessage::NcLeftButtonUp:
    if (!host.releasesMaximizeButton(wParam))
      return false;
    giveAnswer(result, 0);
    return true;
  }
  return false;
}

} // namespace deskflow::gui::chrome
