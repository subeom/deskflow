/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ArrangementCanvasTests.h"

#include "gui/widgets/ArrangementCanvas.h"

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QSignalSpy>

using namespace deskflow::server;
using namespace deskflow::server::arrangement;

namespace {

constexpr auto kServerShape = "0,0,3440,1440;753,1440,2064,1347";
constexpr auto kWindowsShape = "-1920,360,1920,1080;0,0,2560,1440";
constexpr auto kHd = "0,0,1920,1080";

Computer pc(const char *name, const char *monitors, double x, double y, double zoom = 1.0, bool server = false)
{
  const auto layout = MonitorLayout::parse(monitors);
  if (!layout)
    qFatal("the monitors \"%s\" were not accepted", monitors);
  Computer computer;
  computer.name = name;
  computer.monitors = layout.layout->monitors();
  computer.originX = x;
  computer.originY = y;
  computer.zoom = zoom;
  computer.isServer = server;
  return computer;
}

// the MacBook with the DELL, and a Windows PC with two monitors on the left of the DELL, a Mac mini on its right, and a
// laptop under the DELL and beside the MacBook
Arrangement userArrangement()
{
  return {{
      pc("Subeoms-MacBook-Pro.local", kServerShape, 0, 0, 1.0, true),
      pc("9800X3D-5080", kWindowsShape, -2560, 0),
      pc("Subeoms-Mac-mini.local", kHd, 3440, 0),
      pc("SUBEOM-COPILOT", kHd, -783, 1440, 0.8),
  }};
}

std::unique_ptr<ArrangementCanvas> canvasOf(const Arrangement &arrangement, QSize size = {900, 520})
{
  auto canvas = std::make_unique<ArrangementCanvas>();
  canvas->resize(size);
  canvas->setArrangement(arrangement);
  return canvas;
}

const Computer &computerOf(const ArrangementCanvas &canvas, const char *name)
{
  const auto &arrangement = canvas.arrangement();
  return arrangement.computers[static_cast<size_t>(arrangement.indexOf(name))];
}

// the pixel of the widget that is \p units of the arrangement from \p from, which is a place of the widget
QPoint plus(const ArrangementCanvas &canvas, const QPointF &from, double unitsX, double unitsY)
{
  const auto scale = canvas.boxOf("A").width() / 1920.0;
  return QPointF(from.x() + unitsX * scale, from.y() + unitsY * scale).toPoint();
}

void drag(ArrangementCanvas &canvas, const QPoint &from, const QPoint &to)
{
  QTest::mousePress(&canvas, Qt::LeftButton, {}, from);
  // in steps, like a hand
  for (int i = 1; i <= 8; ++i)
    QTest::mouseMove(&canvas, from + (to - from) * i / 8);
  QTest::mouseRelease(&canvas, Qt::LeftButton, {}, to);
}

Arrangement twoApart()
{
  return {{pc("A", kHd, 0, 0), pc("B", kHd, 2600, 300)}};
}

// the pointer moves over the widget with \p buttons down and \p modifiers held
void moveWith(QWidget *widget, const QPoint &point, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
  QMouseEvent event(
      QEvent::MouseMove, QPointF(point), widget->mapToGlobal(QPointF(point)), Qt::NoButton, buttons, modifiers
  );
  QApplication::sendEvent(widget, &event);
}

// a drag with a modifier held
void dragWith(
    QWidget *widget, const QPoint &from, const QPoint &to, Qt::KeyboardModifiers modifiers, bool release = true
)
{
  QTest::mousePress(widget, Qt::LeftButton, modifiers, from);
  for (int i = 1; i <= 8; ++i)
    moveWith(widget, from + (to - from) * i / 8, Qt::LeftButton, modifiers);
  if (release)
    QTest::mouseRelease(widget, Qt::LeftButton, modifiers, to);
}

// the pointer moves over the widget with no button down
void hover(QWidget *widget, const QPoint &point)
{
  QMouseEvent event(
      QEvent::MouseMove, QPointF(point), widget->mapToGlobal(QPointF(point)), Qt::NoButton, Qt::NoButton, Qt::NoModifier
  );
  QApplication::sendEvent(widget, &event);
}

QPixmap render(ArrangementCanvas &canvas)
{
  QPixmap pixmap(canvas.size() * 2);
  pixmap.setDevicePixelRatio(2.0);
  canvas.render(&pixmap);
  return pixmap;
}

void save(const QPixmap &pixmap, const QString &name)
{
  const auto directory = qEnvironmentVariable("DESKFLOW_SCREENSHOT_DIR");
  if (directory.isEmpty())
    return;
  QDir().mkpath(directory);
  QVERIFY2(pixmap.save(QDir(directory).filePath(name)), qPrintable(name));
}

} // namespace

namespace {
QPoint onBox(const QRectF &r, double x, double y, double dx = 0, double dy = 0)
{
  return QPoint(
      static_cast<int>(std::lround(r.left() + r.width() * x + dx)),
      static_cast<int>(std::lround(r.top() + r.height() * y + dy))
  );
}
} // namespace

