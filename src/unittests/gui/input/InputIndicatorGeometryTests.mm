/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "InputIndicatorGeometryTests.h"

#include "gui/input/InputIndicatorGeometry.h"
#include "gui/input/MacInputIndicatorCatArtwork.h"
#include "gui/input/MacInputIndicatorPanel.h"

#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>

#include <QTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

using namespace deskflow::gui::input;

namespace {

std::vector<InputIndicatorRect> oneScreen(double width = 1920, double height = 1080)
{
  return {{.x = 0, .y = 0, .width = width, .height = height}};
}

double linearChannel(double channel)
{
  return channel <= 0.04045
      ? channel / 12.92
      : std::pow((channel + 0.055) / 1.055, 2.4);
}

double relativeLuminance(MacInputIndicatorRgb color)
{
  return 0.2126 * linearChannel(color.red) +
      0.7152 * linearChannel(color.green) +
      0.0722 * linearChannel(color.blue);
}

double contrastRatio(
    MacInputIndicatorRgb foreground,
    MacInputIndicatorRgb background
)
{
  const auto foregroundLuminance =
      relativeLuminance(foreground);
  const auto backgroundLuminance =
      relativeLuminance(background);
  return (
      std::max(foregroundLuminance, backgroundLuminance) +
      0.05
  ) / (
      std::min(foregroundLuminance, backgroundLuminance) +
      0.05
  );
}

NSBezierPath *artworkPath(
    std::span<const MacInputIndicatorCatPathCommand> commands
)
{
  NSBezierPath *path = [NSBezierPath bezierPath];
  for (const auto &command : commands) {
    switch (command.type) {
    case MacInputIndicatorCatPathCommandType::Move:
      [path moveToPoint:NSMakePoint(command.x1, command.y1)];
      break;
    case MacInputIndicatorCatPathCommandType::Line:
      [path lineToPoint:NSMakePoint(command.x1, command.y1)];
      break;
    case MacInputIndicatorCatPathCommandType::Cubic:
      [path curveToPoint:NSMakePoint(command.x3, command.y3)
          controlPoint1:NSMakePoint(command.x1, command.y1)
          controlPoint2:NSMakePoint(command.x2, command.y2)];
      break;
    case MacInputIndicatorCatPathCommandType::Close:
      [path closePath];
      break;
    }
  }
  return path;
}

NSBezierPath *artworkPath(
    const InputIndicatorCatPose &pose,
    std::span<const MacInputIndicatorCatPathCommand> commands,
    const InputIndicatorCatIdleTailFrame &tailFrame
)
{
  NSBezierPath *path = [NSBezierPath bezierPath];
  const auto deformed =
      [&pose, &tailFrame](double x, double y) {
        const auto torsoPoint =
            inputIndicatorCatTorsoPoint(
                pose, {.x = x, .y = y}
            );
        return inputIndicatorCatIdleTailPoint(
            torsoPoint, tailFrame
        );
      };
  for (const auto &command : commands) {
    switch (command.type) {
    case MacInputIndicatorCatPathCommandType::Move: {
      const auto point =
          deformed(command.x1, command.y1);
      [path moveToPoint:NSMakePoint(point.x, point.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Line: {
      const auto point =
          deformed(command.x1, command.y1);
      [path lineToPoint:NSMakePoint(point.x, point.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Cubic: {
      const auto control1 =
          deformed(command.x1, command.y1);
      const auto control2 =
          deformed(command.x2, command.y2);
      const auto point =
          deformed(command.x3, command.y3);
      [path curveToPoint:NSMakePoint(point.x, point.y)
          controlPoint1:NSMakePoint(control1.x, control1.y)
          controlPoint2:NSMakePoint(control2.x, control2.y)];
      break;
    }
    case MacInputIndicatorCatPathCommandType::Close:
      [path closePath];
      break;
    }
  }
  return path;
}

NSBitmapImageRep *pixelBitmap(
    int scale,
    int paddingPoints
)
{
  const auto style =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  return [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nil
                    pixelsWide:
                        static_cast<NSInteger>(
                            std::lround(
                                style.width +
                                paddingPoints * 2.0
                            )
                        ) * scale
                    pixelsHigh:
                        static_cast<NSInteger>(
                            std::lround(
                                style.height +
                                paddingPoints * 2.0
                            )
                        ) * scale
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSDeviceRGBColorSpace
                   bytesPerRow:0
                  bitsPerPixel:0];
}

NSBitmapImageRep *catBodyBitmap(
    const InputIndicatorCatPose &pose,
    const MacInputIndicatorCatArtwork &artwork,
    const InputIndicatorCatIdleTailFrame &tailFrame,
    bool facesRight,
    int scale,
    int paddingPoints,
    bool includeHalo
)
{
  NSBitmapImageRep *bitmap =
      pixelBitmap(scale, paddingPoints);
  if (!bitmap) {
    return nil;
  }
  NSGraphicsContext *graphics =
      [NSGraphicsContext
          graphicsContextWithBitmapImageRep:bitmap];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:graphics];
  CGContextRef context = [graphics CGContext];
  CGContextSaveGState(context);
  CGContextScaleCTM(context, scale, scale);
  CGContextTranslateCTM(
      context, paddingPoints, paddingPoints
  );
  const auto displayOrigin =
      macInputIndicatorDisplayPoint({});
  CGContextTranslateCTM(
      context, displayOrigin.x, displayOrigin.y
  );
  CGContextScaleCTM(
      context,
      macInputIndicatorPanelStyle().visualScale,
      macInputIndicatorPanelStyle().visualScale
  );
  const auto model =
      macInputIndicatorRenderModel(
          InputIndicatorCompanion::Cat
      );
  if (!facesRight) {
    CGContextTranslateCTM(
        context, model.totalWidth, 0
    );
    CGContextScaleCTM(context, -1, 1);
  }

  NSBezierPath *far = artworkPath(artwork.far);
  NSBezierPath *near =
      artworkPath(pose, artwork.near, tailFrame);
  if (includeHalo) {
    NSBezierPath *halo =
        [NSBezierPath bezierPath];
    [halo appendBezierPath:far];
    [halo appendBezierPath:near];
    constexpr double kMaximumHaloScale =
        1.0 + (1.22 - 1.0) * 0.30;
    NSAffineTransform *transform =
        [NSAffineTransform transform];
    [transform translateXBy:15 yBy:14];
    [transform scaleXBy:kMaximumHaloScale
                    yBy:kMaximumHaloScale];
    [transform translateXBy:-15 yBy:-14];
    [halo transformUsingAffineTransform:transform];
    [[NSColor blackColor] setFill];
    [halo fill];
  }
  [[NSColor blackColor] setFill];
  if (includeHalo) {
    [[NSColor blackColor] setStroke];
    [near setLineWidth:0.55];
    [far setLineWidth:0.55];
    [near setLineJoinStyle:NSLineJoinStyleRound];
    [far setLineJoinStyle:NSLineJoinStyleRound];
    [near stroke];
    [far stroke];
  }
  [near fill];
  [far fill];
  CGContextRestoreGState(context);
  [NSGraphicsContext restoreGraphicsState];
  return bitmap;
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

  QCOMPARE(cat.width, 33.0);
  QCOMPARE(cat.height, 31.0);
  QCOMPARE(dog.width, 33.0);
  QCOMPARE(dog.height, 31.0);
  QCOMPARE(turtle.width, 33.0);
  QCOMPARE(turtle.height, 31.0);
  QCOMPARE(cat.visualScale, 1.1);
  QCOMPARE(cat.logicalWidth, 30.0);
  QCOMPARE(cat.logicalHeight, 28.0);
  QCOMPARE(cat.cursorGap, 7.0);
  QCOMPARE(cat.screenMargin, 7.0);
  QCOMPARE(cat.labelSize, 8.0);
  QCOMPARE(cat.maximumLag, 9.0);
  QCOMPARE(cat.maximumMotion, 9.0);
  QCOMPARE(cat.transitionMilliseconds, 200.0);
  QCOMPARE(cat.opacityPercent, 65);

  const MacInputIndicatorPanelOptions options;
  QCOMPARE(options.opacityPercent, 65);
  QCOMPARE(options.companion, InputIndicatorCompanion::Cat);
}

void InputIndicatorGeometryTests::companionDisplayModelScalesAllLogicalGeometry()
{
  const auto point =
      macInputIndicatorDisplayPoint(
          {.x = 15, .y = 14}
      );
  QVERIFY(std::abs(point.x - 16.5) < 1e-9);
  QVERIFY(std::abs(point.y - 15.5) < 1e-9);

  const auto rect =
      macInputIndicatorDisplayRect(
          {.x = 1, .y = 2, .width = 28, .height = 26}
      );
  QVERIFY(std::abs(rect.x - 1.1) < 1e-9);
  QVERIFY(std::abs(rect.y - 2.3) < 1e-9);
  QVERIFY(std::abs(rect.width - 30.8) < 1e-9);
  QVERIFY(std::abs(rect.height - 28.6) < 1e-9);

  const auto preference =
      macInputIndicatorDisplayLabelPreference(
          deskflow::input::InputIndicatorSource::Korean2Set,
          InputIndicatorMotionState::Run
      );
  QVERIFY(std::abs(preference.pointSize - 6.71) < 1e-9);
  QVERIFY(std::abs(
      preference.minimumPointSize - 6.38
  ) < 1e-9);
}

void InputIndicatorGeometryTests::companionFacingTurnCompressesAndSwitchesOnce()
{
  const auto start =
      macInputIndicatorFacingPose(
          true, false, 0, false
      );
  QVERIFY(start.facesRight);
  QVERIFY(start.turning);
  QCOMPARE(start.horizontalScale, 1.0);

  const auto middle =
      macInputIndicatorFacingPose(
          true, false, 140, false
      );
  QVERIFY(!middle.facesRight);
  QVERIFY(middle.turning);
  QCOMPARE(middle.horizontalScale, 0.8);

  const auto end =
      macInputIndicatorFacingPose(
          true, false, 280, false
      );
  QVERIFY(!end.facesRight);
  QVERIFY(!end.turning);
  QCOMPARE(end.horizontalScale, 1.0);

  const auto reduced =
      macInputIndicatorFacingPose(
          true, false, 140, true
      );
  QVERIFY(!reduced.facesRight);
  QVERIFY(!reduced.turning);
  QCOMPARE(reduced.horizontalScale, 1.0);

  QVERIFY(macInputIndicatorFacingTurnStarts(
      true, false, false
  ));
  QVERIFY(!macInputIndicatorFacingTurnStarts(
      true, true, false
  ));
  QVERIFY(!macInputIndicatorFacingTurnStarts(
      true, false, true
  ));
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
    QVERIFY(!model.languageMirrorsWithSilhouette);
    QCOMPARE(
        model.silhouettePrimitive,
        InputIndicatorRenderPrimitive::SilhouetteUnion
    );
    QCOMPARE(
        model.languagePrimitive,
        InputIndicatorRenderPrimitive::LanguageGlyph
    );
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

void InputIndicatorGeometryTests::catRenderModelUsesSideProfileAndPersistentFacing()
{
  const auto model =
      macInputIndicatorRenderModel(InputIndicatorCompanion::Cat);
  QVERIFY(model.usesSideProfile);
  QVERIFY(model.mirrorsWithMotion);
  QVERIFY(model.sourceFacesRight);
  QVERIFY(!model.usesTorsoPad);
  QVERIFY(model.headBounds.x > model.torsoBounds.x);
  QVERIFY(
      model.tailBounds.x <
      model.torsoBounds.x
  );
  QVERIFY(model.pawBounds.y <= model.torsoBounds.y);
  QVERIFY(
      model.languageBounds.x >= model.torsoBounds.x &&
      model.languageBounds.y >= model.torsoBounds.y &&
      model.languageBounds.x + model.languageBounds.width <=
          model.torsoBounds.x + model.torsoBounds.width &&
      model.languageBounds.y + model.languageBounds.height <=
          model.torsoBounds.y + model.torsoBounds.height
  );

  const auto insideSilhouette =
      [silhouette = model.silhouetteBounds](
          const InputIndicatorRect &bounds
      ) {
        return bounds.x >= silhouette.x &&
               bounds.y >= silhouette.y &&
               bounds.x + bounds.width <=
                   silhouette.x + silhouette.width &&
               bounds.y + bounds.height <=
                   silhouette.y + silhouette.height;
      };
  QVERIFY(insideSilhouette(model.headBounds));
  QVERIFY(insideSilhouette(model.torsoBounds));
  QVERIFY(insideSilhouette(model.pawBounds));
  QVERIFY(insideSilhouette(model.tailBounds));

  QVERIFY(inputIndicatorCompanionFacesRight(false, 1.0));
  QVERIFY(!inputIndicatorCompanionFacesRight(true, -1.0));
  QVERIFY(inputIndicatorCompanionFacesRight(true, 0.0));
  QVERIFY(!inputIndicatorCompanionFacesRight(false, 0.0));
  QVERIFY(!inputIndicatorCompanionFacesRight(
      false, std::numeric_limits<double>::quiet_NaN()
  ));
}

void InputIndicatorGeometryTests::catTorsoContourExpandsWithoutPadPrimitive()
{
  const auto idle = inputIndicatorCatPose(
      InputIndicatorMotionState::Idle, 0.0, false
  );
  const auto walk = inputIndicatorCatPose(
      InputIndicatorMotionState::Walk, 0.34, false
  );
  const auto run = inputIndicatorCatPose(
      InputIndicatorMotionState::Run, 0.34, false
  );
  const double idleCenterX =
      idle.innerLanguageBounds.x +
      idle.innerLanguageBounds.width / 2.0;
  const double idleCenterY =
      idle.innerLanguageBounds.y +
      idle.innerLanguageBounds.height / 2.0;

  const auto idleUpper = inputIndicatorCatTorsoPoint(
      idle, {.x = idleCenterX, .y = idleCenterY + 2.5}
  );
  QCOMPARE(idleUpper.x, idleCenterX);
  QVERIFY(idleUpper.y > idleCenterY + 2.5);
  QVERIFY(idleUpper.y <= idleCenterY + 4.0);

  for (const auto *pose : {&walk, &run}) {
    QVERIFY(pose->torsoBounds.height >= 8.0);
    QCOMPARE(
        pose->innerLanguageBounds.width, 8.0
    );
    QCOMPARE(
        pose->innerLanguageBounds.height, 7.0
    );
    QVERIFY(pose->silhouetteBounds.width <=
        macInputIndicatorCatArtwork(pose->artworkFrame)
            .silhouetteBounds.width +
        1.5);

    const double centerX =
        pose->innerLanguageBounds.x +
        pose->innerLanguageBounds.width / 2.0;
    const double centerY =
        pose->innerLanguageBounds.y +
        pose->innerLanguageBounds.height / 2.0;
    for (const double offset : {-3.5, 3.5}) {
      const auto input =
          InputIndicatorPoint{
              .x = centerX,
              .y = centerY + offset,
          };
      const auto output =
          inputIndicatorCatTorsoPoint(*pose, input);
      QCOMPARE(output.x, input.x);
      QVERIFY(std::abs(output.y - input.y) > 0);
      QVERIFY(std::abs(output.y - input.y) <= 1.5);
    }
    const auto outside =
        inputIndicatorCatTorsoPoint(
            *pose,
            {
                .x = centerX + 9,
                .y = centerY + 4,
            }
        );
    QCOMPARE(outside.x, centerX + 9);
    QCOMPARE(outside.y, centerY + 4);
  }
}

void InputIndicatorGeometryTests::catArtworkFramesFollowApprovedPoseSheet()
{
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0.0, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::IdleNeutral
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0.25, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::IdleRise
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Idle, 0.75, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::IdleFall
  );

  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Walk, 0.0, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::WalkReach
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Walk, 0.34, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::WalkPass
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Walk, 0.67, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::WalkPush
  );

  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.0, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::RunCoil
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.34, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::RunAir
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.67, false
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::RunLand
  );
  QCOMPARE(
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.75, true
      ).artworkFrame,
      InputIndicatorCatArtworkFrame::IdleNeutral
  );
}

void InputIndicatorGeometryTests::catArtworkMetricsMatchApprovedSource()
{
  const auto idle = inputIndicatorCatPose(
      InputIndicatorMotionState::Idle, 0.0, false
  );
  const auto walk = inputIndicatorCatPose(
      InputIndicatorMotionState::Walk, 0.0, false
  );
  const auto run = inputIndicatorCatPose(
      InputIndicatorMotionState::Run, 0.34, false
  );
  const auto model =
      macInputIndicatorRenderModel(InputIndicatorCompanion::Cat);

  QVERIFY(std::abs(
      idle.silhouetteBounds.width - 20.6824
  ) < 0.02);
  QVERIFY(std::abs(
      idle.silhouetteBounds.height - 15.9470
  ) < 0.02);
  QVERIFY(std::abs(
      walk.silhouetteBounds.width - 21.8200
  ) < 0.02);
  QVERIFY(std::abs(
      walk.silhouetteBounds.height - 13.1184
  ) < 0.02);
  QVERIFY(
      walk.silhouetteBounds.height <= 13.1184
  );
  QVERIFY(std::abs(
      run.silhouetteBounds.width - 28.0000
  ) < 0.02);
  QVERIFY(
      run.silhouetteBounds.height >= 12.7445
  );
  QVERIFY(run.silhouetteBounds.height <= 12.745);

  const auto assertGlyph =
      [&model](
          const InputIndicatorCatPose &pose,
          double centerX,
          double centerY
      ) {
        QCOMPARE(pose.languageBounds.width, 9.0);
        QCOMPARE(pose.languageBounds.height, 8.0);
        QVERIFY(std::abs(
            pose.languageBounds.x +
                pose.languageBounds.width / 2.0 -
                centerX
        ) < 0.02);
        QVERIFY(std::abs(
            pose.languageBounds.y +
                pose.languageBounds.height / 2.0 -
                centerY
        ) < 0.02);
        QVERIFY(
            pose.languageBounds.x >= pose.silhouetteBounds.x &&
            pose.languageBounds.y >= pose.silhouetteBounds.y &&
            pose.languageBounds.x + pose.languageBounds.width <=
                pose.silhouetteBounds.x +
                    pose.silhouetteBounds.width &&
            pose.languageBounds.y + pose.languageBounds.height <=
                pose.silhouetteBounds.y +
                    pose.silhouetteBounds.height
        );
        const auto leftBounds =
            inputIndicatorCatLanguageBounds(pose, false);
        const auto mirroredCenterX =
            leftBounds.x + leftBounds.width / 2.0;
        QVERIFY(std::abs(
            mirroredCenterX - (model.totalWidth - centerX)
        ) < 0.02);
      };
  assertGlyph(idle, 17.4985, 9.0907);
  assertGlyph(walk, 16.3336, 10.5258);
  assertGlyph(run, 16.6132, 11.1921);

  const struct
  {
    InputIndicatorMotionState state;
    double phase;
    InputIndicatorCatArtworkFrame frame;
  } keyframes[] = {
      {.state = InputIndicatorMotionState::Idle,
       .phase = 0.0,
       .frame = InputIndicatorCatArtworkFrame::IdleNeutral},
      {.state = InputIndicatorMotionState::Idle,
       .phase = 0.25,
       .frame = InputIndicatorCatArtworkFrame::IdleRise},
      {.state = InputIndicatorMotionState::Idle,
       .phase = 0.75,
       .frame = InputIndicatorCatArtworkFrame::IdleFall},
      {.state = InputIndicatorMotionState::Walk,
       .phase = 0.0,
       .frame = InputIndicatorCatArtworkFrame::WalkReach},
      {.state = InputIndicatorMotionState::Walk,
       .phase = 0.34,
       .frame = InputIndicatorCatArtworkFrame::WalkPass},
      {.state = InputIndicatorMotionState::Walk,
       .phase = 0.67,
       .frame = InputIndicatorCatArtworkFrame::WalkPush},
      {.state = InputIndicatorMotionState::Run,
       .phase = 0.0,
       .frame = InputIndicatorCatArtworkFrame::RunCoil},
      {.state = InputIndicatorMotionState::Run,
       .phase = 0.34,
       .frame = InputIndicatorCatArtworkFrame::RunAir},
      {.state = InputIndicatorMotionState::Run,
       .phase = 0.67,
       .frame = InputIndicatorCatArtworkFrame::RunLand},
  };
  for (const auto &keyframe : keyframes) {
    const auto pose = inputIndicatorCatPose(
        keyframe.state, keyframe.phase, false
    );
    const auto artwork =
        macInputIndicatorCatArtwork(keyframe.frame);
    QCOMPARE(pose.artworkFrame, keyframe.frame);
    QVERIFY(!artwork.near.empty());
    QVERIFY(!artwork.far.empty());
    QCOMPARE(
        pose.silhouetteBounds.x,
        artwork.silhouetteBounds.x
    );
    QCOMPARE(
        pose.languageBounds.x,
        artwork.languageBounds.x
    );
    assertGlyph(
        pose,
        artwork.languageBounds.x +
            artwork.languageBounds.width / 2.0,
        artwork.languageBounds.y +
            artwork.languageBounds.height / 2.0
    );
  }
}

void InputIndicatorGeometryTests::catLabelLayoutFitsEveryArtworkPoseAndSource()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  const struct
  {
    Source source;
    InputIndicatorMotionState state;
    double fontSize;
    double minimumFontSize;
  } preferences[] = {
      {.source = Source::LatinABC,
       .state = InputIndicatorMotionState::Idle,
       .fontSize = 8.0,
       .minimumFontSize = 7.2},
      {.source = Source::LatinABC,
       .state = InputIndicatorMotionState::Walk,
       .fontSize = 8.0,
       .minimumFontSize = 7.2},
      {.source = Source::LatinABC,
       .state = InputIndicatorMotionState::Run,
       .fontSize = 7.6,
       .minimumFontSize = 7.0},
      {.source = Source::Korean2Set,
       .state = InputIndicatorMotionState::Idle,
       .fontSize = 6.8,
       .minimumFontSize = 6.3},
      {.source = Source::Korean2Set,
       .state = InputIndicatorMotionState::Walk,
       .fontSize = 6.4,
       .minimumFontSize = 6.0},
      {.source = Source::Korean2Set,
       .state = InputIndicatorMotionState::Run,
       .fontSize = 6.1,
       .minimumFontSize = 5.8},
  };
  for (const auto &expected : preferences) {
    const auto preference =
        macInputIndicatorLabelPreference(
            expected.source, expected.state
        );
    QCOMPARE(preference.pointSize, expected.fontSize);
    QCOMPARE(
        preference.minimumPointSize,
        expected.minimumFontSize
    );
  }

  const struct
  {
    InputIndicatorMotionState state;
    double phase;
  } poses[] = {
      {.state = InputIndicatorMotionState::Idle, .phase = 0.0},
      {.state = InputIndicatorMotionState::Idle, .phase = 0.25},
      {.state = InputIndicatorMotionState::Idle, .phase = 0.75},
      {.state = InputIndicatorMotionState::Walk, .phase = 0.0},
      {.state = InputIndicatorMotionState::Walk, .phase = 0.34},
      {.state = InputIndicatorMotionState::Walk, .phase = 0.67},
      {.state = InputIndicatorMotionState::Run, .phase = 0.0},
      {.state = InputIndicatorMotionState::Run, .phase = 0.34},
      {.state = InputIndicatorMotionState::Run, .phase = 0.67},
  };
  const Source sources[] = {Source::LatinABC, Source::Korean2Set};
  for (const auto &poseInput : poses) {
    const auto pose = inputIndicatorCatPose(
        poseInput.state, poseInput.phase, false
    );
    for (const auto source : sources) {
      const auto preference =
          macInputIndicatorLabelPreference(
              source, poseInput.state
          );
      const MacInputIndicatorLabelMeasurement measurement{
          .origin = {
              .x = preference.pointSize *
                  (source == Source::Korean2Set ? 0.022 : 0.034),
              .y = source == Source::Korean2Set
                  ? -preference.pointSize * 0.055
                  : 0,
          },
          .size = {
              .width = preference.pointSize *
                  (source == Source::Korean2Set ? 0.831 : 0.643),
              .height = preference.pointSize *
                  (source == Source::Korean2Set ? 0.867 : 0.705),
          },
      };
      const auto layout = macInputIndicatorFitLabelInkLayout(
          preference.pointSize,
          preference.minimumPointSize,
          pose.innerLanguageBounds,
          measurement
      );
      QVERIFY(layout);
      QCOMPARE(layout->fontSize, preference.pointSize);
      QVERIFY(layout->inkBounds.x >= pose.innerLanguageBounds.x);
      QVERIFY(layout->inkBounds.y >= pose.innerLanguageBounds.y);
      QVERIFY(
          layout->inkBounds.x + layout->inkBounds.width <=
          pose.innerLanguageBounds.x +
              pose.innerLanguageBounds.width
      );
      QVERIFY(
          layout->inkBounds.y + layout->inkBounds.height <=
          pose.innerLanguageBounds.y +
              pose.innerLanguageBounds.height
      );
      if (source == Source::Korean2Set) {
        QVERIFY(measurement.origin.y < 0);
      }
      QVERIFY(pose.torsoBounds.height >=
          (poseInput.state == InputIndicatorMotionState::Idle
              ? 7.2
              : 8.0));
      QVERIFY(pose.innerLanguageBounds.width >= 8.0 - 1e-9);
      QVERIFY(pose.innerLanguageBounds.height >= 7.0 - 1e-9);
    }
  }

  const auto container =
      inputIndicatorCatPose(
          InputIndicatorMotionState::Run, 0.34, false
      ).innerLanguageBounds;
  const auto oversized = macInputIndicatorFitLabelInkLayout(
      6.1,
  5.8,
      container,
      {
          .origin = {.x = 0.25, .y = 0},
      .size = {.width = 8.0, .height = 7.2},
      }
  );
  QVERIFY(oversized);
  QVERIFY(oversized->fontSize < 6.1);
  QVERIFY(oversized->fontSize >= 5.8);
  QVERIFY(oversized->inkBounds.x >= container.x);
  QVERIFY(oversized->inkBounds.y >= container.y);
  QVERIFY(
      oversized->inkBounds.x + oversized->inkBounds.width <=
      container.x + container.width
  );
  QVERIFY(
      oversized->inkBounds.y + oversized->inkBounds.height <=
      container.y + container.height
  );

  QVERIFY(!macInputIndicatorFitLabelInkLayout(
      6.8,
      6.3,
      container,
      {
          .origin = {.x = 0, .y = 0},
          .size = {
              .width = std::numeric_limits<double>::quiet_NaN(),
              .height = 1,
          },
      }
  ));
  QVERIFY(!macInputIndicatorFitLabelInkLayout(
      0,
      6.0,
      container,
      {
          .origin = {.x = 0, .y = 0},
          .size = {.width = 1, .height = 1},
      }
  ));
  QVERIFY(!macInputIndicatorFitLabelInkLayout(
      6.1,
      5.8,
      container,
      {
          .origin = {.x = 0, .y = 0},
          .size = {.width = 12.0, .height = 9.0},
      }
  ));
}

void InputIndicatorGeometryTests::coreTextInkBoundsExposeGlyphOrigin()
{
  @autoreleasepool {
    NSFont *hanFont =
        [NSFont systemFontOfSize:6.8
                          weight:NSFontWeightSemibold];
    const auto han =
        macInputIndicatorCoreTextLabelMeasurement(
            @"한", hanFont
        );
    QVERIFY(han);
    QVERIFY(han->ink.origin.y < -0.30);
    QVERIFY(han->typographicBounds.y < han->ink.origin.y);
    QVERIFY(han->ink.size.height < han->typographicBounds.height);

    NSFont *latinFont =
        [NSFont systemFontOfSize:8
                          weight:NSFontWeightSemibold];
    const auto latin =
        macInputIndicatorCoreTextLabelMeasurement(
            @"A", latinFont
        );
    QVERIFY(latin);
    QVERIFY(std::abs(latin->ink.origin.y) < 0.001);
  }
}

void InputIndicatorGeometryTests::coreTextCatGlyphPixelsStayInsideSilhouette()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  struct PaintPose
  {
    InputIndicatorMotionState state;
    double phase;
    InputIndicatorCatIdleTailFrame tail;
  };
  std::vector<PaintPose> poses;
  const double idlePhases[] = {0.0, 0.25, 0.75};
  const InputIndicatorCatIdleTailMotion idleMotions[] = {
      InputIndicatorCatIdleTailMotion::RelaxedSway,
      InputIndicatorCatIdleTailMotion::TipFlick,
      InputIndicatorCatIdleTailMotion::CurlUnfurl,
      InputIndicatorCatIdleTailMotion::LazySweep,
      InputIndicatorCatIdleTailMotion::QuestionLift,
  };
  const double idleProgress[] = {0.2, 0.4, 0.6, 0.8};
  for (const double phase : idlePhases) {
    for (const auto motion : idleMotions) {
      for (const double progress : idleProgress) {
        poses.push_back({
            .state = InputIndicatorMotionState::Idle,
            .phase = phase,
            .tail = {
                .motion = motion,
                .progress = progress,
                .active = true,
            },
        });
      }
    }
  }
  for (const double phase : {0.0, 0.34, 0.67}) {
    poses.push_back({
        .state = InputIndicatorMotionState::Walk,
        .phase = phase,
    });
    poses.push_back({
        .state = InputIndicatorMotionState::Run,
        .phase = phase,
    });
  }

  const Source sources[] = {Source::LatinABC, Source::Korean2Set};
  const bool directions[] = {true, false};
  constexpr NSInteger scale = 4;
  constexpr int paddingPoints = 4;
  const auto panelStyle =
      macInputIndicatorPanelStyle(
          InputIndicatorCompanion::Cat
      );
  int cases = 0;
  int outsidePixels = 0;

  for (const auto &poseInput : poses) {
    const auto pose = inputIndicatorCatPose(
        poseInput.state, poseInput.phase, false
    );
    const auto artwork =
        macInputIndicatorCatArtwork(pose.artworkFrame);
    QVERIFY(pose.torsoBounds.height >=
        (poseInput.state == InputIndicatorMotionState::Idle
            ? 7.2
            : 8.0));
    const double unionLeft =
        std::min(
            artwork.silhouetteBounds.x,
            pose.torsoBounds.x
        );
    const double unionRight =
        std::max(
            artwork.silhouetteBounds.x +
                artwork.silhouetteBounds.width,
            pose.torsoBounds.x + pose.torsoBounds.width
        );
    QVERIFY(
        unionRight - unionLeft <=
        artwork.silhouetteBounds.width + 1.5
    );

    for (const auto source : sources) {
      NSString *label =
          source == Source::LatinABC ? @"A" : @"한";
      for (const bool facesRight : directions) {
        int caseOutsidePixels = 0;
        NSBitmapImageRep *bodyMask =
            catBodyBitmap(
                pose,
                artwork,
                poseInput.tail,
                facesRight,
                scale,
                paddingPoints,
                false
            );
        NSBitmapImageRep *panelMask =
            catBodyBitmap(
                pose,
                artwork,
                poseInput.tail,
                facesRight,
                scale,
                paddingPoints,
                true
            );
        NSBitmapImageRep *glyphMask =
            pixelBitmap(scale, paddingPoints);
        QVERIFY(bodyMask);
        QVERIFY(panelMask);
        QVERIFY(glyphMask);

        auto target =
            macInputIndicatorDisplayRect(
                inputIndicatorCatInnerLanguageBounds(
                    pose, facesRight
                )
            );
        target.x += paddingPoints;
        target.y += paddingPoints;
        const auto displayPreference =
            macInputIndicatorDisplayLabelPreference(
                source, poseInput.state
            );
        double pointSize = displayPreference.pointSize;
        std::optional<MacInputIndicatorLabelLayout> layout;
        NSFont *font = nil;
        for (int attempt = 0; attempt < 5; ++attempt) {
          font =
              [NSFont systemFontOfSize:pointSize
                                weight:NSFontWeightSemibold];
          const auto measured =
              macInputIndicatorCoreTextLabelMeasurement(
                  label, font
              );
          QVERIFY(measured);
          layout = macInputIndicatorFitLabelInkLayout(
              pointSize,
              displayPreference.minimumPointSize, target, measured->ink
          );
          QVERIFY(layout);
          if (std::abs(
                  layout->fontSize - pointSize
              ) < 0.01) {
            break;
          }
          pointSize = layout->fontSize;
        }
        QVERIFY(layout);
        QVERIFY(font);

        NSGraphicsContext *glyphContext =
            [NSGraphicsContext
                graphicsContextWithBitmapImageRep:glyphMask];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:glyphContext];
        CGContextRef glyphCG =
            [glyphContext CGContext];
        CGContextSaveGState(glyphCG);
        CGContextScaleCTM(glyphCG, scale, scale);
        macInputIndicatorDrawCoreTextLabel(
            glyphCG,
            label,
            font,
            [NSColor blackColor],
            *layout
        );
        CGContextRestoreGState(glyphCG);
        [NSGraphicsContext restoreGraphicsState];

        const NSInteger panelLeft =
            paddingPoints * scale;
        const NSInteger panelBottom =
            paddingPoints * scale;
        const NSInteger panelRight =
            panelLeft +
            static_cast<NSInteger>(
                std::lround(panelStyle.width)
            ) *
                scale;
        const NSInteger panelTop =
            panelBottom +
            static_cast<NSInteger>(
                std::lround(panelStyle.height)
            ) *
                scale;
        for (NSInteger y = 0;
             y < glyphMask.pixelsHigh;
             ++y) {
          for (NSInteger x = 0;
               x < glyphMask.pixelsWide;
               ++x) {
            const double glyphAlpha =
                [glyphMask colorAtX:x y:y].alphaComponent;
            const double bodyAlpha =
                [bodyMask colorAtX:x y:y].alphaComponent;
            const double panelAlpha =
                [panelMask colorAtX:x y:y].alphaComponent;
            const bool glyphOutsideBody =
                glyphAlpha > 0.02 &&
                bodyAlpha <= 0.02;
            const bool paintOutsidePanel =
                panelAlpha > 0.02 &&
                (x < panelLeft ||
                 x >= panelRight ||
                 y < panelBottom ||
                 y >= panelTop);
            if (glyphOutsideBody ||
                paintOutsidePanel) {
              ++caseOutsidePixels;
            }
          }
        }
        if (caseOutsidePixels > 0) {
          qInfo() << "contour-spill"
                  << static_cast<int>(poseInput.state)
                  << poseInput.phase
                  << static_cast<int>(source)
                  << facesRight
                  << static_cast<int>(
                         poseInput.tail.motion
                     )
                  << poseInput.tail.progress
                  << caseOutsidePixels;
        }
        outsidePixels += caseOutsidePixels;
        [bodyMask release];
        [panelMask release];
        [glyphMask release];
        ++cases;
      }
    }
  }
  qInfo() << "outsidePixels" << outsidePixels
          << "cases" << cases;
  QCOMPARE(
      cases,
      static_cast<int>(
          poses.size() *
          (sizeof(sources) / sizeof(sources[0])) *
          (sizeof(directions) /
           sizeof(directions[0]))
      )
  );
  QCOMPARE(outsidePixels, 0);
}

