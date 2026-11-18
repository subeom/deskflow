/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ChromeGeometryTests.h"

#include "gui/chrome/ChromeGeometry.h"

using namespace deskflow::gui::chrome;

Q_DECLARE_METATYPE(Region)

namespace {

ChromeMetrics window()
{
  ChromeMetrics metrics;
  metrics.windowSize = QSize(1000, 700);
  metrics.resizeBorder = 6;
  metrics.headerHeight = 40;
  metrics.buttonWidth = 46;
  return metrics;
}

} // namespace

void ChromeGeometryTests::buttons_areAtTheRightEdgeOfTheHeader_closeLast()
{
  const auto metrics = window();
  QCOMPARE(closeRect(metrics), QRect(954, 0, 46, 40));
  QCOMPARE(maximizeRect(metrics), QRect(908, 0, 46, 40));
  QCOMPARE(minimizeRect(metrics), QRect(862, 0, 46, 40));

  // they follow the width of the window
  auto wider = metrics;
  wider.windowSize = QSize(1280, 700);
  QCOMPARE(closeRect(wider).right(), 1279);
}

void ChromeGeometryTests::buttons_areNotThereWhereTheSystemDrawsThem()
{
  auto metrics = window();
  metrics.buttonWidth = 0;
  QVERIFY(closeRect(metrics).isEmpty());
  QVERIFY(maximizeRect(metrics).isEmpty());
  QVERIFY(minimizeRect(metrics).isEmpty());
  // the right end of the header is the header then
  QCOMPARE(hitTest(metrics, QPoint(980, 20)), Region::Caption);
}

void ChromeGeometryTests::hitTest_says_whatIsAtAPoint_data()
{
  QTest::addColumn<QPoint>("point");
  QTest::addColumn<Region>("region");

  QTest::newRow("the content") << QPoint(500, 400) << Region::Client;
  QTest::newRow("the empty header") << QPoint(400, 20) << Region::Caption;
  QTest::newRow("the header up to the first button") << QPoint(861, 20) << Region::Caption;
  QTest::newRow("minimize") << QPoint(870, 20) << Region::Minimize;
  QTest::newRow("maximize") << QPoint(930, 20) << Region::Maximize;
  QTest::newRow("close") << QPoint(980, 20) << Region::Close;
  QTest::newRow("just under the header") << QPoint(400, 40) << Region::Client;
  QTest::newRow("the last row of the header") << QPoint(400, 39) << Region::Caption;

  QTest::newRow("left edge") << QPoint(2, 300) << Region::Left;
  QTest::newRow("right edge") << QPoint(997, 300) << Region::Right;
  QTest::newRow("top edge") << QPoint(400, 2) << Region::Top;
  QTest::newRow("bottom edge") << QPoint(500, 698) << Region::Bottom;
  QTest::newRow("top left") << QPoint(2, 2) << Region::TopLeft;
  QTest::newRow("top right") << QPoint(997, 2) << Region::TopRight;
  QTest::newRow("bottom left") << QPoint(2, 698) << Region::BottomLeft;
  QTest::newRow("bottom right") << QPoint(997, 698) << Region::BottomRight;
  QTest::newRow("the first pixel inside the border") << QPoint(6, 300) << Region::Client;
  QTest::newRow("the last pixel of the border") << QPoint(5, 300) << Region::Left;
  QTest::newRow("left of the header is the edge") << QPoint(2, 20) << Region::Left;

  // the buttons are the whole height of the header: the strip of the top edge does not take their top rows
  QTest::newRow("top row of close") << QPoint(980, 0) << Region::Close;
  QTest::newRow("top row of maximize") << QPoint(930, 1) << Region::Maximize;
  QTest::newRow("top row of minimize") << QPoint(870, 2) << Region::Minimize;
  QTest::newRow("the corner is the corner, though close is there") << QPoint(998, 1) << Region::TopRight;
  QTest::newRow("beside the buttons, the top edge") << QPoint(861, 2) << Region::Top;

  QTest::newRow("outside, left") << QPoint(-1, 100) << Region::Client;
  QTest::newRow("outside, right") << QPoint(1000, 100) << Region::Client;
  QTest::newRow("outside, below") << QPoint(100, 700) << Region::Client;
}