void ArrangementCanvasTests::drag_aComputerSnapsToTheEdgeOfAnother()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy changed(canvas.get(), &ArrangementCanvas::arrangementChanged);
  const auto from = canvas->boxOf("B").center().toPoint();
  // to a few units from the right edge of A, a little under the top: the edges snap
  drag(*canvas, from, plus(*canvas, from, 1930 - 2600, 12 - 300));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(computerOf(*canvas, "B").originX, 1920.0);
  QCOMPARE(computerOf(*canvas, "B").originY, 0.0);
  const auto links = canvas->derived().of("A", Direction::Right);
  QCOMPARE(links.size(), size_t(1));
  QCOMPARE(links.front().to, std::string("B"));
  QVERIFY(canvas->issues().empty());
  QCOMPARE(canvas->selected(), QStringLiteral("B"));
}

void ArrangementCanvasTests::drag_belowTheThreshold_isAClick()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy changed(canvas.get(), &ArrangementCanvas::arrangementChanged);
  const auto at = canvas->boxOf("B").center().toPoint();
  QTest::mouseClick(canvas.get(), Qt::LeftButton, {}, at);
  QCOMPARE(changed.count(), 0);
  QCOMPARE(computerOf(*canvas, "B").originX, 2600.0);
}

void ArrangementCanvasTests::drag_ontoAnotherComputer_isPushedOff()
{
  auto canvas = canvasOf(twoApart());
  const auto from = canvas->boxOf("B").center().toPoint();
  drag(*canvas, from, plus(*canvas, from, 1500 - 2600, 200 - 300));
  QVERIFY(canvas->issues().empty());
  // it is on the side of A that is nearest, not on A
  QCOMPARE(computerOf(*canvas, "B").originX, 1920.0);
}

void ArrangementCanvasTests::drag_escapeCancelsIt()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy changed(canvas.get(), &ArrangementCanvas::arrangementChanged);
  const auto from = canvas->boxOf("B").center().toPoint();
  QTest::mousePress(canvas.get(), Qt::LeftButton, {}, from);
  QTest::mouseMove(canvas.get(), from + QPoint(-60, -10));
  QTest::keyClick(canvas.get(), Qt::Key_Escape);
  QTest::mouseRelease(canvas.get(), Qt::LeftButton, {}, from + QPoint(-60, -10));
  QCOMPARE(changed.count(), 0);
  QCOMPARE(computerOf(*canvas, "B").originX, 2600.0);
}

void ArrangementCanvasTests::drag_aRightClickInTheMiddleOfIt_doesNotEndIt()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy changed(canvas.get(), &ArrangementCanvas::arrangementChanged);
  const auto from = canvas->boxOf("B").center().toPoint();
  const auto to = plus(*canvas, from, 1930 - 2600, 12 - 300);
  QTest::mousePress(canvas.get(), Qt::LeftButton, {}, from);
  QTest::mouseMove(canvas.get(), from + (to - from) / 2);
  // the other button goes down and up while the first is held: the drag goes on
  QTest::mousePress(canvas.get(), Qt::RightButton, {}, from + (to - from) / 2);
  QTest::mouseRelease(canvas.get(), Qt::RightButton, {}, from + (to - from) / 2);
  QCOMPARE(changed.count(), 0);
  QTest::mouseMove(canvas.get(), to);
  QTest::mouseRelease(canvas.get(), Qt::LeftButton, {}, to);
  QCOMPARE(changed.count(), 1);
  QCOMPARE(computerOf(*canvas, "B").originX, 1920.0);
}

void ArrangementCanvasTests::assumedSize_isMarkedOnTheComputerThatHasIt()
{
  auto canvas = canvasOf(twoApart());
  const auto plain = render(*canvas).toImage();
  const auto inA = [&](const QImage &image) {
    const auto box = canvas->boxOf("A");
    return image.copy(QRect(box.topLeft().toPoint() * 2, box.size().toSize() * 2));
  };

  canvas->setAssumedSizes({"B"});
  QCOMPARE(canvas->assumedSizes(), QStringList({"B"}));
  const auto marked = render(*canvas).toImage();
  QVERIFY(marked != plain);
  // the computer that has its size is as it was
  QCOMPARE(inA(marked), inA(plain));

  // a computer that is not on the canvas is not marked
  canvas->setAssumedSizes({"nobody"});
  QCOMPARE(render(*canvas).toImage(), plain);
}