void InputIndicatorGeometryTests::catPoseUsesDistinctSeatedWalkRunGeometry()
{
  const auto model =
      macInputIndicatorRenderModel(InputIndicatorCompanion::Cat);
  const auto idle = inputIndicatorCatPose(
      InputIndicatorMotionState::Idle, 0.0, false
  );
  const auto walk = inputIndicatorCatPose(
      InputIndicatorMotionState::Walk, 0.0, false
  );
  const auto run = inputIndicatorCatPose(
      InputIndicatorMotionState::Run, 0.34, false
  );

  QVERIFY(idle.torsoBounds.height > walk.torsoBounds.height);
  QVERIFY(walk.torsoBounds.width > idle.torsoBounds.width);
  QVERIFY(run.torsoBounds.width > walk.torsoBounds.width);
  QVERIFY(run.torsoBounds.height <= walk.torsoBounds.height);
  QVERIFY(std::abs(idle.bodyAngleDegrees) < 1e-9);
  QVERIFY(std::abs(walk.bodyAngleDegrees) < 1e-9);
  QVERIFY(std::abs(run.bodyAngleDegrees) > 0);
  QVERIFY(std::abs(run.bodyAngleDegrees) <= 8.0);
  QVERIFY(run.frontPawBounds.x > walk.frontPawBounds.x);
  QVERIFY(run.rearPawBounds.x < walk.rearPawBounds.x);
  QVERIFY(walk.tailStraightness > idle.tailStraightness);
  QVERIFY(run.tailStraightness > walk.tailStraightness);

  const auto insideSilhouette =
      [silhouette = model.silhouetteBounds](
          const InputIndicatorRect &bounds
      ) {
        constexpr double tolerance = 0.0001;
        return bounds.x >= silhouette.x - tolerance &&
               bounds.y >= silhouette.y - tolerance &&
               bounds.x + bounds.width <=
                   silhouette.x + silhouette.width + tolerance &&
               bounds.y + bounds.height <=
                   silhouette.y + silhouette.height + tolerance;
      };
  const InputIndicatorCatPose poses[] = {idle, walk, run};
  for (const auto &pose : poses) {
    QVERIFY(insideSilhouette(pose.headBounds));
    QVERIFY(insideSilhouette(pose.torsoBounds));
    QVERIFY(insideSilhouette(pose.frontPawBounds));
    QVERIFY(insideSilhouette(pose.rearPawBounds));
    QVERIFY(
        pose.languageBounds.x >= pose.torsoBounds.x &&
        pose.languageBounds.y >= pose.torsoBounds.y &&
        pose.languageBounds.x + pose.languageBounds.width <=
            pose.torsoBounds.x + pose.torsoBounds.width &&
        pose.languageBounds.y + pose.languageBounds.height <=
            pose.torsoBounds.y + pose.torsoBounds.height
    );
  }

  const auto reduced = inputIndicatorCatPose(
      InputIndicatorMotionState::Run, 0.75, true
  );
  QCOMPARE(reduced.torsoBounds.x, idle.torsoBounds.x);
  QCOMPARE(reduced.torsoBounds.y, idle.torsoBounds.y);
  QCOMPARE(reduced.torsoBounds.width, idle.torsoBounds.width);
  QCOMPARE(reduced.torsoBounds.height, idle.torsoBounds.height);
  QCOMPARE(reduced.frontPawBounds.x, idle.frontPawBounds.x);
  QCOMPARE(reduced.rearPawBounds.x, idle.rearPawBounds.x);
  QCOMPARE(reduced.bodyAngleDegrees, 0.0);
  QCOMPARE(reduced.tailStraightness, idle.tailStraightness);
}

