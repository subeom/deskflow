/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ShellWidgetsTests.h"

#include "gui/style/Appearance.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/PanelWidget.h"
#include "gui/widgets/NavRail.h"
#include "gui/widgets/SegmentedControl.h"
#include "gui/widgets/SidebarShell.h"

#include <QAccessible>
#include <QApplication>
#include <QLabel>
#include <QPixmapCache>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTranslator>

using namespace deskflow::gui::style;

namespace {

DeskflowStyle *install(bool reduceMotion = true)
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.reduceTransparency = true;
  look.reduceMotion = reduceMotion;
  style->useLook(look);
  return style;
}

bool near(const QColor &a, const QColor &b, int tolerance = 3)
{
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

// the sidebar of the main window in small: two pages, two actions, a status line at its foot
void fill(NavRail &rail)
{
  rail.addPage(QStringLiteral("home"), QStringLiteral("desktop"), QStringLiteral("Computers"));
  rail.addPage(QStringLiteral("other"), QStringLiteral("gear"), QStringLiteral("Other"));
  rail.addSection(QStringLiteral("Settings"));
  rail.addAction(QStringLiteral("prefs"), QStringLiteral("sliders-horizontal"), QStringLiteral("Preferences"));
  rail.addAction(QStringLiteral("log"), QStringLiteral("terminal-window"), QStringLiteral("Log"))->setCheckable(true);
  rail.addStatus(QStringLiteral("tls"), QStringLiteral("shield-check"), QStringLiteral("Encrypted (TLS)"));
  rail.addAction(
      QStringLiteral("print"), QStringLiteral("fingerprint"), QStringLiteral("View local fingerprint"),
      NavRail::Region::Bottom
  );
}

QAccessibleInterface *a11y(QObject *object)
{
  auto *interface = QAccessible::queryAccessibleInterface(object);
  Q_ASSERT(interface);
  return interface;
}

void showAndActivate(QWidget &widget)
{
  widget.show();
  QVERIFY(QTest::qWaitForWindowExposed(&widget));
  widget.activateWindow();
  QTest::qWait(10);
}

} // namespace

void ShellWidgetsTests::initTestCase()
{
  QAccessible::setActive(true);
}

void ShellWidgetsTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QPixmapCache::clear();
}

void ShellWidgetsTests::rail_hasTwoWidths_andTheIconsStayWhereTheyAreWhenItFolds()
{
  install();
  NavRail rail;
  fill(rail);
  rail.resize(rail.width(), 500);
  QCOMPARE(NavRail::expandedPanelWidth(), 236);
  QCOMPARE(NavRail::collapsedPanelWidth(), 76);
  QCOMPARE(rail.panelWidth(), 236);
  QCOMPARE(rail.width(), NavRail::widgetWidthFor(236));
  const auto home = rail.item(QStringLiteral("home"));
  const auto xUnfolded = home->x();
  const auto toggle = rail.toggleButton()->x();

  rail.setExpanded(false, false);
  QVERIFY(!rail.isExpanded());
  QCOMPARE(rail.panelWidth(), 76);
  QCOMPARE(rail.width(), NavRail::widgetWidthFor(76));
  // the icons are in the first square of an entry, and the entry starts where it did
  QCOMPARE(home->x(), xUnfolded);
  QCOMPARE(rail.toggleButton()->x(), toggle);
  // the entry of a folded rail is a circle of 44 px with the room of the ring around it
  QCOMPARE(home->width(), 44 + 2 * NavItem::kRing);

  rail.setExpanded(true, false);
  QCOMPARE(rail.panelWidth(), 236);
  QCOMPARE(home->x(), xUnfolded);
}

void ShellWidgetsTests::rail_foldsAtOnceWhenItIsNotShownOrMotionIsReduced_andMovesOtherwise()
{
  install(false);
  NavRail rail;
  fill(rail);
  QSignalSpy widths(&rail, &NavRail::panelWidthChanged);

  // not shown: nothing to move
  rail.setExpanded(false);
  QCOMPARE(rail.panelWidth(), 76);

  rail.setExpanded(true, false);
  rail.resize(rail.width(), 500);
  rail.show();
  QVERIFY(QTest::qWaitForWindowExposed(&rail));
  widths.clear();
  rail.setExpanded(false);
  // it is on its way: not at the end yet, and told of each width
  QVERIFY(rail.panelWidth() > 76);
  QTRY_COMPARE_WITH_TIMEOUT(rail.panelWidth(), 76, 3000);
  QVERIFY(widths.size() > 2);
  QCOMPARE(widths.last().first().toInt(), 76);

  // motion reduced: at once
  auto *style = qobject_cast<DeskflowStyle *>(QApplication::style());
  Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
  rail.setExpanded(true);
  QCOMPARE(rail.panelWidth(), 236);
}

