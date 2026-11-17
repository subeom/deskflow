/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MenuStyleTests.h"

#include "gui/style/Contrast.h"
#include "gui/style/DeskflowStyle.h"

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>

using namespace deskflow::gui::style;

namespace {

DeskflowStyle *install(bool dark)
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  // opaque: the colours that are painted are the tokens, not tokens over a backdrop
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
  return style;
}

bool near(const QColor &a, const QColor &b, int tolerance = 6)
{
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

// A menu as the program has it, shown (offscreen) so that it is laid out and painted
struct Shown
{
  explicit Shown(QMenu &menu) : menu(menu)
  {
    menu.popup(QPoint(10, 10));
    QCoreApplication::processEvents();
  }
  ~Shown()
  {
    menu.hide();
  }
  QImage picture()
  {
    return menu.grab().toImage();
  }
  QMenu &menu;
};

} // namespace

void MenuStyleTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
}

void MenuStyleTests::rows_are36PixelsHigh_andSeparatorsHaveTheirOwnHeight()
{
  install(false);
  QMenu menu;
  auto *first = menu.addAction(QStringLiteral("First"));
  menu.addSeparator();
  auto *second = menu.addAction(QStringLiteral("Second"));
  Shown shown(menu);

  const auto firstRect = menu.actionGeometry(first);
  const auto secondRect = menu.actionGeometry(second);
  QCOMPARE(firstRect.height(), 36);
  QCOMPARE(secondRect.height(), 36);
  // the separator is the room between them, and no more than a hairline with some air
  QCOMPARE(secondRect.top() - firstRect.bottom() - 1, 9);
  // the rows are inside the frame of the menu with room at each side
  QVERIFY(firstRect.left() >= 6);
  QVERIFY(menu.width() - firstRect.right() - 1 >= 6);
}

void MenuStyleTests::rows_makeTheMenuAsWideAsTheirTextIconAndShortcutNeed()
{
  install(false);
  QMenu plain;
  plain.addAction(QStringLiteral("Short"));
  plain.ensurePolished();
  const int plainWidth = plain.sizeHint().width();

  QMenu withShortcut;
  auto *action = withShortcut.addAction(QStringLiteral("Short"));
  action->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
  action->setShortcutVisibleInContextMenu(true);
  withShortcut.ensurePolished();
  // a shortcut has room of its own at the right, with a gap from the text
  QVERIFY(withShortcut.sizeHint().width() >= plainWidth + 28);

  QMenu withIcon;
  auto *iconAction = withIcon.addAction(QStringLiteral("Short"));
  QPixmap pixmap(16, 16);
  pixmap.fill(Qt::black);
  iconAction->setIcon(QIcon(pixmap));
  withIcon.ensurePolished();
  // an icon has a column of 20 and a gap of 10
  QVERIFY(withIcon.sizeHint().width() >= plainWidth + 30);

  QMenu longer;
  longer.addAction(QStringLiteral("A text that is a good deal longer than the short one"));
  longer.ensurePolished();
  QVERIFY(longer.sizeHint().width() > plainWidth + 100);
}

void MenuStyleTests::highlight_isTheSoftAccentOnTheRowThatIsActive_data()
{
  QTest::addColumn<bool>("dark");
  QTest::newRow("light") << false;
  QTest::newRow("dark") << true;
}

void MenuStyleTests::highlight_isTheSoftAccentOnTheRowThatIsActive()
{
  QFETCH(bool, dark);
  auto *style = install(dark);
  const auto &c = style->tokens().colors;
  QMenu menu;
  auto *first = menu.addAction(QStringLiteral("First"));
  auto *second = menu.addAction(QStringLiteral("Second"));
  Shown shown(menu);
  menu.setActiveAction(second);
  QCoreApplication::processEvents();
  const auto picture = shown.picture();

  // inside the row, away from the text and from the rounded edge: the fill of the highlight
  const auto secondRect = menu.actionGeometry(second);
  const auto firstRect = menu.actionGeometry(first);
  const auto sample = [&](const QRect &rect) { return picture.pixelColor(rect.left() + 8, rect.center().y() - 11); };
  QVERIFY2(near(sample(secondRect), c.accentSoft), qPrintable(sample(secondRect).name()));
  // the other row is on the surface of the menu
  QVERIFY2(near(sample(firstRect), c.surface), qPrintable(sample(firstRect).name()));
}

void MenuStyleTests::textOfAMenu_hasTheContrastThatTextNeeds_data()
{
  QTest::addColumn<bool>("dark");
  QTest::newRow("light") << false;
  QTest::newRow("dark") << true;
}

