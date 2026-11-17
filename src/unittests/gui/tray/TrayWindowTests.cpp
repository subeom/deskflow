/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayWindowTests.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/StyleUtils.h"
#include "gui/core/CoreProcess.h"
#include "gui/shell/MainShell.h"
#include "gui/shell/StatusModel.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/tray/TrayController.h"
#include "gui/tray/TrayMenuBuilder.h"
#include "gui/widgets/StatusBar.h"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QPixmapCache>
#include <QSystemTrayIcon>
#include <QTimer>

#include <memory>

// What the main window keeps to itself: the tests reach it through this one class (the window names it as its friend)
struct MainWindowTestAccess
{
  using ConnectionState = deskflow::core::ConnectionState;
  using ProcessState = deskflow::core::ProcessState;

  static QSystemTrayIcon *trayIcon(MainWindow &w)
  {
    return w.m_trayIcon;
  }
  static void states(MainWindow &w, ProcessState process, ConnectionState connection)
  {
    w.m_coreProcess.setProcessState(process);
    w.m_coreProcess.setConnectionState(connection);
    // the window is told of the state of the process through the event loop
    QCoreApplication::processEvents();
  }
  static void hide(MainWindow &w)
  {
    w.hide();
  }
  static void showAndActivate(MainWindow &w)
  {
    w.showAndActivate();
  }
  //! What the window does when the person closes it (a close that the OS sends): true when the program goes on
  static bool closeAsTheOsDoes(MainWindow &w)
  {
    return w.maybeHideToTray();
  }
  static deskflow::gui::tray::TrayController *controller(MainWindow &w)
  {
    return w.m_trayController;
  }
  static deskflow::gui::MainShell *shell(MainWindow &w)
  {
    return w.m_shell;
  }
  static StatusBar *statusBar(MainWindow &w)
  {
    return w.m_statusBar;
  }
  static QList<QAction *> sharedActions(MainWindow &w)
  {
    return {w.m_actionStartCore, w.m_actionRestartCore, w.m_actionStopCore, w.m_actionMinimize,
            w.m_actionRestore,   w.m_actionTrayQuit,    w.m_actionQuit,     w.m_actionSettings};
  }
  //! The setting is changed in the settings of the window: it is written, and the window is told
  static void applySetting(MainWindow &w, const QString &key, const QVariant &value)
  {
    Settings::setValue(key, value);
    w.settingApplied(key);
  }
  static void setTrayAvailable(MainWindow &w, bool available)
  {
    w.m_trayAvailable = [available] { return available; };
  }
};

