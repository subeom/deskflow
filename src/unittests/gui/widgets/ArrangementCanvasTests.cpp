/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ArrangementCanvasTests.h"

#include "gui/widgets/ArrangementCanvas.h"

#include <QDir>
#include <QImage>
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
