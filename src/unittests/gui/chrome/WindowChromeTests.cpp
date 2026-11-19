/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "WindowChromeTests.h"

#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/chrome/MenuMigration.h"
#include "gui/chrome/WindowHeader.h"
#include "gui/core/CoreProcess.h"
#include "gui/shell/MainShell.h"
#include "gui/style/Appearance.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/NavRail.h"
#include "gui/widgets/SegmentedControl.h"
#include "gui/widgets/StatusHero.h"

#include "../FontEnvironment.h"

#include <QAccessible>
#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPixmapCache>
#include <QScopeGuard>
#include <QSet>
#include <QSignalSpy>

#include <memory>

// What the main window keeps to itself: the tests reach it through this one class (the window names it as its friend)
struct MainWindowTestAccess
{
  static deskflow::gui::MainShell *shell(MainWindow &w)
  {
    return w.m_shell;
  }
  static deskflow::gui::chrome::WindowHeader *header(MainWindow &w)
  {
    return w.m_header;
  }
  static deskflow::gui::chrome::FrameDecision frame(MainWindow &w)
  {
    return w.m_frame;
  }
  static QAction *actionFor(MainWindow &w, deskflow::gui::chrome::MenuAction action)
  {
    return w.actionFor(action);
  }
  static void hideMenuBar(MainWindow &w)
  {
    w.hideMenuBar();
  }
  static QList<QMenu *> oldMenus(MainWindow &w)
  {
    return {w.m_menuFile, w.m_menuEdit, w.m_menuView, w.m_menuHelp};
  }
  static QAction *toggleSidebarAction(MainWindow &w)
  {
    return w.m_actionToggleSidebar;
  }
  static void showAndActivate(MainWindow &w)
  {
    w.showAndActivate();
  }
  //! The window has been hidden to the tray: the tray offers it again (on a Mac it is the program that is hidden)
  static bool offeredBackByTheTray(MainWindow &w)
  {
    return w.m_actionRestore->isVisible();
  }
  static void setTrayAvailable(MainWindow &w, bool available)
  {
    w.m_trayAvailable = [available] { return available; };
  }
};

namespace {

using Access = MainWindowTestAccess;
using namespace deskflow::gui;
using namespace deskflow::gui::chrome;
using namespace deskflow::gui::style;

constexpr auto kFrameVariable = "DESKFLOW_WINDOW_FRAME";

// the pictures of the report are made in both schemes
bool g_darkLook = false;

void installDeskflowStyle()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.scheme = g_darkLook ? Scheme::Dark : Scheme::Light;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

// A window as the application makes it: the settings are those of the test, and the core is not started
std::unique_ptr<MainWindow> makeWindow(bool deskflowLook, Settings::CoreMode mode = Settings::CoreMode::Server)
{
  if (deskflowLook)
    installDeskflowStyle();
  else
    QApplication::setStyle(QStringLiteral("Fusion"));
  Settings::setValue(Settings::Security::TlsEnabled, false);
  // the messages of the first start and of the first connection are dialogs that wait for the user
  Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  Settings::setValue(Settings::Core::CoreMode, mode);
  return std::make_unique<MainWindow>();
}

// The window with the header: the frame of the new look is asked for, as a person does
std::unique_ptr<MainWindow> makeClientSideWindow(Settings::CoreMode mode = Settings::CoreMode::Server)
{
  qputenv(kFrameVariable, "client");
  return makeWindow(true, mode);
}

void showWindow(MainWindow &window)
{
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QCoreApplication::processEvents();
}

QString nameOf(QObject *object)
{
  auto *interface = QAccessible::queryAccessibleInterface(object);
  return interface ? interface->text(QAccessible::Name) : QString();
}

// The key of every action that has one, as text (an action with no key has none)
QSet<QString> keysOf(const QList<QAction *> &actions)
{
  QSet<QString> keys;
  for (const auto *action : actions) {
    for (const auto &key : action->shortcuts()) {
      if (!key.isEmpty())
        keys.insert(key.toString(QKeySequence::PortableText));
    }
  }
  return keys;
}

QList<QAction *> withoutSeparators(const QList<QAction *> &actions)
{
  QList<QAction *> found;
  for (auto *action : actions) {
    if (!action->isSeparator())
      found << action;
  }
  return found;
}

} // namespace

