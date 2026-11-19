/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MainShellTests.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/chrome/WindowChrome.h"
#include "gui/chrome/WindowHeader.h"
#include "gui/core/CoreProcess.h"
#include "gui/settings/RestartBanner.h"
#include "gui/settings/SettingsPage.h"
#include "gui/settings/SettingsPages.h"
#include "gui/shell/ClientConnectionSection.h"
#include "gui/shell/MainShell.h"
#include "gui/shell/StatusModel.h"
#include "gui/style/Appearance.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/PanelWidget.h"
#include "gui/widgets/LayoutNoticeBar.h"
#include "gui/widgets/LogDock.h"
#include "gui/widgets/NavRail.h"
#include "gui/widgets/SegmentedControl.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/SidebarShell.h"
#include "gui/widgets/StatusBar.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/StatusHero.h"

#include "../FontEnvironment.h"

#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMetaEnum>
#include <QPixmapCache>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStackedWidget>

#include <algorithm>
#include <functional>
#include <memory>

// What the main window keeps to itself: the tests reach it through this one class (the window names it as its friend)
struct MainWindowTestAccess
{
  using ConnectionState = deskflow::core::ConnectionState;
  using ProcessState = deskflow::core::ProcessState;

  //! The work area of the screen that a window opens on: the screen of a test is 800x600, a desktop is larger
  static void setWorkArea(const QRect &area)
  {
    s_workAreaOf = area;
    MainWindow::s_workArea = [] { return s_workAreaOf; };
  }
  inline static QRect s_workAreaOf;

  static deskflow::gui::MainShell *shell(MainWindow &w)
  {
    return w.m_shell;
  }
  static deskflow::gui::CoreProcess &core(MainWindow &w)
  {
    return w.m_coreProcess;
  }
  static deskflow::gui::chrome::FrameDecision frame(const MainWindow &w)
  {
    return w.m_frame;
  }
  static deskflow::gui::chrome::WindowHeader *header(const MainWindow &w)
  {
    return w.m_header;
  }
  static deskflow::gui::chrome::WindowChrome *chrome(const MainWindow &w)
  {
    return w.m_chrome;
  }
  static void states(MainWindow &w, ProcessState process, ConnectionState connection)
  {
    w.m_coreProcess.setProcessState(process);
    w.m_coreProcess.setConnectionState(connection);
    // the window is told of the state of the process through the event loop
    QCoreApplication::processEvents();
  }
  static StatusBar *statusBar(MainWindow &w)
  {
    return w.m_statusBar;
  }
  static LogDock *logDock(MainWindow &w)
  {
    return w.m_logDock;
  }
  static LayoutNoticeBar *noticeBar(MainWindow &w)
  {
    return w.m_layoutNoticeBar;
  }
  static bool errorShownThisRun(MainWindow &w)
  {
    return w.m_logShownByErrorThisRun;
  }
  static QAction *toggleSidebarAction(MainWindow &w)
  {
    return w.m_actionToggleSidebar;
  }
  static deskflow::gui::settings::SettingsPages *settingsPages(MainWindow &w)
  {
    return w.m_settingsPages;
  }
  static void openSettings(MainWindow &w)
  {
    w.openSettings();
  }
  static QAction *settingsAction(MainWindow &w)
  {
    return w.m_actionSettings;
  }
  //! What the window does when the person closes it (a close that the OS sends): true when the program goes on
  static bool closeAsTheOsDoes(MainWindow &w)
  {
    return w.maybeHideToTray();
  }
};

namespace {

using Access = MainWindowTestAccess;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;
using namespace deskflow::gui;
using namespace deskflow::gui::style;

void installDeskflowStyle()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
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

void showWindow(MainWindow &window)
{
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QCoreApplication::processEvents();
}

template <class T> T *child(const QObject &parent, const QString &name)
{
  auto *found = parent.findChild<T *>(name);
  Q_ASSERT_X(found, "child", qPrintable(name));
  return found;
}

// the computers of a server with two others: PC and MINI (the server is this computer)
void writeComputers()
{
  Settings::setValue(Settings::Core::ComputerName, QStringLiteral("srv"));
  auto &settings = Settings::proxy();
  settings.beginGroup(QStringLiteral("internalConfig"));
  settings.remove(QString());
  settings.beginWriteArray(QStringLiteral("screens"));
  for (int i = 0; i < 15; ++i) {
    settings.setArrayIndex(i);
    settings.setValue(
        QStringLiteral("name"), i == 6   ? QStringLiteral("PC")
                                : i == 7 ? QStringLiteral("srv")
                                : i == 8 ? QStringLiteral("MINI")
                                         : QString()
    );
  }
  settings.endArray();
  settings.endGroup();
}

QString roleName(QAccessible::Role role)
{
  return QString::fromLatin1(QMetaEnum::fromType<QAccessible::Role>().valueToKey(role));
}

QString stateFlags(const QAccessible::State &state)
{
  QStringList flags;
  if (state.checkable)
    flags << (state.checked ? QStringLiteral("checked") : QStringLiteral("unchecked"));
  if (state.selected)
    flags << QStringLiteral("selected");
  if (state.expanded)
    flags << QStringLiteral("expanded");
  if (state.collapsed)
    flags << QStringLiteral("collapsed");
  if (state.disabled)
    flags << QStringLiteral("disabled");
  if (state.focusable)
    flags << QStringLiteral("focusable");
  return flags.join(QLatin1Char(','));
}

// One line for each thing that a screen reader walks to: its role, its name and its state
void dumpTree(QAccessibleInterface *interface, int depth, QStringList &lines)
{
  if (interface->state().invisible)
    return;
  auto line =
      QStringLiteral("%1%2 \"%3\"")
          .arg(QString(depth * 2, QLatin1Char(' ')), roleName(interface->role()), interface->text(QAccessible::Name));
  if (const auto flags = stateFlags(interface->state()); !flags.isEmpty())
    line += QStringLiteral(" [%1]").arg(flags);
  lines << line;
  for (int i = 0; i < interface->childCount(); ++i) {
    if (auto *child = interface->child(i))
      dumpTree(child, depth + 1, lines);
  }
}

// where a widget is seen in the window: a panel is the rectangle inside of its shadow
QRect rectIn(const QWidget &window, const QWidget *widget)
{
  const auto *panel = qobject_cast<const PanelWidget *>(widget);
  const auto local = panel ? panel->panelRect() : widget->rect();
  return QRect(widget->mapTo(&window, local.topLeft()), local.size());
}

// where a widget is, for the message of a check
QString placed(const QWidget &window, const QWidget *widget)
{
  const auto rect = rectIn(window, widget);
  return QStringLiteral("%1 at %2,%3 size %4x%5 in a window of %6x%7")
      .arg(QString::fromLatin1(widget->metaObject()->className()))
      .arg(rect.x())
      .arg(rect.y())
      .arg(rect.width())
      .arg(rect.height())
      .arg(window.width())
      .arg(window.height());
}

// The font of a test: of the family that is asked for, and the size of the platform (basePixels 0) or of a body of
// basePixels, times the scale of larger text. The spacing of the letters is that in percent (100 is as the font has
// it): the fonts of Windows are wider than the ones that a test platform has, and a wider spacing is a font that is
// wider
QFont scaledFont(QFont font, const QString &family, int basePixels, qreal scale, int spacing = 100)
{
  if (!family.isEmpty())
    font.setFamily(family);
  if (spacing != 100)
    font.setLetterSpacing(QFont::PercentageSpacing, spacing);
  if (basePixels > 0)
    font.setPixelSize(qRound(basePixels * scale));
  else if (font.pointSizeF() > 0)
    font.setPointSizeF(font.pointSizeF() * scale);
  else
    font.setPixelSize(qRound(font.pixelSize() * scale));
  return font;
}

// The visible widgets that ask for the most width, for the message of a check that the window is not asked for more
QString widestMinimum(const QWidget &window)
{
  QList<QPair<int, QString>> asked;
  for (const auto *widget : window.findChildren<QWidget *>()) {
    if (!widget->isVisibleTo(&window))
      continue;
    asked.append(
        {widget->minimumSizeHint().width(),
         QStringLiteral("%1 (%2)").arg(QString::fromLatin1(widget->metaObject()->className()), widget->objectName())}
    );
  }
  std::sort(asked.begin(), asked.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
  QStringList lines;
  for (int i = 0; i < qMin<qsizetype>(8, asked.size()); ++i)
    lines << QStringLiteral("%1 %2").arg(asked[i].first).arg(asked[i].second);
  return QStringLiteral("the window asks for %1: %2")
      .arg(window.minimumSizeHint().width())
      .arg(lines.join(QStringLiteral("; ")));
}

} // namespace

void MainShellTests::initTestCase()
{
  // the width of text is what these tests measure: without fonts every letter is a box (see FontEnvironment.h)
  SKIP_WITHOUT_FONTS();
  QVERIFY(m_dir.isValid());
  // the window listens on a local socket that is named in the temporary directory: it is the one of this test
  qputenv("TMPDIR", m_dir.path().toUtf8());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
  // the screen of a test is 800x600; the windows open as they do on a desktop unless a test says otherwise
  Access::setWorkArea(QRect(0, 0, 1920, 1040));
}

void MainShellTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QApplication::setFont(QFont());
  QPixmapCache::clear();
  for (const auto &key :
       {Settings::Gui::SidebarCollapsed, Settings::Gui::ShellLogExpanded, Settings::Gui::LogExpanded,
        Settings::Gui::WindowGeometry, Settings::Gui::ShellWindowGeometry, Settings::Core::CoreMode,
        Settings::Core::ComputerName, Settings::Security::TlsEnabled, Settings::Client::RemoteHost,
        Settings::Client::RecentRemoteHosts, Settings::Gui::ShownServerFirstStartMessage,
        Settings::Gui::ShownFirstConnectedMessage, Settings::Gui::AutoStartCore, Settings::Core::Port,
        Settings::Gui::Autohide, Settings::Gui::ShowVersionInTitle})
    Settings::setValue(key);
  auto &settings = Settings::proxy();
  settings.beginGroup(QStringLiteral("internalConfig"));
  settings.remove(QString());
  settings.endGroup();
}