void ShellWidgetsTests::rail_theFirstPageIsShown_aPressSelectsAPage_andAnActionDoesNot()
{
  install();
  NavRail rail;
  fill(rail);
  rail.resize(rail.width(), 500);
  QCOMPARE(rail.currentPage(), QStringLiteral("home"));
  QVERIFY(rail.item(QStringLiteral("home"))->isCurrent());
  QSignalSpy activated(&rail, &NavRail::activated);
  QSignalSpy selected(&rail, &NavRail::pageSelected);

  rail.item(QStringLiteral("other"))->click();
  QCOMPARE(rail.currentPage(), QStringLiteral("other"));
  QVERIFY(!rail.item(QStringLiteral("home"))->isCurrent());
  QCOMPARE(selected.size(), 1);
  QCOMPARE(selected.last().first().toString(), QStringLiteral("other"));

  rail.item(QStringLiteral("prefs"))->click();
  // an action opens something: the page that is shown is the same
  QCOMPARE(rail.currentPage(), QStringLiteral("other"));
  QCOMPARE(selected.size(), 1);
  QCOMPARE(activated.size(), 2);
  QCOMPARE(activated.last().first().toString(), QStringLiteral("prefs"));

  // the entry that is checkable keeps its state
  rail.item(QStringLiteral("log"))->click();
  QVERIFY(rail.item(QStringLiteral("log"))->isChecked());
}

void ShellWidgetsTests::rail_theLabelIsTheNameAndTheTooltipOfTheFoldedRail()
{
  install();
  NavRail rail;
  fill(rail);
  auto *prefs = rail.item(QStringLiteral("prefs"));
  QVERIFY(prefs->toolTip().isEmpty());
  QCOMPARE(a11y(prefs)->text(QAccessible::Name), QStringLiteral("Preferences"));

  rail.setExpanded(false, false);
  QCOMPARE(prefs->toolTip(), QStringLiteral("Preferences"));
  // the name does not depend on what is drawn
  QCOMPARE(a11y(prefs)->text(QAccessible::Name), QStringLiteral("Preferences"));

  rail.setItemText(QStringLiteral("prefs"), QStringLiteral("Preferences..."));
  QCOMPARE(prefs->toolTip(), QStringLiteral("Preferences..."));
  QCOMPARE(a11y(prefs)->text(QAccessible::Name), QStringLiteral("Preferences..."));
  rail.setExpanded(true, false);
  QVERIFY(prefs->toolTip().isEmpty());
}

void ShellWidgetsTests::rail_isOneStopForTheTabKey_andTheArrowKeysMoveInIt()
{
  install();
  QWidget window;
  auto *rail = new NavRail(&window);
  fill(*rail);
  window.resize(rail->width(), 520);
  rail->resize(rail->width(), 520);
  showAndActivate(window);

  int stops = 0;
  for (auto *item : rail->items()) {
    if (item->focusPolicy() & Qt::TabFocus)
      ++stops;
  }
  QCOMPARE(stops, 1);
  // the stop is the page that is shown, and the button of the rail is a stop of its own
  QVERIFY(rail->item(QStringLiteral("home"))->focusPolicy() & Qt::TabFocus);
  QVERIFY(rail->toggleButton()->focusPolicy() & Qt::TabFocus);

  rail->item(QStringLiteral("home"))->setFocus(Qt::TabFocusReason);
  QTRY_COMPARE(QApplication::focusWidget(), rail->item(QStringLiteral("home")));
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
  QCOMPARE(QApplication::focusWidget(), rail->item(QStringLiteral("other")));
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
  QCOMPARE(QApplication::focusWidget(), rail->item(QStringLiteral("prefs")));
  // the stop moves with the focus, so that coming back with Tab lands where it was
  QVERIFY(rail->item(QStringLiteral("prefs"))->focusPolicy() & Qt::TabFocus);
  QVERIFY(!(rail->item(QStringLiteral("home"))->focusPolicy() & Qt::TabFocus));

  QTest::keyClick(QApplication::focusWidget(), Qt::Key_End);
  QCOMPARE(QApplication::focusWidget(), rail->item(QStringLiteral("print")));
  // the status line has no focus: the last stop is the action at the foot
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
  QCOMPARE(QApplication::focusWidget(), rail->item(QStringLiteral("print")));
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Home);
  QCOMPARE(QApplication::focusWidget(), rail->item(QStringLiteral("home")));
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Up);
  QCOMPARE(QApplication::focusWidget(), rail->item(QStringLiteral("home")));

  // Enter and Space press the entry
  QSignalSpy activated(rail, &NavRail::activated);
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
  QCOMPARE(activated.size(), 1);
  QCOMPARE(activated.last().first().toString(), QStringLiteral("other"));
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Space);
  QCOMPARE(activated.size(), 2);
}

