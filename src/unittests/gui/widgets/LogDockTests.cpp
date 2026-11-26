/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LogDockTests.h"

#include "gui/style/DeskflowStyle.h"
#include "gui/style/PanelWidget.h"
#include "gui/widgets/LogDock.h"
#include "gui/widgets/SearchWidget.h"

#include <QAccessible>
#include <QApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

using namespace deskflow::gui::style;

namespace {

DeskflowStyle *installDeskflowStyle()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
  return style;
}

QPushButton *buttonWithToolTip(const QWidget &parent, const QString &toolTip)
{
  for (auto *button : parent.findChildren<QPushButton *>())
    if (button->toolTip() == toolTip)
      return button;
  return nullptr;
}

bool near(const QColor &a, const QColor &b, int tolerance = 6)
{
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

} // namespace

void LogDockTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
}

void LogDockTests::systemLook_isTheDockItHasAlwaysBeen()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  LogDock dock;
  // the title bar has the title, the search and the two buttons; there is no card
  auto *title = dock.titleBarWidget();
  QVERIFY(title);
  QVERIFY(title->findChild<QLabel *>());
  QCOMPARE(title->findChild<QLabel *>()->text(), QStringLiteral("Log"));
  QVERIFY(title->findChild<SearchWidget *>());
  QVERIFY(buttonWithToolTip(*title, QStringLiteral("Close Log")));
  QVERIFY(buttonWithToolTip(*title, QStringLiteral("Detach from window")));
  QVERIFY(dock.findChildren<PanelWidget *>().isEmpty());
  // the lines are in a text with the frame that it has always had, and the buttons have no names but their tool tips
  auto *edit = dock.findChild<QPlainTextEdit *>();
  QVERIFY(edit);
  QVERIFY(edit->frameShape() != QFrame::NoFrame);
  QVERIFY(buttonWithToolTip(*title, QStringLiteral("Close Log"))->accessibleName().isEmpty());
  QVERIFY(buttonWithToolTip(*title, QStringLiteral("Close Log"))->icon().name() != QStringLiteral("x"));
}

void LogDockTests::deskflowLook_isACardWithItsHeaderAndItsLines()
{
  installDeskflowStyle();
  LogDock dock;
  // the title bar of the dock is empty: the header is in the card
  auto *title = dock.titleBarWidget();
  QVERIFY(title);
  QVERIFY(title->findChildren<QLabel *>().isEmpty());
  QVERIFY(title->findChildren<QPushButton *>().isEmpty());
  QCOMPARE(title->maximumHeight(), 0);

  const auto panels = dock.findChildren<PanelWidget *>();
  QCOMPARE(panels.size(), 1);
  auto *panel = panels.first();
  QCOMPARE(panel->density(), PanelWidget::Density::Dense);
  QVERIFY(!panel->isElevated());
  // the title, the search, the buttons and the lines are in the card
  const auto inside = [panel](const QWidget *w) { return w && panel->isAncestorOf(w); };
  QVERIFY(inside(dock.findChild<SearchWidget *>()));
  QVERIFY(inside(buttonWithToolTip(dock, QStringLiteral("Close Log"))));
  QVERIFY(inside(buttonWithToolTip(dock, QStringLiteral("Detach from window"))));
  auto *edit = dock.findChild<QPlainTextEdit *>();
  QVERIFY(inside(edit));
  // the lines have no frame and no ground of their own: they are on the card
  QCOMPARE(edit->frameShape(), QFrame::NoFrame);
  QCOMPARE(edit->palette().color(QPalette::Base).alpha(), 0);
  QVERIFY(!edit->viewport()->autoFillBackground());
  QVERIFY(edit->isReadOnly());
  QCOMPARE(edit->accessibleName(), QStringLiteral("Log"));
}