void InputIndicatorGeometryTests::companionPaletteUsesNeutralBodyAndReadableGlyph()
{
  const auto automatic =
      macInputIndicatorColorModel(InputIndicatorTone::Automatic);
  const auto light =
      macInputIndicatorColorModel(InputIndicatorTone::Light);
  const auto dark =
      macInputIndicatorColorModel(InputIndicatorTone::Dark);

  const MacInputIndicatorColorModel colors[] = {
      automatic, light, dark
  };
  for (const auto &color : colors) {
    QCOMPARE(color.body.red, 242.0 / 255.0);
    QCOMPARE(color.body.green, 243.0 / 255.0);
    QCOMPARE(color.body.blue, 245.0 / 255.0);
    QVERIFY(color.latinGlyph.blue > color.latinGlyph.red);
    QVERIFY(color.koreanGlyph.red > color.koreanGlyph.blue);
    QVERIFY(contrastRatio(color.latinGlyph, color.body) >= 4.5);
    QVERIFY(contrastRatio(color.koreanGlyph, color.body) >= 4.5);
    QVERIFY(color.latinGlyph.red < 0.75);
    QVERIFY(color.koreanGlyph.red < 0.75);
  }
  QCOMPARE(light.body.red, automatic.body.red);
  QCOMPARE(dark.body.red, automatic.body.red);
  QCOMPARE(light.edge.green, automatic.edge.green);
  QCOMPARE(dark.edge.green, automatic.edge.green);

  const InputIndicatorTone tones[] = {
      InputIndicatorTone::Automatic,
      InputIndicatorTone::Light,
      InputIndicatorTone::Dark,
  };
  for (const auto tone : tones) {
    const auto palette = macInputIndicatorColorModel(tone);
    const auto tintedLatinBody = macInputIndicatorBodyColor(
        deskflow::input::InputIndicatorSource::LatinABC, tone
    );
    const auto tintedKoreanBody = macInputIndicatorBodyColor(
        deskflow::input::InputIndicatorSource::Korean2Set, tone
    );
    QVERIFY(contrastRatio(
        palette.latinGlyph, tintedLatinBody
    ) >= 4.5);
    QVERIFY(contrastRatio(
        palette.koreanGlyph, tintedKoreanBody
    ) >= 4.5);
  }

  const auto latinBody = macInputIndicatorBodyColor(
      deskflow::input::InputIndicatorSource::LatinABC,
      InputIndicatorTone::Automatic
  );
  const auto koreanBody = macInputIndicatorBodyColor(
      deskflow::input::InputIndicatorSource::Korean2Set,
      InputIndicatorTone::Automatic
  );
  QVERIFY(std::abs(
      latinBody.red - automatic.body.red
  ) <= std::abs(
      automatic.latinGlyph.red - automatic.body.red
  ) * 0.08 + 1e-12);
  QVERIFY(std::abs(
      koreanBody.blue - automatic.body.blue
  ) <= std::abs(
      automatic.koreanGlyph.blue - automatic.body.blue
  ) * 0.08 + 1e-12);
}

