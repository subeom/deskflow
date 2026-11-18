/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TextSizeShellTests.h"

#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/core/CoreProcess.h"
#include "gui/settings/SettingsPage.h"
#include "gui/settings/SettingsPages.h"
#include "gui/shell/MainShell.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/PanelWidget.h"
#include "gui/style/TextSize.h"
#include "gui/widgets/NavRail.h"
#include "gui/widgets/SegmentedControl.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/SidebarShell.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/StatusHero.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFontInfo>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPixmapCache>
#include <QPushButton>
#include <QScrollArea>

#include <memory>

struct MainWindowTestAccess
{
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
  static deskflow::gui::settings::SettingsPages *settingsPages(MainWindow &w)
  {
    return w.m_settingsPages;
  }
};

namespace {

using Access = MainWindowTestAccess;
using namespace deskflow::gui;
using namespace deskflow::gui::style;

// The system as the test makes it up: a font in pixels of a family, and what the OS says about the size of text
struct FakeSystem
{
  int pixels = 12;
  QString family;
  double osScale = 1.0;
  TextPlatform platform = TextPlatform::Windows;

  TextSizeEnvironment environment()
  {
    TextSizeEnvironment env;
    env.systemFont = [this] { return font(); };
    env.osTextScale = [this] { return osScale; };
    env.platform = platform;
    return env;
  }

  QFont font() const
  {
    QFont font = QApplication::font();
    if (!family.isEmpty())
      font.setFamily(family);
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

// The service and a window as the application makes them with the new look: the core is not started
std::unique_ptr<MainWindow> makeWindow(
    FakeSystem &system, int percent, Settings::CoreMode mode = Settings::CoreMode::Server, bool withService = true
)
{
  installDeskflowStyle();
  Settings::setValue(Settings::Security::TlsEnabled, false);
  Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  Settings::setValue(Settings::Core::CoreMode, mode);
  Settings::setValue(Settings::Gui::TextSize, percent);
  if (withService) {
    QApplication::setFont(system.font());
    TextSize::install(system.environment());
  }
  return std::make_unique<MainWindow>();
}

void showWindow(MainWindow &window)
{
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QCoreApplication::processEvents();
}

// The round badge of the state: the child of the hero that has a size of its own, as high as it is wide
QSize orbSizeOf(const QWidget &hero)
{
  for (auto *child : hero.findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
    if (child->minimumSize() == child->maximumSize() && child->minimumWidth() == child->minimumHeight() &&
        child->minimumWidth() > 40)
      return child->minimumSize();
  }
  return {};
}

int fontPixels(const QWidget *widget)
{
  return QFontInfo(widget->font()).pixelSize();
}

QRect rectIn(const QWidget &window, const QWidget *widget)
{
  const auto *panel = qobject_cast<const PanelWidget *>(widget);
  const auto local = panel ? panel->panelRect() : widget->rect();
  return QRect(widget->mapTo(&window, local.topLeft()), local.size());
}

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

void TextSizeShellTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  qputenv("TMPDIR", m_dir.path().toUtf8());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
  // the screen of a test is 800x600; the windows open as they do on a desktop
  Access::setWorkArea(QRect(0, 0, 1920, 1040));
}

void TextSizeShellTests::cleanup()
{
  TextSize::uninstall();
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QApplication::setFont(QFont());
  QPixmapCache::clear();
  for (const auto &key :
       {Settings::Gui::TextSize, Settings::Gui::SidebarCollapsed, Settings::Gui::ShellLogExpanded,
        Settings::Gui::WindowGeometry, Settings::Gui::ShellWindowGeometry, Settings::Core::CoreMode,
        Settings::Security::TlsEnabled, Settings::Gui::ShownServerFirstStartMessage,
        Settings::Gui::ShownFirstConnectedMessage})
    Settings::setValue(key);
}

void TextSizeShellTests::systemLook_hasNoKeysForTheSizeOfText()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  Settings::setValue(Settings::Security::TlsEnabled, false);
  Settings::setValue(Settings::Gui::ShownServerFirstStartMessage, true);
  Settings::setValue(Settings::Gui::ShownFirstConnectedMessage, true);
  MainWindow window;
  QVERIFY(!window.findChild<QAction *>(QStringLiteral("actionTextLarger")));
  QVERIFY(!window.findChild<QAction *>(QStringLiteral("actionTextSmaller")));
  QVERIFY(!window.findChild<QAction *>(QStringLiteral("actionTextReset")));
  QVERIFY(!TextSize::instance());
  QCOMPARE(metricScale(), 1.0);
}

void TextSizeShellTests::keys_makeTheTextLargerAndSmaller_andGoBackToAutomatic()
{
  FakeSystem system;
  auto window = makeWindow(system, 100);
  showWindow(*window);
  window->activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(window.get()));
  auto *service = TextSize::instance();
  QVERIFY(service);
  QCOMPARE(QFontInfo(QApplication::font()).pixelSize(), 14);