void ChromeGeometryTests::hitTest_says_whatIsAtAPoint()
{
  QFETCH(QPoint, point);
  QFETCH(Region, region);
  QCOMPARE(hitTest(window(), point), region);
}

void ChromeGeometryTests::hitTest_ofAMaximizedWindow_hasNoEdges()
{
  auto metrics = window();
  metrics.maximized = true;
  // the screen has the edges, not the window: the corner of the header is the header
  QCOMPARE(hitTest(metrics, QPoint(2, 2)), Region::Caption);
  QCOMPARE(hitTest(metrics, QPoint(400, 1)), Region::Caption);
  QCOMPARE(hitTest(metrics, QPoint(2, 300)), Region::Client);
  QCOMPARE(hitTest(metrics, QPoint(500, 698)), Region::Client);
  // and the buttons are where they were
  QCOMPARE(hitTest(metrics, QPoint(980, 1)), Region::Close);
  QCOMPARE(hitTest(metrics, QPoint(930, 20)), Region::Maximize);
}

void ChromeGeometryTests::hitTest_ofAWindowThatIsNotResized_hasNoEdges()
{
  auto metrics = window();
  metrics.resizeBorder = 0;
  QCOMPARE(hitTest(metrics, QPoint(2, 300)), Region::Client);
  QCOMPARE(hitTest(metrics, QPoint(400, 2)), Region::Caption);
  QCOMPARE(hitTest(metrics, QPoint(997, 698)), Region::Client);
}

void ChromeGeometryTests::hitTest_whereTheMenuButtonIs_isTheClient()
{
  // the menu button of the header takes the mouse for itself: a press on it is not the start of a drag
  const QList<QRect> interactive{QRect(800, 4, 40, 32)};
  QCOMPARE(hitTest(window(), QPoint(820, 20), interactive), Region::Client);
  QCOMPARE(hitTest(window(), QPoint(760, 20), interactive), Region::Caption);
  // the edge of the window is the edge though the button is under it
  QCOMPARE(hitTest(window(), QPoint(820, 2), interactive), Region::Top);
  // a rectangle that is below the header is not a part of the header
  QCOMPARE(hitTest(window(), QPoint(500, 400), {QRect(400, 300, 200, 200)}), Region::Client);
}

void ChromeGeometryTests::hitTest_everyPointOfTheWindow_isSomething_andTheEdgesDoNotCoverTheContent()
{
  const auto metrics = window();
  const int w = metrics.windowSize.width();
  const int h = metrics.windowSize.height();
  const int border = metrics.resizeBorder;
  const auto isEdge = [](Region region) {
    return region != Region::Client && region != Region::Caption && region != Region::Minimize &&
           region != Region::Maximize && region != Region::Close;
  };
  const auto isButton = [](Region region) {
    return region == Region::Minimize || region == Region::Maximize || region == Region::Close;
  };
  int captions = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const auto region = hitTest(metrics, QPoint(x, y));
      const bool inStrip = x < border || x >= w - border || y < border || y >= h - border;
      if (inStrip) {
        // the strip along the four sides resizes, except where the top rows of a button are (not its corner)
        const bool topOfAButton = y < border && x >= w - 3 * 46 && x < w - border;
        QVERIFY2(
            isEdge(region) || (topOfAButton && isButton(region)), qPrintable(QStringLiteral("%1,%2").arg(x).arg(y))
        );
      } else {
        // what is inside the strip is never an edge
        QVERIFY2(!isEdge(region), qPrintable(QStringLiteral("%1,%2").arg(x).arg(y)));
        // and under the header there is the content, whole
        if (y >= metrics.headerHeight)
          QCOMPARE(region, Region::Client);
        if (region == Region::Caption)
          ++captions;
      }
    }
  }
  QVERIFY(captions > 0);
}

void ChromeGeometryTests::maximizedInsets_areTheThicknessOfTheFrameOnEverySide()
{
  QCOMPARE(maximizedInsets(8), QMargins(8, 8, 8, 8));
  QCOMPARE(maximizedInsets(0), QMargins());
  // a thickness that cannot be is none
  QCOMPARE(maximizedInsets(-3), QMargins());
}

QTEST_MAIN(ChromeGeometryTests)