void MainShellTests::systemLook_hasNoShell_andKeepsItsFixedSizeAndItsStatusBar()
{
  Settings::setValue(Settings::Gui::LogExpanded, false);
  auto window = makeWindow(false);

  QVERIFY(!Access::shell(*window));
  QVERIFY(window->findChildren<SidebarShell *>().isEmpty());
  QVERIFY(window->findChildren<NavRail *>().isEmpty());
  QVERIFY(!Access::statusBar(*window)->isHidden());
  QVERIFY(!child<QGroupBox>(*window, QStringLiteral("groupBox"))->isHidden());
  // the log is folded: the window is the size of its controls and the user does not change it
  QCOMPARE(window->minimumSize(), window->maximumSize());
  QVERIFY(window->minimumWidth() != MainShell::kMinimumWindowWidth);
}

void MainShellTests::deskflowLook_hasTheShell_hidesTheStatusBar_andCanBeResizedFrom720()
{
  auto window = makeWindow(true);
  auto *shell = Access::shell(*window);
  QVERIFY(shell);

  QVERIFY(Access::statusBar(*window)->isHidden());
  QVERIFY(child<QGroupBox>(*window, QStringLiteral("groupBox"))->isHidden());
  QCOMPARE(window->minimumWidth(), 720);
  QVERIFY(window->minimumSize() != window->maximumSize());
  QVERIFY(window->width() >= 1000);

  showWindow(*window);
  window->resize(720, 640);
  QTRY_COMPARE(window->width(), 720);
  // the window gives the layout what it asks for at this width: nothing is cut off on the sides
  QVERIFY(window->minimumSizeHint().width() <= 720);
  const auto bounds = window->rect();
  for (const QWidget *widget :
       {shell->widget(), static_cast<QWidget *>(shell->hero()), static_cast<QWidget *>(shell->modeControl()),
        static_cast<QWidget *>(shell->rail())}) {
    QVERIFY2(bounds.contains(rectIn(*window, widget)), qPrintable(placed(*window, widget)));
  }
}

void MainShellTests::deskflowLook_theModeIsOneChoiceInTheControlAndInTheRadioButtons()
{
  auto window = makeWindow(true);
  auto *shell = Access::shell(*window);
  auto *server = child<QRadioButton>(*window, QStringLiteral("rbModeServer"));
  auto *client = child<QRadioButton>(*window, QStringLiteral("rbModeClient"));
  showWindow(*window);

  QVERIFY(server->isChecked());
  QCOMPARE(shell->modeControl()->current(), 0);
  QCOMPARE(shell->mode(), MainShell::Mode::Server);

  QTest::mouseClick(shell->modeControl()->segment(1), Qt::LeftButton);
  QVERIFY(client->isChecked());
  QVERIFY(!server->isChecked());
  QCOMPARE(shell->modeControl()->current(), 1);
  QCOMPARE(shell->mode(), MainShell::Mode::Client);
  QCOMPARE(Settings::value(Settings::Core::CoreMode).value<Settings::CoreMode>(), Settings::CoreMode::Client);

  // what the window itself says (a setting, the other radio button) is followed by the control
  server->setChecked(true);
  QCOMPARE(shell->modeControl()->current(), 0);
  QCOMPARE(shell->mode(), MainShell::Mode::Server);
}

void MainShellTests::deskflowLook_theHeroSaysTheStateOfTheCoreAndTheStatusBar()
{
  auto window = makeWindow(true);
  auto *shell = Access::shell(*window);
  showWindow(*window);
  QCOMPARE(shell->hero()->headline(), QStringLiteral("Not sharing"));

  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  QCOMPARE(shell->hero()->headline(), QStringLiteral("Waiting for clients"));
  QCOMPARE(shell->hero()->kind(), StatusChip::Kind::Info);
  // the sentence under the headline is the one of the status bar that the window keeps (it comes after the queued
  // update)
  QTRY_COMPARE(shell->hero()->detail(), Access::statusBar(*window)->statusText());
  QVERIFY(!shell->hero()->detail().isEmpty());

  Q_EMIT Access::core(*window).connectedClientsChanged({QStringLiteral("PC")});
  Access::states(*window, ProcessState::Started, ConnectionState::Connected);
  QCOMPARE(shell->hero()->headline(), QStringLiteral("Sharing with 1 computer"));
  QCOMPARE(shell->hero()->kind(), StatusChip::Kind::Success);

  Q_EMIT Access::core(*window).connectedClientsChanged({QStringLiteral("PC"), QStringLiteral("MINI")});
  QCOMPARE(shell->hero()->headline(), QStringLiteral("Sharing with 2 computers"));

  // an error is told until the core starts again
  shell->status()->setError(QStringLiteral("Cannot start"), QStringLiteral("what to check"));
  QCOMPARE(shell->hero()->headline(), QStringLiteral("Cannot start"));
  QCOMPARE(shell->hero()->kind(), StatusChip::Kind::Danger);
  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  QCOMPARE(shell->hero()->headline(), QStringLiteral("Starting…"));

  // the security chip is the one of the status bar, with its text
  Access::states(*window, ProcessState::Stopped, ConnectionState::Disconnected);
  QVERIFY2(
      shell->hero()->headline() == QStringLiteral("Not sharing"),
      qPrintable(shell->hero()->headline() + QStringLiteral(" / model: ") + shell->status()->view().headline)
  );
}