void WindowChromeTests::initTestCase()
{
  // the width of text is what these tests measure: without fonts every letter is a box (see FontEnvironment.h)
  SKIP_WITHOUT_FONTS();
  QVERIFY(m_dir.isValid());
  // the window listens on a local socket that is named in the temporary directory: it is the one of this test
  qputenv("TMPDIR", m_dir.path().toUtf8());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
}

void WindowChromeTests::cleanup()
{
  g_darkLook = false;
  qunsetenv(kFrameVariable);
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QApplication::setFont(QFont());
  QPixmapCache::clear();
  for (const auto &key :
       {Settings::Gui::SidebarCollapsed, Settings::Gui::ShellLogExpanded, Settings::Gui::LogExpanded,
        Settings::Gui::WindowGeometry, Settings::Gui::ShellWindowGeometry, Settings::Core::CoreMode,
        Settings::Security::TlsEnabled, Settings::Gui::ShownServerFirstStartMessage,
        Settings::Gui::ShownFirstConnectedMessage, Settings::Gui::AutoStartCore, Settings::Gui::NativeWindowFrame,
        Settings::Gui::CloseToTray, Settings::Gui::CloseReminder, Settings::Core::ComputerName})
    Settings::setValue(key);
}

void WindowChromeTests::systemLook_hasNoHeader_andKeepsItsMenuBar()
{
  // even where the person asks for the new frame: the look of the system is the window as it was
  qputenv(kFrameVariable, "client");
  auto window = makeWindow(false);
  QVERIFY(!Access::header(*window));
  QVERIFY(!Access::shell(*window));
  showWindow(*window);
  QVERIFY(window->menuBar() != nullptr);
  QVERIFY(!window->menuBar()->isHidden());
  QCOMPARE(window->windowFlags() & Qt::FramelessWindowHint, Qt::WindowFlags());
  QVERIFY(window->findChildren<WindowHeader *>().isEmpty());
}

void WindowChromeTests::offscreen_keepsTheFrameOfTheSystem_unlessAskedOtherwise()
{
  auto window = makeWindow(true);
  QVERIFY(Access::shell(*window));
  QVERIFY(!Access::header(*window));
  QVERIFY(!Access::frame(*window).clientSide());
  QVERIFY(window->findChildren<WindowHeader *>().isEmpty());
}

void WindowChromeTests::clientSideFrame_putsTheHeaderAboveTheShell_andTheTitleFollowsTheWindow()
{
  auto window = makeClientSideWindow();
  auto *header = Access::header(*window);
  QVERIFY(header);
  QVERIFY(Access::frame(*window).clientSide());
  showWindow(*window);

  // the header is the top of the window, over the width of it, and the sidebar and the page are below it
  const auto top = header->mapTo(window.get(), QPoint(0, 0));
  QCOMPARE(header->height(), WindowHeader::kHeight);
  QCOMPARE(header->width(), window->width());
  auto *shell = Access::shell(*window);
  QVERIFY2(
      shell->rail()->mapTo(window.get(), QPoint(0, 0)).y() >= top.y() + header->height(),
      "the sidebar starts under the header"
  );
  QVERIFY(shell->pageTitle()->mapTo(window.get(), QPoint(0, 0)).y() >= top.y() + header->height());

  // the name of the program is the title of the window, whatever it becomes (a Mac shows it in its own title bar, and
  // the header says it only to the screen reader)
  const auto shown = [&](const QString &title) { return header->options().title ? title : QString(); };
  QCOMPARE(header->title(), shown(window->windowTitle()));
  QCOMPARE(header->accessibleName(), window->windowTitle());
  window->setWindowTitle(QStringLiteral("another title"));
  QCOMPARE(header->title(), shown(QStringLiteral("another title")));
  QCOMPARE(header->accessibleName(), QStringLiteral("another title"));
}

void WindowChromeTests::optOut_keepsTheFrameOfTheSystem_data()
{
  QTest::addColumn<QString>("environment");
  QTest::addColumn<bool>("setting");

  QTest::newRow("the variable says native") << QStringLiteral("native") << false;
  QTest::newRow("the setting says native") << QString() << true;
  // the person who has the variable for the new frame and the setting for the old one has the old one
  QTest::newRow("the setting is stronger than a wish for the new frame") << QStringLiteral("client") << true;
}