void ArrangementCanvasTests::resize_theEdgesOfTheOneRectangleOfAComputer_changeItsSize()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  QSignalSpy changed(canvas.get(), &ArrangementCanvas::arrangementChanged);
  canvas->setSelected("B");
  auto box = canvas->boxOf("B");
  const auto scale = box.width() / 1920.0;
  const auto at = [&](const QRectF &r, double x, double y) {
    return QPoint(
        static_cast<int>(std::lround(r.left() + r.width() * x)), static_cast<int>(std::lround(r.top() + r.height() * y))
    );
  };

  // the pointer is a resize cursor over an edge or a corner of the selected computer, and a hand over the rest of it
  hover(canvas.get(), at(box, 1.0, 0.5));
  QCOMPARE(canvas->cursor().shape(), Qt::SizeHorCursor);
  hover(canvas.get(), at(box, 0.5, 1.0));
  QCOMPARE(canvas->cursor().shape(), Qt::SizeVerCursor);
  hover(canvas.get(), at(box, 1.0, 1.0));
  QCOMPARE(canvas->cursor().shape(), Qt::SizeFDiagCursor);
  hover(canvas.get(), at(box, 0.5, 0.5));
  QCOMPARE(canvas->cursor().shape(), Qt::OpenHandCursor);

  // the right edge goes 200 units to the right: it is 2120 wide, and where it was
  drag(*canvas, at(box, 1.0, 0.5), at(box, 1.0, 0.5) + QPoint(static_cast<int>(std::lround(200 * scale)), 0));
  QCOMPARE(resized.count(), 1);
  QCOMPARE(resized.at(0).at(0).toString(), QStringLiteral("B"));
  QVERIFY2(
      std::abs(resized.at(0).at(3).toInt() - 2120) <= 2.0 / scale,
      qPrintable(QString::number(resized.at(0).at(3).toInt()))
  );
  QCOMPARE(resized.at(0).at(4).toInt(), 1080);
  QCOMPARE(changed.count(), 1);
  QCOMPARE(computerOf(*canvas, "B").originX, 2600.0);
  QCOMPARE(computerOf(*canvas, "A").monitors.front().w, 1920);

  // the left edge goes 100 units to the right: the right edge stays where it is
  box = canvas->boxOf("B");
  const auto rightBefore = box.right();
  drag(*canvas, at(box, 0.0, 0.5), at(box, 0.0, 0.5) + QPoint(static_cast<int>(std::lround(100 * scale)), 0));
  QCOMPARE(resized.count(), 2);
  QVERIFY(std::abs(canvas->boxOf("B").right() - rightBefore) <= 1.0);
  // (the view is fitted again after a change)
  const auto fitted = canvas->boxOf("A").width() / 1920.0;
  QVERIFY(std::abs(canvas->boxOf("B").width() / fitted - resized.at(1).at(3).toInt()) <= 1.0);
  QVERIFY(computerOf(*canvas, "B").originX > 2600.0);
}

void ArrangementCanvasTests::resize_anEdgeSnapsToTheNeighbour_theRatiosAndThePresets()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  canvas->setSelected("B");
  auto box = canvas->boxOf("B");
  const auto scale = box.width() / 1920.0;
  const auto point = [&](const QRectF &r, double x, double y) {
    return QPoint(
        static_cast<int>(std::lround(r.left() + r.width() * x)), static_cast<int>(std::lround(r.top() + r.height() * y))
    );
  };

  // the left edge of B 5 units from touching the right edge of A: it touches
  drag(*canvas, point(box, 0.0, 0.5), point(box, 0.0, 0.5) - QPoint(static_cast<int>(std::lround(675 * scale)), 0));
  QCOMPARE(resized.count(), 1);
  QCOMPARE(computerOf(*canvas, "B").originX, 1920.0);
  QVERIFY(canvas->issues().empty());

  // the corner, near 2560 x 1440 (a preset, which is also a ratio): that size
  auto other = canvasOf(twoApart());
  QSignalSpy sized(other.get(), &ArrangementCanvas::computerResized);
  other->setSelected("B");
  box = other->boxOf("B");
  drag(
      *other, point(box, 1.0, 1.0),
      point(box, 1.0, 1.0) +
          QPoint(static_cast<int>(std::lround(637 * scale)), static_cast<int>(std::lround(357 * scale)))
  );
  QCOMPARE(sized.count(), 1);
  QCOMPARE(sized.at(0).at(3).toInt(), 2560);
  QCOMPARE(sized.at(0).at(4).toInt(), 1440);
}

void ArrangementCanvasTests::resize_aComputerThatIsZoomed_isSizedInItsOwnUnits()
{
  // a Windows PC at 125 %: a unit of the computer is 0.8 of the drawing
  auto canvas = canvasOf({{pc("A", kHd, 0, 0), pc("B", kHd, 2600, 300, 0.8)}});
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  canvas->setSelected("B");
  const auto box = canvas->boxOf("B");
  const auto scale = canvas->boxOf("A").width() / 1920.0;
  const QPoint right(static_cast<int>(std::lround(box.right())), static_cast<int>(std::lround(box.center().y())));
  drag(*canvas, right, right + QPoint(static_cast<int>(std::lround(100 * scale)), 0));
  QCOMPARE(resized.count(), 1);
  // 1536 + 100 units of the drawing are 2045 pixels of the computer
  QVERIFY2(std::abs(resized.at(0).at(3).toInt() - 2045) <= 3, qPrintable(QString::number(resized.at(0).at(3).toInt())));
  QCOMPARE(computerOf(*canvas, "B").originX, 2600.0);
  QVERIFY(std::abs(computerOf(*canvas, "B").box().w - resized.at(0).at(3).toInt() * 0.8) < 1e-6);
}