void InputIndicatorGeometryTests::companionHaloPulsesStayBoundedAndReducedMotionSafe()
{
  using Source =
      deskflow::input::InputIndicatorSource;
  std::vector<MacInputIndicatorHaloPulse> pulses;

  QVERIFY(macInputIndicatorAppendHaloPulse(
      pulses, Source::LatinABC, 0
  ));
  QVERIFY(!macInputIndicatorAppendHaloPulse(
      pulses, Source::Korean2Set, 49
  ));
  QVERIFY(macInputIndicatorAppendHaloPulse(
      pulses, Source::Korean2Set, 50
  ));
  QVERIFY(macInputIndicatorAppendHaloPulse(
      pulses, Source::LatinABC, 100
  ));
  QVERIFY(macInputIndicatorAppendHaloPulse(
      pulses, Source::Korean2Set, 150
  ));
  QCOMPARE(
      pulses.size(), kMacInputIndicatorMaximumHaloWaves
  );
  QCOMPARE(pulses[0].source, Source::Korean2Set);
  QCOMPARE(pulses[1].source, Source::LatinABC);
  QCOMPARE(pulses[2].source, Source::Korean2Set);

  const auto early = macInputIndicatorHaloFrame(
      pulses.back(), 160, false
  );
  const auto peak = macInputIndicatorHaloFrame(
      pulses.back(), 200, false
  );
  const auto decay = macInputIndicatorHaloFrame(
      pulses.back(), 360, false
  );
  const auto ended = macInputIndicatorHaloFrame(
      pulses.back(), 430, false
  );
  const auto reduced = macInputIndicatorHaloFrame(
      pulses.back(), 200, true
  );

  QVERIFY(early.active);
  QVERIFY(peak.active);
  QVERIFY(decay.active);
  QVERIFY(!ended.active);
  QVERIFY(!reduced.active);
  QVERIFY(peak.scale <= 1.25);
  QVERIFY(peak.alpha <= 0.34);
  QVERIFY(decay.alpha < peak.alpha);
  QCOMPARE(
      macInputIndicatorActiveHaloFrames(
          pulses, 200, true
      ).size(),
      size_t{0}
  );
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
      root, 10, 0, 0, 0.15, true
  );
  const auto atZero = inputIndicatorCatTailPose(
      root, 10, 0, 0, 0.15, false
  );
  const auto atQuarter = inputIndicatorCatTailPose(
      root, 10, 0.25, 0, 0.15, false
  );
  const auto positiveVelocity = inputIndicatorCatTailPose(
      root, 10, 0.25, 8, 0.15, false
  );
  const auto negativeVelocity = inputIndicatorCatTailPose(
      root, 10, 0.25, -8, 0.15, false
  );
  const auto reducedFast = inputIndicatorCatTailPose(
      root, 10, 0.25, 8, 0.15, true
  );
  const auto runTail = inputIndicatorCatTailPose(
      root, 10, 0.25, 8, 0.85, false
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
  QVERIFY(
      std::abs(runTail.tip.y - runTail.middle.y) <
      std::abs(positiveVelocity.tip.y - positiveVelocity.middle.y)
  );
  QVERIFY(positiveVelocity.tip.x >= root.x);
  QVERIFY(positiveVelocity.tip.x <= root.x + 10.5);
  QCOMPARE(reducedFast.middle.x, neutral.middle.x);
  QCOMPARE(reducedFast.middle.y, neutral.middle.y);
  QCOMPARE(reducedFast.tip.x, neutral.tip.x);
  QCOMPARE(reducedFast.tip.y, neutral.tip.y);
}