void WindowChromeTests::optOut_keepsTheFrameOfTheSystem()
{
  QFETCH(QString, environment);
  QFETCH(bool, setting);

  if (!environment.isEmpty())
    qputenv(kFrameVariable, environment.toUtf8());
  Settings::setValue(Settings::Gui::NativeWindowFrame, setting);
  auto window = makeWindow(true);
  QVERIFY(Access::shell(*window));
  QVERIFY(!Access::header(*window));
  QVERIFY(!Access::frame(*window).clientSide());
  showWindow(*window);
  QCOMPARE(window->windowFlags() & Qt::FramelessWindowHint, Qt::WindowFlags());
  // the menu bar of the system is what the window has then
  QVERIFY(!window->menuBar()->isHidden());
}

void WindowChromeTests::header_hasTheButtonsOfTheWindowWithTheirNames_andSaysWhatWasAskedFor()
{
  installDeskflowStyle();
  WindowHeader header({}, nullptr);
  header.setFixedWidth(600);
  header.show();
  QVERIFY(QTest::qWaitForWindowExposed(&header));

  auto *minimize = header.captionButton(Region::Minimize);
  auto *maximize = header.captionButton(Region::Maximize);
  auto *close = header.captionButton(Region::Close);
  QVERIFY(minimize && maximize && close);
  QVERIFY(header.moreButton());
  QVERIFY(!header.captionButton(Region::Caption));
  QCOMPARE(minimize->accessibleName(), QStringLiteral("Minimize"));
  QCOMPARE(maximize->accessibleName(), QStringLiteral("Maximize"));
  QCOMPARE(close->accessibleName(), QStringLiteral("Close"));
  QCOMPARE(header.moreButton()->accessibleName(), QStringLiteral("More"));

  // the buttons of the window are at the right edge, the close button last, each of the width of one of Windows 11
  QCOMPARE(close->geometry().right() + 1, header.width());
  QCOMPARE(close->width(), WindowHeader::kCaptionButtonWidth);
  QCOMPARE(maximize->geometry().right() + 1, close->geometry().left());
  QCOMPARE(minimize->geometry().right() + 1, maximize->geometry().left());

  QSignalSpy minimized(&header, &WindowHeader::minimizeRequested);
  QSignalSpy toggled(&header, &WindowHeader::maximizeToggleRequested);
  QSignalSpy closed(&header, &WindowHeader::closeRequested);
  minimize->click();
  maximize->click();
  close->click();
  QCOMPARE(minimized.count(), 1);
  QCOMPARE(toggled.count(), 1);
  QCOMPARE(closed.count(), 1);

  // a maximized window offers to restore it, and says so
  header.setMaximized(true);
  QCOMPARE(maximize->glyph(), HeaderButton::Glyph::Restore);
  QCOMPARE(maximize->accessibleName(), QStringLiteral("Restore"));
  header.setMaximized(false);
  QCOMPARE(maximize->glyph(), HeaderButton::Glyph::Maximize);
  QCOMPARE(maximize->accessibleName(), QStringLiteral("Maximize"));
}

void WindowChromeTests::header_ofAMac_hasNoButtonsOfItsOwn_andLeavesRoomForTheOnesOfTheSystem()
{
  installDeskflowStyle();
  WindowHeader::Options options;
  options.captionButtons = false;
  options.leftInset = WindowHeader::kTrafficLightsInset;
  options.title = false;
  WindowHeader header(options, nullptr);
  header.setFixedWidth(600);
  header.setTitle(QStringLiteral("Deskflow"));
  header.show();
  QVERIFY(QTest::qWaitForWindowExposed(&header));

  QVERIFY(!header.captionButton(Region::Close));
  QVERIFY(!header.captionButton(Region::Minimize));
  QVERIFY(!header.captionButton(Region::Maximize));
  // the system draws its buttons over the left end: nothing of the header is there, and the title is the one of the
  // title bar of the system
  QVERIFY(header.title().isEmpty());
  QVERIFY(header.moreButton()->geometry().left() >= WindowHeader::kTrafficLightsInset);
  // the menu button is at the right edge
  QVERIFY(header.moreButton()->geometry().right() + 1 <= header.width());
}

