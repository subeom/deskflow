/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TunnelShellTests.h"

#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/core/CoreProcess.h"
#include "gui/shell/ClientConnectionSection.h"
#include "gui/shell/MainShell.h"
#include "gui/shell/StatusModel.h"
#include "gui/shell/TunnelChip.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/PanelWidget.h"
#include "gui/tunnel/TunnelController.h"
#include "gui/widgets/InlineHint.h"
#include "gui/widgets/NavRail.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/TunnelStatusWidget.h"

#include "../FontEnvironment.h"

#include <QAccessible>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QPixmapCache>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedLayout>
#include <QSystemTrayIcon>

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
  static QSystemTrayIcon *trayIcon(MainWindow &w)
  {
    return w.m_trayIcon;
  }
  static deskflow::gui::TunnelStatusWidget *tunnelStatus(MainWindow &w)
  {
    return w.m_tunnelStatus;
  }
  //! The configuration was saved by a dialog: the window reads it again
  static void refreshTunnel(MainWindow &w)
  {
    w.m_tunnelStatus->refresh();
  }
  static deskflow::tunnel::TunnelController *tunnelController(MainWindow &w)
  {
    return w.m_tunnelController;
  }
  static void linkIs(MainWindow &w, deskflow::core::ConnectionState state)
  {
    w.updateTunnelServerLink(state);
  }
};

namespace {

using Access = MainWindowTestAccess;
using namespace deskflow::gui;
using deskflow::tunnel::ServerLink;
using deskflow::tunnel::TunnelFailure;
using deskflow::tunnel::TunnelState;
using deskflow::tunnel::TunnelStatus;
using Kind = StatusChip::Kind;

void installDeskflowStyle()
{
  auto *style = new style::DeskflowStyle;
  QApplication::setStyle(style);
  style::Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

// A window as the application makes it, a client unless it is said that it is not; the tunnel is on or off as the
// settings say
std::unique_ptr<MainWindow>
makeWindow(bool deskflowLook, bool tunnelOn, Settings::CoreMode mode = Settings::CoreMode::Client)
{
  if (deskflowLook)
    installDeskflowStyle();
  else
    QApplication::setStyle(QStringLiteral("Fusion"));
  Settings::setValue(Settings::Security::TlsEnabled, false);
  Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  Settings::setValue(Settings::Core::CoreMode, mode);
  if (tunnelOn) {
    Settings::setValue(Settings::Tunnel::Enabled, true);
    Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("mbp.example.com"));
    Settings::setValue(Settings::Tunnel::LocalPort, 24811);
  }
  return std::make_unique<MainWindow>();
}

void showWindow(MainWindow &window)
{
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QCoreApplication::processEvents();
}

// The rows of the menu of the tray that say the state of the tunnel
QStringList tunnelRows(MainWindow &window)
{
  QStringList rows;
  if (const auto *menu = Access::trayIcon(window)->contextMenu()) {
    for (const auto *action : menu->actions()) {
      if (action->isVisible() && action->text().startsWith(QStringLiteral("Tunnel:")))
        rows << action->text();
    }
  }
  return rows;
}

QStringList chipTexts(MainWindow &window)
{
  QStringList texts;
  for (const auto &chip : Access::shell(window)->status()->view().chips)
    texts << chip.text;
  return texts;
}

QLabel *pathLine(MainWindow &window)
{
  return window.findChild<QLabel *>(QStringLiteral("tunnelPathLine"));
}

InlineHint *statusLine(MainWindow &window)
{
  return window.findChild<InlineHint *>(QStringLiteral("tunnelStatusLine"));
}

} // namespace

Q_DECLARE_METATYPE(ServerLink)

void TunnelShellTests::initTestCase()
{
  // the width of text is what some of these tests measure (see FontEnvironment.h)
  SKIP_WITHOUT_FONTS();
  QVERIFY(m_dir.isValid());
  // no test of this program reaches the keychain or the Credential Manager: the secure store is the one in memory
  qputenv("DESKFLOW_SECRET_STORE", "memory");
  qputenv("TMPDIR", m_dir.path().toUtf8());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
  Access::setWorkArea(QRect(0, 0, 1920, 1040));
}

void TunnelShellTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QApplication::setFont(QFont());
  QPixmapCache::clear();
  for (const auto &key :
       {Settings::Core::CoreMode, Settings::Core::ComputerName, Settings::Security::TlsEnabled,
        Settings::Client::RemoteHost, Settings::Gui::ShownServerFirstStartMessage,
        Settings::Gui::ShownFirstConnectedMessage, Settings::Gui::WindowGeometry, Settings::Gui::ShellWindowGeometry,
        Settings::Tunnel::Enabled, Settings::Tunnel::Hostname, Settings::Tunnel::LocalPort,
        Settings::Tunnel::CloudflaredPath})
    Settings::setValue(key);
}