void ArrangementCanvasTests::resize_theServer_isPutAndNotResized()
{
  auto canvas = canvasOf(userArrangement());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  QSignalSpy scaled(canvas.get(), &ArrangementCanvas::computerScaled);
  const char *name = "Subeoms-MacBook-Pro.local";
  canvas->setSelected(name);
  const auto box = canvas->boxOf(name);
  // the bottom edge of the MacBook, which no other computer is at
  const auto edge = onBox(box, 0.5, 1.0);
  hover(canvas.get(), edge);
  QVERIFY(canvas->cursor().shape() != Qt::SizeVerCursor);
  QVERIFY(canvas->hoveredComputer().isEmpty());
  // dragged by its outline it is moved, as by its middle, and not resized
  drag(*canvas, edge, edge + QPoint(30, 0));
  QCOMPARE(resized.count(), 0);
  QCOMPARE(scaled.count(), 0);
}

void ArrangementCanvasTests::resize_aPresetSnapOnOneEdge_doesNotMoveTheOtherOne()
{
  // C is under A and touches it: the bottom of A is where it is when its right edge goes to the size of a preset
  auto canvas = canvasOf({{pc("A", "0,0,1920,1090", 0, 0), pc("C", "0,0,1000,500", 0, 1090)}});
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  canvas->setSelected("A");
  const auto box = canvas->boxOf("A");
  const auto scale = box.width() / 1920.0;
  const QPoint right(static_cast<int>(std::lround(box.right())), static_cast<int>(std::lround(box.center().y())));
  // 20 units to the right: 1940 wide, which is near 1920 x 1080 (the height is not: it is where C needs it)
  drag(*canvas, right, right + QPoint(static_cast<int>(std::lround(20 * scale)), 0));
  QCOMPARE(resized.count(), 1);
  QCOMPARE(resized.at(0).at(3).toInt(), 1920);
  QCOMPARE(resized.at(0).at(4).toInt(), 1090);
  QVERIFY(canvas->issues().empty());
}

void ArrangementCanvasTests::resize_beyondWhatALayoutCanHave_isNeverStored()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  canvas->setSelected("B");
  const auto box = canvas->boxOf("B");
  const auto scale = box.width() / 1920.0;
  const QPoint right(static_cast<int>(std::lround(box.right())), static_cast<int>(std::lround(box.center().y())));

  // in one step far beyond the numbers that a layout has (16 bits): nothing is done
  QTest::mousePress(canvas.get(), Qt::LeftButton, {}, right);
  const auto far = right + QPoint(static_cast<int>(std::lround(60000 * scale)), 0);
  QTest::mouseMove(canvas.get(), far);
  QTest::mouseRelease(canvas.get(), Qt::LeftButton, {}, far);
  QCOMPARE(resized.count(), 0);
  QCOMPARE(computerOf(*canvas, "B").monitors.front().w, 1920);

  // in steps: whatever is last done is a layout that can be
  drag(*canvas, right, right + QPoint(static_cast<int>(std::lround(100000 * scale)), 0));
  for (const auto &args : resized) {
    const deskflow::server::MonitorRect rect{
        args.at(1).toInt(), args.at(2).toInt(), args.at(3).toInt(), args.at(4).toInt()
    };
    QVERIFY2(static_cast<bool>(MonitorLayout::create({rect})), qPrintable(QStringLiteral("%1").arg(rect.w)));
  }
}

void ArrangementCanvasTests::resize_overAnotherComputer_isNotDone()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  canvas->setSelected("B");
  const auto box = canvas->boxOf("B");
  const auto scale = box.width() / 1920.0;
  const QPoint left(static_cast<int>(std::lround(box.left())), static_cast<int>(std::lround(box.center().y())));
  // in one step over A
  QTest::mousePress(canvas.get(), Qt::LeftButton, {}, left);
  QTest::mouseMove(canvas.get(), left - QPoint(static_cast<int>(std::lround(1600 * scale)), 0));
  QTest::mouseRelease(canvas.get(), Qt::LeftButton, {}, left - QPoint(static_cast<int>(std::lround(1600 * scale)), 0));
  QCOMPARE(resized.count(), 0);
  QCOMPARE(computerOf(*canvas, "B").originX, 2600.0);
  QCOMPARE(computerOf(*canvas, "B").monitors.front().w, 1920);
}