void WindowChromeTests::header_saysWhereItTakesTheMouse_andWhereTheWindowIsDragged()
{
  installDeskflowStyle();
  WindowHeader header({}, nullptr);
  header.setFixedWidth(600);
  header.show();
  QVERIFY(QTest::qWaitForWindowExposed(&header));

  // what takes the mouse for itself is the menu button; the rest of the header is the caption
  const auto interactive = header.interactiveRects();
  QCOMPARE(interactive.count(), 1);
  QCOMPARE(interactive.first(), header.moreButton()->geometry());

  QSignalSpy dragged(&header, &WindowHeader::dragRequested);
  QSignalSpy doubleClicked(&header, &WindowHeader::captionDoubleClicked);
  QSignalSpy systemMenu(&header, &WindowHeader::systemMenuRequested);
  QTest::mousePress(&header, Qt::LeftButton, {}, QPoint(20, header.height() / 2));
  QTest::mouseRelease(&header, Qt::LeftButton, {}, QPoint(20, header.height() / 2));
  QCOMPARE(dragged.count(), 1);
  QTest::mouseDClick(&header, Qt::LeftButton, {}, QPoint(20, header.height() / 2));
  QCOMPARE(doubleClicked.count(), 1);
  QContextMenuEvent menu(QContextMenuEvent::Mouse, QPoint(20, 10), header.mapToGlobal(QPoint(20, 10)));
  QCoreApplication::sendEvent(&header, &menu);
  QCOMPARE(systemMenu.count(), 1);

  // a press on the menu button does not drag
  dragged.clear();
  QTest::mouseClick(header.moreButton(), Qt::LeftButton);
  QCOMPARE(dragged.count(), 0);
}

void WindowChromeTests::header_followsTheLanguage()
{
  installDeskflowStyle();
  WindowHeader header({}, nullptr);
  header.moreButton()->setAccessibleName(QStringLiteral("stale"));
  header.captionButton(Region::Close)->setToolTip(QStringLiteral("stale"));
  header.retranslate();
  QCOMPARE(header.moreButton()->accessibleName(), QStringLiteral("More"));
  QCOMPARE(header.captionButton(Region::Close)->toolTip(), QStringLiteral("Close"));
}

void WindowChromeTests::headerMenu_isTheTableOfTheOldMenus_inItsOrder_withALineBetweenGroups()
{
  auto window = makeClientSideWindow();
  auto *header = Access::header(*window);
  QVERIFY(header && header->menu());
  QCOMPARE(header->menu()->objectName(), QStringLiteral("headerMenu"));

  const bool mac =
#ifdef Q_OS_MACOS
      true;
#else
      false;
#endif
  QList<QAction *> expected;
  int group = -1;
  for (const auto &entry : menuMigrations(mac)) {
    auto *action = Access::actionFor(*window, entry.action);
    QVERIFY(action);
    if (group >= 0 && entry.group != group)
      expected << nullptr;
    group = entry.group;
    expected << action;
  }

  const auto actions = header->menu()->actions();
  QCOMPARE(actions.count(), expected.count());
  for (int i = 0; i < actions.count(); ++i) {
    if (!expected[i])
      QVERIFY2(actions[i]->isSeparator(), qPrintable(QStringLiteral("a line at %1").arg(i)));
    else
      QCOMPARE(actions[i], expected[i]);
  }
  // no line at the ends and no two together
  QVERIFY(!actions.first()->isSeparator());
  QVERIFY(!actions.last()->isSeparator());
}

void WindowChromeTests::headerMenu_hasAnActionOfTheWindowForEveryEntryOfTheTable()
{
  auto window = makeClientSideWindow();
  const bool mac =
#ifdef Q_OS_MACOS
      true;
#else
      false;
#endif
  const auto entries = menuMigrations(mac);
  QCOMPARE(entries.count(), 12);
  QSet<QAction *> found;
  for (const auto &entry : entries) {
    auto *action = Access::actionFor(*window, entry.action);
    QVERIFY2(action, "every entry of the table is an action of the window");
    QVERIFY2(!found.contains(action), "two entries are not the same action");
    found.insert(action);
    QVERIFY(entry.places.contains(Place::HeaderMenu));
    // a row of a menu says what it does: it has a text (the window sets it as it is shown)
    QVERIFY2(!action->text().isEmpty(), qPrintable(QStringLiteral("action %1 has no text").arg(int(entry.action))));
  }
}

