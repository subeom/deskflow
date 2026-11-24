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

  QCOMPARE(cat.width, 30.0);
  QCOMPARE(cat.height, 28.0);
  QCOMPARE(dog.width, 30.0);
  QCOMPARE(dog.height, 28.0);
  QCOMPARE(turtle.width, 30.0);
  QCOMPARE(turtle.height, 28.0);
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

void InputIndicatorGeometryTests::companionRenderModelUsesUnifiedSilhouetteAndInternalLabel()
{
  const InputIndicatorCompanion companions[] = {
      InputIndicatorCompanion::Cat,
      InputIndicatorCompanion::Dog,
      InputIndicatorCompanion::Turtle,
  };

  for (const auto companion : companions) {
    const auto model = macInputIndicatorRenderModel(companion);
    QVERIFY(!model.usesEyePrimitives);
    QCOMPARE(
        model.silhouettePrimitive,
        InputIndicatorRenderPrimitive::SilhouetteUnion
    );
    QCOMPARE(model.tileWidth, 24.0);
    QCOMPARE(model.tileHeight, 24.0);
    QVERIFY(model.totalWidth <= 30.0);
    QVERIFY(model.totalHeight <= 28.0);
    QVERIFY(model.languageBounds.x >= model.silhouetteBounds.x);
    QVERIFY(model.languageBounds.y >= model.silhouetteBounds.y);
    QVERIFY(
        model.languageBounds.x + model.languageBounds.width <=
        model.silhouetteBounds.x + model.silhouetteBounds.width
    );
    QVERIFY(
        model.languageBounds.y + model.languageBounds.height <=
        model.silhouetteBounds.y + model.silhouetteBounds.height
    );
  }
}

void InputIndicatorGeometryTests::companionRenderAlphaFollowsSettingAndBoundsDetail()
{
  const auto minimum =
      macInputIndicatorRenderAlpha(59);
  QCOMPARE(minimum.body, 0.6);
  QCOMPARE(minimum.detail, 0.7);

  const auto normal =
      macInputIndicatorRenderAlpha(65);
  QCOMPARE(normal.body, 0.65);
  QCOMPARE(normal.detail, 0.75);

  const auto maximum =
      macInputIndicatorRenderAlpha(101);
  QCOMPARE(maximum.body, 1.0);
  QCOMPARE(maximum.detail, 1.0);

  for (const auto &alpha :
       {minimum, normal, maximum}) {
    QVERIFY(alpha.detail >= alpha.body);
    QVERIFY(alpha.detail - alpha.body <= 0.10000001);
  }
}

void InputIndicatorGeometryTests::catTailUsesContinuousPhaseLaggedCurve()
{
  const InputIndicatorPoint root{.x = 3, .y = 8};
  const auto neutral = inputIndicatorCatTailPose(
      root, 10, 0, 0, true
  );
  const auto atZero = inputIndicatorCatTailPose(
      root, 10, 0, 0, false
  );
  const auto atQuarter = inputIndicatorCatTailPose(
      root, 10, 0.25, 0, false
  );
  const auto positiveVelocity = inputIndicatorCatTailPose(
      root, 10, 0.25, 8, false
  );
  const auto negativeVelocity = inputIndicatorCatTailPose(
      root, 10, 0.25, -8, false
  );
  const auto reducedFast = inputIndicatorCatTailPose(
      root, 10, 0.25, 8, true
  );

  QCOMPARE(positiveVelocity.root.x, root.x);
  QCOMPARE(positiveVelocity.root.y, root.y);
  QVERIFY(std::abs(
      (positiveVelocity.middle.x -
       positiveVelocity.middleIncomingControl.x) -
      (positiveVelocity.middleOutgoingControl.x -
       positiveVelocity.middle.x)
  ) < 1e-9);
  QVERIFY(std::abs(
      (positiveVelocity.middle.y -
       positiveVelocity.middleIncomingControl.y) -
      (positiveVelocity.middleOutgoingControl.y -
       positiveVelocity.middle.y)
  ) < 1e-9);

  const auto middleDelta =
      atQuarter.middle.y - atZero.middle.y;
  const auto tipDelta =
      atQuarter.tip.y - atZero.tip.y;
  QVERIFY(tipDelta < middleDelta);
  QVERIFY(positiveVelocity.tip.y < atQuarter.tip.y);
  QVERIFY(negativeVelocity.tip.y > atQuarter.tip.y);

  QVERIFY(std::abs(positiveVelocity.tip.y - root.y) <= 6.0);
  QVERIFY(positiveVelocity.tip.x >= root.x);
  QVERIFY(positiveVelocity.tip.x <= root.x + 10.5);
  QCOMPARE(reducedFast.middle.x, neutral.middle.x);
  QCOMPARE(reducedFast.middle.y, neutral.middle.y);
  QCOMPARE(reducedFast.tip.x, neutral.tip.x);
  QCOMPARE(reducedFast.tip.y, neutral.tip.y);
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

void InputIndicatorGeometryTests::companionMotionStateClassifiesIdleWalkRun()
{
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Idle, 0, 180, false
      ),
      InputIndicatorMotionState::Idle
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Idle, 30, 0, false
      ),
      InputIndicatorMotionState::Walk
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 650, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 900, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 900, 0, true
      ),
      InputIndicatorMotionState::Idle
  );
}

