/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorGeometryTests.h"

#include "gui/input/InputIndicatorGeometry.h"
#include "gui/input/MacInputIndicatorPanel.h"

#include <QTest>

#include <limits>

using namespace deskflow::gui::input;

namespace {

std::vector<InputIndicatorRect> oneScreen(double width = 1920, double height = 1080)
{
  return {{.x = 0, .y = 0, .width = width, .height = height}};
}

} // namespace

void InputIndicatorGeometryTests::bottomRightOffsetsCursor()
{
  const auto result = positionInputIndicator(
      {.x = 400, .y = 500}, {.width = 40, .height = 28}, oneScreen(),
      InputIndicatorPosition::CursorBottomRight, 8, 8
  );

  QVERIFY(result.has_value());
  QCOMPARE(result->x, 408.0);
  QCOMPARE(result->y, 464.0);
}

void InputIndicatorGeometryTests::positionsFlipInsideScreen()
{
  const InputIndicatorSize badge{.width = 40, .height = 28};
  const auto bottom = positionInputIndicator(
      {.x = 1918, .y = 2}, badge, oneScreen(), InputIndicatorPosition::CursorBottomRight, 8, 8
  );
  QVERIFY(bottom.has_value());
  QCOMPARE(bottom->x, 1872.0);
  QCOMPARE(bottom->y, 8.0);

  const auto topLeft = positionInputIndicator(
      {.x = 2, .y = 1078}, badge, oneScreen(), InputIndicatorPosition::CursorTopLeft, 8, 8
  );
  QVERIFY(topLeft.has_value());
  QCOMPARE(topLeft->x, 8.0);
  QCOMPARE(topLeft->y, 1044.0);
}

void InputIndicatorGeometryTests::choosesScreenContainingCursor()
{
  const std::vector<InputIndicatorRect> screens{
      {.x = -1280, .y = 0, .width = 1280, .height = 800},
      {.x = 0, .y = 0, .width = 1920, .height = 1080},
  };
  const auto result = positionInputIndicator(
      {.x = -100, .y = 200}, {.width = 40, .height = 28}, screens,
      InputIndicatorPosition::CursorBottomRight, 8, 8
  );

  QVERIFY(result.has_value());
  QCOMPARE(result->x, -92.0);
  QCOMPARE(result->y, 164.0);
}

void InputIndicatorGeometryTests::everyPositionIsPointerRelative_data()
{
  QTest::addColumn<int>("position");
  QTest::addColumn<double>("x");
  QTest::addColumn<double>("y");

  QTest::newRow("bottom-right")
      << static_cast<int>(InputIndicatorPosition::CursorBottomRight)
      << 408.0 << 464.0;
  QTest::newRow("top-right")
      << static_cast<int>(InputIndicatorPosition::CursorTopRight)
      << 408.0 << 508.0;
  QTest::newRow("bottom-left")
      << static_cast<int>(InputIndicatorPosition::CursorBottomLeft)
      << 352.0 << 464.0;
  QTest::newRow("top-left")
      << static_cast<int>(InputIndicatorPosition::CursorTopLeft)
      << 352.0 << 508.0;
}

void InputIndicatorGeometryTests::everyPositionIsPointerRelative()
{
  QFETCH(int, position);
  QFETCH(double, x);
  QFETCH(double, y);

  const auto result = positionInputIndicator(
      {.x = 400, .y = 500},
      {.width = 40, .height = 28},
      oneScreen(),
      static_cast<InputIndicatorPosition>(position),
      8,
      8
  );

  QVERIFY(result.has_value());
  QCOMPARE(result->x, x);
  QCOMPARE(result->y, y);
}

void InputIndicatorGeometryTests::unknownOrInvalidGeometryHides_data()
{
  QTest::addColumn<int>("condition");

  QTest::newRow("no screens") << 0;
  QTest::newRow("cursor outside") << 1;
  QTest::newRow("zero width") << 2;
  QTest::newRow("zero badge height") << 3;
  QTest::newRow("invalid screen") << 4;
}

void InputIndicatorGeometryTests::unknownOrInvalidGeometryHides()
{
  QFETCH(int, condition);
  InputIndicatorPoint cursor{.x = 100, .y = 100};
  InputIndicatorSize badge{.width = 40, .height = 28};
  auto screens = oneScreen();

  switch (condition) {
  case 0:
    screens.clear();
    break;
  case 1:
    cursor = {.x = 3000, .y = 3000};
    break;
  case 2:
    badge.width = 0;
    break;
  case 3:
    badge.height = 0;
    break;
  case 4:
    screens.front().height = 0;
    break;
  default:
    QFAIL("unknown test row");
  }

  QVERIFY(!positionInputIndicator(
               cursor, badge, screens, InputIndicatorPosition::CursorBottomRight, 8, 8
  ).has_value());
}

