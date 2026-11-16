/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXDisplaysTests.h"

#include "deskflow/MonitorReport.h"
#include "platform/OSXDisplays.h"

#include <QTest>

#include <CoreGraphics/CoreGraphics.h>

#include <algorithm>

// These read the displays of the Mac that runs them (CGGetActiveDisplayList and CGDisplayBounds): no cursor is read or
// moved, no event is posted, nothing is changed. A process without a window server has no displays and skips them.

namespace {

// the shape that OSXComputer::updateComputerShape makes of the displays, written again from the Quartz calls
deskflow::server::MonitorRect shapeOfTheComputer()
{
  CGDisplayCount count = 0;
  if (CGGetActiveDisplayList(0, nullptr, &count) != kCGErrorSuccess || count == 0)
    return {};
  std::vector<CGDirectDisplayID> displays(count);
  if (CGGetActiveDisplayList(count, displays.data(), &count) != kCGErrorSuccess)
    return {};

  CGRect total = CGRectZero;
  for (CGDisplayCount i = 0; i < count; ++i)
    total = CGRectUnion(total, CGDisplayBounds(displays[i]));
  return {
      static_cast<int32_t>(total.origin.x), static_cast<int32_t>(total.origin.y),
      static_cast<int32_t>(total.size.width), static_cast<int32_t>(total.size.height)
  };
}

} // namespace

void OSXDisplaysTests::rectanglesAreThoseOfTheRectangleList()
{
  const auto info = deskflow::platform::osxActiveDisplayInfo();
  const auto rects = deskflow::platform::osxActiveDisplays();

  QCOMPARE(info.size(), rects.size());
  for (size_t i = 0; i < info.size(); ++i)
    QVERIFY(info[i].rect == rects[i]);
}

// what the server checks a report against is the shape in DINF: the box of the monitors has to be that shape
void OSXDisplaysTests::boxOfTheMonitorsIsTheShapeOfTheComputer()
{
  const auto info = deskflow::platform::osxActiveDisplayInfo();
  if (info.empty())
    QSKIP("no display to ask");

  int32_t left = info.front().rect.x;
  int32_t top = info.front().rect.y;
  int32_t right = info.front().rect.right();
  int32_t bottom = info.front().rect.bottom();
  for (const auto &monitor : info) {
    left = std::min(left, monitor.rect.x);
    top = std::min(top, monitor.rect.y);
    right = std::max(right, monitor.rect.right());
    bottom = std::max(bottom, monitor.rect.bottom());
  }
  const auto shape = shapeOfTheComputer();

  QCOMPARE(left, shape.x);
  QCOMPARE(top, shape.y);
  QCOMPARE(right - left, shape.w);
  QCOMPARE(bottom - top, shape.h);
}

void OSXDisplaysTests::scaleAndPrimaryAreReported()
{
  const auto info = deskflow::platform::osxActiveDisplayInfo();
  if (info.empty())
    QSKIP("no display to ask");

  QCOMPARE(std::ranges::count_if(info, [](const auto &monitor) { return monitor.primary; }) >= 1, true);
  for (const auto &monitor : info) {
    // a display has at least a pixel to the point, and no more than a few
    QVERIFY2(
        monitor.scalePercent >= 100 && monitor.scalePercent <= 400, qPrintable(QString::number(monitor.scalePercent))
    );
    QVERIFY(monitor.rect.w > 0 && monitor.rect.h > 0);
  }
}

// the same preparation that a client makes before it reports: the monitors of this Mac tile its shape
void OSXDisplaysTests::monitorsCanBeReported()
{
  const auto info = deskflow::platform::osxActiveDisplayInfo();
  if (info.empty())
    QSKIP("no display to ask");

  const auto prepared = deskflow::monitors::prepareMonitors(info, shapeOfTheComputer());
  QVERIFY2(prepared.ok(), prepared.problem.c_str());
  QVERIFY(!prepared.monitors.empty());
}

QTEST_MAIN(OSXDisplaysTests)