void MainShellTests::deskflowLook_theClientConnectionKeepsItsControlsWorking_andHasPlacesForMore()
{
  auto window = makeWindow(true, Settings::CoreMode::Client);
  auto *shell = Access::shell(*window);
  auto *section = shell->clientSection();
  auto *hostname = child<QComboBox>(*window, QStringLiteral("cbHostname"));
  auto *test = child<QPushButton>(*window, QStringLiteral("btnTestConnection"));
  showWindow(*window);

  QCOMPARE(shell->mode(), MainShell::Mode::Client);
  QVERIFY(section->isVisibleTo(shell->widget()));
  QCOMPARE(section->card()->title(), QStringLiteral("Server"));

  // the address is the field that it always was, and the Test connection button follows it
  QCOMPARE(section->addressStack()->currentWidget(), hostname);
  QVERIFY(!test->isEnabled());
  hostname->lineEdit()->setText(QStringLiteral("192.168.0.2"));
  QVERIFY(test->isEnabled());
  QVERIFY(section->isAncestorOf(hostname));
  QVERIFY(section->isAncestorOf(test));

  // the places for what comes later: a line that stands in for the address, and lines below it
  QCOMPARE(section->statusLineCount(), 0);
  auto *line = new QLabel(QStringLiteral("a line"));
  section->addStatusLine(line);
  QCOMPARE(section->statusLineCount(), 1);
  QVERIFY(line->isVisibleTo(section));
  section->removeStatusLine(line);
  QCOMPARE(section->statusLineCount(), 0);
  delete line;

  auto *fixed = new QLabel(QStringLiteral("an address that cannot be typed in"));
  const int index = section->addressStack()->addWidget(fixed);
  section->addressStack()->setCurrentIndex(index);
  QVERIFY(fixed->isVisibleTo(section));
  QVERIFY(hostname->isHidden());
  section->addressStack()->setCurrentIndex(0);
  QVERIFY(!hostname->isHidden());
  QVERIFY(fixed->isHidden());
}

void MainShellTests::deskflowLook_theComputersAreThoseOfTheConfig_andTheConnectedOnesSayConnected()
{
  writeComputers();
  auto window = makeWindow(true);
  auto *shell = Access::shell(*window);
  showWindow(*window);

  const auto rows = [&] {
    // the rows that were replaced are deleted when the event loop gets to them
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    return shell->computersCard()->findChildren<SettingRow *>(QStringLiteral("computerRow"));
  };
  QCOMPARE(rows().size(), 2);
  QStringList names;
  for (const auto *row : rows())
    names << row->title();
  names.sort();
  QCOMPARE(names, (QStringList{QStringLiteral("MINI"), QStringLiteral("PC")}));
  for (const auto *row : rows())
    QCOMPARE(row->findChild<StatusChip *>()->text(), QStringLiteral("Not connected"));

  // the computers that are connected are told while the core runs as a server
  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  Q_EMIT Access::core(*window).connectedClientsChanged({QStringLiteral("PC")});
  QStringList said;
  for (const auto *row : rows())
    said << QStringLiteral("%1: %2").arg(row->title(), row->findChild<StatusChip *>()->text());
  said.sort();
  QCOMPARE(said, (QStringList{QStringLiteral("MINI: Not connected"), QStringLiteral("PC: Connected")}));

  // no connection once the core stopped
  Access::states(*window, ProcessState::Started, ConnectionState::Connected);
  Access::states(*window, ProcessState::Stopped, ConnectionState::Disconnected);
  for (const auto *row : rows())
    QCOMPARE(row->findChild<StatusChip *>()->text(), QStringLiteral("Not connected"));
}

void MainShellTests::deskflowLook_theSidebarIsRemembered_andANarrowWindowDoesNotChangeIt()
{
  {
    auto window = makeWindow(true);
    auto *sidebar = Access::shell(*window)->sidebar();
    showWindow(*window);
    window->resize(1100, 700);
    QTRY_COMPARE(window->width(), 1100);
    QVERIFY(!sidebar->isCollapsed());
    QVERIFY(sidebar->isRailExpanded());
    QVERIFY(!Settings::value(Settings::Gui::SidebarCollapsed).toBool());

    // the toggle of the sidebar is the one that the user presses
    QTest::mouseClick(Access::shell(*window)->rail()->toggleButton(), Qt::LeftButton);
    QVERIFY(sidebar->isCollapsed());
    QVERIFY(Settings::value(Settings::Gui::SidebarCollapsed).toBool());
  }
  {
    // the next window starts as the user left it
    auto window = makeWindow(true);
    auto *sidebar = Access::shell(*window)->sidebar();
    showWindow(*window);
    window->resize(1100, 700);
    QTRY_COMPARE(window->width(), 1100);
    QVERIFY(sidebar->isCollapsed());
    QVERIFY(!sidebar->isRailExpanded());
    Settings::setValue(Settings::Gui::SidebarCollapsed, false);

    // a narrow window folds the sidebar to the rail without changing what the user chose
    sidebar->setCollapsed(false);
    window->resize(780, 700);
    QTRY_VERIFY(sidebar->isNarrow());
    QVERIFY(!sidebar->isRailExpanded());
    QVERIFY(!Settings::value(Settings::Gui::SidebarCollapsed).toBool());
    window->resize(1100, 700);
    QTRY_VERIFY(!sidebar->isNarrow());
    QVERIFY(sidebar->isRailExpanded());
    QVERIFY(!Settings::value(Settings::Gui::SidebarCollapsed).toBool());
  }
}

void MainShellTests::deskflowLook_theLogIsFoldedUntilTheUserOrAnErrorUnfoldsIt()
{
  auto window = makeWindow(true);
  auto *dock = Access::logDock(*window);
  // the setting of the system look is true by default; the shell has a key of its own
  QVERIFY(Settings::value(Settings::Gui::LogExpanded).toBool());
  QVERIFY(dock->isHidden());
  showWindow(*window);
  QVERIFY(!dock->isVisible());

  // a line of the log that is not an error leaves it folded
  Q_EMIT Access::core(*window).logLine(QStringLiteral("[2026-11-17T10:58:01] NOTE: started server"));
  QVERIFY(!dock->isVisible());

  // an error unfolds it, and this is not what the user chose
  Q_EMIT Access::core(*window).logLine(QStringLiteral("[2026-11-17T10:58:02] ERROR: cannot bind"));
  QVERIFY(dock->isVisible());
  QVERIFY(!Settings::value(Settings::Gui::ShellLogExpanded).toBool());

  // once for each run of the core: the user who folded it again is not forced to see the next lines
  dock->toggleViewAction()->trigger();
  QVERIFY(!dock->isVisible());
  Q_EMIT Access::core(*window).logLine(QStringLiteral("[2026-11-17T10:58:03] ERROR: again"));
  QVERIFY(!dock->isVisible());
  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  QVERIFY(!Access::errorShownThisRun(*window));
  Q_EMIT Access::core(*window).logLine(QStringLiteral("[2026-11-17T10:58:04] FATAL: next run"));
  QVERIFY2(
      dock->isVisible(), qPrintable(QStringLiteral("dock hidden %1, window visible %2, error run %3")
                                        .arg(dock->isHidden())
                                        .arg(window->isVisible())
                                        .arg(Access::errorShownThisRun(*window)))
  );
  QVERIFY(!Settings::value(Settings::Gui::ShellLogExpanded).toBool());

  // what the user unfolds is remembered
  dock->toggleViewAction()->trigger();
  QVERIFY(!dock->isVisible());
  dock->toggleViewAction()->trigger();
  QVERIFY(dock->isVisible());
  QVERIFY(Settings::value(Settings::Gui::ShellLogExpanded).toBool());
  // the entry of the sidebar shows it
  QVERIFY(Access::shell(*window)->rail()->item(QStringLiteral("log"))->isChecked());
}

