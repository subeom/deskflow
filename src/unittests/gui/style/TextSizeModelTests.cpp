/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TextSizeModelTests.h"

#include "gui/style/TextSizeModel.h"

using namespace deskflow::gui::style;

Q_DECLARE_METATYPE(TextPlatform)

void TextSizeModelTests::minimumBodyPixels_isTheBodyOfTheSettingsOfTheOs()
{
  QCOMPARE(minimumBodyPixels(TextPlatform::Windows), 14.0);
  QCOMPARE(minimumBodyPixels(TextPlatform::Mac), 13.0);
  QCOMPARE(minimumBodyPixels(TextPlatform::Linux), 13.0);
  QCOMPARE(minimumBodyPixels(TextPlatform::Other), 13.0);
}

void TextSizeModelTests::plan_data()
{
  QTest::addColumn<TextPlatform>("platform");
  QTest::addColumn<double>("system");
  QTest::addColumn<double>("osScale");
  QTest::addColumn<int>("percent");
  QTest::addColumn<double>("body");
  QTest::addColumn<double>("metric");
  QTest::addColumn<bool>("raised");

  // Windows: Segoe UI 9 pt is 12 px, and the settings of Windows 11 have 14 px
  QTest::newRow("Windows 9 pt") << TextPlatform::Windows << 12.0 << 1.0 << 100 << 14.0 << 1.0 << true;
  QTest::newRow("Windows with a system font of 14 px")
      << TextPlatform::Windows << 14.0 << 1.0 << 100 << 14.0 << 1.0 << false;
  QTest::newRow("Windows Text size 125 %")
      << TextPlatform::Windows << 12.0 << 1.25 << 100 << 15.0 << 15.0 / 14.0 << false;
  QTest::newRow("Windows Text size 150 %")
      << TextPlatform::Windows << 12.0 << 1.5 << 100 << 18.0 << 18.0 / 14.0 << false;
  QTest::newRow("Windows Text size 100 % and a larger font than the floor")
      << TextPlatform::Windows << 16.0 << 1.0 << 100 << 16.0 << 16.0 / 14.0 << false;
  QTest::newRow("Windows smaller 90 %") << TextPlatform::Windows << 12.0 << 1.0 << 90 << 12.6 << 1.0 << true;
  QTest::newRow("Windows large 115 %") << TextPlatform::Windows << 12.0 << 1.0 << 115 << 16.1 << 16.1 / 14.0 << true;
  QTest::newRow("Windows larger 130 %") << TextPlatform::Windows << 12.0 << 1.0 << 130 << 18.2 << 18.2 / 14.0 << true;
  QTest::newRow("Windows largest 150 %") << TextPlatform::Windows << 12.0 << 1.0 << 150 << 21.0 << 1.5 << true;
  QTest::newRow("Windows largest 150 % with Text size 150 %")
      << TextPlatform::Windows << 12.0 << 1.5 << 150 << 27.0 << 27.0 / 14.0 << false;

  // macOS: the system font is 13 pt, which the settings of macOS use: nothing is raised
  QTest::newRow("macOS 13 pt") << TextPlatform::Mac << 13.0 << 1.0 << 100 << 13.0 << 1.0 << false;
  QTest::newRow("macOS smaller 90 %") << TextPlatform::Mac << 13.0 << 1.0 << 90 << 11.7 << 1.0 << false;
  QTest::newRow("macOS largest 150 %") << TextPlatform::Mac << 13.0 << 1.0 << 150 << 19.5 << 19.5 / 14.0 << false;

  // Linux: GNOME has 14.7 px (11 pt Cantarell), KDE 13.3 px, and a bare Qt 12 px
  QTest::newRow("GNOME 11 pt") << TextPlatform::Linux << 14.67 << 1.0 << 100 << 14.67 << 14.67 / 14.0 << false;
  QTest::newRow("KDE 10 pt") << TextPlatform::Linux << 13.33 << 1.0 << 100 << 13.33 << 1.0 << false;
  QTest::newRow("Qt default 9 pt") << TextPlatform::Linux << 12.0 << 1.0 << 100 << 13.0 << 1.0 << true;
  QTest::newRow("Other 12 px") << TextPlatform::Other << 12.0 << 1.0 << 100 << 13.0 << 1.0 << true;
}

void TextSizeModelTests::plan()
{
  QFETCH(TextPlatform, platform);
  QFETCH(double, system);
  QFETCH(double, osScale);
  QFETCH(int, percent);
  QFETCH(double, body);
  QFETCH(double, metric);
  QFETCH(bool, raised);

  TextSizeInput input;
  input.platform = platform;
  input.systemPixels = system;
  input.osTextScale = osScale;
  input.percent = percent;
  const auto plan = planTextSize(input);
  QVERIFY2(
      qAbs(plan.bodyPixels - body) < 0.01,
      qPrintable(QStringLiteral("body %1, expected %2").arg(plan.bodyPixels).arg(body))
  );
  QVERIFY2(
      qAbs(plan.metricScale - metric) < 0.01,
      qPrintable(QStringLiteral("metric %1, expected %2").arg(plan.metricScale).arg(metric))
  );
  QCOMPARE(plan.raisedByFloor, raised);
}

