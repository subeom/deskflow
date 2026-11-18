/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "WindowChromeWin.h"

#ifdef Q_OS_WIN

#include "ChromeGeometry.h"
#include "WindowHeader.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QMainWindow>
#include <QOperatingSystemVersion>

#include <shellapi.h>
#include <windows.h>
#include <windowsx.h>

namespace deskflow::gui::chrome {

namespace {

// What Windows 11 has and the headers of an older kit do not: the corners of a window are round
constexpr DWORD kWindowCornerPreference = 33;
constexpr DWORD kCornerRound = 2;

// MARGINS of the DWM, in the order of its fields
struct DwmMargins
{
  int left;
  int right;
  int top;
  int bottom;
};

using DwmExtendFrameFn = HRESULT(WINAPI *)(HWND, const DwmMargins *);
using DwmSetWindowAttributeFn = HRESULT(WINAPI *)(HWND, DWORD, const void *, DWORD);
using GetDpiForWindowFn = UINT(WINAPI *)(HWND);
using GetSystemMetricsForDpiFn = int(WINAPI *)(int, UINT);

template <class Function> Function procedure(HMODULE module, const char *name)
{
  return module ? reinterpret_cast<Function>(reinterpret_cast<void *>(GetProcAddress(module, name))) : nullptr;
}

// The thickness of the frame that a maximized window is bigger than the monitor by, at the scale of its monitor
int frameThickness(HWND hwnd)
{
  static const auto user32 = GetModuleHandleW(L"user32.dll");
  static const auto dpiOf = procedure<GetDpiForWindowFn>(user32, "GetDpiForWindow");
  static const auto metricOf = procedure<GetSystemMetricsForDpiFn>(user32, "GetSystemMetricsForDpi");
  if (dpiOf && metricOf) {
    const UINT dpi = dpiOf(hwnd);
    return metricOf(SM_CXFRAME, dpi) + metricOf(SM_CXPADDEDBORDER, dpi);
  }
  return GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

// The taskbar that hides itself at an edge of the monitor is brought back by the mouse at that edge: a maximized window
// leaves one pixel of it
void keepAutoHiddenTaskbar(HWND hwnd, RECT &rect)
{
  const auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  if (!monitor || !GetMonitorInfoW(monitor, &info))
    return;
  for (const UINT edge : {ABE_LEFT, ABE_TOP, ABE_RIGHT, ABE_BOTTOM}) {
    APPBARDATA bar{};
    bar.cbSize = sizeof(bar);
    bar.uEdge = edge;
    bar.rc = info.rcMonitor;
    if (!SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &bar))
      continue;
    switch (edge) {
    case ABE_LEFT:
      rect.left += 1;
      break;
    case ABE_TOP:
      rect.top += 1;
      break;
    case ABE_RIGHT:
      rect.right -= 1;
      break;
    default:
      rect.bottom -= 1;
      break;
    }
  }
}

LRESULT hitCodeFor(Region region, bool snapLayouts)
{
  switch (region) {
  case Region::Caption:
    return HTCAPTION;
  // the button of maximizing is the system's for the flyout of the snap layouts; the other buttons are the program's
  case Region::Maximize:
    return snapLayouts ? HTMAXBUTTON : HTCLIENT;
  case Region::Left:
    return HTLEFT;
  case Region::Right:
    return HTRIGHT;
  case Region::Top:
    return HTTOP;
  case Region::Bottom:
    return HTBOTTOM;
  case Region::TopLeft:
    return HTTOPLEFT;
  case Region::TopRight:
    return HTTOPRIGHT;
  case Region::BottomLeft:
    return HTBOTTOMLEFT;
  case Region::BottomRight:
    return HTBOTTOMRIGHT;
  case Region::Minimize:
  case Region::Close:
  case Region::Client:
    break;
  }
  return HTCLIENT;
}

class WindowsFrameFilter : public QObject, public QAbstractNativeEventFilter
{
public:
  WindowsFrameFilter(QMainWindow *window, WindowHeader *header, HWND hwnd, QObject *owner)
      : QObject(owner),
        m_window(window),
        m_header(header),
        m_hwnd(hwnd),
        m_snapLayouts(QOperatingSystemVersion::current().microVersion() >= 22000)
  {
    QCoreApplication::instance()->installNativeEventFilter(this);
  }

  ~WindowsFrameFilter() override
  {
    if (auto *app = QCoreApplication::instance())
      app->removeNativeEventFilter(this);
  }

  bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override
  {
    if (eventType != "windows_generic_MSG")
      return false;
    auto *msg = static_cast<MSG *>(message);
    if (msg->hwnd != m_hwnd)
      return false;

    switch (msg->message) {
    case WM_NCCALCSIZE:
      // the client area is the whole window: there is no title bar and no frame to take a part of it
      if (msg->wParam == TRUE && IsZoomed(m_hwnd)) {
        auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
        RECT &rect = params->rgrc[0];
        const int frame = frameThickness(m_hwnd);
        // Windows makes a maximized window bigger than the monitor by the thickness of its frame
        rect.left += frame;
        rect.top += frame;
        rect.right -= frame;
        rect.bottom -= frame;
        keepAutoHiddenTaskbar(m_hwnd, rect);
      }
      *result = 0;
      return true;

    case WM_NCHITTEST: {
      POINT at{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
      ScreenToClient(m_hwnd, &at);
      RECT client{};
      GetClientRect(m_hwnd, &client);
      const auto dpr = m_window->devicePixelRatioF();

      ChromeMetrics metrics;
      metrics.windowSize = QSize(client.right - client.left, client.bottom - client.top);
      metrics.maximized = IsZoomed(m_hwnd) != FALSE;
      metrics.resizeBorder = qRound(kEdge * dpr);
      metrics.headerHeight = qRound(WindowHeader::kHeight * dpr);
      metrics.buttonWidth = qRound(WindowHeader::kCaptionButtonWidth * dpr);

      QList<QRect> interactive;
      for (const auto &rect : m_header->interactiveRects()) {
        const auto topLeft = m_header->mapTo(m_window, rect.topLeft());
        interactive << QRect(
            qRound(topLeft.x() * dpr), qRound(topLeft.y() * dpr), qRound(rect.width() * dpr),
            qRound(rect.height() * dpr)
        );
      }
      *result = hitCodeFor(hitTest(metrics, QPoint(at.x, at.y), interactive), m_snapLayouts);
      return true;
    }

    // the mouse is over the button of maximizing, which the system has for itself: the program draws what the mouse
    // does
    case WM_NCMOUSEMOVE:
      if (m_snapLayouts && msg->wParam == HTMAXBUTTON) {
        if (!m_hover) {
          m_hover = true;
          TRACKMOUSEEVENT track{};
          track.cbSize = sizeof(track);
          track.dwFlags = TME_LEAVE | TME_NONCLIENT;
          track.hwndTrack = m_hwnd;
          TrackMouseEvent(&track);
        }
        showMaximizeState();
        return false;
      }
      if (m_hover) {
        m_hover = false;
        m_pressed = false;
        showMaximizeState();
      }
      return false;

    case WM_NCMOUSELEAVE:
      m_hover = false;
      m_pressed = false;
      showMaximizeState();
      return false;

    case WM_NCLBUTTONDOWN:
      if (m_snapLayouts && msg->wParam == HTMAXBUTTON) {
        m_pressed = true;
        showMaximizeState();
        *result = 0;
        return true;
      }
      return false;

    case WM_NCLBUTTONUP:
      if (m_snapLayouts && msg->wParam == HTMAXBUTTON) {
        const bool wasPressed = m_pressed;
        m_pressed = false;
        showMaximizeState();
        if (wasPressed)
          ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        *result = 0;
        return true;
      }
      return false;

    default:
      break;
    }
    return false;
  }

private:
  static constexpr int kEdge = 6;

  void showMaximizeState()
  {
    if (auto *button = m_header->captionButton(Region::Maximize))
      button->setNativeState(m_hover, m_pressed);
  }

  QMainWindow *m_window;
  WindowHeader *m_header;
  HWND m_hwnd;
  bool m_snapLayouts;
  bool m_hover = false;
  bool m_pressed = false;
};

} // namespace

bool installWindowsFrame(QMainWindow *window, WindowHeader *header, QObject *owner)
{
  // the window of the system is made now (it is not shown), so that there is a handle to change
  const auto hwnd = reinterpret_cast<HWND>(window->winId());
  if (!hwnd)
    return false;

  static const auto dwm = LoadLibraryW(L"dwmapi.dll");
  const auto extend = procedure<DwmExtendFrameFn>(dwm, "DwmExtendFrameIntoClientArea");
  if (!extend)
    return false;

  new WindowsFrameFilter(window, header, hwnd, owner);

  // one pixel of the frame is the glass of the DWM, which keeps the shadow of the window that has no frame of its own
  const DwmMargins margins{1, 1, 1, 1};
  extend(hwnd, &margins);
  if (QOperatingSystemVersion::current().microVersion() >= 22000) {
    if (const auto attribute = procedure<DwmSetWindowAttributeFn>(dwm, "DwmSetWindowAttribute")) {
      const DWORD preference = kCornerRound;
      attribute(hwnd, kWindowCornerPreference, &preference, sizeof(preference));
    }
  }
  // the system calculates the client area again, now through the filter
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  return true;
}

} // namespace deskflow::gui::chrome

#endif