void MainShellTests::deskflowLook_theNoticeOfTheLayoutMemoryStaysAboveThePage()
{
  auto window = makeWindow(true);
  auto *shell = Access::shell(*window);
  auto *bar = Access::noticeBar(*window);
  showWindow(*window);
  QVERIFY(bar->isHidden());
  const auto heroTop = rectIn(*window, shell->hero()).top();

  bar->setText(QStringLiteral("Layout restored"), true);
  QCoreApplication::processEvents();
  QVERIFY(!bar->isHidden());
  QVERIFY(bar->isVisibleTo(window.get()));
  QVERIFY(rectIn(*window, bar).bottom() < rectIn(*window, shell->hero()).top());
  QVERIFY(rectIn(*window, shell->hero()).top() > heroTop);
  // the window has no fixed size to adjust: it stays resizable
  QVERIFY(window->minimumSize() != window->maximumSize());

  QSignalSpy undo(bar, &LayoutNoticeBar::undoRequested);
  QSignalSpy dismiss(bar, &LayoutNoticeBar::dismissRequested);
  auto *undoButton = child<QPushButton>(*bar, QStringLiteral("btnLayoutNoticeUndo"));
  auto *dismissButton = child<QPushButton>(*bar, QStringLiteral("btnLayoutNoticeDismiss"));
  QVERIFY(!undoButton->isHidden());
  QVERIFY(!dismissButton->isHidden());
  QTest::mouseClick(undoButton, Qt::LeftButton);
  QCOMPARE(undo.count(), 1);
  QTest::mouseClick(dismissButton, Qt::LeftButton);
  QCOMPARE(dismiss.count(), 1);

  bar->setNotice(std::nullopt);
  QCoreApplication::processEvents();
  QVERIFY(bar->isHidden());
  QCOMPARE(rectIn(*window, shell->hero()).top(), heroTop);
}

void MainShellTests::deskflowLook_theWindowHasTheNamesAndTheRolesOfTheShell()
{
  writeComputers();
  auto window = makeWindow(true);
  showWindow(*window);
  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  QCoreApplication::processEvents();

  QStringList lines;
  dumpTree(QAccessible::queryAccessibleInterface(window.get()), 0, lines);
  const auto dump = lines.join(QLatin1Char('\n'));

  // the sidebar is a list of entries, the page that is shown is the selected one, the toggle says what it would do
  QVERIFY2(dump.contains(QStringLiteral("List ")), qPrintable(dump));
  QVERIFY2(dump.contains(QStringLiteral("Button \"Collapse sidebar\" [expanded,focusable]")), qPrintable(dump));
  QVERIFY2(
      dump.contains(QRegularExpression(QStringLiteral("ListItem \"Computers\" \\[[a-z,]*selected"))), qPrintable(dump)
  );
  QVERIFY2(dump.contains(QRegularExpression(QStringLiteral("ListItem \"General\""))), qPrintable(dump));
  QVERIFY2(dump.contains(QStringLiteral("ListItem \"Security & clipboard\"")), qPrintable(dump));
  QVERIFY2(dump.contains(QStringLiteral("Heading \"Computers\"")), qPrintable(dump));
  QVERIFY2(dump.contains(QStringLiteral("ListItem \"Log\" [unchecked,focusable]")), qPrintable(dump));
  // the mode is a group of two choices, one of them chosen
  QVERIFY2(dump.contains(QStringLiteral("Grouping \"Mode\"")), qPrintable(dump));
  QVERIFY2(
      dump.contains(QRegularExpression(QStringLiteral("RadioButton \"Share this computer\" \\[[a-z,]*checked"))),
      qPrintable(dump)
  );
  QVERIFY2(dump.contains(QStringLiteral("RadioButton \"Use another computer\"")), qPrintable(dump));
  // the state, and the computers
  QVERIFY2(dump.contains(QStringLiteral("Grouping \"Waiting for clients\"")), qPrintable(dump));
  QVERIFY2(dump.contains(QStringLiteral("Grouping \"PC\"")), qPrintable(dump));
  QVERIFY2(dump.contains(QStringLiteral("StaticText \"Not connected\"")), qPrintable(dump));

  // the state of the toggle follows the collapse
  auto *shell = Access::shell(*window);
  shell->sidebar()->setCollapsed(true);
  QTRY_VERIFY(!shell->rail()->isExpanded());
  lines.clear();
  dumpTree(QAccessible::queryAccessibleInterface(window.get()), 0, lines);
  const auto collapsed = lines.join(QLatin1Char('\n'));
  QVERIFY2(
      collapsed.contains(QStringLiteral("Button \"Expand sidebar\" [collapsed,focusable]")), qPrintable(collapsed)
  );
  // the button that has only an icon has its name
  QVERIFY2(!collapsed.contains(QStringLiteral("Button \"\"")), qPrintable(collapsed));
  // the rail that shows only icons is still named in words
  QVERIFY2(collapsed.contains(QRegularExpression(QStringLiteral("ListItem \"General\""))), qPrintable(collapsed));

  // the walk is written for whoever wants to read it (the report of the step), when the environment says where
  if (const auto path = qEnvironmentVariable("DESKFLOW_A11Y_DUMP"); !path.isEmpty()) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      file.write("expanded sidebar:\n" + dump.toUtf8() + "\n\ncollapsed sidebar:\n" + collapsed.toUtf8() + '\n');
    }
  }
}

void MainShellTests::deskflowLook_theSettingsArePagesOfTheSidebar_andPreferencesOpensTheFirst()
{
  auto window = makeWindow(true);
  auto *shell = Access::shell(*window);
  showWindow(*window);
  auto *rail = shell->rail();

  // the pages of the settings are entries of the sidebar under "Settings", in this order, before Log and About; the
  // entry that opened the dialog is not shown any more
  QStringList order;
  for (auto *item : rail->items()) {
    if (!item->isHidden() && item->kind() != NavItem::Kind::Status && item->id() != QLatin1String("fingerprint") &&
        item->id() != QLatin1String("update"))
      order << item->id();
  }
  QCOMPARE(
      order,
      (QStringList{
          QStringLiteral("home"), QStringLiteral("settings.security"), QStringLiteral("settings.display"),
          QStringLiteral("settings.keyboard"), QStringLiteral("settings.network"), QStringLiteral("settings.general"),
          QStringLiteral("settings.advanced"), QStringLiteral("log"), QStringLiteral("about")
      })
  );
  QVERIFY(rail->item(QStringLiteral("preferences"))->isHidden());

  // a page that is chosen is the one that is shown, and the entry is the one that is marked
  QCOMPARE(shell->currentPageId(), QStringLiteral("home"));
  QTest::mouseClick(rail->item(QStringLiteral("settings.network")), Qt::LeftButton);
  QCOMPARE(shell->currentPageId(), QStringLiteral("settings.network"));
  auto *page = Access::settingsPages(*window)->page(QStringLiteral("settings.network"));
  QCOMPARE(shell->pages()->currentWidget(), page);
  QVERIFY(page->isVisible());
  // the title of the page is a heading, and is what the page is called in the sidebar
  QCOMPARE(page->title(), rail->item(QStringLiteral("settings.network"))->text());

  // Preferences (the menu, Ctrl+,) goes to the first page of the general settings
  QTest::mouseClick(rail->item(QStringLiteral("home")), Qt::LeftButton);
  QCOMPARE(shell->currentPageId(), QStringLiteral("home"));
  Access::settingsAction(*window)->trigger();
  QCOMPARE(shell->currentPageId(), QStringLiteral("settings.general"));
  QCOMPARE(shell->pages()->currentWidget(), Access::settingsPages(*window)->page(QStringLiteral("settings.general")));
  Access::openSettings(*window);
  QCOMPARE(shell->currentPageId(), QStringLiteral("settings.general"));

  // the window of the system look has the dialog still
  window.reset();
  auto system = makeWindow(false);
  QVERIFY(!Access::settingsPages(*system));
}