void ArrangementCanvasTests::outline_aClient_hasResizeCursorsWithAToleranceInScreenPixels()
{
  // the same on a canvas that shows everything small and one that shows it big: the tolerance is in screen pixels
  for (const auto &size : {QSize(900, 520), QSize(320, 200)}) {
    auto canvas = canvasOf(twoApart(), size);
    const auto box = canvas->boxOf("B");
    QVERIFY(canvas->selected().isEmpty());
    hover(canvas.get(), onBox(box, 1.0, 0.5));
    QCOMPARE(canvas->cursor().shape(), Qt::SizeHorCursor);
    QCOMPARE(canvas->hoveredComputer(), QStringLiteral("B"));
    hover(canvas.get(), onBox(box, 1.0, 0.5, 5, 0));
    QCOMPARE(canvas->cursor().shape(), Qt::SizeHorCursor);
    hover(canvas.get(), onBox(box, 1.0, 0.5, -5, 0));
    QCOMPARE(canvas->cursor().shape(), Qt::SizeHorCursor);
    hover(canvas.get(), onBox(box, 1.0, 0.5, 14, 0));
    QVERIFY(canvas->cursor().shape() != Qt::SizeHorCursor);
    QVERIFY(canvas->hoveredComputer().isEmpty());
    hover(canvas.get(), onBox(box, 1.0, 0.5, -14, 0));
    QCOMPARE(canvas->cursor().shape(), Qt::OpenHandCursor);
    hover(canvas.get(), onBox(box, 0.5, 1.0, 0, 4));
    QCOMPARE(canvas->cursor().shape(), Qt::SizeVerCursor);
    hover(canvas.get(), onBox(box, 1.0, 1.0, 3, 3));
    QCOMPARE(canvas->cursor().shape(), Qt::SizeFDiagCursor);
    hover(canvas.get(), onBox(box, 0.0, 1.0, -3, 3));
    QCOMPARE(canvas->cursor().shape(), Qt::SizeBDiagCursor);
    hover(canvas.get(), onBox(box, 0.5, 0.5));
    QCOMPARE(canvas->cursor().shape(), Qt::OpenHandCursor);
  }
}

void ArrangementCanvasTests::outline_pressSelectsTheComputer_andResizesIt()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  QVERIFY(canvas->selected().isEmpty());
  const auto box = canvas->boxOf("B");
  const auto scale = box.width() / 1920.0;
  drag(*canvas, onBox(box, 1.0, 0.5), onBox(box, 1.0, 0.5, 200 * scale, 0));
  QCOMPARE(canvas->selected(), QStringLiteral("B"));
  QCOMPARE(resized.count(), 1);
  QVERIFY(std::abs(resized.at(0).at(3).toInt() - 2120) <= 2.0 / scale);
}

void ArrangementCanvasTests::outline_isHighlightedUnderThePointer()
{
  auto canvas = canvasOf(twoApart());
  const auto plain = render(*canvas).toImage();
  const auto box = canvas->boxOf("B");
  hover(canvas.get(), onBox(box, 1.0, 0.5));
  const auto highlighted = render(*canvas).toImage();
  QVERIFY(highlighted != plain);
  // the pointer leaves: it is as it was
  hover(canvas.get(), QPoint(2, 2));
  QVERIFY(canvas->hoveredComputer().isEmpty());
  QCOMPARE(render(*canvas).toImage(), plain);
}

void ArrangementCanvasTests::outline_shiftKeepsTheAspect_altTakesTheSnappingAway()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  const auto box = canvas->boxOf("B");
  const auto scale = box.width() / 1920.0;

  // Shift: 400 units wider, and the height follows (a free drag leaves it)
  dragWith(canvas.get(), onBox(box, 1.0, 0.5), onBox(box, 1.0, 0.5, 400 * scale, 0), Qt::ShiftModifier);
  QCOMPARE(resized.count(), 1);
  const double w = resized.at(0).at(3).toInt();
  const double h = resized.at(0).at(4).toInt();
  QVERIFY(std::abs(w / h - 1920.0 / 1080.0) < 0.01);
  QVERIFY(w > 2250);

  // Alt: the left edge 25 units from the right edge of A is not snapped to it (it is, without Alt)
  auto other = canvasOf(twoApart());
  QSignalSpy resizedWithAlt(other.get(), &ArrangementCanvas::computerResized);
  const auto b = other->boxOf("B");
  const auto s = b.width() / 1920.0;
  dragWith(other.get(), onBox(b, 0.0, 0.5), onBox(b, 0.0, 0.5, -655 * s, 0), Qt::AltModifier);
  QCOMPARE(resizedWithAlt.count(), 1);
  QVERIFY(std::abs(computerOf(*other, "B").originX - 1945.0) <= 8.0);
  auto snapping = canvasOf(twoApart());
  const auto c = snapping->boxOf("B");
  drag(*snapping, onBox(c, 0.0, 0.5), onBox(c, 0.0, 0.5, -655 * s, 0));
  QCOMPARE(computerOf(*snapping, "B").originX, 1920.0);
}

void ArrangementCanvasTests::outline_escapeCancelsADrag()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  QSignalSpy changed(canvas.get(), &ArrangementCanvas::arrangementChanged);
  const auto box = canvas->boxOf("B");
  const auto scale = box.width() / 1920.0;
  const auto from = onBox(box, 1.0, 0.5);
  const auto to = onBox(box, 1.0, 0.5, 200 * scale, 0);
  dragWith(canvas.get(), from, to, Qt::NoModifier, false);
  QTest::keyClick(canvas.get(), Qt::Key_Escape);
  QTest::mouseRelease(canvas.get(), Qt::LeftButton, {}, to);
  QCOMPARE(resized.count(), 0);
  QCOMPARE(changed.count(), 0);
  QCOMPARE(computerOf(*canvas, "B").monitors.front().w, 1920);
}