void WindowChromeTests::everyKeyOfTheOldMenus_isKeptByTheWindowWithoutAMenuBar()
{
  // the keys of the window of the system, with its menus
  QSet<QString> oldKeys;
  QList<QAction *> oldActions;
  {
    auto window = makeWindow(false);
    for (auto *menu : Access::oldMenus(*window))
      oldActions << withoutSeparators(menu->actions());
    oldKeys = keysOf(oldActions);
    QVERIFY2(oldKeys.contains(QStringLiteral("Ctrl+S")), "the window of the system has a key for starting");
    QVERIFY(oldKeys.contains(QStringLiteral("Ctrl+T")));
    QVERIFY(oldKeys.contains(QStringLiteral("F1")));
    QCOMPARE(withoutSeparators(oldActions).count(), 8);
  }

  auto window = makeClientSideWindow();
  Access::hideMenuBar(*window);
  QVERIFY(window->menuBar()->isHidden());
  // a key of an action works when the action belongs to the window: the keys of the menus that are not shown too
  const auto kept = keysOf(window->actions());
  for (const auto &key : oldKeys)
    QVERIFY2(kept.contains(key), qPrintable(QStringLiteral("the key %1 is kept").arg(key)));
  // and that of the sidebar, which the old window has in its View menu as well
  QVERIFY(window->actions().contains(Access::toggleSidebarAction(*window)));
  for (auto *action : window->actions()) {
    if (action->shortcut().isEmpty())
      continue;
    QVERIFY2(
        action->shortcutContext() != Qt::WidgetShortcut,
        qPrintable(QStringLiteral("%1 works anywhere in the window").arg(action->text()))
    );
  }
}

void WindowChromeTests::theKeyOfTheSidebar_worksWhenTheMenuBarIsHidden()
{
  auto window = makeClientSideWindow();
  Access::hideMenuBar(*window);
  showWindow(*window);
  window->activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(window.get()));

  auto *rail = Access::shell(*window)->rail();
  const bool wasExpanded = rail->isExpanded();
  auto *action = Access::toggleSidebarAction(*window);
  QSignalSpy triggered(action, &QAction::triggered);
  QTest::keySequence(window.get(), action->shortcut());
  QCOMPARE(triggered.count(), 1);
  QTRY_VERIFY(rail->isExpanded() != wasExpanded);
}

void WindowChromeTests::closeOfTheHeader_goesToTheTrayOrEndsTheProgram_asTheSettingsSay()
{
  // with a tray, the window is hidden and the program goes on
  {
    Settings::setValue(Settings::Gui::CloseToTray, true);
    Settings::setValue(Settings::Gui::CloseReminder, false);
    auto window = makeClientSideWindow();
    Access::setTrayAvailable(*window, true);
    Access::showAndActivate(*window);
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    QVERIFY(Access::header(*window));
    QVERIFY(!Access::offeredBackByTheTray(*window));
    Q_EMIT Access::header(*window)->closeRequested();
    QVERIFY(Access::offeredBackByTheTray(*window));
    QVERIFY(!window->isMinimized());
  }
  // with no tray, a window that is hidden could not be found: it is minimized
  {
    Settings::setValue(Settings::Gui::CloseToTray, true);
    Settings::setValue(Settings::Gui::CloseReminder, false);
    auto window = makeClientSideWindow();
    Access::setTrayAvailable(*window, false);
    showWindow(*window);
    Q_EMIT Access::header(*window)->closeRequested();
    QVERIFY(window->isMinimized());
  }
  // with the setting to end the program, the window is closed
  {
    Settings::setValue(Settings::Gui::CloseToTray, false);
    auto window = makeClientSideWindow();
    showWindow(*window);
    Q_EMIT Access::header(*window)->closeRequested();
    QVERIFY(!window->isVisible());
  }
}

void WindowChromeTests::headerAndShell_keepTheMinimumWidth_withWideFonts_data()
{
  QTest::addColumn<QString>("family");
  QTest::addColumn<qreal>("fontScale");
  QTest::addColumn<int>("basePixels");
  QTest::addColumn<int>("spacing");
  QTest::addColumn<bool>("client");

  // the font of the test platform, and the body of Windows 11 (14 px: the least that the new look gives the text
  // there), also with letters 30 % apart, as wide as the fonts of the PC of the checks of Windows were
  for (const int basePixels : {0, 14}) {
    for (const int spacing : basePixels ? QList<int>{100, 130} : QList<int>{100}) {
      for (const QString &family : {QString(), QStringLiteral("Verdana")}) {
        for (const qreal scale : {1.0, 1.15, 1.3, 1.5}) {
          for (const bool client : {false, true}) {
            QTest::addRow(
                "%s%s%s x%.2f %s", basePixels ? "14 px " : "", spacing != 100 ? "wide " : "",
                family.isEmpty() ? "default" : qPrintable(family), scale, client ? "client" : "server"
            ) << family
              << scale << basePixels << spacing << client;
          }
        }
      }
    }
  }
}

