/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutAuditTests.h"

#include "../FontEnvironment.h"
#include "../KoreanTranslator.h"
#include "../LayoutAudit.h"
#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/chrome/WindowChrome.h"
#include "gui/core/CoreProcess.h"
#include "gui/settings/SettingsPages.h"
#include "gui/shell/MainShell.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/TextSize.h"
#include "gui/widgets/NavRail.h"
#include "gui/widgets/StatusHero.h"

#include <QAccessible>
#include <QApplication>
#include <QLabel>
#include <QPixmapCache>
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
using namespace deskflow::gui::style;

// The height of the title bar of a Mac (what its Qt says of the safe area of a window)
constexpr int kTitleBar = 28;

koreantest::RecordingTranslator *s_translator = nullptr;

// The system as the test makes it up: the font that the system gives text, in pixels
struct FakeSystem
{
  int pixels = 13;
  TextPlatform platform = TextPlatform::Mac;

  TextSizeEnvironment environment()
  {
    TextSizeEnvironment env;
    env.systemFont = [this] { return font(); };
    env.osTextScale = [] { return 1.0; };
    env.platform = platform;
    return env;
  }

  QFont font() const
  {
    QFont font = QApplication::font();
    font.setPixelSize(pixels);
    return font;
  }
};

void installDeskflowStyle()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

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

std::unique_ptr<MainWindow> makeWindow(FakeSystem &system, int percent, Settings::CoreMode mode)
{
  installDeskflowStyle();
  Settings::setValue(Settings::Security::TlsEnabled, false);
  Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  Settings::setValue(Settings::Core::CoreMode, mode);
  Settings::setValue(Settings::Gui::TextSize, percent);
  QApplication::setFont(system.font());
  TextSize::install(system.environment());
  return std::make_unique<MainWindow>();
}

// A server that listens with four addresses: the line under its state that said "Suggested IP" on three lines
void sayFourAddresses(MainWindow &window)
{
  auto *label = window.findChild<QLabel *>(QStringLiteral("lblIpAddresses"));
  QVERIFY(label);
  const auto text = QCoreApplication::translate("MainWindow", "Suggested IP: ");
  const auto all = QCoreApplication::translate("MainWindow", "all %1 addresses").arg(4);
  label->setText(text + QStringLiteral("10.0.21.36 (<a href=\"addresses\">") + all + QStringLiteral("</a>)"));
}

QStringList pagesOf(MainShell *shell)
{
  QStringList pages;
  for (const auto *item : shell->rail()->items()) {
    if (item->kind() == NavItem::Kind::Page)
      pages << item->id();
  }
  pages << QStringLiteral("settings.client") << QStringLiteral("home");
  pages.removeDuplicates();
  return pages;
}

} // namespace

void LayoutAuditTests::initTestCase()
{
  // the width of text is what the audit measures: without fonts every letter is a box (see FontEnvironment.h)
  SKIP_WITHOUT_FONTS();
  QVERIFY(m_dir.isValid());
  qputenv("DESKFLOW_SECRET_STORE", "memory");
  qputenv("TMPDIR", m_dir.path().toUtf8());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
  Access::setWorkArea(QRect(0, 0, 1920, 1040));
}

void LayoutAuditTests::init()
{
  // the header of the new look is the one that has the title bar of a Mac in it (the offscreen platform would have
  // none)
  qputenv("DESKFLOW_WINDOW_FRAME", "client");
#ifdef Q_OS_MACOS
  deskflow::gui::chrome::WindowChrome::s_titleBarInset = [](const QWidget &) { return kTitleBar; };
#endif
  s_translator = nullptr;
}