void LogDockTests::deskflowLook_theButtonsHaveTheIconsAndTheNamesOfTheLook()
{
  installDeskflowStyle();
  LogDock dock;
  auto *close = buttonWithToolTip(dock, QStringLiteral("Close Log"));
  auto *detach = buttonWithToolTip(dock, QStringLiteral("Detach from window"));
  QVERIFY(close && detach);
  // pictures of the set of the new look; a button with only a picture is named for a screen reader
  QCOMPARE(close->icon().name(), QStringLiteral("x"));
  QCOMPARE(detach->icon().name(), QStringLiteral("arrow-square-out"));
  QCOMPARE(close->accessibleName(), QStringLiteral("Close Log"));
  QCOMPARE(detach->accessibleName(), QStringLiteral("Detach from window"));
  // buttons that are easy to hit: more than 24 px (WCAG 2.2)
  QVERIFY(close->width() >= 28 && close->height() >= 28);
  QVERIFY(detach->width() >= 28 && detach->height() >= 28);

  auto *search = buttonWithToolTip(dock, QStringLiteral("Search"));
  auto *next = buttonWithToolTip(dock, QStringLiteral("Find next"));
  auto *previous = buttonWithToolTip(dock, QStringLiteral("Find previous"));
  QVERIFY(search && next && previous);
  QCOMPARE(search->icon().name(), QStringLiteral("magnifying-glass"));
  QCOMPARE(next->icon().name(), QStringLiteral("caret-down"));
  QCOMPARE(previous->icon().name(), QStringLiteral("caret-up"));
  QCOMPARE(search->accessibleName(), QStringLiteral("Search"));
  QCOMPARE(next->accessibleName(), QStringLiteral("Find next"));
  QCOMPARE(previous->accessibleName(), QStringLiteral("Find previous"));

  // a screen reader is told these names for the buttons
  QAccessible::setActive(true);
  auto *interface = QAccessible::queryAccessibleInterface(close);
  QVERIFY(interface);
  QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Close Log"));
  QCOMPARE(interface->role(), QAccessible::Button);
  // the icon of the two arrows is drawn: it is a picture, not nothing
  QVERIFY(!close->icon().pixmap(QSize(18, 18)).isNull());
  QVERIFY(!previous->icon().pixmap(QSize(18, 18)).isNull());
}

void LogDockTests::deskflowLook_detachingChangesTheButtonAndTheHeader()
{
  installDeskflowStyle();
  LogDock dock;
  auto *detach = buttonWithToolTip(dock, QStringLiteral("Detach from window"));
  auto *close = buttonWithToolTip(dock, QStringLiteral("Close Log"));
  auto *label = dock.findChild<QLabel *>();
  QVERIFY(detach && close && label);

  detach->setChecked(true);
  // the dock is a window now: it has the title bar of the OS, so the card has no title and no close button
  QCOMPARE(detach->toolTip(), QStringLiteral("Attach to window"));
  QCOMPARE(detach->accessibleName(), QStringLiteral("Attach to window"));
  QCOMPARE(detach->icon().name(), QStringLiteral("arrow-u-up-left"));
  QVERIFY(!label->isVisibleTo(&dock));
  QVERIFY(!close->isVisibleTo(&dock));

  detach->setChecked(false);
  QCOMPARE(detach->toolTip(), QStringLiteral("Detach from window"));
  QCOMPARE(detach->accessibleName(), QStringLiteral("Detach from window"));
  QCOMPARE(detach->icon().name(), QStringLiteral("arrow-square-out"));
  QVERIFY(!label->isHidden());
  QVERIFY(!close->isHidden());
}

void LogDockTests::deskflowLook_theCardIsPaintedAsASurface()
{
  auto *style = installDeskflowStyle();
  const auto &c = style->tokens().colors;
  LogDock dock;
  dock.resize(500, 200);
  dock.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dock));
  QCoreApplication::processEvents();
  const auto picture = dock.grab().toImage();
  const auto *panel = dock.findChild<PanelWidget *>();
  const auto top = panel->mapTo(&dock, QPoint(0, 0));
  // inside the card, below the header and away from its text: the colour of the surface (opaque: less transparency is on)
  const auto sample = picture.pixelColor(top.x() + panel->width() - 40, top.y() + panel->height() - 12);
  QVERIFY2(near(sample, c.surface, 10), qPrintable(sample.name() + QStringLiteral(" vs ") + c.surface.name()));
}

void LogDockTests::lines_areAppended_inBothLooks()
{
  for (const bool deskflow : {false, true}) {
    if (deskflow)
      installDeskflowStyle();
    else
      QApplication::setStyle(QStringLiteral("Fusion"));
    LogDock dock;
    dock.appendLine(QStringLiteral("[2026-11-17T10:00:00] NOTE: hello"));
    dock.appendLine(QStringLiteral("[2026-11-17T10:00:01] ERROR: not good"));
    auto *edit = dock.findChild<QPlainTextEdit *>();
    QVERIFY(edit->toPlainText().contains(QStringLiteral("NOTE: hello")));
    QVERIFY(edit->toPlainText().contains(QStringLiteral("ERROR: not good")));
  }
}

QTEST_MAIN(LogDockTests)