void WindowChromeTests::headerAndShell_keepTheMinimumWidth_withWideFonts()
{
  QFETCH(QString, family);
  QFETCH(qreal, fontScale);
  QFETCH(int, basePixels);
  QFETCH(int, spacing);
  QFETCH(bool, client);

  auto window = makeClientSideWindow(client ? Settings::CoreMode::Client : Settings::CoreMode::Server);
  auto *header = Access::header(*window);
  QVERIFY(header);
  auto font = window->font();
  if (!family.isEmpty())
    font.setFamily(family);
  if (spacing != 100)
    font.setLetterSpacing(QFont::PercentageSpacing, spacing);
  if (basePixels > 0)
    font.setPixelSize(qRound(basePixels * fontScale));
  else if (font.pointSizeF() > 0)
    font.setPointSizeF(font.pointSizeF() * fontScale);
  else
    font.setPixelSize(qRound(font.pixelSize() * fontScale));
  window->setFont(font);
  showWindow(*window);

  // the header does not make the window ask for more than it promised
  window->resize(MainShell::kMinimumWindowWidth, 760);
  QTRY_COMPARE(window->width(), MainShell::kMinimumWindowWidth);
  QVERIFY2(
      window->minimumSizeHint().width() <= MainShell::kMinimumWindowWidth,
      qPrintable(QStringLiteral("the window asks for %1").arg(window->minimumSizeHint().width()))
  );
  for (const int width : {MainShell::kMinimumWindowWidth, 800, 1000}) {
    window->resize(width, 760);
    QTRY_COMPARE(window->width(), width);
    QCoreApplication::processEvents();
    QVERIFY(header->width() == width);
    // the buttons of the header are all inside of it, and the menu button does not touch the buttons of the window
    for (auto *button : header->findChildren<HeaderButton *>()) {
      QVERIFY2(header->rect().contains(button->geometry()), qPrintable(button->objectName()));
      QVERIFY(button->isVisible());
    }
    if (auto *minimize = header->captionButton(Region::Minimize))
      QVERIFY(header->moreButton()->geometry().right() < minimize->geometry().left());
    QVERIFY(header->height() == WindowHeader::kHeight);
  }
}

// The texts in a tree that a screen reader walks that say again what the group above them is named: it would say it
// twice (once as the group, once as the text)
QStringList sayAgain(QAccessibleInterface *node)
{
  QStringList found;
  if (!node || node->state().invisible)
    return found;
  const auto name = node->text(QAccessible::Name);
  for (int i = 0; i < node->childCount(); ++i) {
    auto *child = node->child(i);
    if (!child || child->state().invisible)
      continue;
    if (child->role() == QAccessible::StaticText && !name.isEmpty() && child->text(QAccessible::Name) == name &&
        node->role() != QAccessible::StaticText)
      found << QStringLiteral("\"%1\" says it again in a text of its own").arg(name);
    found << sayAgain(child);
  }
  return found;
}

