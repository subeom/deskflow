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

void ArrangementCanvasTests::resize_anEdgeSnapsToTheNeighbour_theRatiosAndTheReportedSize()
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

  // the corner, near 2560 x 1440 which is what the computer reports: that size, which is also a ratio
  auto other = canvasOf(twoApart());
  QSignalSpy sized(other.get(), &ArrangementCanvas::computerResized);
  other->setReportedSizes({{QStringLiteral("B"), QSize(2560, 1440)}});
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

void ArrangementCanvasTests::resize_theServerAndAComputerOfSeveralMonitors_arePutAndNotResized()
{
  auto canvas = canvasOf(userArrangement());
  QSignalSpy resized(canvas.get(), &ArrangementCanvas::computerResized);
  for (const char *name : {"Subeoms-MacBook-Pro.local", "9800X3D-5080"}) {
    canvas->setSelected(name);
    const auto box = canvas->boxOf(name);
    const QPoint edge(
        static_cast<int>(std::lround(box.right())), static_cast<int>(std::lround(box.top() + box.height() / 4))
    );
    hover(canvas.get(), edge);
    QVERIFY(canvas->cursor().shape() != Qt::SizeHorCursor);
  }
  QCOMPARE(resized.count(), 0);
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

  // a computer of one rectangle, selected: the handles on its edges, and one of them dragged
  auto resizing = canvasOf(userArrangement());
  resizing->setSelected("Subeoms-Mac-mini.local");
  const auto miniBox = resizing->boxOf("Subeoms-Mac-mini.local");
  const QPoint corner(qRound(miniBox.right()), qRound(miniBox.bottom()));
  save(render(*resizing), QStringLiteral("12-canvas-single-rectangle-resize-handles.png"));
  QTest::mousePress(resizing.get(), Qt::LeftButton, {}, corner);
  const auto grown = corner + QPoint(qRound(640 * scale), qRound(360 * scale));
  for (int i = 1; i <= 8; ++i)
    QTest::mouseMove(resizing.get(), corner + (grown - corner) * i / 8);
  save(render(*resizing), QStringLiteral("13-canvas-resizing-a-computer.png"));
  QTest::mouseRelease(resizing.get(), Qt::LeftButton, {}, grown);

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