void TunnelShellTests::tunnelChip_isAFewWordsAndSaysHowTheTunnelIs_data()
{
  QTest::addColumn<int>("state");
  QTest::addColumn<int>("failure");
  QTest::addColumn<ServerLink>("link");
  QTest::addColumn<QString>("text");
  QTest::addColumn<Kind>("kind");

  const auto row = [](const char *name, TunnelState state, TunnelFailure failure, ServerLink link, const char *text,
                      Kind kind) {
    QTest::newRow(name) << int(state) << int(failure) << link << QString::fromLatin1(text) << kind;
  };
  row("stopped", TunnelState::Idle, TunnelFailure::None, ServerLink::Unknown, "Tunnel: stopped", Kind::Neutral);
  row("starting", TunnelState::Starting, TunnelFailure::None, ServerLink::Unknown, "Tunnel: starting", Kind::Info);
  row("ready, no word of the server", TunnelState::Listening, TunnelFailure::None, ServerLink::Unknown, "Tunnel: ready",
      Kind::Info);
  row("ready, the server is being reached", TunnelState::Listening, TunnelFailure::None, ServerLink::Connecting,
      "Tunnel: connecting", Kind::Info);
  row("connected", TunnelState::Listening, TunnelFailure::None, ServerLink::Connected, "Tunnel: connected",
      Kind::Success);
  // a tunnel that listens and has a problem to tell is not the colour of one that works, whatever the core says
  row("a problem while it listens", TunnelState::Listening, TunnelFailure::HandshakeRejected, ServerLink::Connected,
      "Tunnel: problem", Kind::Warning);
  row("stopping", TunnelState::Stopping, TunnelFailure::None, ServerLink::Unknown, "Tunnel: stopping", Kind::Neutral);
  row("a pause before it is started again", TunnelState::Backoff, TunnelFailure::ProcessExited, ServerLink::Unknown,
      "Tunnel: restarting", Kind::Warning);
  row("failed", TunnelState::Failed, TunnelFailure::CloudflaredMissing, ServerLink::Unknown, "Tunnel: failed",
      Kind::Danger);
  // the link to the server says nothing of a tunnel that does not run
  row("failed, with a link", TunnelState::Failed, TunnelFailure::LoginRequired, ServerLink::Connected, "Tunnel: failed",
      Kind::Danger);
  row("stopped, with a link", TunnelState::Idle, TunnelFailure::None, ServerLink::Connected, "Tunnel: stopped",
      Kind::Neutral);
}

void TunnelShellTests::tunnelChip_isAFewWordsAndSaysHowTheTunnelIs()
{
  QFETCH(int, state);
  QFETCH(int, failure);
  QFETCH(ServerLink, link);
  QFETCH(QString, text);
  QFETCH(Kind, kind);
  TunnelStatus status;
  status.state = TunnelState(state);
  status.failure = TunnelFailure(failure);
  const auto chip = tunnelChip(status, link);
  QCOMPARE(chip.text, text);
  QCOMPARE(chip.kind, kind);
  // a chip is as wide as its text and does not cut it: a few words, never a sentence
  QVERIFY(chip.text.size() <= 24);
}

void TunnelShellTests::tunnelOff_nothingIsAddedToTheShell()
{
  auto window = makeWindow(true, false);
  showWindow(*window);
  auto *section = Access::shell(*window)->clientSection();

  QVERIFY(!Access::tunnelStatus(*window)->isTunnelUsed());
  // the address is the field that it always was, and nothing is under it
  QCOMPARE(section->addressStack()->currentIndex(), 0);
  QCOMPARE(section->statusLineCount(), 0);
  QVERIFY(pathLine(*window) != nullptr);
  QVERIFY(!pathLine(*window)->isVisibleTo(window.get()));
  QVERIFY(!statusLine(*window)->isVisibleTo(window.get()));
  for (const auto &text : chipTexts(*window))
    QVERIFY2(!text.startsWith(QStringLiteral("Tunnel")), qPrintable(text));
  QVERIFY(tunnelRows(*window).isEmpty());
  // the widget that knows what the lines say is never shown by itself (it was a stray child of the window)
  QVERIFY(!Access::tunnelStatus(*window)->isVisibleTo(window.get()));
}

void TunnelShellTests::aServer_hasNoLineOfTheTunnel_whateverTheSettingsSay()
{
  auto window = makeWindow(true, true, Settings::CoreMode::Server);
  showWindow(*window);
  auto *section = Access::shell(*window)->clientSection();

  QVERIFY(!Access::tunnelStatus(*window)->isTunnelUsed());
  QCOMPARE(section->addressStack()->currentIndex(), 0);
  QCOMPARE(section->statusLineCount(), 0);
  QVERIFY(tunnelRows(*window).isEmpty());
  for (const auto &text : chipTexts(*window))
    QVERIFY2(!text.startsWith(QStringLiteral("Tunnel")), qPrintable(text));
}