// The buttons of a header, found as a screen reader finds them: named, buttons, and telling the truth about the
// keyboard. The state of a button says that it can take the focus exactly when it is a stop of the keyboard (its policy
// of focus): the window of Windows has the three buttons of the window that are not stops (the keys of the system,
// Alt+Space and Win+Up, do what they do), and the button of the menu that is one
void checkTheButtons(const WindowHeader &header)
{
  const auto buttons = header.findChildren<HeaderButton *>();
  QVERIFY(!buttons.isEmpty());
  for (auto *button : buttons) {
    auto *interface = QAccessible::queryAccessibleInterface(button);
    QVERIFY2(interface, qPrintable(button->objectName()));
    QCOMPARE(interface->role(), QAccessible::Button);
    QVERIFY2(
        !interface->text(QAccessible::Name).isEmpty(), qPrintable(button->objectName() + QStringLiteral(" has no name"))
    );
    QVERIFY2(
        interface->state().focusable == (button->focusPolicy() != Qt::NoFocus),
        qPrintable(button->objectName() + QStringLiteral(": the state says what the policy of focus says"))
    );
    // every button of the header can be pressed by a screen reader (it is the way to reach a button that is no stop)
    auto *actions = interface->actionInterface();
    QVERIFY2(actions, qPrintable(button->objectName()));
    QVERIFY2(
        actions->actionNames().contains(QAccessibleActionInterface::pressAction()), qPrintable(button->objectName())
    );
  }
  // each name is its own: no two buttons are said the same
  QSet<QString> names;
  for (auto *button : buttons)
    names.insert(nameOf(button));
  QCOMPARE(names.count(), buttons.count());
}

void WindowChromeTests::theHeader_saysItsNameOnce_whateverTheSystemDrawsInIt_data()
{
  QTest::addColumn<bool>("captionButtons");
  QTest::addColumn<bool>("title");
  QTest::addColumn<int>("leftInset");

  // the header of a Mac (the buttons of the window and the title are the system's: nothing of its own but the menu),
  // the one of Windows (the name, the menu and the three buttons), and the one that has the name and no buttons of its
  // own
  QTest::newRow("macOS") << false << false << WindowHeader::kTrafficLightsInset;
  QTest::newRow("Windows and Linux") << true << true << 0;
  QTest::newRow("the name, without buttons") << false << true << 0;
}

void WindowChromeTests::theHeader_saysItsNameOnce_whateverTheSystemDrawsInIt()
{
  QFETCH(bool, captionButtons);
  QFETCH(bool, title);
  QFETCH(int, leftInset);

  auto window = makeClientSideWindow();
  showWindow(*window);
  WindowHeader::Options options;
  options.captionButtons = captionButtons;
  options.title = title;
  options.leftInset = leftInset;
  WindowHeader header(options, window.get());
  header.setTitle(QStringLiteral("Deskflow"));
  header.setGeometry(0, 0, window->width(), WindowHeader::kHeight);
  header.show();
  QCoreApplication::processEvents();

  auto *interface = QAccessible::queryAccessibleInterface(&header);
  QVERIFY(interface);
  // the header is named by the name of the program, and the name is not a text again below it
  QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Deskflow"));
  const auto again = sayAgain(interface);
  QVERIFY2(again.isEmpty(), qPrintable(again.join(QLatin1Char('\n'))));
  // the same of the whole window, as the window of the platform has it
  const auto whole = sayAgain(QAccessible::queryAccessibleInterface(window.get()));
  QVERIFY2(whole.isEmpty(), qPrintable(whole.join(QLatin1Char('\n'))));
}

void WindowChromeTests::theButtonsOfTheHeader_areNamedForTheScreenReader_andAreButtons()
{
  auto window = makeClientSideWindow();
  auto *header = Access::header(*window);
  QVERIFY(header);
  showWindow(*window);
  checkTheButtons(*header);
}