void ShellWidgetsTests::rail_aHiddenEntryTakesNoRoomAndNoFocus_aStatusLineHasNoFocus()
{
  install();
  QWidget window;
  auto *rail = new NavRail(&window);
  fill(*rail);
  rail->resize(rail->width(), 520);
  showAndActivate(window);

  auto *prefs = rail->item(QStringLiteral("prefs"));
  auto *log = rail->item(QStringLiteral("log"));
  const auto logY = log->y();
  rail->setItemVisible(QStringLiteral("prefs"), false);
  QVERIFY(prefs->isHidden());
  // the entry after it moved up into its place
  QVERIFY(log->y() < logY);

  rail->item(QStringLiteral("other"))->setFocus(Qt::TabFocusReason);
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
  QCOMPARE(QApplication::focusWidget(), log);

  rail->setItemVisible(QStringLiteral("prefs"), true);
  QVERIFY(!prefs->isHidden());
  QCOMPARE(rail->item(QStringLiteral("tls"))->focusPolicy(), Qt::NoFocus);

  // the stop is never lost: when the entry that held it goes, another one holds it
  rail->item(QStringLiteral("print"))->setFocus(Qt::TabFocusReason);
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_End);
  rail->setItemVisible(QStringLiteral("print"), false);
  int stops = 0;
  for (auto *item : rail->items()) {
    if ((item->focusPolicy() & Qt::TabFocus) && !item->isHidden())
      ++stops;
  }
  QCOMPARE(stops, 1);
}

void ShellWidgetsTests::rail_paintsThePageThatIsShownInTheAccent_andTheIconsWhenFolded()
{
  install();
  const auto &c = currentTokens().colors;
  QWidget window;
  auto *rail = new NavRail(&window);
  fill(*rail);
  window.resize(rail->width(), 520);
  rail->resize(rail->width(), 520);

  auto *home = rail->item(QStringLiteral("home"));
  auto *other = rail->item(QStringLiteral("other"));
  const auto shown = home->grab().toImage();
  const auto notShown = other->grab().toImage();
  // the capsule of the page that is shown: the accent, at the right of the icon where there is no text
  const QPoint inside(shown.width() - 40, shown.height() / 2);
  QVERIFY2(near(shown.pixelColor(inside), c.accent), qPrintable(shown.pixelColor(inside).name()));
  QVERIFY(!near(notShown.pixelColor(inside), c.accent, 30));

  // the rail of icons draws the icon and no label: something in the icon square, nothing right of it
  rail->setExpanded(false, false);
  const auto folded = other->grab().toImage();
  bool icon = false;
  for (int x = NavItem::kRing + 8; x < NavItem::kRing + 36; ++x) {
    for (int y = 12; y < folded.height() - 12; ++y)
      icon = icon || !near(folded.pixelColor(x, y), folded.pixelColor(2, 2), 40);
  }
  QVERIFY(icon);

  // the keyboard shows a ring around the entry that has the focus, and the mouse does not
  rail->setExpanded(true, false);
  QWidget &w = window;
  showAndActivate(w);
  other->setFocus(Qt::MouseFocusReason);
  const auto mouse = other->grab().toImage();
  other->clearFocus();
  other->setFocus(Qt::TabFocusReason);
  const auto key = other->grab().toImage();
  QVERIFY(mouse != key);
  QVERIFY2(
      near(key.pixelColor(key.width() / 2, 1), c.accent) || near(key.pixelColor(key.width() / 2, 2), c.accent),
      qPrintable(key.pixelColor(key.width() / 2, 2).name())
  );
}