void ArrangementCanvasTests::scale_aClientWithAReportedSize_isScaledAndNotResized()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  QSignalSpy scaled(canvas.get(), &ArrangementCanvas::computerScaled);
  canvas->setReportedSizes({{QStringLiteral("B"), QSize(1920, 1080)}});
  const auto box = canvas->boxOf("B");
  const auto scale = box.width() / 1920.0;
  const auto before = computerOf(*canvas, "B").monitors;

  // the corner goes 192 x 108 units: 10 % bigger on the canvas, in the same shape, and in the same pixels
  drag(*canvas, onBox(box, 1.0, 1.0), onBox(box, 1.0, 1.0, 192 * scale, 108 * scale));
  QCOMPARE(resized.count(), 0);
  QCOMPARE(scaled.count(), 1);
  QCOMPARE(scaled.at(0).at(0).toString(), QStringLiteral("B"));
  QVERIFY(std::abs(scaled.at(0).at(1).toDouble() - 1.1) < 0.01);
  QCOMPARE(computerOf(*canvas, "B").monitors, before);
  QCOMPARE(computerOf(*canvas, "B").zoom, scaled.at(0).at(1).toDouble());
  const auto drawn = computerOf(*canvas, "B").box();
  QVERIFY(std::abs(drawn.w / drawn.h - 1920.0 / 1080.0) < 1e-9);
  // the top left corner stays where it was
  QCOMPARE(drawn.x, 2600.0);
  QCOMPARE(drawn.y, 300.0);

  // near a display scale that there is (125 %: 0.8) it is exactly that, and not with Alt
  auto other = canvasOf(twoApart());
  QSignalSpy sized(other.get(), &ArrangementCanvas::computerScaled);
  other->setReportedSizes({{QStringLiteral("B"), QSize(1920, 1080)}});
  const auto b = other->boxOf("B");
  const auto s = b.width() / 1920.0;
  drag(*other, onBox(b, 1.0, 1.0), onBox(b, 1.0, 1.0, -378 * s, -212 * s));
  QCOMPARE(sized.count(), 1);
  QCOMPARE(sized.at(0).at(1).toDouble(), 0.8);
  auto free = canvasOf(twoApart());
  QSignalSpy unsnapped(free.get(), &ArrangementCanvas::computerScaled);
  free->setReportedSizes({{QStringLiteral("B"), QSize(1920, 1080)}});
  const auto f = free->boxOf("B");
  dragWith(free.get(), onBox(f, 1.0, 1.0), onBox(f, 1.0, 1.0, -378 * s, -212 * s), Qt::AltModifier);
  QCOMPARE(unsnapped.count(), 1);
  QVERIFY(unsnapped.at(0).at(1).toDouble() != 0.8);
  QVERIFY(std::abs(unsnapped.at(0).at(1).toDouble() - 0.8) < 0.01);
}

void ArrangementCanvasTests::scale_aComputerOfSeveralMonitors_isScaledAsAWhole()
{
  auto canvas = canvasOf(userArrangement());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  QSignalSpy scaled(canvas.get(), &ArrangementCanvas::computerScaled);
  const char *name = "9800X3D-5080";
  const auto before = computerOf(*canvas, name);
  const auto box = canvas->boxOf(name);
  const auto scale = box.width() / 4480.0;
  // the left edge of the whole computer goes 400 units to the right
  hover(canvas.get(), onBox(box, 0.0, 0.5));
  QCOMPARE(canvas->cursor().shape(), Qt::SizeHorCursor);
  drag(*canvas, onBox(box, 0.0, 0.5), onBox(box, 0.0, 0.5, 400 * scale, 0));
  QCOMPARE(resized.count(), 0);
  QCOMPARE(scaled.count(), 1);
  const auto &after = computerOf(*canvas, name);
  // the monitors are as they were, the zoom is smaller, and the shape is the same
  QCOMPARE(after.monitors, before.monitors);
  QVERIFY(after.zoom < before.zoom * 0.95);
  QVERIFY(std::abs(after.box().w / after.box().h - before.box().w / before.box().h) < 1e-9);
  // the right edge stays where it was
  QVERIFY(std::abs(after.box().right() - before.box().right()) < 1e-6);
}

void ArrangementCanvasTests::scale_neverOverAnotherComputer_andTheZoomStaysInItsLimits()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy scaled(canvas.get(), &ArrangementCanvas::computerScaled);
  canvas->setReportedSizes({{QStringLiteral("B"), QSize(1920, 1080)}});
  const auto box = canvas->boxOf("B");
  const auto scale = box.width() / 1920.0;
  // in one step over A
  QTest::mousePress(canvas.get(), Qt::LeftButton, {}, onBox(box, 0.0, 0.0));
  const auto over = onBox(box, 0.0, 0.0, -1900 * scale, -100 * scale);
  QTest::mouseMove(canvas.get(), over);
  QTest::mouseRelease(canvas.get(), Qt::LeftButton, {}, over);
  QCOMPARE(scaled.count(), 0);
  QCOMPARE(computerOf(*canvas, "B").zoom, 1.0);

  // a size that no zoom can make
  drag(*canvas, onBox(box, 1.0, 1.0), onBox(box, 1.0, 1.0, 200000 * scale, 200000 * scale));
  for (const auto &args : scaled) {
    QVERIFY(args.at(1).toDouble() >= deskflow::server::arrangement::kMinZoom);
    QVERIFY(args.at(1).toDouble() <= deskflow::server::arrangement::kMaxZoom);
  }
  auto tiny = canvasOf(twoApart());
  QSignalSpy shrunk(tiny.get(), &ArrangementCanvas::computerScaled);
  tiny->setReportedSizes({{QStringLiteral("B"), QSize(1920, 1080)}});
  const auto b = tiny->boxOf("B");
  drag(*tiny, onBox(b, 1.0, 1.0), onBox(b, 0.0, 0.0, 5, 5));
  for (const auto &args : shrunk)
    QVERIFY(args.at(1).toDouble() >= deskflow::server::arrangement::kMinZoom);
}