void TunnelShellTests::tunnelOn_theAddressIsTheLineOfThePath_andTheStateIsInTheCardTheChipAndTheTray()
{
  auto window = makeWindow(true, true);
  showWindow(*window);
  auto *shell = Access::shell(*window);
  auto *section = shell->clientSection();

  QVERIFY(Access::tunnelStatus(*window)->isTunnelUsed());
  // the path line stands in the place of the address field (the field keeps the address that was saved)
  QCOMPARE(section->addressStack()->currentWidget(), pathLine(*window));
  QCOMPARE(pathLine(*window)->text(), deskflow::tunnel::tunnelPathLine(QStringLiteral("mbp.example.com"), 24811));
  QVERIFY(pathLine(*window)->isVisibleTo(window.get()));
  QVERIFY(section->isAncestorOf(pathLine(*window)));
  const auto *hostname = window->findChild<QComboBox *>(QStringLiteral("cbHostname"));
  QVERIFY(hostname != nullptr);
  QVERIFY(!hostname->isVisibleTo(window.get()));

  // the status line is under it
  QCOMPARE(section->statusLineCount(), 1);
  QVERIFY(statusLine(*window)->isVisibleTo(window.get()));
  QCOMPARE(statusLine(*window)->text(), Access::tunnelStatus(*window)->statusText());
  QVERIFY(statusLine(*window)->text().startsWith(QStringLiteral("Tunnel: stopped")));
  // the whole line is also the tool tip of what is cut
  QCOMPARE(statusLine(*window)->toolTip(), statusLine(*window)->text());

  // the chip is beside the state of sharing, and the tray has the row
  QVERIFY2(chipTexts(*window).contains(QStringLiteral("Tunnel: stopped")), qPrintable(chipTexts(*window).join(u'|')));
  QCOMPARE(tunnelRows(*window), QStringList{Access::tunnelStatus(*window)->statusText()});

  // the saved address was not touched by any of it, and the Test connection button can be used without it
  QVERIFY(Settings::value(Settings::Client::RemoteHost).toString().isEmpty());
  QVERIFY(window->findChild<QPushButton *>(QStringLiteral("btnTestConnection"))->isEnabled());
}

void TunnelShellTests::tunnelOn_theLinesFollowTheChoiceThatIsMadeAndUnmade()
{
  auto window = makeWindow(true, false);
  showWindow(*window);
  auto *section = Access::shell(*window)->clientSection();
  QCOMPARE(section->statusLineCount(), 0);

  // the dialog (or the page) saved the tunnel on: the window reads the configuration again
  Settings::setValue(Settings::Tunnel::Enabled, true);
  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("a.example.com"));
  Access::refreshTunnel(*window);
  QCOMPARE(section->addressStack()->currentWidget(), pathLine(*window));
  QCOMPARE(section->statusLineCount(), 1);
  QVERIFY(pathLine(*window)->text().contains(QStringLiteral("a.example.com")));
  QVERIFY(chipTexts(*window).contains(QStringLiteral("Tunnel: stopped")));
  QCOMPARE(tunnelRows(*window).size(), 1);

  // another hostname is another path line, at once
  Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("b.example.com"));
  Access::refreshTunnel(*window);
  QVERIFY(pathLine(*window)->text().contains(QStringLiteral("b.example.com")));
  QCOMPARE(section->statusLineCount(), 1);

  // and off again: everything is as if it had never been on
  Settings::setValue(Settings::Tunnel::Enabled, false);
  Access::refreshTunnel(*window);
  QCOMPARE(section->addressStack()->currentIndex(), 0);
  QVERIFY(section->addressStack()->currentWidget() != pathLine(*window));
  QCOMPARE(section->statusLineCount(), 0);
  for (const auto &text : chipTexts(*window))
    QVERIFY2(!text.startsWith(QStringLiteral("Tunnel")), qPrintable(text));
  QVERIFY(tunnelRows(*window).isEmpty());
}

