/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "KoreanLookTests.h"

#include "../FontEnvironment.h"
#include "../KoreanTranslator.h"
#include "common/Enums.h"
#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/core/CoreProcess.h"
#include "gui/dialogs/AboutDialog.h"
#include "gui/dialogs/ConnectionTestDialog.h"
#include "gui/dialogs/ServerAddressesDialog.h"
#include "gui/settings/SettingsPages.h"
#include "gui/shell/ClientConnectionSection.h"
#include "gui/shell/MainShell.h"
#include "gui/shell/TunnelChip.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/PanelWidget.h"
#include "gui/tunnel/TunnelTypes.h"
#include "gui/widgets/NavRail.h"
#include "gui/widgets/SegmentedControl.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SidebarShell.h"
#include "gui/widgets/StatusHero.h"

#include <QAccessible>
#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QPixmapCache>
#include <QRegularExpression>
#include <QScopeGuard>

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
using namespace deskflow::gui;
using deskflow::core::ConnectionState;
using deskflow::core::ProcessState;

// the translation that is shipped, and what it is asked for
koreantest::RecordingTranslator *s_translator = nullptr;

void installDeskflowStyle()
{
  auto *style = new style::DeskflowStyle;
  QApplication::setStyle(style);
  style::Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

std::unique_ptr<MainWindow> makeWindow(Settings::CoreMode mode, bool tunnel = false)
{
  installDeskflowStyle();
  Settings::setValue(Settings::Security::TlsEnabled, tunnel);
  Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  Settings::setValue(Settings::Core::CoreMode, mode);
  if (tunnel) {
    Settings::setValue(Settings::Tunnel::Enabled, true);
    Settings::setValue(Settings::Tunnel::Hostname, QStringLiteral("mbp.example.com"));
    Settings::setValue(Settings::Tunnel::LocalPort, 24811);
  }
  return std::make_unique<MainWindow>();
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

void showWindow(MainWindow &window)
{
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QCoreApplication::processEvents();
}

// What was asked for and is not translated, as the message of a failure: one name on each line
QString namesOf(const QStringList &missing)
{
  return QStringLiteral("not translated (%1):\n  %2").arg(missing.size()).arg(missing.join(QStringLiteral("\n  ")));
}

// A font as larger text makes it (the steps of the setting are 90, 100, 115, 130 and 150 percent)
QFont scaledFont(QFont font, int basePixels, qreal scale)
{
  if (basePixels > 0)
    font.setPixelSize(qRound(basePixels * scale));
  else if (font.pointSizeF() > 0)
    font.setPointSizeF(font.pointSizeF() * scale);
  else
    font.setPixelSize(qRound(font.pixelSize() * scale));
  return font;
}

// where a widget is seen in the window: a panel is the rectangle inside of its shadow
QRect rectIn(const QWidget &window, const QWidget *widget)
{
  const auto *panel = qobject_cast<const style::PanelWidget *>(widget);
  const auto local = panel ? panel->panelRect() : widget->rect();
  return QRect(widget->mapTo(&window, local.topLeft()), local.size());
}

} // namespace

void KoreanLookTests::initTestCase()
{
  // the width of text is what the layout test measures: without fonts every letter is a box (see FontEnvironment.h)
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

void KoreanLookTests::init()
{
  // what a person with a Korean system has: the translation of the program is the last one that is installed, and the
  // first that is asked
  s_translator = new koreantest::RecordingTranslator;
  QVERIFY2(s_translator->loadShipped(), qPrintable(koreantest::shippedQm()));
  QVERIFY(QCoreApplication::installTranslator(s_translator));
}

void KoreanLookTests::cleanup()
{
  QCoreApplication::removeTranslator(s_translator);
  delete s_translator;
  s_translator = nullptr;
  qunsetenv("DESKFLOW_WINDOW_FRAME");
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

void KoreanLookTests::translation_isShippedAndTranslatesTheWordsOfTheShell()
{
  // the translation is the one that is shipped and the recorder asks it: a word that it has comes back in Korean ...
  const char *shellContext = "deskflow::gui::MainShell";
  const char *home = "Home";
  const char *settings = "Settings";
  QCOMPARE(QCoreApplication::translate(shellContext, home), QStringLiteral("홈"));
  QCOMPARE(QCoreApplication::translate(shellContext, settings), QStringLiteral("설정"));
  QVERIFY(s_translator->missing().isEmpty());
  // ... and a word that it does not have is the English, and is named: this is what the tests below rely on
  // (the words are in variables: lupdate reads the words that are written in the call, and would add them to the file)
  const char *sentence = "A sentence that no translation has";
  const char *sentenceContext = "KoreanLookTests";
  QCOMPARE(
      QCoreApplication::translate(sentenceContext, sentence), QStringLiteral("A sentence that no translation has")
  );
  QCOMPARE(
      s_translator->missing(), QStringList{QStringLiteral("KoreanLookTests | A sentence that no translation has")}
  );
  // the texts of Qt itself are not the program's, and a text without a letter is the same in every language
  s_translator->forget();
  const char *qtContext = "QDialogButtonBox";
  const char *qtWord = "Cancel";
  const char *numbers = "%1 × %2";
  QCoreApplication::translate(qtContext, qtWord);
  QCoreApplication::translate(sentenceContext, numbers);
  QVERIFY(s_translator->missing().isEmpty());
}

void KoreanLookTests::mainWindow_asksForNothingThatIsNotTranslated_data()
{
  QTest::addColumn<bool>("client");
  QTest::addColumn<bool>("tunnel");
  QTest::addColumn<bool>("clientSideFrame");

  // the two modes; a client that connects through the tunnel (its card, its chip, its line); and the window that has
  // the header of the new look, with the button of the menu (Windows and Linux)
  QTest::newRow("server") << false << false << false;
  QTest::newRow("client") << true << false << false;
  QTest::newRow("client, tunnel") << true << true << false;
  QTest::newRow("server, the header of the new look") << false << false << true;
  QTest::newRow("client, tunnel, the header of the new look") << true << true << true;
}

void KoreanLookTests::mainWindow_asksForNothingThatIsNotTranslated()
{
  QFETCH(bool, client);
  QFETCH(bool, tunnel);
  QFETCH(bool, clientSideFrame);

  if (clientSideFrame)
    qputenv("DESKFLOW_WINDOW_FRAME", "client");
  auto window = makeWindow(client ? Settings::CoreMode::Client : Settings::CoreMode::Server, tunnel);
  auto *shell = Access::shell(*window);
  QVERIFY(shell);
  showWindow(*window);

  // every page of the sidebar is shown: the home page, the pages of the settings and the page of the client
  QStringList pages;
  for (const auto *item : shell->rail()->items())
    pages << item->id();
  QVERIFY(pages.size() >= 5);
  for (const auto &id : std::as_const(pages))
    shell->showPage(id);
  shell->showPage(QStringLiteral("settings.client"));
  shell->showPage(QStringLiteral("home"));
  QCoreApplication::processEvents();

  QVERIFY2(s_translator->missing().isEmpty(), qPrintable(namesOf(s_translator->missing())));
}

void KoreanLookTests::mainWindow_everyStateOfTheCoreAsksForNothingThatIsNotTranslated()
{
  qputenv("DESKFLOW_WINDOW_FRAME", "client");
  for (const bool client : {false, true}) {
    auto window = makeWindow(client ? Settings::CoreMode::Client : Settings::CoreMode::Server, client);
    showWindow(*window);
    // the texts of the hero, the chips, the tray and the status line, for each state of the process and of the link
    for (const auto process :
         {ProcessState::Starting, ProcessState::Started, ProcessState::Stopping, ProcessState::Stopped,
          ProcessState::RetryPending}) {
      for (const auto connection :
           {ConnectionState::Disconnected, ConnectionState::Connecting, ConnectionState::Connected,
            ConnectionState::Listening}) {
        Access::states(*window, process, connection);
      }
    }
  }
  QVERIFY2(s_translator->missing().isEmpty(), qPrintable(namesOf(s_translator->missing())));
}

void KoreanLookTests::settingsPages_askForNothingThatIsNotTranslated()
{
  installDeskflowStyle();
  settings::SettingsPages pages;
  QVERIFY(pages.entries().size() >= 6);
  QVERIFY2(s_translator->missing().isEmpty(), qPrintable(namesOf(s_translator->missing())));
}

void KoreanLookTests::dialogs_askForNothingThatIsNotTranslated()
{
  installDeskflowStyle();
  using deskflow::diagnosis::FailureKind;
  using deskflow::diagnosis::Hint;
  // the dialogs of the new look that the main window opens
  AboutDialog about;
  const QList<AddressInfo> addresses{
      {QStringLiteral("10.0.21.36"), QStringLiteral("en7"), AddressKind::Lan},
      {QStringLiteral("100.76.209.252"), QStringLiteral("utun100"), AddressKind::Overlay},
      {QStringLiteral("172.28.80.1"), QStringLiteral("vEthernet"), AddressKind::Virtual},
      {QStringLiteral("169.254.88.9"), QStringLiteral("Ethernet 3"), AddressKind::LinkLocal},
  };
  ServerAddressesDialog serverAddresses(nullptr, addresses);

  // the words of the test of the connection: what each hint says, and what each result means
  for (int value = static_cast<int>(Hint::ServerFirewall); value <= static_cast<int>(Hint::SlowNetwork); ++value)
    ConnectionTestDialog::hintText(static_cast<Hint>(value), 24800, 2.0);
  ProbeResult result;
  result.host = result.address = QStringLiteral("10.0.0.5");
  result.port = 24800;
  result.open = true;
  result.elapsedMs = 12;
  ConnectionTestDialog::statusText(result, 2.0);
  result.elapsedMs = 2500;
  ConnectionTestDialog::statusText(result, 2.0);
  result.open = false;
  for (const auto kind :
       {FailureKind::TimedOut, FailureKind::Refused, FailureKind::NoRoute, FailureKind::DnsFailure, FailureKind::Reset,
        FailureKind::AccessDenied, FailureKind::Unknown}) {
    result.kind = kind;
    ConnectionTestDialog::statusText(result, 2.0);
  }
  QVERIFY2(s_translator->missing().isEmpty(), qPrintable(namesOf(s_translator->missing())));
}

void KoreanLookTests::tunnelTexts_everyFailureAndStateAsksForNothingThatIsNotTranslated()
{
  using namespace deskflow::tunnel;
  // the line of the path, the line and the chip of the state in each state of the tunnel and of the link to the server,
  // and the sentence of every failure
  tunnelPathLine(QString(), 0);
  tunnelPathLine(QStringLiteral("mbp.example.com"), 24811);
  for (int state = static_cast<int>(TunnelState::Idle); state <= static_cast<int>(TunnelState::Failed); ++state) {
    for (int failure = static_cast<int>(TunnelFailure::None);
         failure <= static_cast<int>(TunnelFailure::SettingsChanged); ++failure) {
      TunnelStatus status;
      status.state = static_cast<TunnelState>(state);
      status.failure = static_cast<TunnelFailure>(failure);
      status.hostname = QStringLiteral("mbp.example.com");
      status.localPort = 24811;
      status.exitCode = 3;
      status.restartInSeconds = 4;
      for (const auto link :
           {ServerLink::Unknown, ServerLink::Connecting, ServerLink::Connected, ServerLink::Reconnecting}) {
        tunnelStatusLine(status, link);
        // the chip beside the state of sharing says it in a few words
        tunnelChip(status, link);
      }
    }
  }
  for (int failure = static_cast<int>(TunnelFailure::None); failure <= static_cast<int>(TunnelFailure::SettingsChanged);
       ++failure)
    failureText(static_cast<TunnelFailure>(failure), 3);
  QVERIFY2(s_translator->missing().isEmpty(), qPrintable(namesOf(s_translator->missing())));
}

void KoreanLookTests::sidebar_saysWhatTheTranslationSays()
{
  auto window = makeWindow(Settings::CoreMode::Server);
  auto *shell = Access::shell(*window);
  showWindow(*window);
  // the first page of a server is its computers, and of a client its connection
  QCOMPARE(shell->rail()->item(QStringLiteral("home"))->text(), QStringLiteral("컴퓨터"));
  QCOMPARE(shell->pageTitle()->text(), QStringLiteral("컴퓨터"));
  QStringList texts;
  QStringList english;
  for (const auto *item : shell->rail()->items()) {
    // the entry of an update that can be installed has no text until there is one
    if (item->text().isEmpty())
      continue;
    texts << item->text();
    // the entries of the pages and of the settings are in Korean: each text has a Hangul syllable in it
    if (!item->text().contains(QRegularExpression(QStringLiteral("[가-힣]"))))
      english << QStringLiteral("%1: \"%2\"").arg(item->id(), item->text());
  }
  QVERIFY2(english.isEmpty(), qPrintable(english.join(QLatin1Char('\n'))));
  QVERIFY(texts.size() >= 5);
}

void KoreanLookTests::mainWindow_fitsAtEveryWidthAndTextStep_inKorean_data()
{
  QTest::addColumn<int>("width");
  QTest::addColumn<qreal>("fontScale");
  QTest::addColumn<int>("basePixels");
  QTest::addColumn<bool>("client");

  // the narrow widths that the window promises, one that shows the labels of the sidebar and a wide one; the steps of
  // larger text of the setting (115, 130 and 150 percent) on the body of Windows 11 (14 px) and on the font of the
  // platform of the test
  for (const int basePixels : {0, 14}) {
    for (const int width : {720, 760, 900, 1280}) {
      for (const qreal scale : {1.0, 1.15, 1.3, 1.5}) {
        for (const bool client : {false, true}) {
          QTest::addRow(
              "%s%d px, x%.2f, %s", basePixels ? "14 px body, " : "", width, scale, client ? "client" : "server"
          ) << width
            << scale << basePixels << client;
        }
      }
    }
  }
}

void KoreanLookTests::mainWindow_fitsAtEveryWidthAndTextStep_inKorean()
{
  QFETCH(int, width);
  QFETCH(qreal, fontScale);
  QFETCH(int, basePixels);
  QFETCH(bool, client);

  writeComputers();
  auto window = makeWindow(client ? Settings::CoreMode::Client : Settings::CoreMode::Server);
  auto *shell = Access::shell(*window);
  window->setFont(scaledFont(window->font(), basePixels, fontScale));
  showWindow(*window);
  window->resize(width, 760);
  QTRY_COMPARE(window->width(), width);
  QCoreApplication::processEvents();

  // the width that the person asks for is the width that the window has: the longer words need no more than this
  QCOMPARE(window->size().width(), width);
  QVERIFY2(
      window->minimumSizeHint().width() <= width,
      qPrintable(QStringLiteral("minimum %1").arg(window->minimumSizeHint().width()))
  );
  const auto bounds = window->rect();
  for (const QWidget *widget :
       {static_cast<QWidget *>(shell->hero()), static_cast<QWidget *>(shell->modeControl()),
        static_cast<QWidget *>(shell->rail()), static_cast<QWidget *>(shell->pageTitle())}) {
    QVERIFY2(!widget->isHidden(), widget->metaObject()->className());
    QVERIFY2(
        bounds.contains(rectIn(*window, widget)),
        qPrintable(QStringLiteral("%1 is out of the window: %2,%3 %4x%5 in %6x%7")
                       .arg(QString::fromLatin1(widget->metaObject()->className()))
                       .arg(rectIn(*window, widget).x())
                       .arg(rectIn(*window, widget).y())
                       .arg(rectIn(*window, widget).width())
                       .arg(rectIn(*window, widget).height())
                       .arg(bounds.width())
                       .arg(bounds.height()))
    );
  }
  // nothing is squeezed to less than it needs
  QVERIFY(shell->hero()->contentsRect().width() >= shell->hero()->layout()->minimumSize().width());
  QVERIFY(shell->modeControl()->width() >= shell->modeControl()->minimumSizeHint().width());
  QWidget *modePage =
      client ? static_cast<QWidget *>(shell->clientSection()) : static_cast<QWidget *>(shell->computersCard());
  QVERIFY(modePage->isVisibleTo(window.get()));
  QVERIFY(rectIn(*window, modePage).left() >= 0 && rectIn(*window, modePage).right() <= bounds.right());
  QVERIFY(rectIn(*window, modePage).width() >= modePage->minimumSizeHint().width());
  // the hero and the control of the mode are not on each other
  QVERIFY(
      rectIn(*window, shell->hero()).bottom() < rectIn(*window, shell->modeControl()).top() ||
      rectIn(*window, shell->modeControl()).bottom() < rectIn(*window, shell->hero()).top()
  );
  // every entry of the sidebar has the room for its text (in the rail that shows labels: from the width of 900)
  QCOMPARE(shell->sidebar()->isRailExpanded(), width >= 900);
  // and the pages of the settings are as wide as the window lets them be, whatever is written on them
  for (const auto *item : shell->rail()->items()) {
    if (item->id() == QLatin1String("home") || item->isHidden())
      continue;
    shell->showPage(item->id());
    QCoreApplication::processEvents();
    QVERIFY2(
        shell->pageTitle()->sizeHint().width() <= bounds.width(),
        qPrintable(QStringLiteral("the title of %1").arg(item->id()))
    );
  }
}

QTEST_MAIN(KoreanLookTests)