void MainShellTests::deskflowLook_aChangeThatTheCoreReadsAsksForARestart_andOnlyWhileItRuns()
{
  auto window = makeWindow(true);
  auto *shell = Access::shell(*window);
  auto *banner = shell->restartBanner();
  auto *pages = Access::settingsPages(*window);
  showWindow(*window);
  auto *port = pages->page(QStringLiteral("settings.network"))->findChild<QSpinBox *>(QStringLiteral("sbPort"));
  auto *autoHide =
      pages->page(QStringLiteral("settings.general"))->findChild<QComboBox *>(QStringLiteral("comboAutoHide"));
  QVERIFY(port && autoHide);
  QVERIFY(banner->isHidden());

  // the core is stopped: it reads the settings when it starts, nothing is to be restarted
  port->setValue(port->value() + 1);
  QVERIFY(banner->isHidden());

  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  // a setting that the window follows by itself asks for nothing
  autoHide->setCurrentIndex(autoHide->currentIndex() == 0 ? 1 : 0);
  Q_EMIT autoHide->activated(autoHide->currentIndex());
  QVERIFY(banner->isHidden());
  // a setting that the core reads asks for the restart, and the banner is above the pages
  port->setValue(port->value() + 1);
  QVERIFY(!banner->isHidden());
  QVERIFY(banner->isVisibleTo(window.get()));
  QVERIFY(rectIn(*window, banner).bottom() < rectIn(*window, shell->pages()).top());
  QCOMPARE(banner->button()->text(), QStringLiteral("Restart sharing"));

  // a core that stops has nothing to restart
  Access::states(*window, ProcessState::Stopped, ConnectionState::Disconnected);
  QVERIFY(banner->isHidden());
  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  QVERIFY(banner->isHidden());
  port->setValue(port->value() + 1);
  QVERIFY(!banner->isHidden());
  // and so has a core that starts again (it reads the settings as they are)
  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  QVERIFY(banner->isHidden());
  Access::states(*window, ProcessState::Stopped, ConnectionState::Disconnected);

  // the title of the window follows a setting that is changed on a page, at once
  auto *version =
      pages->page(QStringLiteral("settings.general"))->findChild<QCheckBox *>(QStringLiteral("cbShowVersion"));
  QVERIFY(version);
  version->setChecked(true);
  QVERIFY(window->windowTitle().contains(QLatin1Char('-')));
  version->setChecked(false);
  QVERIFY(!window->windowTitle().contains(QLatin1Char('-')));
}

void MainShellTests::deskflowLook_aChangeOfLanguageKeepsTheShell()
{
  writeComputers();
  auto window = makeWindow(true);
  auto *shell = Access::shell(*window);
  showWindow(*window);

  QEvent change(QEvent::LanguageChange);
  QCoreApplication::sendEvent(window.get(), &change);
  QCoreApplication::processEvents();

  QCOMPARE(Access::shell(*window), shell);
  // the entries of the settings are named again too
  QCOMPARE(shell->rail()->item(QStringLiteral("settings.general"))->text(), QStringLiteral("General"));
  QVERIFY(!shell->rail()->item(QStringLiteral("preferences"))->isVisibleTo(shell->rail()));
  QCOMPARE(shell->rail()->item(QStringLiteral("about"))->text().isEmpty(), false);
  QCOMPARE(shell->modeControl()->segmentText(0), QStringLiteral("Share this computer"));
  QCOMPARE(shell->computersCard()->findChildren<SettingRow *>(QStringLiteral("computerRow")).size(), 2);
}

// The settings of the client are a page, and the sidebar has no entry for it: one more entry makes the window higher
// than a small screen has the room for (deskflowLook_canBeLowEnoughForASmallScreen). It is reached from the button of
// the client on the home page and from a row on the page of the network, and the entry of the network is the one marked
void MainShellTests::deskflowLook_theClientSettings_haveNoEntryOfTheirOwn_andAreReachedFromHomeAndNetwork()
{
  auto window = makeWindow(true);
  auto *shell = Access::shell(*window);
  showWindow(*window);
  auto *rail = shell->rail();
  const auto client = QStringLiteral("settings.client");
  const auto network = QStringLiteral("settings.network");
  auto *pages = Access::settingsPages(*window);
  QVERIFY(rail->item(client) == nullptr);
  QVERIFY(pages->page(client) != nullptr);

  // the button of the client on the home page
  window->findChild<QPushButton *>(QStringLiteral("btnConfigureClient"))->click();
  QCOMPARE(shell->currentPageId(), client);
  QCOMPARE(shell->pages()->currentWidget(), pages->page(client));
  QVERIFY(pages->page(client)->isVisible());
  QCOMPARE(rail->currentPage(), network);
  QVERIFY(rail->item(network)->isCurrent());

  // the entry of the network that is marked leads to the page of the network, and the page leads on with its row
  QTest::mouseClick(rail->item(network), Qt::LeftButton);
  QCOMPARE(shell->currentPageId(), network);
  QCOMPARE(shell->pages()->currentWidget(), pages->page(network));
  auto *open = pages->page(network)->findChild<QPushButton *>(QStringLiteral("btnOpenClientSettings"));
  QVERIFY(open != nullptr);
  QVERIFY(open->isVisibleTo(pages->page(network)));
  open->click();
  QCOMPARE(shell->currentPageId(), client);
  QCOMPARE(shell->pages()->currentWidget(), pages->page(client));

  // home is home again
  QTest::mouseClick(rail->item(QStringLiteral("home")), Qt::LeftButton);
  QCOMPARE(shell->currentPageId(), QStringLiteral("home"));
  QCOMPARE(shell->pages()->currentIndex(), 0);
}

void MainShellTests::deskflowLook_theViewMenuFoldsTheSidebar_withAShortcutThatNothingElseHas()
{
  auto window = makeWindow(true);
  auto *sidebar = Access::shell(*window)->sidebar();
  auto *action = Access::toggleSidebarAction(*window);
  QVERIFY(action);
  showWindow(*window);
  window->resize(1100, 700);
  QTRY_COMPARE(window->width(), 1100);

  // in the View menu, with the key that the platform gives a sidebar
  QVERIFY(window->menuBar()->actions().size() >= 3);
  bool inViewMenu = false;
  for (auto *menuAction : window->menuBar()->actions()) {
    if (menuAction->menu() && menuAction->menu()->actions().contains(action))
      inViewMenu = true;
  }
  QVERIFY(inViewMenu);
#ifdef Q_OS_MACOS
  QCOMPARE(action->shortcut(), QKeySequence(Qt::CTRL | Qt::META | Qt::Key_S));
#else
  QCOMPARE(action->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_B));
#endif
  // none of the other actions of the window has the same key (Start and Restart share Ctrl+S, which is a finding of
  // its own: only one of them is shown at a time)
  for (auto *other : window->findChildren<QAction *>()) {
    if (other != action && !other->shortcut().isEmpty())
      QVERIFY2(other->shortcut() != action->shortcut(), qPrintable(other->text()));
  }

  QCOMPARE(action->text(), QStringLiteral("Collapse sidebar"));
  action->trigger();
  QVERIFY(sidebar->isCollapsed());
  QCOMPARE(action->text(), QStringLiteral("Expand sidebar"));
  QVERIFY(Settings::value(Settings::Gui::SidebarCollapsed).toBool());
  action->trigger();
  QVERIFY(!sidebar->isCollapsed());
  QCOMPARE(action->text(), QStringLiteral("Collapse sidebar"));

  // in a narrow window the same action opens the sidebar over the page and does not change what the user chose
  window->resize(780, 700);
  QTRY_VERIFY(sidebar->isNarrow());
  action->trigger();
  QVERIFY(sidebar->isOverlayOpen());
  QVERIFY(!Settings::value(Settings::Gui::SidebarCollapsed).toBool());
  action->trigger();
  QVERIFY(!sidebar->isOverlayOpen());
}

