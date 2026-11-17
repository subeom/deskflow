/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "LayoutTestData.h"
#include "gui/layout/LayoutMemory.h"

#include <QStringList>

#include <optional>
#include <vector>

// A host that is a computer in memory: displays, an arrangement, a core that runs or not, a clock. Nothing real is read
// or changed.

namespace layout_test {

using namespace deskflow::gui::layout;

inline PlacedComputer placed(const QString &name, double x, double y, double zoom = 1.0, bool server = false)
{
  return {name, server, x, y, zoom};
}

// where the computers are when the server has the T
inline Layout layoutForT()
{
  return {{placed("srv", 0, 0, 1.0, true), placed("PC1", -1920, 0), placed("PC2", 3440, 0), placed("MINI", 964, 2422)}};
}

// where they are when the server is the laptop alone
inline Layout layoutForLaptop()
{
  return {{placed("srv", 0, 0, 1.0, true), placed("PC1", -1920, 0), placed("PC2", 1512, -321), placed("MINI", 0, 982)}};
}

// where they are in some third situation
inline Layout layoutForTwoMonitors()
{
  return {{placed("srv", 0, 0, 1.0, true), placed("PC1", 0, -1080), placed("PC2", 1920, 0), placed("MINI", -1920, 0)}};
}

class FakeHost : public ILayoutHost
{
public:
  // the world
  bool isEnabled = true;
  bool canApply = true;
  std::vector<DisplayInfo> shownDisplays = tShape();
  Layout layout = layoutForT();
  bool running = true;
  qint64 wallClock = 1'000'000;
  // what the core was started with: the displays and the arrangement
  QString runningKey;
  // how the fallback answers
  std::optional<Layout> reattachLayout;
  QStringList reattachedNames;
  QStringList notReattachedNames;
  // what goes wrong
  bool applyFails = false;
  bool displaysUnreadable = false;
  bool appliedWhileBlocked = false;

  // what happened
  int applyCalls = 0;
  int restartCalls = 0;
  int displayReads = 0;
  int reattachCalls = 0;
  std::vector<Layout> applied;
  QStringList infoLines;
  QStringList warnLines;
  QStringList debugLines;
  std::vector<Notice> notices;
  std::vector<Millis> restartTimes;
  Millis clockNow = 0;

  FakeHost()
  {
    runningKey = currentKey();
  }

  QString currentKey() const
  {
    // what the core is given: the rectangles of the displays (the links are made from them) and where the computers are
    QStringList parts;
    for (const auto &r : DisplaySignature::fromDisplays(shownDisplays).monitors())
      parts << QStringLiteral("d%1,%2,%3,%4").arg(r.x).arg(r.y).arg(r.w).arg(r.h);
    parts.sort();
    QStringList computers;
    for (const auto &c : layout.computers)
      computers << QStringLiteral("%1@%2,%3,%4").arg(c.name.toLower()).arg(c.x).arg(c.y).arg(c.zoom);
    computers.sort();
    return (parts + computers).join(QLatin1Char('\n'));
  }

  bool enabled() const override
  {
    return isEnabled;
  }
  bool canApplyNow() const override
  {
    return canApply;
  }
  std::vector<DisplayInfo> displays() override
  {
    ++displayReads;
    return displaysUnreadable ? std::vector<DisplayInfo>{} : shownDisplays;
  }
  Layout currentLayout() const override
  {
    return layout;
  }
  ApplyOutcome applyLayout(const Layout &wanted) override
  {
    ++applyCalls;
    if (!canApply)
      appliedWhileBlocked = true;
    ApplyOutcome outcome;
    if (applyFails) {
      outcome.error = QStringLiteral("it does not fit");
      return outcome;
    }
    auto merged = layout;
    for (auto &c : merged.computers) {
      if (const auto *w = wanted.find(c.name)) {
        c.x = w->x;
        c.y = w->y;
        if (!c.isServer)
          c.zoom = w->zoom;
      } else {
        outcome.notInLayout << c.name;
      }
    }
    outcome.applied = true;
    outcome.changed = !merged.sameAs(layout);
    layout = merged;
    applied.push_back(wanted);
    return outcome;
  }
  ReattachOutcome reattach(const DisplaySignature &, const DisplaySignature &) override
  {
    ++reattachCalls;
    return {reattachLayout, reattachedNames, notReattachedNames};
  }
  bool coreRunning() const override
  {
    return running;
  }
  bool coreConfigDiffers() const override
  {
    return currentKey() != runningKey;
  }
  void restartCore() override
  {
    ++restartCalls;
    restartTimes.push_back(clockNow);
    runningKey = currentKey();
  }
  int64_t wallClockMs() const override
  {
    return wallClock;
  }
  void debug(const QString &line) override
  {
    debugLines << line;
  }
  void info(const QString &line) override
  {
    infoLines << line;
  }
  void warn(const QString &line) override
  {
    warnLines << line;
  }
  void notify(const Notice &notice) override
  {
    notices.push_back(notice);
  }
};

} // namespace layout_test
