/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "gui/layout/DisplaySignature.h"

#include <QList>

#include <memory>
#include <vector>

class QScreen;

namespace deskflow::gui::layout {

//! Where the displays of this computer are read from (experimental: layout profiles)
/*!
The one thing about the platform that the remembering of layouts needs. It only asks: no display is changed, no cursor is
moved. A test gives a fake.
*/
class IDisplayProvider
{
public:
  virtual ~IDisplayProvider() = default;
  //! The displays now, nothing when they cannot be asked
  virtual std::vector<DisplayInfo> displays() const = 0;
};

//! The displays as Qt knows them, on any platform
/*!
The identity of a display is what Qt says of the monitor (the maker, the model and the serial number); a platform that says
none of them gives displays that are told apart by their kind and place only. The built in display is told by the name that
the platform gives it (eDP, LVDS, DSI), which is a guess.
*/
std::vector<DisplayInfo> displaysFromScreens(const QList<QScreen *> &screens, const QScreen *primary);

class QtDisplayProvider : public IDisplayProvider
{
public:
  std::vector<DisplayInfo> displays() const override;
};

//! The best that this platform has: on macOS the displays of Quartz with their UUID and numbers, elsewhere Qt's
std::unique_ptr<IDisplayProvider> makePlatformDisplayProvider();

} // namespace deskflow::gui::layout