namespace {

using Access = MainWindowTestAccess;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;

void installDeskflowStyle()
{
  auto *style = new deskflow::gui::style::DeskflowStyle;
  QApplication::setStyle(style);
  deskflow::gui::style::Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

// A window as the application makes it: the settings are those of the test, and the core is not started
std::unique_ptr<MainWindow> makeWindow(Settings::CoreMode mode = Settings::CoreMode::Server, bool deskflowLook = false)
{
  if (deskflowLook)
    installDeskflowStyle();
  else
    QApplication::setStyle(QStringLiteral("Fusion"));
  Settings::setValue(Settings::Security::TlsEnabled, false);
  Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  Settings::setValue(Settings::Core::CoreMode, mode);
  return std::make_unique<MainWindow>();
}

// One line for each row of a menu: what a person sees (the text, the shortcut, whether the row is there and can be
// chosen), the icon that it asks for and what the OS does with it
QStringList describe(const QMenu *menu)
{
  QStringList rows;
  for (const auto *action : menu->actions()) {
    if (action->isSeparator()) {
      rows << QStringLiteral("---");
      continue;
    }
    rows << QStringLiteral("%1 | %2 | %3 | %4 | %5 | role %6")
                .arg(
                    action->text(), action->shortcut().toString(QKeySequence::PortableText),
                    action->isVisible() ? QStringLiteral("shown") : QStringLiteral("hidden"),
                    action->isEnabled() ? QStringLiteral("enabled") : QStringLiteral("disabled"),
                    action->icon().name().isEmpty() ? QStringLiteral("no icon") : action->icon().name()
                )
                .arg(static_cast<int>(action->menuRole()));
  }
  return rows;
}

// The row of the dump above for what the program has always put in its tray menu (its shortcuts are those of the OS for
// quitting, and Ctrl+Q on Windows)
QString row(
    const QString &text, const QString &shortcut, bool shown, bool enabled, const QString &icon, int role = 0
)
{
  return QStringLiteral("%1 | %2 | %3 | %4 | %5 | role %6")
      .arg(
          text, shortcut, shown ? QStringLiteral("shown") : QStringLiteral("hidden"),
          enabled ? QStringLiteral("enabled") : QStringLiteral("disabled"), icon
      )
      .arg(role);
}

const QAction *actionNamed(const QMenu *menu, const QString &text)
{
  for (const auto *action : menu->actions())
    if (action->text() == text)
      return action;
  return nullptr;
}

QString quitShortcut()
{
  return deskflow::platform::isWindows() ? QStringLiteral("Ctrl+Q")
                                         : QKeySequence(QKeySequence::Quit).toString(QKeySequence::PortableText);
}

// The menu of the tray as it is when the window is shown (minimize) or hidden (open): the two of them never show both
// at the start, as the window has not been hidden yet
QStringList trayMenu(
    const QString &start, const QString &startIcon, bool startShown, bool restartShown, const QString &stop,
    const QString &stopIcon, bool stopEnabled, bool minimizeShown, bool openShown
)
{
  return {
      row(start, QStringLiteral("Ctrl+S"), startShown, startShown, startIcon),
      row(QStringLiteral("Rest&art"), QStringLiteral("Ctrl+S"), restartShown, restartShown,
          QStringLiteral("view-refresh")),
      row(stop, QStringLiteral("Ctrl+T"), true, stopEnabled, stopIcon),
      QStringLiteral("---"),
      row(QStringLiteral("&Minimize to tray"), {}, minimizeShown, true, QStringLiteral("window-minimize-pip"), 1),
      row(QStringLiteral("&Open Deskflow"), {}, openShown, true, QStringLiteral("window-restore-pip"), 1),
      QStringLiteral("---"),
      row(QStringLiteral("&Quit"), quitShortcut(), true, true, QStringLiteral("application-exit")),
  };
}

} // namespace

void TrayWindowTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  // the window listens on a local socket that is named in the temporary directory: it is the one of this test
  qputenv("TMPDIR", m_dir.path().toUtf8());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
  // the icons that the program has by their names, as the program sets them up at its start
  deskflow::gui::updateIconTheme();
}

void TrayWindowTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QPixmapCache::clear();
  for (const auto &key :
       {Settings::Core::CoreMode, Settings::Core::ComputerName, Settings::Security::TlsEnabled,
        Settings::Client::RemoteHost, Settings::Gui::ShownServerFirstStartMessage,
        Settings::Gui::ShownFirstConnectedMessage, Settings::Gui::AutoStartCore, Settings::Gui::Autohide,
        Settings::Gui::CloseToTray, Settings::Gui::CloseReminder, Settings::Gui::SymbolicTrayIcon, Settings::Gui::WindowGeometry})
    Settings::setValue(key);
}

void TrayWindowTests::systemLook_trayMenuIsTheMenuItHasAlwaysBeen()
{
  auto window = makeWindow();
  const auto *menu = Access::trayIcon(*window)->contextMenu();
  QVERIFY(menu);
  // Start, Restart (not while it is stopped), Stop (not while it is stopped), the window, Quit: no header, no state
  QCOMPARE(
      describe(menu),
      trayMenu(
          QStringLiteral("Start"), QStringLiteral("system-run"), true, false, QStringLiteral("Stop"),
          QStringLiteral("process-stop"), false, true, true
      )
  );
  QVERIFY(!Access::trayIcon(*window)->toolTip().contains(QStringLiteral("sharing")));
}