void InputIndicatorGeometryTests::catIdleTailMotionsCoverFiveFinitePatterns()
{
  auto state =
      inputIndicatorCatIdleTailMotionInitialState(
          0x13579bdfU
      );
  std::vector<InputIndicatorCatIdleTailMotion> motions;
  double now = 0;
  for (int index = 0; index < 10; ++index) {
    const auto step =
        inputIndicatorCatIdleTailMotionAdvance(
            state,
            InputIndicatorMotionState::Idle,
            now,
            false
        );
    QVERIFY(step.frame.active);
    QVERIFY(std::isfinite(step.frame.progress));
    QVERIFY(step.frame.progress >= 0);
    QVERIFY(step.frame.progress < 1);
    if (!motions.empty()) {
      QVERIFY(motions.back() != step.frame.motion);
    }
    motions.push_back(step.frame.motion);
    state = step.state;
    now =
        state.startedMilliseconds +
        inputIndicatorCatIdleTailMotionDuration(
            step.frame.motion
        );
  }

  const std::set<InputIndicatorCatIdleTailMotion> ids(
      motions.begin(),
      motions.begin() + 5
  );
  QCOMPARE(ids.size(), size_t{5});

  const std::array<InputIndicatorCatIdleTailMotion, 5>
      allMotions{{
          InputIndicatorCatIdleTailMotion::RelaxedSway,
          InputIndicatorCatIdleTailMotion::TipFlick,
          InputIndicatorCatIdleTailMotion::CurlUnfurl,
          InputIndicatorCatIdleTailMotion::LazySweep,
          InputIndicatorCatIdleTailMotion::QuestionLift,
      }};
  std::array<std::vector<InputIndicatorPoint>, 5> samples;
  for (size_t motionIndex = 0;
       motionIndex < allMotions.size();
       ++motionIndex) {
    for (int sample = 0; sample <= 100; ++sample) {
      const double progress = sample / 100.0;
      const auto offset =
          inputIndicatorCatIdleTailOffset(
              allMotions[motionIndex], progress
          );
      QVERIFY(std::isfinite(offset.x));
      QVERIFY(std::isfinite(offset.y));
      QVERIFY(std::abs(offset.x) <= 0.90 + 1e-12);
      QVERIFY(std::abs(offset.y) <= 1.35 + 1e-12);
      if (sample % 20 == 0) {
        samples[motionIndex].push_back(offset);
      }
    }
    const auto start =
        inputIndicatorCatIdleTailOffset(
            allMotions[motionIndex], 0
        );
    const auto end =
        inputIndicatorCatIdleTailOffset(
            allMotions[motionIndex], 1
        );
    QCOMPARE(start.x, 0.0);
    QCOMPARE(start.y, 0.0);
    QCOMPARE(end.x, 0.0);
    QCOMPARE(end.y, 0.0);
  }
  for (size_t left = 0; left < samples.size(); ++left) {
    for (size_t right = left + 1;
         right < samples.size();
         ++right) {
      double difference = 0;
      for (size_t index = 0;
           index < samples[left].size();
           ++index) {
        difference = std::max(
            difference,
            std::hypot(
                samples[left][index].x -
                    samples[right][index].x,
                samples[left][index].y -
                    samples[right][index].y
            )
        );
      }
      QVERIFY(difference > 0.1);
    }
  }
}

