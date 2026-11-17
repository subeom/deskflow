/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MainShellTests.h"

#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/core/CoreProcess.h"
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

#include <QAccessible>
#include <QApplication>
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
#include <QSignalSpy>
#include <QStackedLayout>

#include <memory>

// What the main window keeps to itself: the tests reach it through this one class (the window names it as its friend)
struct MainWindowTestAccess
{
  using ConnectionState = deskflow::core::ConnectionState;
  using ProcessState = deskflow::core::ProcessState;

  static deskflow::gui::MainShell *shell(MainWindow &w)
  {
    return w.m_shell;
  }
  static deskflow::gui::CoreProcess &core(MainWindow &w)
  {
    return w.m_coreProcess;
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

} // namespace

void MainShellTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  // the window listens on a local socket that is named in the temporary directory: it is the one of this test
  qputenv("TMPDIR", m_dir.path().toUtf8());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
}

void MainShellTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QApplication::setFont(QFont());
  QPixmapCache::clear();
  for (const auto &key :
       {Settings::Gui::SidebarCollapsed, Settings::Gui::ShellLogExpanded, Settings::Gui::LogExpanded,
        Settings::Gui::WindowGeometry, Settings::Core::CoreMode, Settings::Core::ComputerName,
        Settings::Security::TlsEnabled, Settings::Client::RemoteHost, Settings::Client::RecentRemoteHosts,
        Settings::Gui::ShownServerFirstStartMessage, Settings::Gui::ShownFirstConnectedMessage,
        Settings::Gui::AutoStartCore})
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
  QCOMPARE(section->card()->title(), QStringLiteral("Connection"));

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
  QVERIFY2(dump.contains(QRegularExpression(QStringLiteral("ListItem \"Preferences"))), qPrintable(dump));
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
  QVERIFY2(collapsed.contains(QRegularExpression(QStringLiteral("ListItem \"Preferences"))), qPrintable(collapsed));

  // the walk is written for whoever wants to read it (the report of the step), when the environment says where
  if (const auto path = qEnvironmentVariable("DESKFLOW_A11Y_DUMP"); !path.isEmpty()) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      file.write("expanded sidebar:\n" + dump.toUtf8() + "\n\ncollapsed sidebar:\n" + collapsed.toUtf8() + '\n');
    }
  }
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
  QVERIFY(shell->rail()->item(QStringLiteral("preferences"))->text().startsWith(QStringLiteral("Preferences")));
  QCOMPARE(shell->rail()->item(QStringLiteral("about"))->text().isEmpty(), false);
  QCOMPARE(shell->modeControl()->segmentText(0), QStringLiteral("Share this computer"));
  QCOMPARE(shell->computersCard()->findChildren<SettingRow *>(QStringLiteral("computerRow")).size(), 2);
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
  QTest::addColumn<bool>("korean");
  QTest::addColumn<bool>("client");

  for (const int width : {720, 760, 900, 1000, 1280}) {
    for (const qreal scale : {1.0, 1.3}) {
      for (const bool korean : {false, true}) {
        for (const bool client : {false, true}) {
          QTest::addRow("%d px, font x%.1f, %s, %s", width, scale, korean ? "ko" : "en", client ? "client" : "server")
              << width << scale << korean << client;
        }
      }
    }
  }
}

void MainShellTests::deskflowLook_fitsAtEveryWidth_withLargerTextAndLongKoreanStrings()
{
  QFETCH(int, width);
  QFETCH(qreal, fontScale);
  QFETCH(bool, korean);
  QFETCH(bool, client);

  writeComputers();
  auto window = makeWindow(true, client ? Settings::CoreMode::Client : Settings::CoreMode::Server);
  auto *shell = Access::shell(*window);

  auto font = window->font();
  if (font.pointSizeF() > 0)
    font.setPointSizeF(font.pointSizeF() * fontScale);
  else
    font.setPixelSize(qRound(font.pixelSize() * fontScale));
  window->setFont(font);

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

QTEST_MAIN(MainShellTests)