  // larger: Ctrl+= (and Ctrl++ for the keyboards that have the plus without Shift)
  QTest::keySequence(window.get(), QKeySequence(Qt::CTRL | Qt::Key_Equal));
  QCOMPARE(Settings::value(Settings::Gui::TextSize).toInt(), 115);
  QCOMPARE(service->percent(), 115);
  QTest::keySequence(window.get(), QKeySequence(Qt::CTRL | Qt::Key_Plus));
  QCOMPARE(service->percent(), 130);
  QCOMPARE(QFontInfo(QApplication::font()).pixelSize(), 18);

  // the end is the end
  QTest::keySequence(window.get(), QKeySequence(Qt::CTRL | Qt::Key_Equal));
  QTest::keySequence(window.get(), QKeySequence(Qt::CTRL | Qt::Key_Equal));
  QCOMPARE(service->percent(), 150);

  // back to Automatic
  QTest::keySequence(window.get(), QKeySequence(Qt::CTRL | Qt::Key_0));
  QCOMPARE(service->percent(), 100);
  QCOMPARE(Settings::value(Settings::Gui::TextSize).toInt(), 100);
  QCOMPARE(QFontInfo(QApplication::font()).pixelSize(), 14);

  // smaller
  QTest::keySequence(window.get(), QKeySequence(Qt::CTRL | Qt::Key_Minus));
  QCOMPARE(service->percent(), 90);
  QTest::keySequence(window.get(), QKeySequence(Qt::CTRL | Qt::Key_Minus));
  QCOMPARE(service->percent(), 90);
}

void TextSizeShellTests::theChoiceInTheSettings_followsTheKeys_andTheKeysFollowIt()
{
  FakeSystem system;
  auto window = makeWindow(system, 100);
  showWindow(*window);
  auto *general = Access::settingsPages(*window)->page(QStringLiteral("settings.general"));
  auto *combo = general->findChild<QComboBox *>(QStringLiteral("comboTextSize"));
  QVERIFY(combo);

  // the five steps, in order, Automatic is the one that is chosen
  QCOMPARE(combo->count(), 5);
  QStringList names;
  for (int i = 0; i < combo->count(); ++i)
    names << combo->itemText(i);
  QCOMPARE(
      names, (QStringList{
                 QStringLiteral("Smaller (90 %)"), QStringLiteral("Automatic"), QStringLiteral("Large (115 %)"),
                 QStringLiteral("Larger (130 %)"), QStringLiteral("Largest (150 %)")
             })
  );
  QCOMPARE(combo->currentData().toInt(), 100);

  // a choice is written at once, and the text follows
  combo->setCurrentIndex(3);
  Q_EMIT combo->activated(3);
  QCOMPARE(Settings::value(Settings::Gui::TextSize).toInt(), 130);
  QCOMPARE(TextSize::instance()->percent(), 130);
  QCOMPARE(QFontInfo(QApplication::font()).pixelSize(), 18);

  // a key moves the choice
  window->findChild<QAction *>(QStringLiteral("actionTextLarger"))->trigger();
  QCOMPARE(combo->currentData().toInt(), 150);
  window->findChild<QAction *>(QStringLiteral("actionTextReset"))->trigger();
  QCOMPARE(combo->currentData().toInt(), 100);
  QCOMPARE(QFontInfo(QApplication::font()).pixelSize(), 14);

  // the description says the keys of this computer
  auto *row = general->findChild<SettingRow *>();
  QVERIFY(row);
  const auto hint = QKeySequence(Qt::CTRL | Qt::Key_Equal).toString(QKeySequence::NativeText);
  bool found = false;
  for (auto *label : general->findChildren<QLabel *>())
    found = found || label->text().contains(hint);
  QVERIFY2(found, qPrintable(hint));
}