void LayoutAuditTests::cleanup()
{
  if (s_translator) {
    QCoreApplication::removeTranslator(s_translator);
    delete s_translator;
    s_translator = nullptr;
  }
  deskflow::gui::chrome::WindowChrome::s_titleBarInset = nullptr;
  TextSize::uninstall();
  qunsetenv("DESKFLOW_WINDOW_FRAME");
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QApplication::setFont(QFont());
  QPixmapCache::clear();
  for (const auto &key :
       {Settings::Core::CoreMode, Settings::Core::ComputerName, Settings::Security::TlsEnabled,
        Settings::Gui::ShownServerFirstStartMessage, Settings::Gui::ShownFirstConnectedMessage,
        Settings::Gui::WindowGeometry, Settings::Gui::ShellWindowGeometry, Settings::Gui::TextSize})
    Settings::setValue(key);
}

void LayoutAuditTests::windowFits_atItsLeastSizeAndLarger_data()
{
  QTest::addColumn<int>("pixels");
  QTest::addColumn<bool>("windowsFont");
  QTest::addColumn<bool>("korean");
  QTest::addColumn<int>("percent");
  QTest::addColumn<bool>("client");

  // The floor of the font of a Mac is 13 px and the one of Windows 14 px; the steps of the setting are the ones that
  // the person has; the two modes; the two languages that the program is used in
  for (const bool windowsFont : {false, true}) {
    for (const bool korean : {false, true}) {
      for (const int percent : {90, 100, 115, 130, 150}) {
        for (const bool client : {false, true}) {
          QTest::addRow(
              "%s %s %d %% %s", windowsFont ? "Windows 14 px" : "Mac 13 px", korean ? "ko" : "en", percent,
              client ? "client" : "server"
          ) << (windowsFont ? 14 : 13)
            << windowsFont << korean << percent << client;
        }
      }
    }
  }
}

void LayoutAuditTests::windowFits_atItsLeastSizeAndLarger()
{
  QFETCH(int, pixels);
  QFETCH(bool, windowsFont);
  QFETCH(bool, korean);
  QFETCH(int, percent);
  QFETCH(bool, client);

  if (korean) {
    s_translator = new koreantest::RecordingTranslator;
    QVERIFY2(s_translator->loadShipped(), qPrintable(koreantest::shippedQm()));
    QVERIFY(QCoreApplication::installTranslator(s_translator));
  }
  writeComputers();
  FakeSystem system;
  system.pixels = pixels;
  system.platform = windowsFont ? TextPlatform::Windows : TextPlatform::Mac;
  auto window = makeWindow(system, percent, client ? Settings::CoreMode::Client : Settings::CoreMode::Server);
  auto *shell = Access::shell(*window);
  QVERIFY(shell);
#ifdef Q_OS_MACOS
  layoutaudit::emulateMacTitleBar(*window, kTitleBar);
#endif
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));
  if (!client) {
    Access::states(*window, deskflow::core::ProcessState::Started, deskflow::core::ConnectionState::Listening);
    sayFourAddresses(*window);
  }

  const auto least = window->minimumSize();
  const QList<QSize> sizes{
      least, QSize(qMax(least.width(), 900), least.height()), QSize(1000, qMax(least.height(), 760))
  };
  QStringList found;
  for (const auto &size : sizes) {
    window->resize(size);
    QTest::qWait(20);
    for (const auto &page : pagesOf(shell)) {
      shell->showPage(page);
      // a page makes some of its groups when it is shown, and the layout settles from the event loop
      QTest::qWait(20);
      for (const auto &line : layoutaudit::audit(*window)) {
        found << QStringLiteral("%1x%2 %3: %4").arg(window->width()).arg(window->height()).arg(page, line);
      }
    }
  }
  found.removeDuplicates();
  QVERIFY2(
      found.isEmpty(), qPrintable(QStringLiteral("%1 violations (least size %2x%3):\n  %4")
                                      .arg(found.size())
                                      .arg(least.width())
                                      .arg(least.height())
                                      .arg(found.mid(0, 40).join(QStringLiteral("\n  "))))
  );
}

