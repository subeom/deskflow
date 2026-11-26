/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXScrollSourceTests.h"

#include "platform/OSXScrollSource.h"

#include <QTest>

#include <cstdint>
#include <memory>
#include <type_traits>

namespace {

// Synthetic events are only created and inspected here; nothing in this file ever posts an event.
struct EventDeleter
{
  void operator()(CGEventRef event) const
  {
    if (event != nullptr) {
      CFRelease(event);
    }
  }
};
using EventPtr = std::unique_ptr<std::remove_pointer_t<CGEventRef>, EventDeleter>;

EventPtr makeScrollEvent(
    CGScrollEventUnit unit, int64_t scrollPhase = 0, int64_t momentumPhase = 0, bool forceContinuous = false
)
{
  EventPtr event(CGEventCreateScrollWheelEvent2(nullptr, unit, 2, 3, -2, 0));
  if (scrollPhase != 0) {
    CGEventSetIntegerValueField(event.get(), kCGScrollWheelEventScrollPhase, scrollPhase);
  }
  if (momentumPhase != 0) {
    CGEventSetIntegerValueField(event.get(), kCGScrollWheelEventMomentumPhase, momentumPhase);
  }
  if (forceContinuous) {
    CGEventSetIntegerValueField(event.get(), kCGScrollWheelEventIsContinuous, 1);
  }
  return event;
}

int asInt(ScrollSource source)
{
  return static_cast<int>(source);
}

constexpr int kMouse = static_cast<int>(ScrollSource::Mouse);
constexpr int kTrackpad = static_cast<int>(ScrollSource::Trackpad);
constexpr int kUnknown = static_cast<int>(ScrollSource::Unknown);

} // namespace

void OSXScrollSourceTests::classifyPhases_matrix_data()
{
  QTest::addColumn<qlonglong>("scrollPhase");
  QTest::addColumn<qlonglong>("momentumPhase");
  QTest::addColumn<int>("expected");

  QTest::newRow("neither phase") << 0LL << 0LL << kMouse;

  QTest::newRow("scroll began") << 1LL << 0LL << kTrackpad;
  QTest::newRow("scroll changed") << 2LL << 0LL << kTrackpad;
  QTest::newRow("scroll ended") << 4LL << 0LL << kTrackpad;
  QTest::newRow("scroll cancelled") << 8LL << 0LL << kTrackpad;
  QTest::newRow("scroll may begin") << 128LL << 0LL << kTrackpad;

  QTest::newRow("momentum begin") << 0LL << 1LL << kTrackpad;
  QTest::newRow("momentum continue") << 0LL << 2LL << kTrackpad;
  QTest::newRow("momentum end") << 0LL << 3LL << kTrackpad;

  QTest::newRow("both phases") << 4LL << 2LL << kTrackpad;
}

void OSXScrollSourceTests::classifyPhases_matrix()
{
  QFETCH(qlonglong, scrollPhase);
  QFETCH(qlonglong, momentumPhase);
  QFETCH(int, expected);

  QCOMPARE(asInt(OSXScrollSource::classifyPhases(scrollPhase, momentumPhase)), expected);
}

void OSXScrollSourceTests::classifyEvent_syntheticEvents_data()
{
  QTest::addColumn<qlonglong>("unit");
  QTest::addColumn<qlonglong>("scrollPhase");
  QTest::addColumn<qlonglong>("momentumPhase");
  QTest::addColumn<bool>("forceContinuous");
  QTest::addColumn<int>("expected");

  const auto line = static_cast<qlonglong>(kCGScrollEventUnitLine);
  const auto pixel = static_cast<qlonglong>(kCGScrollEventUnitPixel);

  QTest::newRow("discrete line wheel") << line << 0LL << 0LL << false << kMouse;
  // mouse driver software emits continuous pixel events without any phase: that is not a trackpad
  QTest::newRow("continuous pixel events without phases") << pixel << 0LL << 0LL << false << kMouse;
  QTest::newRow("discrete event flagged continuous") << line << 0LL << 0LL << true << kMouse;

  QTest::newRow("pixel with scroll phase changed") << pixel << 2LL << 0LL << false << kTrackpad;
  QTest::newRow("pixel with momentum continue") << pixel << 0LL << 2LL << false << kTrackpad;
  QTest::newRow("pixel with both phases") << pixel << 4LL << 2LL << false << kTrackpad;
  QTest::newRow("line unit with a scroll phase") << line << 1LL << 0LL << false << kTrackpad;
}