// A tunnel that failed is a problem, in the line under the address and in the chip: the colour of the line is what a
// person sees first. (The path of cloudflared is one that does not exist, so no program is started.)
void TunnelShellTests::tunnelOn_aFailureIsTheColourOfAProblem_inTheLineAndInTheChip()
{
  auto window = makeWindow(true, true);
  Settings::setValue(Settings::Tunnel::CloudflaredPath, QStringLiteral("/nonexistent/folder/cloudflared"));
  showWindow(*window);
  QVERIFY(!statusLine(*window)->isDanger());

  Access::tunnelController(*window)->acquire();
  QCOMPARE(Access::tunnelController(*window)->status().state, deskflow::tunnel::TunnelState::Failed);
  QCoreApplication::processEvents();
  QVERIFY(statusLine(*window)->isDanger());
  QVERIFY(statusLine(*window)->text().startsWith(QStringLiteral("Tunnel: failed")));
  QVERIFY(chipTexts(*window).contains(QStringLiteral("Tunnel: failed")));
  // the row of the tray cuts a long sentence (the whole one is in the line under the address)
  QCOMPARE(tunnelRows(*window).size(), 1);
  QVERIFY(tunnelRows(*window).first().startsWith(QStringLiteral("Tunnel: failed")));
}

void TunnelShellTests::tunnelOn_theLinkToTheServerIsInTheChip()
{
  using deskflow::core::ConnectionState;
  auto window = makeWindow(true, true);
  showWindow(*window);
  // the tunnel does not run (the core is not started here): a link says nothing of a tunnel that is stopped
  Access::linkIs(*window, ConnectionState::Connected);
  QVERIFY(chipTexts(*window).contains(QStringLiteral("Tunnel: stopped")));
  QVERIFY(statusLine(*window)->text().startsWith(QStringLiteral("Tunnel: stopped")));
}

void TunnelShellTests::tunnelOn_theWindowIsNotWiderThanItPromises_withALongHostnameAndLargerText()
{
  Settings::setValue(Settings::Tunnel::Enabled, true);
  Settings::setValue(
      Settings::Tunnel::Hostname, QStringLiteral("a-rather-long-name-for-a-computer.some-long-domain.example.com")
  );
  auto window = makeWindow(true, false);
  auto font = window->font();
  font.setPixelSize(21);
  font.setLetterSpacing(QFont::PercentageSpacing, 130);
  window->setFont(font);
  showWindow(*window);
  // the line that does not wrap does not ask for its width (what does not fit is cut and is in the tool tip)
  QVERIFY(Access::tunnelStatus(*window)->isTunnelUsed());
  QVERIFY2(
      window->minimumSizeHint().width() <= MainShell::minimumWindowWidth(),
      qPrintable(QStringLiteral("the window asks for %1").arg(window->minimumSizeHint().width()))
  );
  window->resize(MainShell::minimumWindowWidth(), 760);
  QTRY_COMPARE(window->width(), MainShell::minimumWindowWidth());
  auto *section = Access::shell(*window)->clientSection();
  QVERIFY(section->width() <= window->width());
  QVERIFY(pathLine(*window)->width() <= section->width());
  QVERIFY(statusLine(*window)->width() <= section->width());
}

void TunnelShellTests::systemLook_hasTheLinesWhereTheyAlwaysWere()
{
  auto window = makeWindow(false, true);
  showWindow(*window);
  // the widget with the two lines is where the system look always had it, in the group of the mode
  auto *widget = Access::tunnelStatus(*window);
  QVERIFY(widget->isTunnelUsed());
  QVERIFY(widget->isVisibleTo(window.get()));
  QVERIFY(!Access::shell(*window));
  QVERIFY(window->findChild<QLabel *>(QStringLiteral("tunnelPathLine")) == nullptr);
  // the address field is hidden while the tunnel is used, and the tray menu has its action of the tunnel (as it always
  // had)
  QVERIFY(!window->findChild<QComboBox *>(QStringLiteral("cbHostname"))->isVisibleTo(window.get()));
  QCOMPARE(tunnelRows(*window).size(), 1);
  // (the Test connection button is the one that it is with the tunnel: it goes through the tunnel)
  QVERIFY(window->findChild<QPushButton *>(QStringLiteral("btnTestConnection"))->isEnabled());
}

void TunnelShellTests::tunnelStatusWidget_saysThePathLineToo()
{
  auto window = makeWindow(true, true);
  auto *widget = Access::tunnelStatus(*window);
  QCOMPARE(widget->pathText(), deskflow::tunnel::tunnelPathLine(QStringLiteral("mbp.example.com"), 24811));
  QSignalSpy spy(widget, &TunnelStatusWidget::pathTextChanged);
  Settings::setValue(Settings::Tunnel::LocalPort, 24812);
  Access::refreshTunnel(*window);
  QCOMPARE(spy.size(), 1);
  QCOMPARE(spy.first().first().toString(), deskflow::tunnel::tunnelPathLine(QStringLiteral("mbp.example.com"), 24812));
  // the same text again is not said again
  Access::refreshTunnel(*window);
  QCOMPARE(spy.size(), 1);
}

QTEST_MAIN(TunnelShellTests)