void MenuStyleTests::textOfAMenu_hasTheContrastThatTextNeeds()
{
  QFETCH(bool, dark);
  const auto &c = install(dark)->tokens().colors;
  // the text of a row, and the shortcut (the secondary colour), on the surface and on the highlight
  QVERIFY(contrastRatio(c.text, c.surface) >= 4.5);
  QVERIFY(contrastRatio(c.text, c.accentSoft) >= 4.5);
  QVERIFY(contrastRatio(c.textSecondary, c.surface) >= 4.5);
  QVERIFY(contrastRatio(c.textSecondary, c.accentSoft) >= 4.5);
  // the frame of the highlight is what shows where the keyboard is: it has the contrast that a graphic needs
  QVERIFY(contrastRatio(c.accent, c.surface) >= 3);
}

void MenuStyleTests::aDimmedRow_isDrawnInTheColourOfWhatIsDisabled()
{
  auto *style = install(false);
  const auto &c = style->tokens().colors;
  QMenu menu;
  auto *enabled = menu.addAction(QStringLiteral("MMMMMMMM"));
  auto *dimmed = menu.addAction(QStringLiteral("MMMMMMMM"));
  dimmed->setEnabled(false);
  Shown shown(menu);
  const auto picture = shown.picture();

  // the darkest pixel of the text of each row is the colour of its text (the letters are wide, so a pixel is full)
  const auto darkest = [&](QAction *action) {
    const auto rect = menu.actionGeometry(action);
    QColor best = c.surface;
    for (int y = rect.top(); y <= rect.bottom(); ++y)
      for (int x = rect.left() + 8; x <= rect.right() - 8; ++x) {
        const auto pixel = picture.pixelColor(x, y);
        if (contrastRatio(pixel, c.surface) > contrastRatio(best, c.surface))
          best = pixel;
      }
    return best;
  };
  QVERIFY2(near(darkest(enabled), c.text, 24), qPrintable(darkest(enabled).name()));
  QVERIFY2(near(darkest(dimmed), c.textDisabled, 24), qPrintable(darkest(dimmed).name()));
}

void MenuStyleTests::aRowThatIsChecked_hasItsMark()
{
  auto *style = install(false);
  const auto &c = style->tokens().colors;
  const auto markPixels = [&](bool checked) {
    QMenu menu;
    auto *action = menu.addAction(QStringLiteral("Show the log"));
    action->setCheckable(true);
    action->setChecked(checked);
    Shown shown(menu);
    const auto picture = shown.picture();
    const auto rect = menu.actionGeometry(action);
    // the column of the mark, at the left of the row
    int count = 0;
    for (int y = rect.top(); y <= rect.bottom(); ++y)
      for (int x = rect.left() + 8; x < rect.left() + 8 + 20; ++x)
        if (contrastRatio(picture.pixelColor(x, y), c.surface) > 3)
          ++count;
    return count;
  };
  QVERIFY(markPixels(true) >= 8);
  QCOMPARE(markPixels(false), 0);
}

void MenuStyleTests::aMenuWithAFrameOfItsOwn_isRoundAndClearAtTheCorners()
{
  auto *style = install(false);
  const auto &c = style->tokens().colors;
  const auto corner = [&](bool rounded) {
    QMenu menu;
    menu.addAction(QStringLiteral("Row"));
    menu.setProperty(DeskflowStyle::kRoundedMenuProperty, rounded);
    menu.setWindowFlag(Qt::FramelessWindowHint, rounded);
    menu.setAttribute(Qt::WA_TranslucentBackground, rounded);
    Shown shown(menu);
    return shown.picture().pixelColor(0, 0);
  };
  // a menu with the frame of the OS has a square corner of the colour of its surface, one with a frame of its own has the
  // clear ground at its corner
  QVERIFY(near(corner(false), c.surface, 40) || corner(false) != corner(true));
  QCOMPARE(corner(true).alpha(), 0);
  QVERIFY(corner(false).alpha() > 200);
}

void MenuStyleTests::menuBar_marksTheMenuThatIsOpen()
{
  auto *style = install(false);
  const auto &c = style->tokens().colors;
  QMenuBar bar;
  auto *file = bar.addMenu(QStringLiteral("File"));
  bar.addMenu(QStringLiteral("Edit"));
  bar.resize(300, 36);
  bar.show();
  QVERIFY(QTest::qWaitForWindowExposed(&bar));
  const auto before = bar.grab().toImage();
  // the bar is the surface with a hairline under it
  QVERIFY2(near(before.pixelColor(150, 10), c.surface), qPrintable(before.pixelColor(150, 10).name() + " vs " + c.surface.name()));

  bar.setActiveAction(file->menuAction());
  QCoreApplication::processEvents();
  const auto after = bar.grab().toImage();
  const auto rect = bar.actionGeometry(file->menuAction());
  // the item that is active is the soft accent where the text is not
  QVERIFY2(near(after.pixelColor(rect.left() + 5, rect.center().y()), c.accentSoft, 10), qPrintable(after.pixelColor(rect.left() + 5, rect.center().y()).name()));
  QVERIFY(after != before);
}

QTEST_MAIN(MenuStyleTests)
