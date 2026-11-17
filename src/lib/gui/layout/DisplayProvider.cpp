/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/layout/DisplayProvider.h"

#include <QGuiApplication>
#include <QScreen>

#include <algorithm>
#include <cmath>

#ifdef Q_OS_MACOS
#include "platform/OSXDisplays.h"
#endif

namespace deskflow::gui::layout {

namespace {

// the names that a platform gives the screen of a laptop
bool looksBuiltIn(const QString &name)
{
  for (const auto &prefix : {QStringLiteral("eDP"), QStringLiteral("LVDS"), QStringLiteral("DSI"), QStringLiteral("Built")}) {
    if (name.startsWith(prefix, Qt::CaseInsensitive))
      return true;
  }
  return false;
}

#ifdef Q_OS_MACOS
class OsxDisplayProvider : public IDisplayProvider
{
public:
  std::vector<DisplayInfo> displays() const override
  {
    std::vector<DisplayInfo> displays;
    for (const auto &detail : deskflow::platform::osxActiveDisplayDetails()) {
      DisplayInfo d;
      d.uuid = QString::fromStdString(detail.uuid);
      d.vendor = detail.vendor;
      d.model = detail.model;
      d.serial = detail.serial;
      d.builtIn = detail.builtIn;
      d.primary = detail.info.primary;
      d.x = detail.info.rect.x;
      d.y = detail.info.rect.y;
      d.w = detail.info.rect.w;
      d.h = detail.info.rect.h;
      d.scalePercent = detail.info.scalePercent;
      d.label = detail.builtIn ? QStringLiteral("Built-in") : QStringLiteral("External");
      displays.push_back(d);
    }
    return displays;
  }
};
#endif

} // namespace

std::vector<DisplayInfo> displaysFromScreens(const QList<QScreen *> &screens, const QScreen *primary)
{
  std::vector<DisplayInfo> displays;
  for (const auto *screen : screens) {
    if (screen == nullptr)
      continue;
    DisplayInfo d;
    const auto geometry = screen->geometry();
    d.x = geometry.x();
    d.y = geometry.y();
    d.w = geometry.width();
    d.h = geometry.height();
    d.scalePercent = std::clamp(static_cast<int>(std::lround(screen->devicePixelRatio() * 100.0)), 0, 1000);
    d.primary = screen == primary;
    d.label = screen->name();
    d.builtIn = looksBuiltIn(screen->name());
    // what Qt knows of the monitor: only when it knows something
    const auto maker = screen->manufacturer().trimmed();
    const auto model = screen->model().trimmed();
    const auto serial = screen->serialNumber().trimmed();
    if (!maker.isEmpty() || !model.isEmpty() || !serial.isEmpty())
      d.uuid = QStringLiteral("qt:%1:%2:%3").arg(maker, model, serial);
    displays.push_back(d);
  }
  return displays;
}

std::vector<DisplayInfo> QtDisplayProvider::displays() const
{
  if (QGuiApplication::instance() == nullptr)
    return {};
  return displaysFromScreens(QGuiApplication::screens(), QGuiApplication::primaryScreen());
}

std::unique_ptr<IDisplayProvider> makePlatformDisplayProvider()
{
#ifdef Q_OS_MACOS
  return std::make_unique<OsxDisplayProvider>();
#else
  return std::make_unique<QtDisplayProvider>();
#endif
}

} // namespace deskflow::gui::layout