void InputIndicatorGeometryTests::catIdleTailMotionGatesMovementAndReducedMotion()
{
  const auto fresh =
      inputIndicatorCatIdleTailMotionInitialState(
          0x2468ace0U
      );
  const auto idle =
      inputIndicatorCatIdleTailMotionAdvance(
          fresh,
          InputIndicatorMotionState::Idle,
          1000,
          false
      );
  QVERIFY(idle.frame.active);

  const auto walk =
      inputIndicatorCatIdleTailMotionAdvance(
          idle.state,
          InputIndicatorMotionState::Walk,
          1100,
          false
      );
  QVERIFY(!walk.frame.active);
  QCOMPARE(walk.frame.progress, 0.0);
  const auto run =
      inputIndicatorCatIdleTailMotionAdvance(
          walk.state,
          InputIndicatorMotionState::Run,
          1200,
          false
      );
  QVERIFY(!run.frame.active);
  QCOMPARE(run.frame.progress, 0.0);

  const auto reentered =
      inputIndicatorCatIdleTailMotionAdvance(
          run.state,
          InputIndicatorMotionState::Idle,
          2000,
          false
      );
  QVERIFY(reentered.frame.active);
  QVERIFY(reentered.frame.motion != idle.frame.motion);
  QCOMPARE(reentered.frame.progress, 0.0);
  QCOMPARE(reentered.state.startedMilliseconds, 2000.0);

  const auto reduced =
      inputIndicatorCatIdleTailMotionAdvance(
          fresh,
          InputIndicatorMotionState::Idle,
          1000,
          true
      );
  QVERIFY(!reduced.frame.active);
  QCOMPARE(reduced.state.seed, fresh.seed);
  QCOMPARE(
      reduced.state.remainingMask,
      fresh.remainingMask
  );
  QCOMPARE(
      reduced.state.hasLastMotion,
      fresh.hasLastMotion
  );
  QCOMPARE(reduced.frame.progress, 0.0);
  const InputIndicatorPoint rest{.x = 3.4, .y = 7.0};
  const auto unchanged =
      inputIndicatorCatIdleTailPoint(
          rest, reduced.frame
      );
  QCOMPARE(unchanged.x, rest.x);
  QCOMPARE(unchanged.y, rest.y);

  const auto invalidTime =
      inputIndicatorCatIdleTailMotionAdvance(
          fresh,
          InputIndicatorMotionState::Idle,
          std::numeric_limits<double>::quiet_NaN(),
          false
      );
  QVERIFY(std::isfinite(
      invalidTime.frame.progress
  ));
  QVERIFY(std::isfinite(
      invalidTime.state.startedMilliseconds
  ));

  const auto nan =
      inputIndicatorCatIdleTailOffset(
          InputIndicatorCatIdleTailMotion::LazySweep,
          std::numeric_limits<double>::quiet_NaN()
      );
  const auto negative =
      inputIndicatorCatIdleTailOffset(
          InputIndicatorCatIdleTailMotion::LazySweep,
          -1
      );
  const auto over =
      inputIndicatorCatIdleTailOffset(
          InputIndicatorCatIdleTailMotion::LazySweep,
          2
      );
  QCOMPARE(nan.x, 0.0);
  QCOMPARE(nan.y, 0.0);
  QCOMPARE(negative.x, 0.0);
  QCOMPARE(negative.y, 0.0);
  QCOMPARE(over.x, 0.0);
  QCOMPARE(over.y, 0.0);
}