void TextSizeShellTests::theControlsFollowTheText_andStayAsDrawnAtTheSizeOfTheFloor()
{
  FakeSystem system;
  // the floor of Windows is 14 px, the size that everything was drawn for
  {
    auto window = makeWindow(system, 100);
    showWindow(*window);
    QCOMPARE(metricScale(), 1.0);
    QCOMPARE(NavItem::itemHeight(), NavItem::kHeight);
    QCOMPARE(SegmentedControl::controlHeight(), SegmentedControl::kHeight);
    QCOMPARE(SettingRow::minimumRowHeight(), 44);
    QCOMPARE(ToggleSwitch::trackSize(false), QSize(46, 28));
    QCOMPARE(NavRail::expandedPanelWidth(), 236);
    QCOMPARE(MainShell::minimumWindowWidth(), MainShell::kMinimumWindowWidth);
    QCOMPARE(fontPixels(Access::shell(*window)->pageTitle()), 26);
    QCOMPARE(orbSizeOf(*Access::shell(*window)->hero()), QSize(64, 64));
    // the least height of a button or an input of the style
    QCOMPARE(QLineEdit().sizeHint().height() >= 30, true);
  }
  TextSize::uninstall();

  // 150 % of that: the controls are half as large again, as the text is
  {
    auto window = makeWindow(system, 150);
    showWindow(*window);
    QCOMPARE(QFontInfo(QApplication::font()).pixelSize(), 21);
    QCOMPARE(metricScale(), 1.5);
    QCOMPARE(NavItem::itemHeight(), 66);
    QCOMPARE(SegmentedControl::controlHeight(), 66);
    QCOMPARE(SettingRow::minimumRowHeight(), 66);
    QCOMPARE(ToggleSwitch::trackSize(false), QSize(69, 42));
    QCOMPARE(NavRail::expandedPanelWidth(), 354);
    QCOMPARE(NavRail::collapsedPanelWidth(), 114);
    QCOMPARE(MainShell::minimumWindowWidth(), 1080);
    // the roles are ratios of the body: the title is 26 / 14 of 21
    QCOMPARE(fontPixels(Access::shell(*window)->pageTitle()), 39);
    // the badge of the state, and the least height of an input that the style gives: half as large again
    QCOMPARE(orbSizeOf(*Access::shell(*window)->hero()), QSize(96, 96));
    QVERIFY2(QLineEdit().sizeHint().height() >= 45, qPrintable(QString::number(QLineEdit().sizeHint().height())));
    QVERIFY2(QPushButton(QStringLiteral("x")).sizeHint().height() >= 45, "a button");
    auto *chip =
        Access::shell(*window)->hero()->chips().isEmpty() ? nullptr : Access::shell(*window)->hero()->chips().first();
    if (chip)
      QCOMPARE(chip->sizeHint().height(), 36);
  }
}