void TrayWindowTests::systemLook_trayMenuFollowsTheStateOfTheCoreAsItAlwaysDid()
{
  auto window = makeWindow();
  const auto *menu = Access::trayIcon(*window)->contextMenu();

  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  QCOMPARE(
      describe(menu),
      trayMenu(
          QStringLiteral("Start"), QStringLiteral("system-run"), false, true, QStringLiteral("Stop"),
          QStringLiteral("process-stop"), true, true, true
      )
  );

  Access::states(*window, ProcessState::Started, ConnectionState::Connected);
  QCOMPARE(
      describe(menu),
      trayMenu(
          QStringLiteral("Start"), QStringLiteral("system-run"), false, true, QStringLiteral("Stop"),
          QStringLiteral("process-stop"), true, true, true
      )
  );

  Access::states(*window, ProcessState::Stopped, ConnectionState::Disconnected);
  QCOMPARE(
      describe(menu),
      trayMenu(
          QStringLiteral("Start"), QStringLiteral("system-run"), true, false, QStringLiteral("Stop"),
          QStringLiteral("process-stop"), false, true, true
      )
  );
}

void TrayWindowTests::systemLook_aClientSaysConnectAndDisconnect()
{
  Settings::setValue(Settings::Client::RemoteHost, QStringLiteral("srv"));
  auto window = makeWindow(Settings::CoreMode::Client);
  const auto *menu = Access::trayIcon(*window)->contextMenu();
  QCOMPARE(
      describe(menu),
      trayMenu(
          QStringLiteral("Connect"), QStringLiteral("network-connect"), true, false, QStringLiteral("Disconnect"),
          QStringLiteral("network-disconnect"), false, true, true
      )
  );
}

void TrayWindowTests::systemLook_theWindowAndTheMenuTakeTurnsToOfferTheWindow()
{
  auto window = makeWindow();
  const auto *menu = Access::trayIcon(*window)->contextMenu();
  const auto *minimize = actionNamed(menu, QStringLiteral("&Minimize to tray"));
  const auto *open = actionNamed(menu, QStringLiteral("&Open Deskflow"));
  QVERIFY(minimize && open);

  Access::hide(*window);
  QVERIFY(open->isVisible());
  QVERIFY(!minimize->isVisible());

  Access::showAndActivate(*window);
  QVERIFY(!open->isVisible());
  QVERIFY(minimize->isVisible());
}

void TrayWindowTests::closeToTray_withATray_hidesTheWindow()
{
  Settings::setValue(Settings::Gui::CloseToTray, true);
  Settings::setValue(Settings::Gui::CloseReminder, false);
  auto window = makeWindow();
  Access::setTrayAvailable(*window, true);
  Access::showAndActivate(*window);
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));
  const auto *open = actionNamed(Access::trayIcon(*window)->contextMenu(), QStringLiteral("&Open Deskflow"));
  QVERIFY(open && !open->isVisible());

  QVERIFY(Access::closeAsTheOsDoes(*window));
  // the window is hidden (on macOS the program is: the tray then offers the window again)
  QVERIFY(open->isVisible());
  QVERIFY(!window->isMinimized());
}

void TrayWindowTests::closeToTray_withoutATray_keepsTheWindowWhereItCanBeFoundAgain()
{
  Settings::setValue(Settings::Gui::CloseToTray, true);
  // the reminder says that the program is in the notification area: with no tray it is not said
  Settings::setValue(Settings::Gui::CloseReminder, true);
  auto window = makeWindow();
  Access::setTrayAvailable(*window, false);
  Access::showAndActivate(*window);
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));
  const auto *open = actionNamed(Access::trayIcon(*window)->contextMenu(), QStringLiteral("&Open Deskflow"));
  QVERIFY(open && !open->isVisible());

  bool reminded = false;
  QTimer::singleShot(200, qApp, [&reminded] {
    if (auto *modal = QApplication::activeModalWidget()) {
      reminded = true;
      modal->close();
    }
  });
  QVERIFY(Access::closeAsTheOsDoes(*window));
  QTest::qWait(400);

  QVERIFY(!reminded);
  QVERIFY(Settings::value(Settings::Gui::CloseReminder).toBool());
  // the window is not hidden for good: it is minimized, and the program goes on. The tray menu does not offer to open it
  QVERIFY(window->isMinimized());
  QVERIFY(window->isVisible());
  QVERIFY(!open->isVisible());
}

void TrayWindowTests::systemLook_hasNoTrayController_andNoStateInTheTrayIcon()
{
  auto window = makeWindow();
  QVERIFY(!Access::controller(*window));
  Access::states(*window, ProcessState::Started, ConnectionState::Connected);
  // the tool tip is what the tray had: nothing was ever set for the state
  QVERIFY(Access::trayIcon(*window)->toolTip().isEmpty());
}