void InputIndicatorGeometryTests::catIdleTailMotionIsSeededAndFacingDeterministic()
{
  auto leftState =
      inputIndicatorCatIdleTailMotionInitialState(
          0x10203040U
      );
  auto rightState =
      inputIndicatorCatIdleTailMotionInitialState(
          0x10203040U
      );
  for (const double time : {0.0, 300.0, 900.0, 1800.0, 2700.0}) {
    const auto left =
        inputIndicatorCatIdleTailMotionAdvance(
            leftState,
            InputIndicatorMotionState::Idle,
            time,
            false
        );
    const auto right =
        inputIndicatorCatIdleTailMotionAdvance(
            rightState,
            InputIndicatorMotionState::Idle,
            time,
            false
        );
    QCOMPARE(left.state.seed, right.state.seed);
    QCOMPARE(
        left.state.remainingMask,
        right.state.remainingMask
    );
    QCOMPARE(
        left.state.lastMotion,
        right.state.lastMotion
    );
    QCOMPARE(left.state.running, right.state.running);
    QCOMPARE(
        left.state.startedMilliseconds,
        right.state.startedMilliseconds
    );
    QCOMPARE(left.frame.motion, right.frame.motion);
    QCOMPARE(left.frame.progress, right.frame.progress);
    leftState = left.state;
    rightState = right.state;
  }

  bool differentOrder = false;
  auto firstState =
      inputIndicatorCatIdleTailMotionInitialState(1);
  auto secondState =
      inputIndicatorCatIdleTailMotionInitialState(
          0xfeedcafeU
      );
  for (int index = 0; index < 5; ++index) {
    const double time =
        index == 0
            ? 0
            : firstState.startedMilliseconds +
                  inputIndicatorCatIdleTailMotionDuration(
                      firstState.lastMotion
                  );
    const auto first =
        inputIndicatorCatIdleTailMotionAdvance(
            firstState,
            InputIndicatorMotionState::Idle,
            time,
            false
        );
    const double secondTime =
        index == 0
            ? 0
            : secondState.startedMilliseconds +
                  inputIndicatorCatIdleTailMotionDuration(
                      secondState.lastMotion
                  );
    const auto second =
        inputIndicatorCatIdleTailMotionAdvance(
            secondState,
            InputIndicatorMotionState::Idle,
            secondTime,
            false
        );
    differentOrder =
        differentOrder ||
        first.frame.motion != second.frame.motion;
    firstState = first.state;
    secondState = second.state;
  }
  QVERIFY(differentOrder);

  const auto active =
      inputIndicatorCatIdleTailMotionAdvance(
          inputIndicatorCatIdleTailMotionInitialState(
              0xabcdef01U
          ),
          InputIndicatorMotionState::Idle,
          0,
          false
      );
  const auto midpoint =
      inputIndicatorCatIdleTailMotionAdvance(
          active.state,
          InputIndicatorMotionState::Idle,
          inputIndicatorCatIdleTailMotionDuration(
              active.frame.motion
          ) * 0.5,
          false
      );
  QVERIFY(midpoint.frame.active);
  const auto sourcePoint =
      InputIndicatorPoint{.x = 3.4, .y = 7.0};
  const auto right =
      inputIndicatorCatIdleTailPoint(
          sourcePoint, midpoint.frame
      );
  const auto left =
      inputIndicatorCatFacingPoint(right, false);
  const auto restored =
      inputIndicatorCatFacingPoint(left, false);
  QCOMPARE(left.x, 30.0 - right.x);
  QCOMPARE(left.y, right.y);
  QCOMPARE(restored.x, right.x);
  QCOMPARE(restored.y, right.y);
  QVERIFY(right.x != sourcePoint.x ||
          right.y != sourcePoint.y);
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
          InputIndicatorMotionState::Idle, 0, 0, 0, 180, false
      ),
      InputIndicatorMotionState::Idle
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Idle, 30, 0, 0, 0, false
      ),
      InputIndicatorMotionState::Walk
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Idle, 0, 3, 0, 0, false
      ),
      InputIndicatorMotionState::Walk
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 650, 0, 0, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 0, 48, 0, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 0, 0, 24, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 900, 0, 0, 0, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 900, 48, 24, 0, true
      ),
      InputIndicatorMotionState::Idle
  );
}

void InputIndicatorGeometryTests::companionMotionStateKeepsMovingForSettleDelay()
{
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 0, 0, 0, 179, false
      ),
      InputIndicatorMotionState::Run
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Run, 0, 0, 0, 180, false
      ),
      InputIndicatorMotionState::Idle
  );
  QCOMPARE(
      inputIndicatorMotionState(
          InputIndicatorMotionState::Walk, 0, 0, 0, 179, false
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