namespace {

// A language that has one word: what the name of the sidebar becomes when the language changes
class OneWordTranslator : public QTranslator
{
public:
  QString translate(const char *context, const char *sourceText, const char *, int) const override
  {
    if (QLatin1String(context) == QLatin1String("NavRail") && QLatin1String(sourceText) == QLatin1String("Sidebar"))
      return QStringLiteral("사이드바");
    return {};
  }
  bool isEmpty() const override
  {
    return false;
  }
};

} // namespace

void ShellWidgetsTests::rail_theNameOfTheListFollowsTheLanguage()
{
  install();
  NavRail rail;
  fill(rail);
  QCOMPARE(a11y(&rail)->text(QAccessible::Name), QStringLiteral("Sidebar"));

  OneWordTranslator translator;
  QVERIFY(QCoreApplication::installTranslator(&translator));
  const auto remove = qScopeGuard([&] { QCoreApplication::removeTranslator(&translator); });
  QCoreApplication::processEvents();
  QCOMPARE(a11y(&rail)->text(QAccessible::Name), QStringLiteral("사이드바"));
}

void ShellWidgetsTests::rail_tellsAScreenReaderTheListTheEntriesAndTheStateOfTheButton()
{
  install();
  NavRail rail;
  fill(rail);

  auto *list = a11y(&rail);
  QCOMPARE(list->role(), QAccessible::List);
  QCOMPARE(list->text(QAccessible::Name), QStringLiteral("Sidebar"));

  auto *home = a11y(rail.item(QStringLiteral("home")));
  QCOMPARE(home->role(), QAccessible::ListItem);
  QVERIFY(home->state().selectable);
  QVERIFY(home->state().selected);
  QVERIFY(!a11y(rail.item(QStringLiteral("other")))->state().selected);
  QCOMPARE(
      a11y(rail.item(QStringLiteral("other")))->actionInterface()->actionNames().first(),
      QAccessibleActionInterface::pressAction()
  );

  auto *log = a11y(rail.item(QStringLiteral("log")));
  QVERIFY(log->state().checkable);
  QVERIFY(!log->state().checked);
  rail.item(QStringLiteral("log"))->click();
  QVERIFY(log->state().checked);

  auto *tls = a11y(rail.item(QStringLiteral("tls")));
  QCOMPARE(tls->role(), QAccessible::StaticText);
  QCOMPARE(tls->text(QAccessible::Name), QStringLiteral("Encrypted (TLS)"));
  QVERIFY(!tls->state().focusable);
  QVERIFY(tls->actionInterface() == nullptr || tls->actionInterface()->actionNames().isEmpty());

  // the button says what it does, and whether the sidebar is unfolded or folded
  auto *button = a11y(rail.toggleButton());
  QCOMPARE(button->role(), QAccessible::Button);
  QCOMPARE(button->text(QAccessible::Name), QStringLiteral("Collapse sidebar"));
  QVERIFY(button->state().expandable);
  QVERIFY(button->state().expanded);
  QVERIFY(!button->state().collapsed);
  rail.setExpanded(false, false);
  QCOMPARE(button->text(QAccessible::Name), QStringLiteral("Expand sidebar"));
  QVERIFY(button->state().collapsed);
  QVERIFY(!button->state().expanded);

  // pressing it by the accessibility action asks for the same as a click
  QSignalSpy toggled(&rail, &NavRail::toggleRequested);
  button->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
  QCOMPARE(toggled.size(), 1);

  rail.setToggleTexts(QStringLiteral("Hide the sidebar"), QStringLiteral("Show the sidebar"));
  QCOMPARE(button->text(QAccessible::Name), QStringLiteral("Show the sidebar"));
}