void ArrangementCanvasTests::drag_aBackgroundDrag_movesTheViewNotTheComputers()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy changed(canvas.get(), &ArrangementCanvas::arrangementChanged);
  const auto before = canvas->boxOf("A").topLeft();
  drag(*canvas, QPoint(5, 5), QPoint(45, 35));
  QCOMPARE(changed.count(), 0);
  QCOMPARE(canvas->boxOf("A").topLeft(), before + QPointF(40, 30));
  QCOMPARE(computerOf(*canvas, "A").originX, 0.0);
  canvas->zoomToFit();
  QCOMPARE(canvas->boxOf("A").topLeft(), before);
}

void ArrangementCanvasTests::keys_moveTheSelectedComputer()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy changed(canvas.get(), &ArrangementCanvas::arrangementChanged);
  canvas->setSelected("B");
  QTest::keyClick(canvas.get(), Qt::Key_Left);
  QCOMPARE(computerOf(*canvas, "B").originX, 2599.0);
  QTest::keyClick(canvas.get(), Qt::Key_Down, Qt::ShiftModifier);
  QCOMPARE(computerOf(*canvas, "B").originY, 310.0);
  QCOMPARE(changed.count(), 2);

  // a key does not put B on A
  canvas->setArrangement({{pc("A", kHd, 0, 0), pc("B", kHd, 1920, 0)}});
  canvas->setSelected("B");
  QTest::keyClick(canvas.get(), Qt::Key_Left);
  QVERIFY(canvas->issues().empty());
}

void ArrangementCanvasTests::doubleClick_asksToEditTheComputer()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy edit(canvas.get(), &ArrangementCanvas::editRequested);
  QTest::mouseDClick(canvas.get(), Qt::LeftButton, {}, canvas->boxOf("A").center().toPoint());
  QCOMPARE(edit.count(), 1);
  QCOMPARE(edit.first().first().toString(), QStringLiteral("A"));
}

void ArrangementCanvasTests::selection_followsTheClicks()
{
  auto canvas = canvasOf(twoApart());
  QSignalSpy selected(canvas.get(), &ArrangementCanvas::selectedChanged);
  QTest::mouseClick(canvas.get(), Qt::LeftButton, {}, canvas->boxOf("A").center().toPoint());
  QCOMPARE(canvas->selected(), QStringLiteral("A"));
  QTest::mouseClick(canvas.get(), Qt::LeftButton, {}, QPoint(4, 4));
  QCOMPARE(canvas->selected(), QString());
  QCOMPARE(selected.count(), 2);
}

void ArrangementCanvasTests::issues_areFoundAndTold()
{
  auto canvas = canvasOf({{pc("A", kHd, 0, 0), pc("B", kHd, 1000, 100), pc("C", kHd, 9000, 0)}});
  QStringList texts;
  for (const auto &issue : canvas->issues())
    texts << ArrangementCanvas::describe(issue);
  QVERIFY2(texts.contains(QStringLiteral("A and B overlap.")), qPrintable(texts.join('|')));
  QVERIFY(std::ranges::any_of(texts, [](const QString &t) {
    return t.startsWith(QStringLiteral("C does not touch"));
  }));
}

void ArrangementCanvasTests::render_showsTheComputers_andCanBeSaved()
{
  auto canvas = canvasOf(userArrangement());
  canvas->setStatus("9800X3D-5080", ArrangementCanvas::Status::Connected);
  canvas->setStatus("Subeoms-Mac-mini.local", ArrangementCanvas::Status::Connected);
  canvas->setStatus("SUBEOM-COPILOT", ArrangementCanvas::Status::Offline);
  canvas->setSelected("Subeoms-MacBook-Pro.local");
  const auto pixmap = render(*canvas);

  // every computer is drawn, in a colour of its own, and the links are on the picture
  const auto image = pixmap.toImage();
  const auto background = image.pixelColor(6, 6);
  QSet<QRgb> colours;
  for (const auto &computer : canvas->arrangement().computers) {
    const auto first = computer.monitorRects().front();
    const auto at = canvas->toWidget(first.x + first.w * 0.05, first.y + first.h * 0.5) * 2;
    const auto colour = image.pixelColor(at.toPoint());
    QVERIFY2(colour != background, computer.name.c_str());
    colours.insert(colour.rgb());
  }
  QVERIFY(colours.size() >= 4);
}

