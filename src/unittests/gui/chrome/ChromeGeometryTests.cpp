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

void ChromeGeometryTests::firstWindowSize_isTheWantedSize_whereTheScreenHasTheRoom_data()
{
  QTest::addColumn<QSize>("wanted");
  QTest::addColumn<QSize>("minimum");
  QTest::addColumn<QSize>("available");
  QTest::addColumn<QSize>("frame");
  QTest::addColumn<QSize>("expected");

  const QSize wanted(1000, 729);
  const QSize minimum(720, 520);
  // the work area of the screens that people have, in logical pixels (the screen, less the taskbar, over the scale)
  QTest::newRow("2304x1536 at 100 %") << wanted << minimum << QSize(2304, 1496) << QSize() << wanted;
  QTest::newRow("1920x1080 at 100 %") << wanted << minimum << QSize(1920, 1040) << QSize() << wanted;
  QTest::newRow("1920x1080 at 125 %") << wanted << minimum << QSize(1536, 816) << QSize() << wanted;
  QTest::newRow("1920x1080 at 150 %") << wanted << minimum << QSize(1280, 680) << QSize() << QSize(1000, 664);
  QTest::newRow("1366x768 at 100 %") << wanted << minimum << QSize(1366, 728) << QSize() << QSize(1000, 712);
  QTest::newRow("1366x768 at 125 %") << wanted << minimum << QSize(1092, 576) << QSize() << QSize(1000, 560);
  QTest::newRow("1280x720 at 125 %") << wanted << minimum << QSize(1024, 530) << QSize() << QSize(1000, 520);
  // a window that has the frame of the system around it has less room inside of it
  QTest::newRow("1366x768 at 125 %, with the frame")
      << wanted << minimum << QSize(1092, 576) << QSize(16, 39) << QSize(1000, 521);
  // the window is never smaller than it can be: a screen that is too small for it gets it as it is
  QTest::newRow("1024x600 at 150 %") << wanted << minimum << QSize(682, 360) << QSize() << minimum;
  QTest::newRow("a window that is meant to be small")
      << QSize(800, 480) << minimum << QSize(1920, 1040) << QSize() << QSize(800, 520);
}

void ChromeGeometryTests::firstWindowSize_isTheWantedSize_whereTheScreenHasTheRoom()
{
  QFETCH(QSize, wanted);
  QFETCH(QSize, minimum);
  QFETCH(QSize, available);
  QFETCH(QSize, frame);
  QFETCH(QSize, expected);
  QCOMPARE(firstWindowSize(wanted, minimum, available, frame), expected);
}

void ChromeGeometryTests::edgesOf_saysWhichEdgesARegionResizes()
{
  QCOMPARE(edgesOf(Region::Left), Qt::Edges(Qt::LeftEdge));
  QCOMPARE(edgesOf(Region::Right), Qt::Edges(Qt::RightEdge));
  QCOMPARE(edgesOf(Region::Top), Qt::Edges(Qt::TopEdge));
  QCOMPARE(edgesOf(Region::Bottom), Qt::Edges(Qt::BottomEdge));
  QCOMPARE(edgesOf(Region::TopLeft), Qt::Edges(Qt::TopEdge | Qt::LeftEdge));
  QCOMPARE(edgesOf(Region::TopRight), Qt::Edges(Qt::TopEdge | Qt::RightEdge));
  QCOMPARE(edgesOf(Region::BottomLeft), Qt::Edges(Qt::BottomEdge | Qt::LeftEdge));
  QCOMPARE(edgesOf(Region::BottomRight), Qt::Edges(Qt::BottomEdge | Qt::RightEdge));
  // nothing else resizes
  for (const auto region : {Region::Client, Region::Caption, Region::Minimize, Region::Maximize, Region::Close})
    QCOMPARE(edgesOf(region), Qt::Edges());
}