void InputIndicatorGeometryTests::companionMotionStateKeepsMovingForSettleDelay()
{
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 0, 179, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 0, 180, false
      ),
      InputIndicatorMotionState::Idle
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 0, 179, false
      ),
      InputIndicatorMotionState::Walk
  );
}

void InputIndicatorGeometryTests::companionMotionPhaseWrapsPerState()
{
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Idle, 0
      ),
      0.0
  );
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Idle, 600
      ),
      0.5
  );
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Idle, 1200
      ),
      0.0
  );
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Walk, 125
      ),
      0.5
  );
  QVERIFY(std::abs(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Run,
          1000.0 / 14.0
      ) - 0.5
  ) < 1e-9);
  QCOMPARE(
      inputIndicatorMotionPhase(
          InputIndicatorMotionState::Run,
          std::numeric_limits<double>::quiet_NaN()
      ),
      0.0
  );
}

void InputIndicatorGeometryTests::companionMotionPoseHonorsBounds()
{
  const auto idle = inputIndicatorMotionPose(
      InputIndicatorMotionState::Idle, 0.25, false
  );
  QCOMPARE(idle.bodyBob, 0.0);
  QCOMPARE(idle.bodyStretch, 0.0);
  QCOMPARE(idle.tailSway, 2.0);

  const auto walk = inputIndicatorMotionPose(
      InputIndicatorMotionState::Walk, 0.25, false
  );
  QCOMPARE(walk.bodyBob, 0.45);
  QCOMPARE(walk.bodyStretch, 0.025);
  QCOMPARE(walk.tailSway, 0.0);

  const auto run = inputIndicatorMotionPose(
      InputIndicatorMotionState::Run, 0.25, false
  );
  QCOMPARE(run.bodyBob, 0.9);
  QCOMPARE(run.bodyStretch, 0.07);
  QCOMPARE(run.tailSway, 0.0);

  const auto reduced = inputIndicatorMotionPose(
      InputIndicatorMotionState::Run, 0.25, true
  );
  QCOMPARE(reduced.bodyBob, 0.0);
  QCOMPARE(reduced.bodyStretch, 0.0);
  QCOMPARE(reduced.tailSway, 0.0);
}

void InputIndicatorGeometryTests::companionMotionFrameIntervalsStayLowDuty()
{
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Idle, false
      ),
      75
  );
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Walk, false
      ),
      8
  );
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Run, false
      ),
      8
  );
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Idle, true
      ),
      0
  );
  QCOMPARE(
      inputIndicatorMotionFrameInterval(
          InputIndicatorMotionState::Run, true
      ),
      0
  );
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