void OSXScrollSourceTests::classifyEvent_syntheticEvents()
{
  QFETCH(qlonglong, unit);
  QFETCH(qlonglong, scrollPhase);
  QFETCH(qlonglong, momentumPhase);
  QFETCH(bool, forceContinuous);
  QFETCH(int, expected);

  const auto event = makeScrollEvent(static_cast<CGScrollEventUnit>(unit), scrollPhase, momentumPhase, forceContinuous);
  QVERIFY(event != nullptr);
  QCOMPARE(asInt(OSXScrollSource::classifyEvent(event.get())), expected);
}

void OSXScrollSourceTests::readFields_reportsRawValues()
{
  const auto event = makeScrollEvent(kCGScrollEventUnitPixel, 4, 2);
  QVERIFY(event != nullptr);

  const auto fields = OSXScrollSource::readFields(event.get());
  QCOMPARE(fields.scrollPhase, int64_t{4});
  QCOMPARE(fields.momentumPhase, int64_t{2});
  QCOMPARE(fields.continuous, int64_t{1});
  QCOMPARE(fields.sourcePid, CGEventGetIntegerValueField(event.get(), kCGEventSourceUnixProcessID));

  const auto none = OSXScrollSource::readFields(nullptr);
  QCOMPARE(none.scrollPhase, int64_t{0});
  QCOMPARE(none.momentumPhase, int64_t{0});
  QCOMPARE(none.continuous, int64_t{0});
  QCOMPARE(none.sourcePid, int64_t{0});
}

void OSXScrollSourceTests::update_reportsOnlyTransitions()
{
  const auto wheel = makeScrollEvent(kCGScrollEventUnitLine);
  const auto touchBegan = makeScrollEvent(kCGScrollEventUnitPixel, 1, 0);
  const auto touchMomentum = makeScrollEvent(kCGScrollEventUnitPixel, 0, 2);

  struct Step
  {
    CGEventRef event;
    int expectedSource;
    bool expectedChanged;
  };
  const Step steps[] = {
      {wheel.get(), kMouse, true}, // first classification differs from "nothing seen yet"
      {wheel.get(), kMouse, false},
      {wheel.get(), kMouse, false},
      {touchBegan.get(), kTrackpad, true}, // device switched
      {touchMomentum.get(), kTrackpad, false},
      {touchBegan.get(), kTrackpad, false},
      {wheel.get(), kMouse, true}, // and back
      {wheel.get(), kMouse, false},
  };

  OSXScrollSource tracker;
  int index = 0;
  for (const auto &step : steps) {
    const auto result = tracker.update(step.event);
    QVERIFY2(asInt(result.source) == step.expectedSource, qPrintable(QStringLiteral("step %1 source").arg(index)));
    QVERIFY2(result.changed == step.expectedChanged, qPrintable(QStringLiteral("step %1 changed").arg(index)));
    ++index;
  }
}

void OSXScrollSourceTests::update_nullEventIsUnknown()
{
  const auto wheel = makeScrollEvent(kCGScrollEventUnitLine);

  OSXScrollSource tracker;
  QCOMPARE(asInt(tracker.update(wheel.get()).source), kMouse);

  const auto lost = tracker.update(nullptr);
  QCOMPARE(asInt(lost.source), kUnknown);
  QVERIFY(lost.changed);

  const auto stillLost = tracker.update(nullptr);
  QCOMPARE(asInt(stillLost.source), kUnknown);
  QVERIFY(!stillLost.changed);
}

QTEST_MAIN(OSXScrollSourceTests)