void TextSizeModelTests::plan_neverMakesTheControlsSmallerThanTheyWereDrawn_andStopsAtTheLimits()
{
  TextSizeInput input;
  input.platform = TextPlatform::Windows;
  // a person who chose smaller text still has the controls that were drawn
  input.systemPixels = 12;
  input.percent = 90;
  QCOMPARE(planTextSize(input).metricScale, 1.0);

  // nonsense is taken for the floor
  for (const double system : {0.0, -3.0}) {
    input.systemPixels = system;
    input.percent = 100;
    QCOMPARE(planTextSize(input).bodyPixels, 14.0);
  }

  // the text stops at 40 px, and the controls at twice the reference
  input.systemPixels = 30;
  input.osTextScale = 2.25;
  input.percent = 150;
  const auto huge = planTextSize(input);
  QCOMPARE(huge.bodyPixels, kMostBodyPixels);
  QCOMPARE(huge.metricScale, kMostMetricScale);

  // the OS that says less than 1 is not trusted to make text smaller than the system gave
  input.systemPixels = 16;
  input.osTextScale = 0.5;
  input.percent = 100;
  QCOMPARE(planTextSize(input).bodyPixels, 16.0);
}

void TextSizeModelTests::plan_aSystemThatIsTooSmallIsRaised_aSystemThatIsLargeEnoughIsKept()
{
  TextSizeInput input;
  input.platform = TextPlatform::Windows;
  input.systemPixels = 13.9;
  QVERIFY(planTextSize(input).raisedByFloor);
  QCOMPARE(planTextSize(input).bodyPixels, 14.0);
  input.systemPixels = 14.0;
  QVERIFY(!planTextSize(input).raisedByFloor);
  input.systemPixels = 14.1;
  QCOMPARE(planTextSize(input).bodyPixels, 14.1);
}

void TextSizeModelTests::plan_thePercentIsOfTheSizeThatTheOsGave_notOfTheReference()
{
  // a person on a Mac who asks for 115 % has 15 px (115 % of the 13 px of the system), and not 16.1
  TextSizeInput input;
  input.platform = TextPlatform::Mac;
  input.systemPixels = 13;
  input.percent = 115;
  QVERIFY(qAbs(planTextSize(input).bodyPixels - 14.95) < 0.01);
  // and on Windows the percent is of the raised size, so that 100 % is what the settings of Windows have
  input.platform = TextPlatform::Windows;
  input.systemPixels = 12;
  input.percent = 100;
  QCOMPARE(planTextSize(input).bodyPixels, 14.0);
}

void TextSizeModelTests::plan_anUnknownPercentIsTheDefault()
{
  TextSizeInput input;
  input.platform = TextPlatform::Mac;
  input.systemPixels = 13;
  for (const int percent : {-10, 0, 95, 200, 1000}) {
    input.percent = percent;
    QCOMPARE(planTextSize(input).bodyPixels, 13.0);
  }
}

void TextSizeModelTests::steps_areFive_andTheDefaultIsOneOfThem()
{
  const auto steps = textSizePercentSteps();
  QCOMPARE(steps, (QList<int>{90, 100, 115, 130, 150}));
  QCOMPARE(kDefaultTextPercent, 100);
  QVERIFY(steps.contains(kDefaultTextPercent));
}

void TextSizeModelTests::snap_keepsAStepAndTakesTheDefaultForAnythingElse()
{
  for (const int step : {90, 100, 115, 130, 150})
    QCOMPARE(snapTextPercent(step), step);
  for (const int other : {-1, 0, 1, 89, 95, 101, 140, 151, 175, 200, 12345})
    QCOMPARE(snapTextPercent(other), kDefaultTextPercent);
}

void TextSizeModelTests::step_goesOneAtATime_stopsAtTheEnds_andZeroGoesBackToTheDefault()
{
  QCOMPARE(stepTextPercent(100, 1), 115);
  QCOMPARE(stepTextPercent(115, 1), 130);
  QCOMPARE(stepTextPercent(130, 1), 150);
  QCOMPARE(stepTextPercent(150, 1), 150);
  QCOMPARE(stepTextPercent(100, -1), 90);
  QCOMPARE(stepTextPercent(90, -1), 90);
  QCOMPARE(stepTextPercent(150, -1), 130);
  // any size of the direction goes one step
  QCOMPARE(stepTextPercent(100, 5), 115);
  QCOMPARE(stepTextPercent(130, -9), 115);
  // 0 resets
  for (const int percent : {90, 100, 115, 130, 150})
    QCOMPARE(stepTextPercent(percent, 0), 100);
  // what is not a step is the default first
  QCOMPARE(stepTextPercent(95, 1), 115);
  QCOMPARE(stepTextPercent(95, -1), 90);
}

void TextSizeModelTests::osTextScale_isAMultipleBetweenOneAndTwoAndAQuarter()
{
  QCOMPARE(osTextScaleFromPercent(100), 1.0);
  QCOMPARE(osTextScaleFromPercent(150), 1.5);
  QCOMPARE(osTextScaleFromPercent(225), 2.25);
  for (const int nonsense : {-5, 0, 50, 99, 226, 300, 100000})
    QCOMPARE(osTextScaleFromPercent(nonsense), 1.0);
}

void TextSizeModelTests::scaledPixels_roundsAndNeverLosesASize()
{
  QCOMPARE(scaledPixels(44, 1.0), 44);
  QCOMPARE(scaledPixels(44, 1.15), 51);
  QCOMPARE(scaledPixels(46, 1.5), 69);
  QCOMPARE(scaledPixels(28, 2.0), 56);
  // a small size is not scaled away
  QCOMPARE(scaledPixels(1, 1.2), 1);
  QCOMPARE(scaledPixels(2, 1.0), 2);
  // nothing and negative sizes stay as they are
  QCOMPARE(scaledPixels(0, 2.0), 0);
  QCOMPARE(scaledPixels(-4, 2.0), -8);
}

QTEST_APPLESS_MAIN(TextSizeModelTests)