void ShellWidgetsTests::segmented_choosesOne_andNeverUnchoosesIt()
{
  install();
  SegmentedControl control;
  control.addSegment(QStringLiteral("Share this computer"), QStringLiteral("Use this computer's keyboard and mouse"));
  control.addSegment(QStringLiteral("Use another computer"));
  QCOMPARE(control.count(), 2);
  QCOMPARE(control.current(), -1);
  QSignalSpy changed(&control, &SegmentedControl::currentChanged);

  control.setCurrent(0);
  QCOMPARE(control.current(), 0);
  QCOMPARE(changed.size(), 1);
  control.show();
  QTest::mouseClick(control.segment(1), Qt::LeftButton);
  QCOMPARE(control.current(), 1);
  QVERIFY(control.segment(1)->isChecked());
  QVERIFY(!control.segment(0)->isChecked());
  QCOMPARE(changed.size(), 2);
  QCOMPARE(changed.last().first().toInt(), 1);

  // a click on the choice that is made keeps it, and says nothing
  QTest::mouseClick(control.segment(1), Qt::LeftButton);
  QCOMPARE(control.current(), 1);
  QVERIFY(control.segment(1)->isChecked());
  QCOMPARE(changed.size(), 2);

  // the size: the height of a target, and the width of what is written
  QCOMPARE(control.sizeHint().height(), 44);
  QVERIFY(control.sizeHint().width() > control.segment(0)->sizeHint().width() + control.segment(1)->sizeHint().width());
  control.setSegmentText(1, QStringLiteral("Use another computer, a longer text"));
  QVERIFY(control.sizeHint().width() > 400);
}

void ShellWidgetsTests::segmented_isOneStopAndTheArrowKeysChoose()
{
  install();
  QWidget window;
  auto *control = new SegmentedControl(&window);
  control->addSegment(QStringLiteral("A"));
  control->addSegment(QStringLiteral("B"));
  control->addSegment(QStringLiteral("C"));
  control->setCurrent(1);
  window.resize(400, 100);
  showAndActivate(window);

  int stops = 0;
  for (int i = 0; i < control->count(); ++i)
    stops += (control->segment(i)->focusPolicy() & Qt::TabFocus) ? 1 : 0;
  QCOMPARE(stops, 1);
  QVERIFY(control->segment(1)->focusPolicy() & Qt::TabFocus);

  control->segment(1)->setFocus(Qt::TabFocusReason);
  QTRY_COMPARE(QApplication::focusWidget(), control->segment(1));
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Right);
  QCOMPARE(control->current(), 2);
  QCOMPARE(QApplication::focusWidget(), control->segment(2));
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Right);
  QCOMPARE(control->current(), 2);
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Left);
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Left);
  QCOMPARE(control->current(), 0);
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Left);
  QCOMPARE(control->current(), 0);
  // the stop moved with the choice
  QVERIFY(control->segment(0)->focusPolicy() & Qt::TabFocus);
  QVERIFY(!(control->segment(1)->focusPolicy() & Qt::TabFocus));
}

void ShellWidgetsTests::segmented_tellsAScreenReaderTheChoices()
{
  install();
  SegmentedControl control;
  control.setAccessibleName(QStringLiteral("Mode"));
  control.addSegment(QStringLiteral("Share this computer"), QStringLiteral("Use this computer's keyboard and mouse"));
  control.addSegment(
      QStringLiteral("Use another computer"), QStringLiteral("Use another computer's keyboard and mouse")
  );
  control.setCurrent(0);

  auto *group = a11y(&control);
  QCOMPARE(group->role(), QAccessible::Grouping);
  QCOMPARE(group->text(QAccessible::Name), QStringLiteral("Mode"));
  auto *first = a11y(control.segment(0));
  QCOMPARE(first->role(), QAccessible::RadioButton);
  QCOMPARE(first->text(QAccessible::Name), QStringLiteral("Share this computer"));
  QCOMPARE(first->text(QAccessible::Description), QStringLiteral("Use this computer's keyboard and mouse"));
  QVERIFY(first->state().checked);
  QVERIFY(!a11y(control.segment(1))->state().checked);

  a11y(control.segment(1))->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
  QCOMPARE(control.current(), 1);
  QVERIFY(a11y(control.segment(1))->state().checked);
  QVERIFY(!first->state().checked);
}