void ChromeGeometryTests::trackSizes_areTheSizesOfTheWindowAsManyTimesAsTheScale_data()
{
  QTest::addColumn<QSize>("minimum");
  QTest::addColumn<QSize>("maximum");
  QTest::addColumn<qreal>("scale");
  QTest::addColumn<QSize>("frame");
  QTest::addColumn<QSize>("expectedMinimum");
  QTest::addColumn<QSize>("expectedMaximum");

  const QSize none(16777215, 16777215);
  // the least size that the window had on Windows with the Log closed and with it open (the height of the second is
  // what made a window that was restored from maximized 16 pixels higher than it was)
  QTest::newRow("100 %") << QSize(519, 551) << none << 1.0 << QSize() << QSize(519, 551) << QSize(0, 0);
  QTest::newRow("the Log open at 100 %") << QSize(519, 767) << none << 1.0 << QSize() << QSize(519, 767) << QSize(0, 0);
  QTest::newRow("125 %") << QSize(519, 551) << none << 1.25 << QSize() << QSize(649, 689) << QSize(0, 0);
  QTest::newRow("150 %") << QSize(519, 551) << none << 1.5 << QSize() << QSize(779, 827) << QSize(0, 0);
  QTest::newRow("200 %") << QSize(519, 551) << none << 2.0 << QSize() << QSize(1038, 1102) << QSize(0, 0);
  QTest::newRow("no least size") << QSize(0, 0) << none << 1.5 << QSize() << QSize(0, 0) << QSize(0, 0);
  QTest::newRow("a side that is not limited")
      << QSize(519, 0) << QSize(none.width(), 800) << 1.0 << QSize() << QSize(519, 0) << QSize(0, 800);
  QTest::newRow("a most size") << QSize(300, 200) << QSize(1200, 900) << 1.5 << QSize() << QSize(450, 300)
                               << QSize(1800, 1350);
  // the window is never meant to be the least size and not the most: a most size under the least one is the least one
  QTest::newRow("a most size under the least size")
      << QSize(500, 400) << QSize(300, 300) << 1.0 << QSize() << QSize(500, 400) << QSize(500, 400);
  // a window that has a frame of the system takes it from the size that its content has
  QTest::newRow("with a frame") << QSize(500, 400) << none << 1.0 << QSize(16, 39) << QSize(516, 439) << QSize(0, 0);
  QTest::newRow("a side that has no least size has no frame added to it")
      << QSize(500, 0) << none << 1.0 << QSize(16, 39) << QSize(516, 0) << QSize(0, 0);
  QTest::newRow("with a frame and a most size")
      << QSize(500, 400) << QSize(1200, 900) << 1.0 << QSize(16, 39) << QSize(516, 439) << QSize(1216, 939);
}

void ChromeGeometryTests::trackSizes_areTheSizesOfTheWindowAsManyTimesAsTheScale()
{
  QFETCH(QSize, minimum);
  QFETCH(QSize, maximum);
  QFETCH(qreal, scale);
  QFETCH(QSize, frame);
  QFETCH(QSize, expectedMinimum);
  QFETCH(QSize, expectedMaximum);
  const auto sizes = trackSizes(minimum, maximum, scale, frame);
  QCOMPARE(sizes.minimum, expectedMinimum);
  QCOMPARE(sizes.maximum, expectedMaximum);
}

void ChromeGeometryTests::trackSizes_ofAWindowWithNoFrame_addNothingToTheSizes()
{
  // the window with no title bar has no frame to add, at any scale and at any size: the least size of a window that
  // is as small as it can be is what the window is restored to, not a bigger one
  for (const qreal scale : {1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0}) {
    for (const int width : {1, 340, 519, 720, 1000}) {
      for (const int height : {1, 230, 551, 767, 1040}) {
        const auto sizes = trackSizes(QSize(width, height), QSize(16777215, 16777215), scale);
        QCOMPARE(sizes.minimum, QSize(qRound(width * scale), qRound(height * scale)));
        QCOMPARE(sizes.maximum, QSize(0, 0));
      }
    }
  }
}

QTEST_MAIN(ChromeGeometryTests)