void ArrangementCanvasTests::screenshots_areMadeOfTheStatesTheUserSees()
{
  // the arrangement of the user's T-shaped screen, with every kind of computer around it
  auto canvas = canvasOf(userArrangement());
  canvas->setStatus("9800X3D-5080", ArrangementCanvas::Status::Connected);
  canvas->setStatus("Subeoms-Mac-mini.local", ArrangementCanvas::Status::Connected);
  canvas->setStatus("SUBEOM-COPILOT", ArrangementCanvas::Status::Offline);
  save(render(*canvas), QStringLiteral("1-canvas-user-t-shape.png"));

  // the classic grid of the user, converted: a row of four
  std::vector<GridCell> cells(4);
  cells[0] = {1, 1, pc("SUBEOM-COPILOT", kHd, 0, 0, 0.8)};
  cells[1] = {2, 1, pc("Subeoms-MacBook-Pro.local", kServerShape, 0, 0)};
  cells[2] = {3, 1, pc("9800X3D-5080", kWindowsShape, 0, 0)};
  cells[3] = {4, 1, pc("Subeoms-Mac-mini.local", kHd, 0, 0)};
  const auto converted = arrangementFromGrid(cells, "Subeoms-MacBook-Pro.local");
  auto fromGrid = canvasOf(converted.arrangement);
  save(render(*fromGrid), QStringLiteral("2-canvas-converted-from-the-classic-grid.png"));
  QVERIFY(converted.notKept.empty());

  // a computer on its way to the edge of another one: the guides, and the links that it would have
  auto dragging = canvasOf(userArrangement());
  dragging->setSelected("Subeoms-Mac-mini.local");
  const auto from = dragging->boxOf("Subeoms-Mac-mini.local").center().toPoint();
  const auto scale = dragging->boxOf("9800X3D-5080").width() / 4480.0;
  QTest::mousePress(dragging.get(), Qt::LeftButton, {}, from);
  const auto to = from + QPoint(qRound(-3600 * scale), qRound(1500 * scale));
  for (int i = 1; i <= 10; ++i)
    QTest::mouseMove(dragging.get(), from + (to - from) * i / 10);
  save(render(*dragging), QStringLiteral("3-canvas-dragging-with-guides.png"));
  QTest::mouseRelease(dragging.get(), Qt::LeftButton, {}, to);

  // the pointer on the outline of a computer that was not selected: it is highlighted, with its handles
  auto hovering = canvasOf(userArrangement());
  const auto miniBox = hovering->boxOf("Subeoms-Mac-mini.local");
  hover(hovering.get(), onBox(miniBox, 1.0, 0.5, 2, 0));
  save(render(*hovering), QStringLiteral("14-canvas-hover-highlights-the-outline.png"));
  QCOMPARE(hovering->hoveredComputer(), QStringLiteral("Subeoms-Mac-mini.local"));

  // a computer of one rectangle, size not known: its corner is dragged, with the size, and the guide where it lines up
  auto resizing = canvasOf(userArrangement());
  const auto corner = onBox(miniBox, 1.0, 1.0);
  QTest::mousePress(resizing.get(), Qt::LeftButton, {}, corner);
  const auto grown = corner + QPoint(qRound(640 * scale), qRound(360 * scale));
  for (int i = 1; i <= 8; ++i)
    QTest::mouseMove(resizing.get(), corner + (grown - corner) * i / 8);
  save(render(*resizing), QStringLiteral("15-canvas-resizing-a-computer-with-size-and-guide.png"));
  QTest::mouseRelease(resizing.get(), Qt::LeftButton, {}, grown);

  // a computer of two monitors: its outline is dragged and the whole of it is scaled, in its own shape
  auto scaling = canvasOf(userArrangement());
  const auto windowsBox = scaling->boxOf("9800X3D-5080");
  const auto edge = onBox(windowsBox, 0.0, 0.5);
  QTest::mousePress(scaling.get(), Qt::LeftButton, {}, edge);
  const auto shrunk = edge + QPoint(qRound(480 * scale), 0);
  for (int i = 1; i <= 8; ++i)
    QTest::mouseMove(scaling.get(), edge + (shrunk - edge) * i / 8);
  save(render(*scaling), QStringLiteral("16-canvas-scaling-a-computer-of-two-monitors.png"));
  QTest::mouseRelease(scaling.get(), Qt::LeftButton, {}, shrunk);

  // the canvas as it is when it is done
  save(render(*scaling), QStringLiteral("17-canvas-final.png"));

  // what is wrong: an overlap, and a computer that nothing touches
  auto wrong = canvasOf({{
      pc("Subeoms-MacBook-Pro.local", kServerShape, 0, 0, 1.0, true),
      pc("9800X3D-5080", kWindowsShape, -2000, 0),
      pc("Subeoms-Mac-mini.local", kHd, 4200, 500),
      pc("SUBEOM-COPILOT", kHd, -783, 1440, 0.8),
  }});
  save(render(*wrong), QStringLiteral("4-canvas-overlap-and-no-link.png"));
  QVERIFY(!wrong->issues().empty());
}

QTEST_MAIN(ArrangementCanvasTests)
