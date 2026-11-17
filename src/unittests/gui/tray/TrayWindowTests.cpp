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
  static void setTrayAvailable(MainWindow &w, bool available)
  {
    w.m_trayAvailable = [available] { return available; };
  }
};

namespace {

using Access = MainWindowTestAccess;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;

// A window as the application makes it: the settings are those of the test, and the core is not started
std::unique_ptr<MainWindow> makeWindow(Settings::CoreMode mode = Settings::CoreMode::Server)
{
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

QTEST_MAIN(TrayWindowTests)
