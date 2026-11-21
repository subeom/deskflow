/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlShellTests.h"

#include "../FontEnvironment.h"
#include "../awdl/AwdlSamples.h"
#include "../awdl/FakeAwdlSystem.h"
#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/awdl/AwdlController.h"
#include "gui/awdl/AwdlPlatform.h"
#include "gui/awdl/AwdlTexts.h"
#include "gui/core/CoreProcess.h"
#include "gui/settings/AwdlBanner.h"
#include "gui/settings/AwdlCard.h"
#include "gui/settings/SettingsPages.h"
#include "gui/shell/MainShell.h"
#include "gui/style/DeskflowStyle.h"

#include <QAccessible>
#include <QApplication>
#include <QPixmapCache>
#include <QPushButton>

#include <memory>

struct MainWindowTestAccess
{
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
  static deskflow::awdl::AwdlController *awdl(MainWindow &w)
  {
    return w.m_awdl;
  }
  static void states(MainWindow &w, deskflow::core::ProcessState process, deskflow::core::ConnectionState connection)
  {
    w.m_coreProcess.setProcessState(process);
    w.m_coreProcess.setConnectionState(connection);
    // the window is told of the state of the process through the event loop
    QCoreApplication::processEvents();
  }
};

namespace {

using Access = MainWindowTestAccess;
using namespace deskflow::awdl;
using namespace awdltest;
using deskflow::awdl::permissionPrompt;
using deskflow::awdl::prompt;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;

// the system that the window was given: the test answers through it
FakeAwdlSystem *s_fake = nullptr;
int s_made = 0;

void installDeskflowStyle()
{
  auto *style = new deskflow::gui::style::DeskflowStyle;
  QApplication::setStyle(style);
  deskflow::gui::style::Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

std::unique_ptr<MainWindow> makeWindow(bool deskflowLook = true)
{
  if (deskflowLook)
    installDeskflowStyle();
  else
    QApplication::setStyle(QStringLiteral("Fusion"));
  Settings::setValue(Settings::Security::TlsEnabled, false);
  Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  Settings::setValue(Settings::Core::CoreMode, Settings::CoreMode::Server);
  return std::make_unique<MainWindow>();
}

void showWindow(MainWindow &window)
{
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QCoreApplication::processEvents();
}

QPushButton *button(MainWindow &window, const char *name)
{
  return window.findChild<QPushButton *>(QString::fromLatin1(name));
}

} // namespace

void AwdlShellTests::initTestCase()
{
  SKIP_WITHOUT_FONTS();
  QVERIFY(m_dir.isValid());
  qputenv("DESKFLOW_SECRET_STORE", "memory");
  qputenv("TMPDIR", m_dir.path().toUtf8());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
  Access::setWorkArea(QRect(0, 0, 1920, 1040));
}

void AwdlShellTests::init()
{
  s_fake = nullptr;
  s_made = 0;
  s_supportedForTest = true;
  // the window is given the fake (the real system is never made by a test)
  s_systemForTest = [](QObject *parent) -> IAwdlSystem * {
    s_fake = new FakeAwdlSystem(parent);
    ++s_made;
    return s_fake;
  };
}

void AwdlShellTests::cleanup()
{
  deskflow::gui::settings::AwdlCard::s_confirmForTest = nullptr;
  s_systemForTest = nullptr;
  s_supportedForTest.reset();
  s_fake = nullptr;
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QApplication::setFont(QFont());
  QPixmapCache::clear();
  for (const auto &key :
       {Settings::Core::CoreMode, Settings::Core::ComputerName, Settings::Security::TlsEnabled,
        Settings::Gui::ShownServerFirstStartMessage, Settings::Gui::ShownFirstConnectedMessage,
        Settings::Gui::WindowGeometry, Settings::Gui::ShellWindowGeometry, Settings::Network::MacAwdlOff})
    Settings::setValue(key);
}

void AwdlShellTests::optionOff_nothingIsReadOrAsked_whateverTheCoreDoes()
{
  auto window = makeWindow();
  QCOMPARE(s_made, 1);
  QVERIFY(Access::awdl(*window));
  for (const auto process :
       {ProcessState::Starting, ProcessState::Started, ProcessState::Stopping, ProcessState::Stopped})
    Access::states(*window, process, ConnectionState::Disconnected);
  QCOMPARE(s_fake->reads, 0);
  QCOMPARE(s_fake->routeLookups, 0);
  QVERIFY(s_fake->measures.isEmpty());
  QVERIFY(s_fake->changes.isEmpty());
  QVERIFY(!Access::shell(*window)->awdlBanner()->isPending());
}

void AwdlShellTests::optionOn_aCoreThatStarts_readsTheLink_andAsksForThePasswordOnceForTheRun()
{
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  auto window = makeWindow();
  QCOMPARE(s_fake->reads, 0);

  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  QCOMPARE(s_fake->reads, 1);
  // the link is up: the dialog of the password, with the sentence of the program, and it is the constant command that
  // is asked for (the direction and the sentence are all that the program gives the system)
  s_fake->answerRead(LinkState::Up);
  QCOMPARE(s_fake->changes.size(), 1);
  QCOMPARE(s_fake->changes.first().first, deskflow::awdl::Direction::Down);
  QCOMPARE(s_fake->changes.first().second, prompt(deskflow::awdl::Direction::Down));

  // a no is logged and is not asked again by itself in this run, however often the state is read
  s_fake->answerLink(AdminResult::Cancelled);
  s_fake->answerRead(LinkState::Up);
  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  s_fake->answerRead(LinkState::Up);
  QCOMPARE(s_fake->changes.size(), 1);
  // up is no problem by itself: there is no banner until the latency has the spikes
  QVERIFY(!Access::shell(*window)->awdlBanner()->isPending());
}

void AwdlShellTests::optionOn_aStoppedCore_restoresNothing_andTheNextRunAsksAgain()
{
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  auto window = makeWindow();
  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  s_fake->answerRead(LinkState::Up);
  s_fake->answerLink(AdminResult::Done);
  s_fake->answerRead(LinkState::Down);
  QCOMPARE(s_fake->downs(), 1);

  // stopping and quitting do not turn it on again: that would ask for the password again
  Access::states(*window, ProcessState::Stopping, ConnectionState::Disconnected);
  Access::states(*window, ProcessState::Stopped, ConnectionState::Disconnected);
  QCOMPARE(s_fake->ups(), 0);
  QCOMPARE(s_fake->changes.size(), 1);

  // the link came back (AirDrop used it) and the core starts again: it is a new run, and it is asked once more
  const auto reads = s_fake->reads;
  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  QVERIFY(s_fake->reads > reads);
  s_fake->answerRead(LinkState::Up);
  QCOMPARE(s_fake->downs(), 2);
}

void AwdlShellTests::optionOn_aRetryOfTheCoreIsTheSameRun()
{
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  auto window = makeWindow();
  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  s_fake->answerRead(LinkState::Up);
  s_fake->answerLink(AdminResult::Cancelled);
  QCOMPARE(s_fake->changes.size(), 1);
  // the core fails to connect and tries again: the person said no for this run, and is not asked at every try
  for (int i = 0; i < 3; ++i) {
    Access::states(*window, ProcessState::RetryPending, ConnectionState::Disconnected);
    Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
    s_fake->answerRead(LinkState::Up);
  }
  QCOMPARE(s_fake->changes.size(), 1);
}

void AwdlShellTests::theSettingChangedWhileTheCoreRuns_isFollowedAtOnce()
{
  auto window = makeWindow();
  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  QCOMPARE(s_fake->reads, 0);
  // the option is turned on while sharing runs: the state is read at once (the page does it through the setting)
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  QCOMPARE(s_fake->reads, 1);
  s_fake->answerRead(LinkState::Up);
  QCOMPARE(s_fake->downs(), 1);
  s_fake->answerLink(AdminResult::Done);
  s_fake->answerRead(LinkState::Down);

  // and turned off again: nothing more is read or asked
  Settings::setValue(Settings::Network::MacAwdlOff, false);
  const auto reads = s_fake->reads;
  QTest::qWait(60);
  QCOMPARE(s_fake->reads, reads);
  QCOMPARE(s_fake->changes.size(), 1);
}

void AwdlShellTests::theBanner_isShownOnlyForAStutteringLink_andItsButtonAsksForThePassword()
{
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  auto window = makeWindow();
  showWindow(*window);
  auto *banner = Access::shell(*window)->awdlBanner();
  QVERIFY(!banner->isVisible());

  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  s_fake->answerRead(LinkState::Up);
  // the person said no to the dialog
  s_fake->answerLink(AdminResult::Cancelled);
  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  QVERIFY(!banner->isVisible());

  // a clean link that is up: no banner. A burst that has the spikes: the banner, with the numbers
  auto *measure = button(*window, "btnAwdlMeasure");
  QVERIFY(measure);
  s_fake->answerRead(LinkState::Up);
  measure->click();
  s_fake->answerRoute(routeOf(LinkKind::WiFi));
  s_fake->answerMeasured(clean());
  QVERIFY(!banner->isVisible());

  measure->click();
  s_fake->answerRoute(routeOf(LinkKind::WiFi));
  s_fake->answerMeasured(stutter());
  QVERIFY(banner->isVisible());
  QVERIFY(banner->text().contains(QStringLiteral("stutter")));
  QVERIFY(banner->text().contains(QStringLiteral("64")));

  // its button is the button "Turn off now": it asks for the password, and while the dialog is open it is off
  const auto before = s_fake->changes.size();
  banner->button()->click();
  QCOMPARE(s_fake->changes.size(), before + 1);
  QCOMPARE(s_fake->changes.last().first, deskflow::awdl::Direction::Down);
  QVERIFY(!banner->button()->isEnabled());
  s_fake->answerLink(AdminResult::Done);
  s_fake->answerRead(LinkState::Down);
  // the link is down: there is nothing to turn off
  QVERIFY(!banner->isVisible());
  QVERIFY(banner->button()->isEnabled());

  // the core stops: the banner goes with the run
  Access::states(*window, ProcessState::Stopped, ConnectionState::Disconnected);
  QVERIFY(!banner->isVisible());
}

void AwdlShellTests::notAMac_thereIsNoControllerNoCardAndNoCall()
{
  s_supportedForTest = false;
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  auto window = makeWindow();
  // the system is not even made
  QCOMPARE(s_made, 0);
  QVERIFY(!Access::awdl(*window));
  QVERIFY(!window->findChild<QWidget *>(QStringLiteral("cardAwdl")));
  QVERIFY(!button(*window, "btnAwdlTurnOff"));
  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  Settings::setValue(Settings::Network::MacAwdlOff, false);
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  QVERIFY(!Access::shell(*window)->awdlBanner()->isPending());
}

void AwdlShellTests::aWindowOfThePlatform_hasNoSystemWithoutAScreen()
{
  // the tests run offscreen: the window that is made with no system of a test never gets the programs of the system
  s_systemForTest = nullptr;
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  auto window = makeWindow();
  QVERIFY(!Access::awdl(*window));
  // the page is there (this is a Mac as far as the test says) and its buttons are off: there is nothing to ask
  auto *off = button(*window, "btnAwdlTurnOff");
  QVERIFY(off);
  QVERIFY(!off->isEnabled());
  Access::states(*window, ProcessState::Started, ConnectionState::Listening);
  QVERIFY(!Access::shell(*window)->awdlBanner()->isPending());
}

void AwdlShellTests::oldLook_theControllerRuns_andThereIsNoBanner()
{
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  auto window = makeWindow(false);
  // the window of the system look has no sidebar, no pages and no banner: the option still does what it says (the log
  // has the lines of the controller), and it is turned on in the file of the settings
  QVERIFY(!Access::shell(*window));
  QVERIFY(Access::awdl(*window));
  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  QCOMPARE(s_fake->reads, 1);
  s_fake->answerRead(LinkState::Up);
  QCOMPARE(s_fake->downs(), 1);
}

void AwdlShellTests::withThePermission_theStartOfSharingAndTheBanner_askNoPassword()
{
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  auto window = makeWindow();
  showWindow(*window);
  // sudo says that the command needs no password
  s_fake->listing = deskflow::awdl::Listing::NoPassword;
  s_fake->filePresent = true;

  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  s_fake->answerRead(deskflow::awdl::LinkState::Up);
  // the start of sharing changes the link with sudo -n: no dialog of the password
  QCOMPARE(s_fake->silentChanges, (QList<deskflow::awdl::Direction>{deskflow::awdl::Direction::Down}));
  QCOMPARE(s_fake->changes.size(), 0);
  s_fake->answerSilent(deskflow::awdl::SudoResult::Done);
  Access::states(*window, ProcessState::Started, ConnectionState::Listening);

  // AirDrop turned it on again and the latency shows the spikes: the banner, and its button changes the link with no
  // dialog
  s_fake->answerRead(deskflow::awdl::LinkState::Up);
  auto *measure = button(*window, "btnAwdlMeasure");
  QVERIFY(measure);
  measure->click();
  s_fake->answerRoute(routeOf(LinkKind::WiFi));
  s_fake->answerMeasured(stutter());
  auto *banner = Access::shell(*window)->awdlBanner();
  QVERIFY(banner->isVisible());
  banner->button()->click();
  QCOMPARE(s_fake->silentChanges.size(), 2);
  QCOMPARE(s_fake->changes.size(), 0);
  QVERIFY(!banner->button()->isEnabled());
  s_fake->answerSilent(deskflow::awdl::SudoResult::Done);
  s_fake->answerRead(deskflow::awdl::LinkState::Down);
  QVERIFY(!banner->isVisible());
  QVERIFY(banner->button()->isEnabled());
}

void AwdlShellTests::withoutThePermission_theWindowAsksForThePasswordAsBefore()
{
  Settings::setValue(Settings::Network::MacAwdlOff, true);
  auto window = makeWindow();
  // the default answer of the fake: sudo says that a password is needed
  Access::states(*window, ProcessState::Starting, ConnectionState::Disconnected);
  s_fake->answerRead(deskflow::awdl::LinkState::Up);
  QCOMPARE(s_fake->silentChanges.size(), 0);
  QCOMPARE(s_fake->changes.size(), 1);
  QCOMPARE(s_fake->changes.first().second, prompt(deskflow::awdl::Direction::Down));
}

void AwdlShellTests::thePermissionButtons_ofTheWindow_askTheQuestion_thenShowTheDialogWithTheSentencesOfTheProgram()
{
  auto window = makeWindow();
  auto *allow = button(*window, "btnAwdlAllow");
  auto *remove = button(*window, "btnAwdlRemovePermission");
  QVERIFY(allow && remove);

  // the window gives its controller the sentences of the dialogs (the program's own, in its language): not an empty
  // dialog
  int questions = 0;
  deskflow::gui::settings::AwdlCard::s_confirmForTest = [&](const QString &, const QString &) {
    ++questions;
    return true;
  };
  QVERIFY(allow->isEnabled());
  allow->click();
  QCOMPARE(questions, 1);
  QCOMPARE(s_fake->installs, (QStringList{permissionPrompt(deskflow::awdl::PermissionAction::Allow)}));
  QVERIFY(!s_fake->installs.first().isEmpty());
  s_fake->listing = deskflow::awdl::Listing::NoPassword;
  s_fake->filePresent = true;
  s_fake->answerPermissionSet(deskflow::awdl::AdminResult::Done);

  QVERIFY(remove->isEnabled());
  remove->click();
  QCOMPARE(questions, 1);
  QCOMPARE(s_fake->removals, (QStringList{permissionPrompt(deskflow::awdl::PermissionAction::Remove)}));
  QVERIFY(!s_fake->removals.first().isEmpty());
}

QTEST_MAIN(AwdlShellTests)