void TrayWindowTests::deskflowLook_trayMenuSaysTheStateOfTheCore()
{
  using deskflow::gui::tray::TrayState;
  auto window = makeWindow(Settings::CoreMode::Server, true);
  auto *tray = Access::controller(*window);
  QVERIFY(tray);
  auto *icon = Access::trayIcon(*window);

  QCOMPARE(tray->state(), TrayState::Stopped);
  QCOMPARE(tray->spec().entries.first().text, QStringLiteral("Not sharing"));
  QCOMPARE(icon->toolTip(), QStringLiteral("Deskflow: Not sharing"));
  QVERIFY(!icon->icon().isNull());
  QVERIFY(tray->builder()->actionFor(QStringLiteral("command:start")));
  QVERIFY(!tray->builder()->actionFor(QStringLiteral("command:stop")));

  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  QCOMPARE(tray->state(), TrayState::Waiting);
  QCOMPARE(icon->toolTip(), QStringLiteral("Deskflow: Waiting for clients"));
  QVERIFY(!tray->builder()->actionFor(QStringLiteral("command:start")));
  QVERIFY(tray->builder()->actionFor(QStringLiteral("command:restart")));
  QVERIFY(tray->builder()->actionFor(QStringLiteral("command:stop")));
  QCOMPARE(tray->builder()->actionFor(QStringLiteral("command:restart"))->text(), QStringLiteral("Rest&art"));
  QCOMPARE(tray->iconSpec().state, TrayState::Waiting);

  Access::states(*window, ProcessState::Started, ConnectionState::Connected);
  QCOMPARE(tray->state(), TrayState::Connected);
  QCOMPARE(icon->toolTip(), QStringLiteral("Deskflow: Sharing with 1 computer"));
  QCOMPARE(tray->iconSpec().state, TrayState::Connected);

  Access::states(*window, ProcessState::RetryPending, ConnectionState::Disconnected);
  QCOMPARE(tray->state(), TrayState::Error);
  QCOMPARE(tray->iconSpec().state, TrayState::Error);

  Access::states(*window, ProcessState::Stopped, ConnectionState::Disconnected);
  QCOMPARE(tray->state(), TrayState::Stopped);
  QVERIFY(tray->builder()->actionFor(QStringLiteral("command:start")));
}

void TrayWindowTests::deskflowLook_trayHasRowsOfItsOwn_theActionsOfTheWindowAreNotInIt()
{
  auto window = makeWindow(Settings::CoreMode::Server, true);
  const auto *menu = Access::trayIcon(*window)->contextMenu();
  QVERIFY(menu);
  const auto shared = Access::sharedActions(*window);
  for (const auto *action : menu->actions())
    QVERIFY2(!shared.contains(const_cast<QAction *>(action)), qPrintable(action->text()));
  // the rows that show a shortcut show it and do not own it: a shortcut of the window is never met by one of the menu
  auto *tray = Access::controller(*window);
  for (const auto *id : {"command:start", "command:quit"}) {
    const auto *row = tray->builder()->actionFor(QLatin1String(id));
    QVERIFY(row);
    QCOMPARE(row->shortcutContext(), Qt::WidgetShortcut);
    QVERIFY(row->isShortcutVisibleInContextMenu());
  }
  // and the menu bar still has the actions it had
  const auto *start = Access::sharedActions(*window).first();
  QCOMPARE(start->shortcut(), QKeySequence(QStringLiteral("Ctrl+S")));
  QVERIFY(Access::controller(*window)->builder()->actionFor(QStringLiteral("command:start"))->shortcut() ==
          start->shortcut());
}