void ShellWidgetsTests::shell_unfoldedFoldedAndTheContentFollows()
{
  install();
  SidebarShell shell;
  fill(*shell.rail());
  auto *content = new QLabel(QStringLiteral("content"));
  shell.setContent(content);
  shell.resize(1100, 700);
  QSignalSpy collapsed(&shell, &SidebarShell::collapsedChanged);

  QVERIFY(!shell.isNarrow());
  QVERIFY(!shell.isCollapsed());
  QVERIFY(shell.isRailExpanded());
  const auto unfoldedLeft = content->x();
  // the panel of the rail is 236 px from the margin, and the content starts a gap after it (less the room of its
  // shadow)
  QCOMPARE(unfoldedLeft, SidebarShell::kMargin + 236 + SidebarShell::kGap - 10);

  shell.setCollapsed(true);
  QVERIFY(shell.isCollapsed());
  QVERIFY(!shell.isRailExpanded());
  QCOMPARE(collapsed.size(), 1);
  QCOMPARE(collapsed.last().first().toBool(), true);
  QCOMPARE(content->x(), SidebarShell::kMargin + 76 + SidebarShell::kGap - 10);
  QVERIFY(content->width() > 900 - 100);

  // the button of the rail does the same as the user
  shell.rail()->toggleButton()->click();
  QVERIFY(!shell.isCollapsed());
  QCOMPARE(collapsed.size(), 2);
  QCOMPARE(content->x(), unfoldedLeft);
  QCOMPARE(content->x() + content->width(), shell.width() - (SidebarShell::kMargin - 10));
}

void ShellWidgetsTests::shell_aNarrowWindowFoldsTheRailWithoutChangingTheChoice()
{
  install();
  SidebarShell shell;
  fill(*shell.rail());
  shell.setContent(new QLabel(QStringLiteral("content")));
  shell.resize(1100, 700);
  // a window that is not shown gets no resize events: the shell that follows the width of its window is shown
  shell.show();
  QVERIFY(QTest::qWaitForWindowExposed(&shell));
  QSignalSpy narrow(&shell, &SidebarShell::narrowChanged);
  QSignalSpy collapsed(&shell, &SidebarShell::collapsedChanged);
  QCOMPARE(shell.autoCollapseWidth(), 900);

  shell.resize(880, 700);
  QVERIFY(shell.isNarrow());
  QVERIFY(!shell.isRailExpanded());
  QVERIFY(!shell.isCollapsed());
  QCOMPARE(narrow.size(), 1);
  QCOMPARE(collapsed.size(), 0);

  // the edge is 900: at 900 there is room
  shell.resize(900, 700);
  QVERIFY(!shell.isNarrow());
  QVERIFY(shell.isRailExpanded());
  shell.resize(899, 700);
  QVERIFY(shell.isNarrow());

  // wide again: unfolded as the user left it
  shell.resize(1000, 700);
  QVERIFY(shell.isRailExpanded());

  // what the user chose stays through the narrow window
  shell.setCollapsed(true);
  shell.resize(700, 700);
  shell.resize(1000, 700);
  QVERIFY(shell.isCollapsed());
  QVERIFY(!shell.isRailExpanded());

  shell.setAutoCollapseWidth(1200);
  QVERIFY(shell.isNarrow());
}