void MainShellTests::deskflowLook_fitsAtEveryWidth_withLargerTextAndLongKoreanStrings_data()
{
  QTest::addColumn<int>("width");
  QTest::addColumn<qreal>("fontScale");
  QTest::addColumn<int>("basePixels");
  QTest::addColumn<int>("spacing");
  QTest::addColumn<bool>("korean");
  QTest::addColumn<bool>("client");

  // the font of the test platform, and the body that Windows 11 has (14 px, which is the least that the new look gives
  // the text on Windows) with the steps of larger text, also with letters 30 % apart (as wide as the fonts of the PC of
  // the checks of Windows were)
  for (const int basePixels : {0, 14}) {
    for (const int spacing : basePixels ? QList<int>{100, 130} : QList<int>{100}) {
      for (const int width : {720, 760, 900, 1000, 1280}) {
        for (const qreal scale : basePixels ? QList<qreal>{1.0, 1.3, 1.5} : QList<qreal>{1.0, 1.3}) {
          for (const bool korean : {false, true}) {
            for (const bool client : {false, true}) {
              QTest::addRow(
                  "%s%s%d px, font x%.1f, %s, %s", basePixels ? "14 px body, " : "", spacing != 100 ? "wide, " : "",
                  width, scale, korean ? "ko" : "en", client ? "client" : "server"
              ) << width
                << scale << basePixels << spacing << korean << client;
            }
          }
        }
      }
    }
  }
}

void MainShellTests::deskflowLook_fitsAtEveryWidth_withLargerTextAndLongKoreanStrings()
{
  QFETCH(int, width);
  QFETCH(qreal, fontScale);
  QFETCH(int, basePixels);
  QFETCH(int, spacing);
  QFETCH(bool, korean);
  QFETCH(bool, client);

  writeComputers();
  auto window = makeWindow(true, client ? Settings::CoreMode::Client : Settings::CoreMode::Server);
  auto *shell = Access::shell(*window);

  window->setFont(scaledFont(window->font(), QString(), basePixels, fontScale, spacing));

  if (korean) {
    // the strings of the translation (ko) that the window gives the shell, and the longest of the radio buttons
    child<QRadioButton>(*window, QStringLiteral("rbModeServer"))
        ->setText(QStringLiteral("이 컴퓨터의 키보드와 마우스를 사용\n(이 컴퓨터를 서버로 설정)"));
    child<QRadioButton>(*window, QStringLiteral("rbModeClient"))
        ->setText(QStringLiteral("다른 컴퓨터의 키보드와 마우스를 사용\n(이 컴퓨터를 클라이언트로 설정)"));
    child<QPushButton>(*window, QStringLiteral("btnTestConnection"))->setText(QStringLiteral("연결 테스트"));
    MainShell::Texts texts;
    texts.preferences = QStringLiteral("환경설정");
    texts.log = QStringLiteral("로그 표시");
    texts.about = QStringLiteral("Deskflow 정보");
    texts.fingerprint = QStringLiteral("내 컴퓨터의 지문 보기");
    texts.updateAvailable = QStringLiteral("새 버전을 사용할 수 있음");
    shell->setTexts(texts);
    shell->setUpdateVersion(QStringLiteral("1.28.0"));
  }

  showWindow(*window);
  window->resize(width, 760);
  QTRY_COMPARE(window->width(), width);
  QCoreApplication::processEvents();

  // the width that the user asks for is the width that the window has (the layout needs no more than this)
  QCOMPARE(window->size().width(), width);
  const auto bounds = window->rect();
  const QList<const QWidget *> parts{shell->hero(), shell->modeControl(), shell->rail(), shell->pageTitle()};
  for (const QWidget *widget : parts) {
    QVERIFY2(!widget->isHidden(), qPrintable(widget->metaObject()->className()));
    QVERIFY2(bounds.contains(rectIn(*window, widget)), qPrintable(placed(*window, widget)));
  }
  // nothing is squeezed to less than it needs: the control of the mode, the page of the mode
  QVERIFY(shell->modeControl()->width() >= shell->modeControl()->minimumSizeHint().width());
  QWidget *modePage =
      client ? static_cast<QWidget *>(shell->clientSection()) : static_cast<QWidget *>(shell->computersCard());
  QVERIFY(modePage->isVisibleTo(window.get()));
  QVERIFY(rectIn(*window, modePage).left() >= 0 && rectIn(*window, modePage).right() <= bounds.right());
  QVERIFY(rectIn(*window, modePage).width() >= modePage->minimumSizeHint().width());
  // the hero and the mode are above each other without covering each other
  QVERIFY(
      rectIn(*window, shell->hero()).bottom() < rectIn(*window, shell->modeControl()).top() ||
      rectIn(*window, shell->modeControl()).bottom() < rectIn(*window, shell->hero()).top()
  );
  // the sidebar is folded to the rail at the narrow widths, and shows its labels from the width that was promised
  QCOMPARE(shell->sidebar()->isRailExpanded(), width >= 900);
}