void TrayWindowTests::deskflowLook_chosenRowsDoWhatTheMenuAlwaysDid()
{
  auto window = makeWindow(Settings::CoreMode::Server, true);
  auto *tray = Access::controller(*window);
  auto *shell = Access::shell(*window);
  Access::showAndActivate(*window);
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));
  const auto *restore = Access::sharedActions(*window).at(4);
  QVERIFY(!restore->isVisible());

  // Minimize to tray hides the window: the window offers to be opened (and the menu says so)
  tray->builder()->actionFor(QStringLiteral("command:minimize"))->trigger();
  QVERIFY(restore->isVisible());
  QVERIFY(!tray->spec().entries.isEmpty());
  QVERIFY(tray->builder()->actionFor(QStringLiteral("command:minimize")) == nullptr ||
          !tray->builder()->actionFor(QStringLiteral("command:minimize"))->isEnabled());

  // Open shows it again
  tray->builder()->actionFor(QStringLiteral("command:open"))->trigger();
  QVERIFY(!restore->isVisible());
  QVERIFY(tray->builder()->actionFor(QStringLiteral("command:minimize")));
  QVERIFY(tray->builder()->actionFor(QStringLiteral("command:minimize"))->isEnabled());

  // Preferences opens the page of the settings of the window of the new look
  shell->showPage(QStringLiteral("home"));
  tray->builder()->actionFor(QStringLiteral("command:preferences"))->trigger();
  QCOMPARE(shell->currentPageId(), QStringLiteral("settings.general"));
}

void TrayWindowTests::deskflowLook_anUpdateIsInTheMenuAndOnTheIcon()
{
  auto window = makeWindow(Settings::CoreMode::Server, true);
  auto *tray = Access::controller(*window);
  QVERIFY(!tray->builder()->actionFor(QStringLiteral("command:update")));
  QVERIFY(!tray->iconSpec().update);

  Access::statusBar(*window)->updateFound(QStringLiteral("9.9.9"));
  // the status bar says it through the event loop
  QTRY_VERIFY(tray->builder()->actionFor(QStringLiteral("command:update")));
  QCOMPARE(
      tray->builder()->actionFor(QStringLiteral("command:update"))->text(),
      QStringLiteral("A new version v9.9.9 is available")
  );
  QVERIFY(tray->iconSpec().update);
  QVERIFY(Access::trayIcon(*window)->toolTip().contains(QStringLiteral("update available")));
}

void TrayWindowTests::deskflowLook_aClientTestsItsConnectionFromTheTray()
{
  Settings::setValue(Settings::Client::RemoteHost, QStringLiteral("srv"));
  auto window = makeWindow(Settings::CoreMode::Client, true);
  auto *tray = Access::controller(*window);
  QVERIFY(tray->builder()->actionFor(QStringLiteral("command:start")));
  QCOMPARE(tray->builder()->actionFor(QStringLiteral("command:start"))->text(), QStringLiteral("Connect"));

  Access::states(*window, ProcessState::RetryPending, ConnectionState::Disconnected);
  QCOMPARE(tray->state(), deskflow::gui::tray::TrayState::Error);
  const auto *test = tray->builder()->actionFor(QStringLiteral("command:test-connection"));
  QVERIFY(test);
  QVERIFY(test->isEnabled());
  QVERIFY(tray->builder()->actionFor(QStringLiteral("command:stop")));
  QCOMPARE(tray->builder()->actionFor(QStringLiteral("command:stop"))->text(), QStringLiteral("Disconnect"));
}

void TrayWindowTests::deskflowLook_theColourOfTheIconFollowsTheSettingAndNeverTheShape()
{
  using deskflow::gui::tray::TrayState;
  using deskflow::gui::tray::TrayTone;
  auto window = makeWindow(Settings::CoreMode::Server, true);
  auto *tray = Access::controller(*window);
  // the bar of a PC with Windows, whose icons have no colour of their own
  tray->setTone(TrayTone::OnLightBar);
  Access::states(*window, ProcessState::Started, ConnectionState::Connected);
  // the setting is Monocolor unless it was changed: no colour in the shapes
  QVERIFY(!tray->iconSpec().coloredBadge);
  QCOMPARE(tray->iconSpec().tone, TrayTone::OnLightBar);

  // Colorful: the shapes have colour (the setting is told to the window, which tells the tray)
  Access::applySetting(*window, Settings::Gui::SymbolicTrayIcon, false);
  QVERIFY(tray->iconSpec().coloredBadge);
  QCOMPARE(tray->iconSpec().state, TrayState::Connected);

  // macOS has no colour in a template: the setting changes nothing there
  tray->setTone(TrayTone::Template);
  QVERIFY(!tray->iconSpec().coloredBadge);
  QCOMPARE(tray->iconSpec().state, TrayState::Connected);
}

QTEST_MAIN(TrayWindowTests)
