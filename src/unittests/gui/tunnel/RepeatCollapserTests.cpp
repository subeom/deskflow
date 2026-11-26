/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "RepeatCollapserTests.h"

#include "gui/tunnel/RepeatCollapser.h"

using namespace deskflow::tunnel;

// A tunnel that is down makes the client try again every second or so, and cloudflared writes the same line for each
// try. The log shows the first one, and says how many more there were, once in a while.

void RepeatCollapserTests::aFirstLine_isShown()
{
  RepeatCollapser collapser(30'000);
  const auto result = collapser.feed(QStringLiteral("failed to connect to origin"), 0);
  QVERIFY(result.show);
  QVERIFY(result.summary.isEmpty());
}

void RepeatCollapserTests::theSameLineAgainInsideTheWindow_isCountedAndNotShown()
{
  RepeatCollapser collapser(30'000);
  QVERIFY(collapser.feed(QStringLiteral("a"), 0).show);
  for (int second = 1; second < 30; ++second) {
    const auto result = collapser.feed(QStringLiteral("a"), second * 1000);
    QVERIFY(!result.show);
    QVERIFY(result.summary.isEmpty());
  }
}

void RepeatCollapserTests::theSameLineAfterTheWindow_isShownWithTheCountOfWhatWasLeftOut()
{
  RepeatCollapser collapser(30'000);
  QVERIFY(collapser.feed(QStringLiteral("a"), 0).show);
  for (int second = 1; second < 30; ++second) {
    collapser.feed(QStringLiteral("a"), second * 1000);
  }
  const auto result = collapser.feed(QStringLiteral("a"), 30'000);
  QVERIFY(result.show);
  QVERIFY2(result.summary.contains(QStringLiteral("29")), qPrintable(result.summary));
  QVERIFY2(result.summary.contains(QStringLiteral("30 s")), qPrintable(result.summary));

  // the window starts again from that line
  QVERIFY(!collapser.feed(QStringLiteral("a"), 31'000).show);
}

void RepeatCollapserTests::aOtherLine_isShownAndFlushesTheCount()
{
  RepeatCollapser collapser(30'000);
  collapser.feed(QStringLiteral("a"), 0);
  collapser.feed(QStringLiteral("a"), 1000);
  collapser.feed(QStringLiteral("a"), 2000);

  const auto result = collapser.feed(QStringLiteral("b"), 3000);
  QVERIFY(result.show);
  QVERIFY2(result.summary.contains(QStringLiteral("2")), qPrintable(result.summary));

  // and "a" after "b" is a new line, shown, with nothing to add
  const auto back = collapser.feed(QStringLiteral("a"), 4000);
  QVERIFY(back.show);
  QVERIFY(back.summary.isEmpty());
}

void RepeatCollapserTests::flush_givesTheCountOnceAndForgetsIt()
{
  RepeatCollapser collapser(30'000);
  QVERIFY(collapser.flush(0).isEmpty());
  collapser.feed(QStringLiteral("a"), 0);
  collapser.feed(QStringLiteral("a"), 1000);
  collapser.feed(QStringLiteral("a"), 4000);
  const auto summary = collapser.flush(5000);
  QVERIFY2(summary.contains(QStringLiteral("2 more times")), qPrintable(summary));
  QVERIFY(collapser.flush(5000).isEmpty());
  // what comes after a flush is shown, it is not a repeat of a line that was reported
  QVERIFY(collapser.feed(QStringLiteral("a"), 6000).show);
  // one line that was left out is "1 more time"
  collapser.feed(QStringLiteral("a"), 7000);
  QVERIFY2(collapser.flush(8000).contains(QStringLiteral("1 more time ")), "singular");
}

// the lines of cloudflared start with the time: the key leaves it out, the caller does (see TunnelSupervisor)
void RepeatCollapserTests::theTimeOfALineInTheKey_doesNotMakeTwoLinesOne()
{
  RepeatCollapser collapser(30'000);
  QVERIFY(collapser.feed(QStringLiteral("x"), 0).show);
  QVERIFY(collapser.feed(QStringLiteral("y"), 1).show);
  QVERIFY(collapser.feed(QStringLiteral("x"), 2).show);
}

void RepeatCollapserTests::reset_forgetsEverything()
{
  RepeatCollapser collapser(30'000);
  collapser.feed(QStringLiteral("a"), 0);
  collapser.feed(QStringLiteral("a"), 1000);
  collapser.reset();
  QVERIFY(collapser.flush(2000).isEmpty());
  QVERIFY(collapser.feed(QStringLiteral("a"), 2000).show);
}

QTEST_MAIN(RepeatCollapserTests)