void TextSizeShellTests::anOpenWindow_isLaidOutAsANewOneIs_whenTheTextSizeChanges()
{
  FakeSystem system;
  // what a window asks for at 130 %, made new
  QSize heroNew;
  QSize modeNew;
  int railNew = 0;
  int titleNew = 0;
  int rowNew = 0;
  {
    auto window = makeWindow(system, 130);
    window->resize(1300, 800);
    showWindow(*window);
    auto *shell = Access::shell(*window);
    heroNew = shell->hero()->minimumSizeHint();
    modeNew = shell->modeControl()->sizeHint();
    railNew = shell->rail()->panelWidth();
    titleNew = fontPixels(shell->pageTitle());
    rowNew = SettingRow::minimumRowHeight();
  }
  TextSize::uninstall();

  // the same window made at Automatic, and then told to be 130 %
  auto window = makeWindow(system, 100);
  window->resize(1300, 800);
  showWindow(*window);
  auto *shell = Access::shell(*window);
  QVERIFY(shell->hero()->minimumSizeHint() != heroNew);
  TextSize::instance()->setPercent(130);
  QCoreApplication::processEvents();
  QCOMPARE(shell->hero()->minimumSizeHint(), heroNew);
  QCOMPARE(shell->modeControl()->sizeHint(), modeNew);
  QCOMPARE(shell->rail()->panelWidth(), railNew);
  QCOMPARE(fontPixels(shell->pageTitle()), titleNew);
  QCOMPARE(SettingRow::minimumRowHeight(), rowNew);
  // every row of the pages is as high as it asks
  for (auto *row : window->findChildren<SettingRow *>())
    QVERIFY2(row->minimumHeight() >= rowNew, qPrintable(row->objectName()));
}

void TextSizeShellTests::theWindowAndTheSidebarFollowTheSizeOfText()
{
  FakeSystem system;
  auto window = makeWindow(system, 100);
  showWindow(*window);
  auto *shell = Access::shell(*window);
  QCOMPARE(window->minimumWidth(), 720);
  QCOMPARE(shell->sidebar()->autoCollapseWidth(), 900);

  TextSize::instance()->setPercent(130);
  // 130 % of 14 px is 18.2 px: the controls are 1.3 as large, and the window can be 936 wide at the least, or as wide
  // as the screen when that is less (but not less than the 720 that it was drawn for)
  const auto screenWidth = window->screen()->availableGeometry().width();
  QCOMPARE(window->minimumWidth(), qMin(936, qMax(720, screenWidth)));
  QCOMPARE(shell->sidebar()->autoCollapseWidth(), 1170);
  QCOMPARE(shell->rail()->panelWidth(), NavRail::expandedPanelWidth());

  // a window that is as wide as it was fits at the new size of text, with the sidebar folded
  window->resize(1000, 760);
  QCoreApplication::processEvents();
  QVERIFY(shell->sidebar()->isNarrow());
  window->resize(1300, 760);
  QCoreApplication::processEvents();
  QVERIFY(!shell->sidebar()->isNarrow());

  // an explicit width stays what it was set to
  shell->sidebar()->setAutoCollapseWidth(500);
  TextSize::instance()->setPercent(150);
  QCOMPARE(shell->sidebar()->autoCollapseWidth(), 500);

  // and back to the width that it was drawn for
  TextSize::instance()->setPercent(100);
  QCOMPARE(window->minimumWidth(), 720);
}

void TextSizeShellTests::layout_keepsItsPromise_atEveryTextSize_data()
{
  QTest::addColumn<int>("percent");
  QTest::addColumn<QString>("family");
  QTest::addColumn<bool>("client");
  QTest::addColumn<double>("osScale");

  for (const int percent : {90, 100, 115, 130, 150}) {
    for (const QString &family : {QString(), QStringLiteral("Verdana")}) {
      for (const bool client : {false, true}) {
        QTest::addRow(
            "%d %% %s %s", percent, family.isEmpty() ? "default" : qPrintable(family), client ? "client" : "server"
        ) << percent
          << family << client << 1.0;
      }
    }
  }
  // the person asked Windows for 150 % text and this for the largest: 27 px of text
  QTest::addRow("150 %% with the Text size of Windows 150 %% default server") << 150 << QString() << false << 1.5;
  QTest::addRow("100 %% with the Text size of Windows 225 %% Verdana client")
      << 100 << QStringLiteral("Verdana") << true << 2.25;
}