void WindowChromeTests::theHeaderOfWindows_hasTheButtonOfTheMenuAsTheOnlyStopOfTheKeyboard()
{
  // the header of Windows is made here whatever the system is: the one of a Mac has no buttons of the window of its own
  auto window = makeClientSideWindow();
  showWindow(*window);
  WindowHeader::Options options;
  options.captionButtons = true;
  WindowHeader header(options, window.get());
  header.setTitle(QStringLiteral("Deskflow"));
  header.setGeometry(0, 0, window->width(), WindowHeader::kHeight);
  header.show();
  QCoreApplication::processEvents();
  checkTheButtons(header);

  // the menu button is reached with Tab and opens the menu with Space and with Enter
  auto *more = header.moreButton();
  QCOMPARE(more->focusPolicy(), Qt::StrongFocus);
  QVERIFY(QAccessible::queryAccessibleInterface(more)->state().focusable);
  auto *menu = new QMenu(&header);
  menu->addAction(QStringLiteral("One"));
  header.setMenu(menu);
  for (const int key : {Qt::Key_Space, Qt::Key_Return, Qt::Key_Enter}) {
    more->setFocus(Qt::TabFocusReason);
    QVERIFY(more->hasFocus());
    QVERIFY(!menu->isVisible());
    QTest::keyClick(more, static_cast<Qt::Key>(key));
    QTRY_VERIFY2(menu->isVisible(), qPrintable(QStringLiteral("the key %1 opens the menu").arg(key)));
    menu->close();
    QTRY_VERIFY(!menu->isVisible());
  }

  // the buttons of the window are no stops, as those of the window of the system are not, and are still buttons that a
  // screen reader presses
  QSignalSpy minimize(&header, &WindowHeader::minimizeRequested);
  QSignalSpy maximize(&header, &WindowHeader::maximizeToggleRequested);
  QSignalSpy close(&header, &WindowHeader::closeRequested);
  const QList<std::pair<Region, QSignalSpy *>> windowButtons = {
      {Region::Minimize, &minimize}, {Region::Maximize, &maximize}, {Region::Close, &close}
  };
  for (const auto &[region, spy] : windowButtons) {
    auto *button = header.captionButton(region);
    QVERIFY(button);
    QCOMPARE(button->focusPolicy(), Qt::NoFocus);
    QVERIFY(!QAccessible::queryAccessibleInterface(button)->state().focusable);
    QAccessible::queryAccessibleInterface(button)->actionInterface()->doAction(
        QAccessibleActionInterface::pressAction()
    );
    QTRY_COMPARE(spy->count(), 1);
  }
}

void WindowChromeTests::theLog_saysWhatTheWindowHadBeforeItWasMaximized_andAfterItIsRestored()
{
  static QStringList lines;
  lines.clear();
  const auto previous = qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) {
    if (message.startsWith(QLatin1String("window state:")))
      lines << message;
  });
  const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });

  auto window = makeClientSideWindow();
  window->resize(1000, 640);
  showWindow(*window);
  window->showMaximized();
  QTRY_VERIFY(window->isMaximized());
  window->showNormal();
  QTRY_VERIFY(!window->isMaximized());
  QCoreApplication::processEvents();

  const auto text = lines.join(QLatin1Char('\n'));
  QVERIFY2(text.contains(QStringLiteral("normal -> maximized")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("maximized -> normal")), qPrintable(text));
  // what the window had before it was maximized is in the line of the restore, to be read against the size that it has
  QVERIFY2(text.contains(QStringLiteral("size before it was maximized 1000x640")), qPrintable(text));
}

void WindowChromeTests::pictures_ofTheHeaderAsWindowsDrawsIt_whenAsked()
{
  const auto directory = qEnvironmentVariable("DESKFLOW_CHROME_SHOTS");
  if (directory.isEmpty())
    QSKIP("DESKFLOW_CHROME_SHOTS is not set");

  // the window of a Mac has the header of its own kind; the one of Windows is drawn on its own and put over the top
  // of the window, which has the same width and the same height of header
  for (const bool dark : {false, true}) {
    g_darkLook = dark;
    Settings::setValue(Settings::Core::ComputerName, QStringLiteral("Studio-Mac"));
    auto window = makeClientSideWindow();
    window->resize(1000, 640);
    showWindow(*window);
    const auto base = window->grab();

    WindowHeader::Options options;
    WindowHeader header(options, window.get());
    header.setTitle(window->windowTitle());
    header.setGeometry(0, 0, window->width(), WindowHeader::kHeight);
    header.show();
    QCoreApplication::processEvents();

    const auto compose = [&](const QString &name, HeaderButton *hovered, bool maximized) {
      header.setMaximized(maximized);
      if (hovered)
        QTest::mouseMove(hovered, hovered->rect().center());
      else
        QTest::mouseMove(window.get(), QPoint(500, 300));
      QCoreApplication::processEvents();
      auto picture = base;
      QPainter painter(&picture);
      painter.drawPixmap(0, 0, header.grab());
      painter.end();
      QVERIFY(picture.save(
          QDir(directory).filePath(QStringLiteral("header-windows-%1-%2.png").arg(dark ? "dark" : "light", name))
      ));
    };
    compose(QStringLiteral("rest"), nullptr, false);
    compose(QStringLiteral("hover-close"), header.captionButton(Region::Close), false);
    compose(QStringLiteral("hover-maximize"), header.captionButton(Region::Maximize), false);
    compose(QStringLiteral("maximized"), nullptr, true);
  }
}

QTEST_MAIN(WindowChromeTests)
