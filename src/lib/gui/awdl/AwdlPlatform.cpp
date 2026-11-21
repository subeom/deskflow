/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlPlatform.h"

#include "MacAwdlSystem.h"
#include "common/PlatformInfo.h"

#include <QGuiApplication>

namespace deskflow::awdl {

bool platformSupported()
{
  if (s_supportedForTest.has_value())
    return *s_supportedForTest;
  return deskflow::platform::isMac();
}

IAwdlSystem *createSystem(QObject *parent)
{
  if (s_systemForTest)
    return s_systemForTest(parent);
  if (!platformSupported() || QGuiApplication::platformName() != QLatin1String("cocoa"))
    return nullptr;
  return new MacAwdlSystem(parent);
}

} // namespace deskflow::awdl