void ShellWidgetsTests::shell_theOverlayUnfoldsOverTheContent_andFoldsAgain()
{
  install();
  QWidget window;
  auto *shell = new SidebarShell(&window);
  fill(*shell->rail());
  auto *content = new QLabel(QStringLiteral("content"));
  shell->setContent(content);
  window.resize(800, 600);
  shell->resize(800, 600);
  showAndActivate(window);
  auto *scrim = shell->findChild<QWidget *>(QStringLiteral("sidebarScrim"));
  QVERIFY(scrim);
  QSignalSpy overlay(shell, &SidebarShell::overlayChanged);

  QVERIFY(shell->isNarrow());
  const auto contentX = content->x();
  QVERIFY(!scrim->isVisible());

  // wide: no overlay (the sidebar is unfolded beside the content)
  window.resize(1000, 600);
  shell->resize(1000, 600);
  shell->openOverlay();
  QVERIFY(!shell->isOverlayOpen());
  window.resize(800, 600);
  shell->resize(800, 600);

  shell->rail()->toggleButton()->click();
  QVERIFY(shell->isOverlayOpen());
  QVERIFY(shell->isRailExpanded());
  QCOMPARE(shell->rail()->panelWidth(), 236);
  // the content stays where it was: the sidebar goes over it
  QCOMPARE(content->x(), contentX);
  QVERIFY(scrim->isVisible());
  QCOMPARE(scrim->geometry(), content->geometry());
  QVERIFY(shell->rail()->geometry().right() > content->geometry().left());
  QCOMPARE(overlay.size(), 1);
  // the focus is in the rail, on the page that is shown
  QTRY_COMPARE(QApplication::focusWidget(), shell->rail()->item(QStringLiteral("home")));

  // Escape folds it, and the focus is back on the button
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
  QVERIFY(!shell->isOverlayOpen());
  QVERIFY(!shell->isRailExpanded());
  QVERIFY(!scrim->isVisible());
  QCOMPARE(QApplication::focusWidget(), shell->rail()->toggleButton());
  QCOMPARE(overlay.size(), 2);

  // a click on the dimmed content folds it
  shell->rail()->toggleButton()->click();
  QVERIFY(shell->isOverlayOpen());
  QTest::mouseClick(scrim, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
  QVERIFY(!shell->isOverlayOpen());

  // a press on an entry is an answer: it folds as well
  shell->openOverlay();
  QVERIFY(shell->isOverlayOpen());
  shell->rail()->item(QStringLiteral("prefs"))->click();
  QVERIFY(!shell->isOverlayOpen());

  // the window that becomes wide takes the overlay away
  shell->openOverlay();
  window.resize(1000, 600);
  shell->resize(1000, 600);
  QVERIFY(!shell->isOverlayOpen());
  QVERIFY(!scrim->isVisible());
  QVERIFY(shell->isRailExpanded());
}

void ShellWidgetsTests::shell_asksForTheRoomOfTheRailThatIsFolded()
{
  install();
  SidebarShell shell;
  fill(*shell.rail());
  auto *content = new QLabel(QStringLiteral("content"));
  content->setMinimumSize(300, 200);
  shell.setContent(content);
  // the narrowest the shell is: the margins, the rail of icons, the gap, and the panels of the content
  QCOMPARE(
      shell.minimumSizeHint().width(),
      SidebarShell::kMargin + 76 + SidebarShell::kGap + (300 - 20) + SidebarShell::kMargin
  );
  QVERIFY(shell.minimumSizeHint().height() >= shell.rail()->minimumSizeHint().height() - 22);
  QVERIFY(shell.sizeHint().width() > shell.minimumSizeHint().width());
}

void ShellWidgetsTests::shell_theOverlayIsAsOpaqueAsAPanelOfText()
{
  install();
  QWidget window;
  auto *shell = new SidebarShell(&window);
  fill(*shell->rail());
  auto *content = new QLabel(QStringLiteral("content"));
  shell->setContent(content);
  window.resize(760, 560);
  shell->resize(760, 560);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  // beside the content the rail is the decoration that it is: it lets the backdrop through
  QVERIFY(!shell->isOverlayOpen());
  QVERIFY(!shell->rail()->isSolid());
  QCOMPARE(shell->rail()->density(), PanelWidget::Density::Normal);

  // over the content it carries text that must not be read through
  shell->openOverlay();
  QVERIFY(shell->rail()->isSolid());
  QCOMPARE(shell->rail()->density(), PanelWidget::Density::Dense);

  shell->closeOverlay();
  QVERIFY(!shell->rail()->isSolid());
  QCOMPARE(shell->rail()->density(), PanelWidget::Density::Normal);
}

void ShellWidgetsTests::rail_thatIsSolidHidesWhatIsBehindIt()
{
  // a look that lets the backdrop through: red behind a rail that is not solid shows in it, and not in one that is
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.reduceTransparency = false;
  look.reduceMotion = true;
  style->useLook(look);

  QWidget window;
  QPalette red;
  red.setColor(QPalette::Window, QColor(255, 0, 0));
  window.setPalette(red);
  window.setAutoFillBackground(true);
  auto *rail = new NavRail(&window);
  fill(*rail);
  window.resize(rail->width(), 520);
  rail->resize(rail->width(), 520);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  // a place of the panel without an entry: just under the entries
  const auto empty = [&](const QImage &image) {
    const auto panel = rail->panelRect();
    return image.pixelColor(panel.center().x(), panel.center().y() + 60);
  };
  const auto translucent = empty(window.grab().toImage());
  rail->setSolid(true);
  const auto solid = empty(window.grab().toImage());
  const auto &surface = currentTokens().colors.surface;
  // the red of the window tints the panel that is not solid, and does not tint the one that is
  QVERIFY2(
      translucent.red() - translucent.green() > solid.red() - solid.green(),
      qPrintable(translucent.name() + solid.name())
  );
  QVERIFY2(near(solid, surface, 40), qPrintable(solid.name() + QStringLiteral(" vs ") + surface.name()));
}

QTEST_MAIN(ShellWidgetsTests)