void InputIndicatorGeometryTests::compactPanelStyleUsesApprovedMetrics()
{
  const auto cat =
      macInputIndicatorPanelStyle(InputIndicatorCompanion::Cat);
  const auto dog =
      macInputIndicatorPanelStyle(InputIndicatorCompanion::Dog);
  const auto turtle =
      macInputIndicatorPanelStyle(InputIndicatorCompanion::Turtle);

  QCOMPARE(cat.width, 28.0);
  QCOMPARE(cat.height, 24.0);
  QCOMPARE(dog.width, 28.0);
  QCOMPARE(dog.height, 24.0);
  QCOMPARE(turtle.width, 30.0);
  QCOMPARE(turtle.height, 22.0);
  QCOMPARE(cat.cursorGap, 6.0);
  QCOMPARE(cat.screenMargin, 6.0);
  QCOMPARE(cat.labelSize, 8.0);
  QCOMPARE(cat.cornerRadius, 7.0);
  QCOMPARE(cat.maximumLag, 8.0);
  QCOMPARE(cat.maximumMotion, 8.0);
  QCOMPARE(cat.transitionMilliseconds, 200.0);
  QCOMPARE(cat.opacityPercent, 65);

  const MacInputIndicatorPanelOptions options;
  QCOMPARE(options.opacityPercent, 65);
  QCOMPARE(options.companion, InputIndicatorCompanion::Cat);
}

void InputIndicatorGeometryTests::companionSettingDefaultsToCat()
{
  QCOMPARE(
      inputIndicatorCompanionFromSetting(QStringLiteral("cat")),
      InputIndicatorCompanion::Cat
  );
  QCOMPARE(
      inputIndicatorCompanionFromSetting(QStringLiteral("dog")),
      InputIndicatorCompanion::Dog
  );
  QCOMPARE(
      inputIndicatorCompanionFromSetting(QStringLiteral("turtle")),
      InputIndicatorCompanion::Turtle
  );
  QCOMPARE(
      inputIndicatorCompanionFromSetting(QStringLiteral("other")),
      InputIndicatorCompanion::Cat
  );
}

void InputIndicatorGeometryTests::motionOffsetClampsAndRespectsReducedMotion()
{
  const auto shortMotion = inputIndicatorMotionOffset(
      {.x = 10, .y = 10}, {.x = 13, .y = 10}, 8, false
  );
  QVERIFY(shortMotion);
  QCOMPARE(shortMotion->x, 3.0);
  QCOMPARE(shortMotion->y, 0.0);

  const auto longMotion = inputIndicatorMotionOffset(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 8, false
  );
  QVERIFY(longMotion);
  QCOMPARE(longMotion->x, 8.0);
  QCOMPARE(longMotion->y, 0.0);

  const auto reduced = inputIndicatorMotionOffset(
      {.x = 0, .y = 0}, {.x = 100, .y = 100}, 8, true
  );
  QVERIFY(reduced);
  QCOMPARE(reduced->x, 0.0);
  QCOMPARE(reduced->y, 0.0);
}

void InputIndicatorGeometryTests::transitionProgressEasesAndRespectsReducedMotion()
{
  QCOMPARE(inputIndicatorTransitionProgress(0, 200, false), 0.0);
  QCOMPARE(inputIndicatorTransitionProgress(100, 200, false), 0.5);
  QCOMPARE(inputIndicatorTransitionProgress(200, 200, false), 1.0);
  QCOMPARE(inputIndicatorTransitionProgress(100, 200, true), 1.0);
}

void InputIndicatorGeometryTests::followerCatchesUpWithoutOvershoot()
{
  const auto first = followInputIndicator(
      {.x = 0, .y = 0}, {.x = 10, .y = 0}, 0.5, 18
  );
  QVERIFY(first);
  QCOMPARE(first->x, 5.0);
  QCOMPARE(first->y, 0.0);

  auto current = *first;
  current = *followInputIndicator(
      current, {.x = 10, .y = 0}, 0.5, 18
  );
  QCOMPARE(current.x, 7.5);
  current = *followInputIndicator(
      current, {.x = 10, .y = 0}, 0.5, 18
  );
  QCOMPARE(current.x, 8.75);
  QVERIFY(current.x <= 10.0);

  const auto approachingFromAhead = followInputIndicator(
      {.x = 12, .y = 0}, {.x = 10, .y = 0}, 0.5, 18
  );
  QVERIFY(approachingFromAhead);
  QCOMPARE(approachingFromAhead->x, 11.0);
  QVERIFY(approachingFromAhead->x >= 10.0);
}

void InputIndicatorGeometryTests::followerCapsTrailingLag()
{
  const auto result = followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 0.5, 18
  );

  QVERIFY(result);
  QCOMPARE(result->x, 82.0);
  QCOMPARE(result->y, 0.0);
}

void InputIndicatorGeometryTests::followerRejectsInvalidInput()
{
  const auto nan = std::numeric_limits<double>::quiet_NaN();

  QVERIFY(!followInputIndicator(
      {.x = nan, .y = 0}, {.x = 10, .y = 0}, 0.5, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 10, .y = nan}, 0.5, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 0, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, -0.1, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 1.1, 18
  ));
  QVERIFY(!followInputIndicator(
      {.x = 0, .y = 0}, {.x = 100, .y = 0}, 0.5, -1
  ));
}

QTEST_MAIN(InputIndicatorGeometryTests)
