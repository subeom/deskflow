/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsMonitors.h"

#include <windows.h>

// not in every set of Windows headers; Windows 8 and later, and Windows 7 with KB2533623, know it
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

namespace deskflow::platform {

namespace {

// MDT_EFFECTIVE_DPI of shellscalingapi.h, which is not included: the library is optional, and loaded by hand
constexpr int kEffectiveDpi = 0;
constexpr UINT kDpiOfOneHundredPercent = 96;

using GetDpiForMonitorFunction = HRESULT(WINAPI *)(HMONITOR, int, UINT *, UINT *);

// GetDpiForMonitor is in shcore.dll, which Windows before 8.1 does not have: it is looked for the first time it is
// asked for, in the system directory only, and never unloaded (it is a system library that every GUI process has)
GetDpiForMonitorFunction getDpiForMonitorFunction()
{
  static const GetDpiForMonitorFunction function = []() -> GetDpiForMonitorFunction {
    HMODULE library = LoadLibraryExW(L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (library == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<GetDpiForMonitorFunction>(
        reinterpret_cast<void *>(GetProcAddress(library, "GetDpiForMonitor"))
    );
  }();
  return function;
}

uint32_t effectiveDpi(HMONITOR monitor)
{
  const auto function = getDpiForMonitorFunction();
  if (function == nullptr) {
    return kDpiOfOneHundredPercent;
  }

  UINT dpiX = 0;
  UINT dpiY = 0;
  if (FAILED(function(monitor, kEffectiveDpi, &dpiX, &dpiY)) || dpiX == 0) {
    return kDpiOfOneHundredPercent;
  }
  return dpiX;
}

BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
  auto *monitors = reinterpret_cast<std::vector<deskflow::monitors::NativeMonitor> *>(data);

  MONITORINFO info = {};
  info.cbSize = sizeof(info);
  if (GetMonitorInfoW(monitor, &info) == 0) {
    // a monitor that went away while it was listed: the others are still listed
    return TRUE;
  }

  deskflow::monitors::NativeMonitor native;
  native.left = info.rcMonitor.left;
  native.top = info.rcMonitor.top;
  native.right = info.rcMonitor.right;
  native.bottom = info.rcMonitor.bottom;
  native.dpi = effectiveDpi(monitor);
  native.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
  monitors->push_back(native);
  return TRUE;
}

} // namespace

std::vector<deskflow::monitors::NativeMonitor> listWindowsMonitors()
{
  std::vector<deskflow::monitors::NativeMonitor> monitors;
  if (EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&monitors)) == 0) {
    monitors.clear();
  }
  return monitors;
}

} // namespace deskflow::platform