void LayoutAuditTests::addressLabel_usesAsFewLinesAsItsRoomAllows_data()
{
  QTest::addColumn<bool>("korean");
  QTest::addColumn<int>("width");

  for (const bool korean : {false, true}) {
    for (const int width : {720, 760, 900, 1000, 1200})
      QTest::addRow("%s, %d wide", korean ? "ko" : "en", width) << korean << width;
  }
}

void LayoutAuditTests::addressLabel_usesAsFewLinesAsItsRoomAllows()
{
  QFETCH(bool, korean);
  QFETCH(int, width);

  if (korean) {
    s_translator = new koreantest::RecordingTranslator;
    QVERIFY2(s_translator->loadShipped(), qPrintable(koreantest::shippedQm()));
    QVERIFY(QCoreApplication::installTranslator(s_translator));
  }
  writeComputers();
  FakeSystem system;
  auto window = makeWindow(system, 100, Settings::CoreMode::Server);
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));
  Access::states(*window, deskflow::core::ProcessState::Started, deskflow::core::ConnectionState::Listening);
  sayFourAddresses(*window);
  window->resize(width, 760);
  QCoreApplication::processEvents();

  auto *label = window->findChild<QLabel *>(QStringLiteral("lblIpAddresses"));
  QVERIFY(label && label->isVisible());
  // the column of the text of the state has the room for the whole text from 900 px: it is on one line
  const int oneLine = QFontMetrics(label->font()).height();
  const int linesNow = label->heightForWidth(label->width()) / oneLine;
  if (width >= 900) {
    QVERIFY2(
        linesNow == 1,
        qPrintable(QStringLiteral("the label is %1 px wide and has %2 lines").arg(label->width()).arg(linesNow))
    );
  }
  // and, whatever the width, never "label / address ( / link": at most two lines, and the audit finds no room that the
  // text left unused
  QVERIFY2(linesNow <= 2, qPrintable(QStringLiteral("%1 lines at %2 px").arg(linesNow).arg(label->width())));
  const auto found = layoutaudit::audit(*window);
  QStringList lines;
  for (const auto &line : found) {
    if (line.startsWith(QLatin1String("lines:")))
      lines << line;
  }
  QVERIFY2(lines.isEmpty(), qPrintable(lines.join(QStringLiteral("\n"))));
}

void LayoutAuditTests::theAuditSeesWhatItIsMadeToSee()
{
  // the walk is only worth something if it finds the faults that it is for: a widget over its sibling, a widget that is
  // outside of its parent, a widget that has less than it asks for, and a text that wraps while it has the room
  QWidget root;
  root.resize(300, 200);
  auto *first = new QLabel(QStringLiteral("first"), &root);
  first->setGeometry(10, 10, 100, 30);
  auto *second = new QLabel(QStringLiteral("second"), &root);
  second->setGeometry(60, 20, 100, 30);
  auto *outside = new QLabel(QStringLiteral("outside"), &root);
  outside->setGeometry(250, 150, 100, 30);
  auto *wraps = new QLabel(QStringLiteral("a text that is long and wraps at a space although the line is free"), &root);
  wraps->setWordWrap(true);
  wraps->setGeometry(10, 100, 60, 90);
  root.show();
  QVERIFY(QTest::qWaitForWindowExposed(&root));

  layoutaudit::Options options;
  const auto found = layoutaudit::audit(root, options);
  const auto joined = found.join(QStringLiteral("\n"));
  QVERIFY2(joined.contains(QLatin1String("overlap: ")), qPrintable(joined));
  QVERIFY2(joined.contains(QLatin1String("inside: ")), qPrintable(joined));
  QVERIFY2(joined.contains(QLatin1String("lines: ")), qPrintable(joined));

  auto *narrow = new QLabel(QStringLiteral("a text that has no room"), &root);
  narrow->setGeometry(10, 60, 20, 10);
  narrow->show();
  QVERIFY2(layoutaudit::audit(root).join(QStringLiteral("\n")).contains(QLatin1String("squeezed: ")), "squeezed");
}

QTEST_MAIN(LayoutAuditTests)