void MainShellTests::deskflowLook_keepsItsMinimumWidth_withWideFonts_data()
{
  QTest::addColumn<QString>("family");
  QTest::addColumn<qreal>("fontScale");
  QTest::addColumn<int>("basePixels");
  QTest::addColumn<int>("spacing");
  QTest::addColumn<bool>("client");

  // The fonts of Windows are wider than the ones that the tests had: the layout must not need more than the width that
  // the window promises (720) whatever the font, so it is tried with a font that is wide, and with larger text. The
  // font of the test platform is small (9 px offscreen): the body of Windows 11, 14 px, is tried as well, and with
  // letters 30 % apart, which is as wide as the fonts of the PC of the checks of Windows were
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

void MainShellTests::deskflowLook_keepsItsMinimumWidth_withWideFonts()
{
  QFETCH(QString, family);
  QFETCH(qreal, fontScale);
  QFETCH(int, basePixels);
  QFETCH(int, spacing);
  QFETCH(bool, client);

  writeComputers();
  auto window = makeWindow(true, client ? Settings::CoreMode::Client : Settings::CoreMode::Server);
  auto *shell = Access::shell(*window);
  window->setFont(scaledFont(window->font(), family, basePixels, fontScale, spacing));
  showWindow(*window);

  // the promise of the window: it can be 720 wide, and the layout asks for no more
  window->resize(MainShell::kMinimumWindowWidth, 760);
  QTRY_COMPARE(window->width(), MainShell::kMinimumWindowWidth);
  QVERIFY2(window->minimumSizeHint().width() <= MainShell::kMinimumWindowWidth, qPrintable(widestMinimum(*window)));
  for (const int width : {MainShell::kMinimumWindowWidth, 760, 800, 850, 900, 1000}) {
    window->resize(width, 760);
    QTRY_COMPARE(window->width(), width);
    QCoreApplication::processEvents();
    // nothing is squeezed to less than it needs (the state has its buttons beside its text only when both fit), and
    // nothing leaves the window
    QVERIFY2(
        shell->hero()->contentsRect().width() >= shell->hero()->layout()->minimumSize().width(),
        qPrintable(QStringLiteral("%1: hero %2 of %3")
                       .arg(width)
                       .arg(shell->hero()->contentsRect().width())
                       .arg(shell->hero()->layout()->minimumSize().width()))
    );
    QVERIFY2(
        shell->modeControl()->width() >= shell->modeControl()->minimumSizeHint().width(),
        qPrintable(QStringLiteral("%1: mode control %2 of %3")
                       .arg(width)
                       .arg(shell->modeControl()->width())
                       .arg(shell->modeControl()->minimumSizeHint().width()))
    );
    const auto bounds = window->rect();
    for (const QWidget *widget :
         {static_cast<QWidget *>(shell->hero()), static_cast<QWidget *>(shell->modeControl()),
          static_cast<QWidget *>(shell->rail()), static_cast<QWidget *>(shell->pageTitle())}) {
      QVERIFY2(bounds.contains(rectIn(*window, widget)), qPrintable(placed(*window, widget)));
    }
  }
}

void MainShellTests::deskflowLook_aClientPageIsCalledConnection_andNeverServer()
{
  auto window = makeWindow(true, Settings::CoreMode::Client);
  auto *shell = Access::shell(*window);
  showWindow(*window);

  // the person chose to use another computer: the page is about the connection to it, and does not call this computer
  // (or the other one) a server
  QCOMPARE(shell->mode(), MainShell::Mode::Client);
  QCOMPARE(shell->pageTitle()->text(), QStringLiteral("Connection"));
  QCOMPARE(shell->rail()->item(QStringLiteral("home"))->text(), QStringLiteral("Connection"));
  // and the card does not repeat the title of the page
  QVERIFY(shell->clientSection()->card()->title() != shell->pageTitle()->text());

  auto *radio = child<QRadioButton>(*window, QStringLiteral("rbModeServer"));
  radio->setChecked(true);
  QCOMPARE(shell->pageTitle()->text(), QStringLiteral("Computers"));
}

void MainShellTests::deskflowLook_theHintOfTheAddressIsInItsRow_andNotAlsoInTheField()
{
  auto window = makeWindow(true, Settings::CoreMode::Client);
  auto *shell = Access::shell(*window);
  auto *edit = child<QComboBox>(*window, QStringLiteral("cbHostname"))->lineEdit();
  showWindow(*window);

  // the row says what to write under its title; the field is empty and has no second copy of the same long sentence
  const auto hint = shell->clientSection()->addressRow()->description();
  QVERIFY(!hint.isEmpty());
  QVERIFY2(edit->placeholderText() != hint, qPrintable(hint));
  QVERIFY(edit->placeholderText().isEmpty());

  // a change of language gives the hint again, once
  QEvent languageChange(QEvent::LanguageChange);
  QCoreApplication::sendEvent(window.get(), &languageChange);
  QCOMPARE(shell->clientSection()->addressRow()->description(), hint);
  QVERIFY(edit->placeholderText().isEmpty());
}

void MainShellTests::deskflowLook_everyButtonHasAName_andNoGroupIsRepeatedByItsOwnText()
{
  for (const auto mode : {Settings::CoreMode::Server, Settings::CoreMode::Client}) {
    writeComputers();
    auto window = makeWindow(true, mode);
    showWindow(*window);
    Access::states(*window, ProcessState::Stopped, ConnectionState::Disconnected);
    QCoreApplication::processEvents();

    QStringList lines;
    dumpTree(QAccessible::queryAccessibleInterface(window.get()), 0, lines);
    const auto dump = lines.join(QLatin1Char('\n'));
    // the pencil of the name of this computer (a button of an icon alone) is named by its tooltip like the others
    QVERIFY2(!dump.contains(QStringLiteral("Button \"\"")), qPrintable(dump));
    QVERIFY2(dump.contains(QStringLiteral("Button \"Edit Computer Name\"")), qPrintable(dump));

    // a group that is named by a text does not have the same text again below it: a screen reader that walks the tree
    // would say it twice
    std::function<QStringList(QAccessibleInterface *)> repeated = [&](QAccessibleInterface *node) {
      QStringList found;
      if (node->state().invisible)
        return found;
      const auto name = node->text(QAccessible::Name);
      // the texts that are below the node without a container of their own that says something
      std::function<void(QAccessibleInterface *)> below = [&](QAccessibleInterface *parent) {
        for (int i = 0; i < parent->childCount(); ++i) {
          auto *child = parent->child(i);
          if (!child || child->state().invisible)
            continue;
          if (child->role() == QAccessible::StaticText && !name.isEmpty() && child->text(QAccessible::Name) == name &&
              node->role() != QAccessible::StaticText)
            found << QStringLiteral("%1 \"%2\" says it again in %3")
                         .arg(roleName(node->role()), name, roleName(child->role()));
          else if (child->role() == QAccessible::Client && child->text(QAccessible::Name).isEmpty())
            below(child);
        }
      };
      below(node);
      for (int i = 0; i < node->childCount(); ++i) {
        if (auto *child = node->child(i))
          found << repeated(child);
      }
      return found;
    };
    const auto again = repeated(QAccessible::queryAccessibleInterface(window.get()));
    QVERIFY2(again.isEmpty(), qPrintable(again.join(QLatin1Char('\n')) + QLatin1Char('\n') + dump));
  }
}

void MainShellTests::deskflowLook_startsAtItsOwnSize_whateverTheSystemLookSaved()
{
  // the window of the system look is small and has a size of its own: it saved that, and the first window of the new
  // look is not that size
  Settings::setValue(Settings::Gui::WindowGeometry, QRect(100, 100, 572, 511));
  auto window = makeWindow(true);
  QVERIFY2(
      window->width() >= 1000 && window->height() >= 620,
      qPrintable(QStringLiteral("%1x%2").arg(window->width()).arg(window->height()))
  );

  // what the new look saves is its own, and the next window of the new look has it; the system look keeps its own
  Settings::setValue(Settings::Gui::CloseReminder, false);
  showWindow(*window);
  window->resize(900, 700);
  QTRY_COMPARE(window->size(), QSize(900, 700));
  QVERIFY(Access::closeAsTheOsDoes(*window));
  QCOMPARE(Settings::value(Settings::Gui::ShellWindowGeometry).toRect().size(), QSize(900, 700));
  QCOMPARE(Settings::value(Settings::Gui::WindowGeometry).toRect(), QRect(100, 100, 572, 511));
  window.reset();
  auto again = makeWindow(true);
  QCOMPARE(again->size(), QSize(900, 700));
}

void MainShellTests::deskflowLook_ifThePlatformCannotGiveTheFrame_theWindowKeepsTheFrameOfTheSystem()
{
  // The frame of the new look is the default, and on a Mac nobody has seen it made. When the platform cannot give it
  // (the content cannot go under the title bar), the window has to be what it was: the frame and the menu bar of the
  // system, and the shell in it, whole. The environment asks for the frame where the platform of the test would not
  qputenv("DESKFLOW_WINDOW_FRAME", "client");
  const auto unset = qScopeGuard([] { qunsetenv("DESKFLOW_WINDOW_FRAME"); });
  const auto reset = qScopeGuard([] { chrome::WindowChrome::s_failsToApply = false; });

  // as a control: the platform gives the frame, and the window has the header, the chrome and no menu bar (not a Mac)
  {
    auto window = makeWindow(true);
    QVERIFY(Access::frame(*window).clientSide());
    QVERIFY(Access::header(*window));
    QVERIFY(Access::chrome(*window));
    QVERIFY(Access::chrome(*window)->isApplied());
    QCOMPARE(window->findChildren<chrome::WindowHeader *>().size(), 1);
    if (!deskflow::platform::isMac())
      QVERIFY(window->menuBar()->isHidden());
  }

  chrome::WindowChrome::s_failsToApply = true;
  auto window = makeWindow(true);
  const auto frame = Access::frame(*window);
  QVERIFY(!frame.clientSide());
  QCOMPARE(frame.kind, chrome::FrameKind::Native);
  QCOMPARE(frame.reason, chrome::FrameReason::PlatformPluginUnsupported);
  // nothing of the frame of the new look is left: no header, no object that does what the header asks, no flag
  QVERIFY(!Access::header(*window));
  QVERIFY(!Access::chrome(*window));
  QVERIFY(window->findChildren<chrome::WindowHeader *>().isEmpty());
  QVERIFY(window->findChildren<chrome::WindowChrome *>().isEmpty());
  QVERIFY(!window->windowFlags().testFlag(Qt::FramelessWindowHint));
  QVERIFY(!window->windowFlags().testFlag(Qt::ExpandedClientAreaHint));
  QVERIFY(!window->windowFlags().testFlag(Qt::NoTitleBarBackgroundHint));
  // the menus that the header would have are the window's own: the menu bar is there, with the menus
  QVERIFY(!window->menuBar()->isHidden());
  QVERIFY(!window->menuBar()->actions().isEmpty());
  // and the shell works: the sidebar and its pages, in a window that has its size
  showWindow(*window);
  QVERIFY(Access::shell(*window));
  QVERIFY(Access::shell(*window)->rail()->isVisible());
  QVERIFY(window->width() >= window->minimumWidth());
}

void MainShellTests::deskflowLook_canBeLowEnoughForASmallScreen_data()
{
  QTest::addColumn<int>("basePixels");
  QTest::addColumn<int>("spacing");
  // the font of the test platform; the body that Windows 11 has (14 px) with letters as far apart as the fonts of the
  // PC of the checks of Windows were wide; and a font with lines that are higher than the ones of the platform
  QTest::newRow("the font of the platform") << 0 << 100;
  QTest::newRow("14 px body") << 14 << 100;
  QTest::newRow("14 px body, wide") << 14 << 130;
  QTest::newRow("20 px, high lines") << 20 << 100;
}

void MainShellTests::deskflowLook_canBeLowEnoughForASmallScreen()
{
  QFETCH(int, basePixels);
  QFETCH(int, spacing);

  // The window that Windows and Linux have: the header of the new look is in the place of the title bar and of the
  // menu bar (which is hidden), so the window is as high as the header and what is under it. (The window of a Mac has a
  // menu bar of its own, which is at the top of the screen and not in the window, but the platform of this test draws
  // it in the window, so what is asked of the window is the height of its content.)
  qputenv("DESKFLOW_WINDOW_FRAME", "client");
  const auto unset = qScopeGuard([] { qunsetenv("DESKFLOW_WINDOW_FRAME"); });
  auto window = makeWindow(true);
  window->setFont(scaledFont(window->font(), QString(), basePixels, 1.0, spacing));
  showWindow(*window);
  const auto *rail = Access::shell(*window)->rail();
  const int content = window->centralWidget()->minimumSizeHint().height();

  // The window can be as low as the screens of laptops have the room for: 1366x768 at 125 % has 576 logical pixels of
  // work area, and the first window keeps 16 of them from the taskbar. The height of the window is not a number for the
  // font of a platform: Windows has 551 (the header and the rail of the sidebar) and the platform of this test less
  // than that with no header, and the limit is the screen
  constexpr int kSmallWorkArea = 576;
  constexpr int kRoomFromTheTaskbar = 16;
  QVERIFY2(
      content <= kSmallWorkArea - kRoomFromTheTaskbar,
      qPrintable(QStringLiteral("the window asks for a height of %1").arg(content))
  );
  // and what asks for it is the rail of the sidebar at the height of its entries when they are the least high, under
  // the header: the page and the cards do not ask for more height than the rail has, at any font
  const int header = chrome::WindowHeader::kHeight;
  QVERIFY2(
      content <= rail->minimumSizeHint().height() + header + 16,
      qPrintable(QStringLiteral("%1 for a rail of %2").arg(content).arg(rail->minimumSizeHint().height()))
  );
  const auto least = window->minimumSizeHint().height();
  QVERIFY(least >= content);
  // and it is not made so low that the entries of the rail are on top of each other
  window->resize(window->width(), least);
  QTRY_COMPARE(window->height(), least);
  QCoreApplication::processEvents();
  QVERIFY(rail->currentItemHeight() >= NavRail::compactItemHeight());
  QList<QRect> placed;
  for (const auto *item : rail->items()) {
    if (item->isHidden())
      continue;
    const auto box = item->geometry().adjusted(NavItem::kRing, NavItem::kRing, -NavItem::kRing, -NavItem::kRing);
    QVERIFY2(
        box.height() >= NavRail::compactItemHeight() || item->kind() == NavItem::Kind::Status,
        qPrintable(item->objectName())
    );
    for (const auto &other : placed)
      QVERIFY2(!other.intersects(box), qPrintable(item->objectName() + QStringLiteral(" is on another entry")));
    placed.append(box);
    QVERIFY2(rail->rect().contains(box), qPrintable(item->objectName() + QStringLiteral(" is out of the rail")));
  }
  // with the room that it was drawn for, the entries have the height that they were drawn for
  window->resize(window->width(), 900);
  QTRY_COMPARE(window->height(), 900);
  QCOMPARE(rail->currentItemHeight(), NavItem::itemHeight());
}

void MainShellTests::deskflowLook_opensInsideTheWorkAreaOfAScreen_data()
{
  QTest::addColumn<QRect>("workArea");
  QTest::addColumn<bool>("saved");

  // the work areas of the screens that people have, in logical pixels
  for (const bool saved : {false, true}) {
    const char *how = saved ? "a kept size, " : "first, ";
    QTest::addRow("%s2304x1536 at 100 %%", how) << QRect(0, 0, 2304, 1496) << saved;
    QTest::addRow("%s1920x1080 at 150 %%", how) << QRect(0, 0, 1280, 680) << saved;
    QTest::addRow("%s1366x768 at 125 %%", how) << QRect(0, 0, 1092, 576) << saved;
    QTest::addRow("%s1280x720 at 125 %%", how) << QRect(0, 0, 1024, 530) << saved;
  }
}

void MainShellTests::deskflowLook_opensInsideTheWorkAreaOfAScreen()
{
  QFETCH(QRect, workArea);
  QFETCH(bool, saved);
  Access::setWorkArea(workArea);
  const auto restore = qScopeGuard([] { Access::setWorkArea(QRect(0, 0, 1920, 1040)); });
  // a size that was kept on a larger screen than this one is brought inside of this one
  if (saved)
    Settings::setValue(Settings::Gui::ShellWindowGeometry, QRect(0, 0, 1600, 1000));

  auto window = makeWindow(true);
  const auto least = window->minimumSizeHint();
  QVERIFY2(
      window->width() <= qMax(workArea.width(), least.width()) &&
          window->height() <= qMax(workArea.height(), least.height()),
      qPrintable(QStringLiteral("%1x%2 in %3x%4")
                     .arg(window->width())
                     .arg(window->height())
                     .arg(workArea.width())
                     .arg(workArea.height()))
  );
  QVERIFY(window->width() >= least.width() && window->height() >= least.height());
  // a screen that has the room gets the window that was asked for (1000 wide, 620 high at the least)
  if (!saved && workArea.width() >= 1100 && workArea.height() >= 700)
    QVERIFY2(
        window->width() >= 1000 && window->height() >= 620,
        qPrintable(QStringLiteral("%1x%2").arg(window->width()).arg(window->height()))
    );
  // the screen of 1366x768 at 125 %: the window is inside of it, with the rail at less than its height
  if (workArea.height() == 576)
    QVERIFY(window->height() <= 576 - 16);
}

QTEST_MAIN(MainShellTests)