void TextSizeShellTests::layout_keepsItsPromise_atEveryTextSize()
{
  QFETCH(int, percent);
  QFETCH(QString, family);
  QFETCH(bool, client);
  QFETCH(double, osScale);

  FakeSystem system;
  system.family = family;
  system.osScale = osScale;
  auto window = makeWindow(system, percent, client ? Settings::CoreMode::Client : Settings::CoreMode::Server);
  auto *shell = Access::shell(*window);
  showWindow(*window);

  const int least = MainShell::minimumWindowWidth();
  const int fold = shell->sidebar()->autoCollapseWidth();
  window->resize(least, 800);
  QTRY_COMPARE(window->width(), least);
  // the promise of the window: it can be this wide, and the layout asks for no more
  QVERIFY2(
      window->minimumSizeHint().width() <= least,
      qPrintable(
          QStringLiteral("the window asks for %1, it promised %2").arg(window->minimumSizeHint().width()).arg(least)
      )
  );
  for (const int width : {least, least + 40, fold - 1, fold + 1, fold + 300}) {
    window->resize(width, 800);
    QTRY_COMPARE(window->width(), width);
    QCoreApplication::processEvents();
    // nothing is squeezed to less than it needs, and nothing leaves the window
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
      QVERIFY2(
          bounds.contains(rectIn(*window, widget)),
          qPrintable(QString::number(width) + QStringLiteral(": ") + placed(*window, widget))
      );
    }
    // the mode control and the title do not sit on each other
    if (!shell->modeControl()->isHidden() && !shell->pageTitle()->isHidden()) {
      const auto control = rectIn(*window, shell->modeControl());
      const auto title = rectIn(*window, shell->pageTitle());
      QVERIFY2(
          !control.intersects(title) || control.top() >= title.bottom() || title.top() >= control.bottom() ||
              control.left() >= title.right() || title.left() >= control.right(),
          qPrintable(
              placed(*window, shell->modeControl()) + QStringLiteral(" and ") + placed(*window, shell->pageTitle())
          )
      );
    }
  }
}

void TextSizeShellTests::pages_areNotCut_atTheLeastWidth_atEveryTextSize_data()
{
  QTest::addColumn<int>("percent");
  QTest::addColumn<QString>("family");

  for (const int percent : {90, 100, 130, 150}) {
    for (const QString &family : {QString(), QStringLiteral("Verdana")})
      QTest::addRow("%d %% %s", percent, family.isEmpty() ? "default" : qPrintable(family)) << percent << family;
  }
}

void TextSizeShellTests::pages_areNotCut_atTheLeastWidth_atEveryTextSize()
{
  QFETCH(int, percent);
  QFETCH(QString, family);

  FakeSystem system;
  system.family = family;
  auto window = makeWindow(system, percent);
  auto *shell = Access::shell(*window);
  showWindow(*window);
  const int least = MainShell::minimumWindowWidth();
  window->resize(least, 800);
  QTRY_COMPARE(window->width(), least);

  const QStringList ids{QStringLiteral("settings.security"), QStringLiteral("settings.display"),
                        QStringLiteral("settings.keyboard"), QStringLiteral("settings.network"),
                        QStringLiteral("settings.general"),  QStringLiteral("settings.advanced")};
  for (const auto &id : ids) {
    shell->showPage(id);
    QCoreApplication::processEvents();
    auto *page = Access::settingsPages(*window)->page(id);
    QVERIFY(page && page->isVisible());
    auto *scroll = page->findChild<QScrollArea *>();
    QVERIFY(scroll);
    // the page is not cut at the side: what it holds is not wider than what it is shown in (it scrolls downwards)
    QVERIFY2(
        scroll->widget()->width() <= scroll->viewport()->width(),
        qPrintable(QStringLiteral("%1: the content is %2 wide in %3")
                       .arg(id)
                       .arg(scroll->widget()->width())
                       .arg(scroll->viewport()->width()))
    );
    QVERIFY2(
        scroll->widget()->minimumSizeHint().width() <= scroll->viewport()->width(),
        qPrintable(QStringLiteral("%1: the content asks for %2 in %3")
                       .arg(id)
                       .arg(scroll->widget()->minimumSizeHint().width())
                       .arg(scroll->viewport()->width()))
    );
    // and no row is squeezed to less than its text
    for (auto *row : page->findChildren<SettingRow *>()) {
      if (!row->isVisibleTo(page))
        continue;
      QVERIFY2(
          row->width() >= row->minimumSizeHint().width(), qPrintable(id + QStringLiteral(": ") + row->objectName())
      );
    }
  }
}

QTEST_MAIN(TextSizeShellTests)
