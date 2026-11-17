/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DialogLayoutTests.h"

#include "../core/FakeProbeNetwork.h"
#include "../layout/LayoutServiceRig.h"
#include "common/Settings.h"
#include "gui/config/Computer.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/ComputerSettingsDialog.h"
#include "gui/dialogs/ConnectionTestDialog.h"
#include "gui/dialogs/LayoutProfilesDialog.h"
#include "gui/dialogs/MonitorEditorDialog.h"
#include "gui/dialogs/ServerAddressesDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/ArrangementCanvas.h"
#include "gui/widgets/ArrangementPanel.h"
#include "gui/widgets/LayoutNoticeBar.h"
#include "gui/widgets/MonitorEditorWidget.h"
#include "server/MonitorEditor.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QList>
#include <QMap>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRect>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTableWidget>
#include <QTest>
#include <QTextBrowser>
#include <QWidget>

#include <optional>
#include <vector>

namespace {

// Set to a directory to get a picture of every scenario there (to look at a failure; the checks stay numeric)
constexpr auto kGrabDirVariable = "DESKFLOW_LAYOUT_TEST_GRAB_DIR";

// The style of the program is not one that the factory of Qt knows by name
const auto kDeskflowStyle = QStringLiteral("Deskflow");

QStyle *applyStyle(const QString &name)
{
  if (name == kDeskflowStyle) {
    auto *style = new deskflow::gui::style::DeskflowStyle;
    QApplication::setStyle(style);
    return style;
  }
  return QApplication::setStyle(name);
}

QString describe(const QWidget *widget)
{
  const auto name = widget->objectName().isEmpty() ? QStringLiteral("<unnamed>") : widget->objectName();
  return QStringLiteral("%1 (%2)").arg(name, QString::fromLatin1(widget->metaObject()->className()));
}

QString describe(const QRect &rect)
{
  return QStringLiteral("%1x%2 at %3,%4").arg(rect.width()).arg(rect.height()).arg(rect.x()).arg(rect.y());
}

// A widget as its layout places it
struct Placed
{
  QWidget *widget;
  // what the layout reserved for the widget; native styles let the widget itself reach past it (shadows, focus rings)
  QLayoutItem *item;
};

// The widgets a layout, and the layouts nested in it, place
void collectPlacedWidgets(QLayoutItem *item, QList<Placed> &placed)
{
  if (auto *widget = item->widget()) {
    placed.append({widget, item});
  } else if (auto *layout = item->layout()) {
    for (int i = 0; i < layout->count(); ++i)
      collectPlacedWidgets(layout->itemAt(i), placed);
  }
}

struct HeightNeeds
{
  // below this the content is squeezed; -1 when the content's height does not depend on the width
  int minimum = -1;
  // what the layout hands out when there is room
  int preferred = -1;
};

// How tall a widget has to be, at its current width, to show everything it holds
HeightNeeds heightNeeded(const QWidget *widget)
{
  if (const auto *layout = widget->layout(); layout != nullptr && layout->hasHeightForWidth())
    return {layout->totalMinimumHeightForWidth(widget->width()), layout->totalHeightForWidth(widget->width())};
  if (widget->hasHeightForWidth()) {
    const auto height = widget->heightForWidth(widget->width());
    return {height, height};
  }
  return {};
}

// Everything that makes the layout of a shown dialog inconsistent at its current size, with the numbers behind it.
QStringList findLayoutViolations(QWidget &root)
{
  QStringList violations;

  QList<QWidget *> owners{&root};
  owners.append(root.findChildren<QWidget *>());

  QList<Placed> placed;
  for (const auto *owner : std::as_const(owners)) {
    if (auto *layout = owner->layout()) {
      for (int i = 0; i < layout->count(); ++i)
        collectPlacedWidgets(layout->itemAt(i), placed);
    }
  }
  placed.removeIf([&root](const Placed &entry) { return !entry.widget->isVisibleTo(&root); });

  // 1. no widget is smaller than the minimum its layout asked for (a maximum size wins over the hint, as in the layout
  // engine); the height of a widget that depends on its width is held to what it needs at that width in step 2
  for (const auto &[widget, item] : std::as_const(placed)) {
    const auto policy = widget->sizePolicy();
    const auto hint = widget->minimumSizeHint().boundedTo(widget->maximumSize());
    const auto minimumWidth = widget->minimumWidth() > 0 ? widget->minimumWidth() : hint.width();
    const auto minimumHeight = widget->minimumHeight() > 0 ? widget->minimumHeight() : hint.height();
    const auto tooNarrow = policy.horizontalPolicy() != QSizePolicy::Ignored && widget->width() < minimumWidth;
    const auto tooShort = policy.verticalPolicy() != QSizePolicy::Ignored && !widget->hasHeightForWidth() &&
                          widget->height() < minimumHeight;
    if (tooNarrow || tooShort) {
      violations << QStringLiteral("%1 is %2x%3 but needs at least %4x%5")
                        .arg(describe(widget))
                        .arg(widget->width())
                        .arg(widget->height())
                        .arg(minimumWidth)
                        .arg(minimumHeight);
    }
  }

  // 2. every widget is tall enough for its content at its width: wrapped text, or a group box holding wrapped text
  QList<const QWidget *> heightChecked{&root};
  for (const auto &entry : std::as_const(placed))
    heightChecked.append(entry.widget);
  for (const auto *widget : std::as_const(heightChecked)) {
    const auto needed = heightNeeded(widget);
    if (needed.minimum > widget->height()) {
      violations << QStringLiteral("%1 is %2 px tall at width %3 but needs %4 px (%5 short, %6 preferred)")
                        .arg(describe(widget))
                        .arg(widget->height())
                        .arg(widget->width())
                        .arg(needed.minimum)
                        .arg(needed.minimum - widget->height())
                        .arg(needed.preferred);
    }
  }

  // 3. every widget lies inside its parent
  for (const auto &entry : std::as_const(placed)) {
    const auto *widget = entry.widget;
    const auto *parent = widget->parentWidget();
    if (parent != nullptr && !parent->rect().contains(widget->geometry())) {
      const auto geometry = describe(widget->geometry());
      violations << QStringLiteral("%1 at %2 sticks out of %3 (%4)")
                        .arg(describe(widget), geometry, describe(parent), describe(parent->rect()));
    }
  }

  // 4. what the layout reserved for widgets in the same parent never overlaps
  QMap<const QWidget *, QList<Placed>> siblings;
  for (const auto &entry : std::as_const(placed))
    siblings[entry.widget->parentWidget()].append(entry);
  for (const auto &group : std::as_const(siblings)) {
    for (qsizetype i = 0; i < group.size(); ++i) {
      for (qsizetype j = i + 1; j < group.size(); ++j) {
        const auto first = group[i].item->geometry();
        const auto second = group[j].item->geometry();
        if (!first.intersects(second))
          continue;
        const auto common = first.intersected(second);
        violations << QStringLiteral("%1 at %2 overlaps %3 at %4 by %5x%6 px")
                          .arg(describe(group[i].widget), describe(first), describe(group[j].widget))
                          .arg(describe(second))
                          .arg(common.width())
                          .arg(common.height());
      }
    }
  }

  return violations;
}

// How the dialog is sized once it is shown
enum class SizeMode
{
  Default,   // what Qt makes of it when it is shown
  Preferred, // as tall as its layout wants at its preferred width
  Minimum    // dragged down to the smallest size it allows
};

const char *sizeModeName(SizeMode mode)
{
  switch (mode) {
  case SizeMode::Preferred:
    return "preferred size";
  case SizeMode::Minimum:
    return "minimum size";
  default:
    return "default size";
  }
}

// Shows the dialog the way the application does, then sizes it as asked
void showAndSettle(QDialog &dialog, qreal fontScale, SizeMode mode)
{
  auto font = dialog.font();
  if (font.pointSizeF() > 0)
    font.setPointSizeF(font.pointSizeF() * fontScale);
  else
    font.setPixelSize(qRound(font.pixelSize() * fontScale));
  dialog.setFont(font);

  dialog.show();
  QCoreApplication::processEvents();

  // a new size can change what the layout asks for, so repeat until it settles
  for (int i = 0; i < 4 && mode != SizeMode::Default; ++i) {
    QSize target = dialog.minimumSize();
    if (mode == SizeMode::Preferred) {
      const auto width = qMax(dialog.sizeHint().width(), dialog.minimumWidth());
      target = QSize(width, qMax(dialog.layout()->totalHeightForWidth(width), dialog.minimumHeight()));
    }
    if (dialog.size() == target)
      break;
    dialog.resize(target);
    QCoreApplication::processEvents();
  }
}

void grabIfRequested(const QString &testName, QWidget &dialog)
{
  const auto dir = qEnvironmentVariable(kGrabDirVariable);
  if (dir.isEmpty() || !QDir().mkpath(dir))
    return;

  auto name = QStringLiteral("%1 %2").arg(testName, QString::fromUtf8(QTest::currentDataTag()));
  name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9.]+")), QStringLiteral("_"));
  dialog.grab().save(QDir(dir).filePath(name + QStringLiteral(".png")));
}

void addScenarios()
{
  QTest::addColumn<QString>("style");
  QTest::addColumn<qreal>("fontScale");
  QTest::addColumn<int>("sizeMode");

  // the styles every Qt build has, plus the platform's own when it is not one of them, and the style of the program
  QStringList styles{QStringLiteral("Fusion"), QStringLiteral("Windows"), kDeskflowStyle};
  for (const auto &key : QStyleFactory::keys()) {
    if (!styles.contains(key, Qt::CaseInsensitive))
      styles.append(key);
  }

  for (const auto &style : std::as_const(styles)) {
    for (const qreal scale : {1.0, 1.3}) {
      for (const auto mode : {SizeMode::Default, SizeMode::Preferred, SizeMode::Minimum}) {
        QTest::addRow("%s, font x%.1f, %s", qPrintable(style), scale, sizeModeName(mode))
            << style << scale << static_cast<int>(mode);
      }
    }
  }
}

} // namespace

void DialogLayoutTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("Deskflow.state")));
  m_originalStyle = QApplication::style()->name();
}

void DialogLayoutTests::cleanup()
{
  QApplication::setStyle(m_originalStyle);
}

void DialogLayoutTests::computerSettingsDialog_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// The Cross-OS Input group once squeezed its wrapped text and rows into too little height, so that the text
// overlapped the rows below it. None of that depends on a pixel value, so it is checked on numbers.
void DialogLayoutTests::computerSettingsDialog_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  Computer computer(QStringLiteral("layout-test"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
  QVERIFY(dialog.isVisible());
  grabIfRequested(QStringLiteral("ComputerSettingsDialog"), dialog);

  const auto violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );
}

void DialogLayoutTests::clientConfigDialog_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

void DialogLayoutTests::computerSettingsDialog_monitorLayout_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// The monitor layout group has a text field that grows, a button, and a status line of wrapped text that is long when
// what is wrong with a layout is said: none of it may be squeezed or overlap
void DialogLayoutTests::computerSettingsDialog_monitorLayout_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Computer computer(QStringLiteral("layout-test"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  auto *edit = dialog.findChild<QPlainTextEdit *>(QStringLiteral("textMonitorLayout"));
  auto *status = dialog.findChild<QLabel *>(QStringLiteral("lblMonitorLayoutStatus"));
  auto *import = dialog.findChild<QPushButton *>(QStringLiteral("btnImportMonitorLayout"));
  QVERIFY(edit != nullptr && status != nullptr && import != nullptr);
  edit->setPlainText(
      QStringLiteral("0,0,1920,1080\n1920,0,1920,1080\n3840,0,1920,1080\n5760,0,1920,1080\n99999999999,0")
  );
  showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
  QVERIFY(dialog.isVisible());
  QVERIFY(edit->isVisibleTo(&dialog) && status->isVisibleTo(&dialog) && import->isVisibleTo(&dialog));
  QVERIFY(status->text().contains(QStringLiteral("Not valid")));
  // the text field is a few lines of its own font, not a line
  QVERIFY2(edit->height() >= edit->fontMetrics().lineSpacing() * 3, qPrintable(describe(edit->geometry())));
  grabIfRequested(QStringLiteral("ComputerSettingsDialogMonitorLayout"), dialog);

  const auto violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
}

void DialogLayoutTests::clientConfigDialog_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  ClientConfigDialog dialog;
  showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
  QVERIFY(dialog.isVisible());
  grabIfRequested(QStringLiteral("ClientConfigDialog"), dialog);

  const auto violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );
}

void DialogLayoutTests::serverConfigDialog_advancedTab_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// The clipboard choices live in the Advanced tab: a check box added to them must not squeeze or overlap the rows
void DialogLayoutTests::serverConfigDialog_advancedTab_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  Settings::setValue(Settings::Server::EnableClipboard, true);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("tabWidget"));
  auto *advanced = dialog.findChild<QWidget *>(QStringLiteral("tabAdvanced"));
  QVERIFY(tabs != nullptr && advanced != nullptr);
  tabs->setCurrentWidget(advanced);
  showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
  QVERIFY(dialog.isVisible());
  QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("cbClipboardTextOnly")) != nullptr);
  QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("cbClipboardTextOnly"))->isVisibleTo(&dialog));
  // the experiment's check box has a group of its own in the same tab
  QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("cbExperimentalMultiMonitor")) != nullptr);
  QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("cbExperimentalMultiMonitor"))->isVisibleTo(&dialog));
  grabIfRequested(QStringLiteral("ServerConfigDialogAdvanced"), dialog);

  const auto violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );
  Settings::setValue(Settings::Server::EnableClipboard);
}

void DialogLayoutTests::monitorEditorDialog_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// The editor that draws the monitors has a status line that is long when the drawing does not match the screen that the
// computer reports, the canvas, two rows of controls, and the numbers: none of it may be squeezed or overlap
void DialogLayoutTests::monitorEditorDialog_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  using deskflow::server::editor::Layout;
  using deskflow::server::editor::Monitor;
  Monitor main;
  main.w = 2560 / 1.5;
  main.h = 960;
  main.scale = 150;
  Monitor left;
  left.x = -1920;
  left.y = -120;
  left.w = 1920;
  left.h = 1080;
  MonitorEditorDialog dialog(
      nullptr, QStringLiteral("a-computer-with-a-long-name"), Layout{{main, left}}, QRect(-1920, 0, 4480, 1440)
  );
  auto *numbers = dialog.findChild<QGroupBox *>(QStringLiteral("groupAdvancedNumbers"));
  QVERIFY(numbers != nullptr);
  numbers->setChecked(true);
  showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
  QVERIFY(dialog.isVisible());
  for (const auto *name :
       {"lblMonitorStatus", "monitorEditorWidget", "cbResolution", "cbScale", "btnRotate", "btnMakeMain",
        "btnAddMonitor", "btnRemoveMonitor", "btnUndo", "btnRedo", "btnStretchToReported", "textNumbers",
        "btnApplyNumbers", "btnImportNumbers"}) {
    auto *widget = dialog.findChild<QWidget *>(QString::fromLatin1(name));
    QVERIFY2(widget != nullptr, name);
    QVERIFY2(widget->isVisibleTo(&dialog), name);
  }
  // the canvas has room to be a canvas, and the numbers are a few lines of their own font
  auto *canvas = dialog.editorWidget();
  QVERIFY2(canvas->height() >= 150, qPrintable(describe(canvas->geometry())));
  auto *edit = dialog.findChild<QPlainTextEdit *>(QStringLiteral("textNumbers"));
  QVERIFY2(edit->height() >= edit->fontMetrics().lineSpacing() * 3, qPrintable(describe(edit->geometry())));
  grabIfRequested(QStringLiteral("MonitorEditorDialog"), dialog);

  const auto violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );
}

void DialogLayoutTests::serverConfigDialog_computersTab_withTheArrangement_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// The computers tab has the canvas of the arrangement in the place of the classic grid, with a title, a hint, the
// issues that there are, and the buttons: none of it may be squeezed or overlap. And when it is only offered, the line
// and the button that offer it have to fit as well.
void DialogLayoutTests::serverConfigDialog_computersTab_withTheArrangement_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  // three computers, the first of them on top of the server, so that an issue is said
  Settings::setValue(Settings::Core::ComputerName, QStringLiteral("srv"));
  auto &proxy = Settings::proxy();
  proxy.beginGroup(QStringLiteral("internalConfig"));
  proxy.remove(QString());
  proxy.beginWriteArray(QStringLiteral("screens"));
  for (int i = 0; i < 15; ++i) {
    proxy.setArrayIndex(i);
    proxy.setValue(
        QStringLiteral("name"), i == 6   ? QStringLiteral("PC-with-a-long-name")
                                : i == 7 ? QStringLiteral("srv")
                                         : QString()
    );
  }
  proxy.endArray();
  proxy.endGroup();
  ServerConfig::overrideServerMonitors(
      std::vector<deskflow::server::MonitorRect>{{0, 0, 3440, 1440}, {753, 1440, 2064, 1347}}
  );
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Computer::ArrangementX.arg("srv"), 0.0);
  Settings::setValue(Settings::Computer::ArrangementY.arg("srv"), 0.0);
  Settings::setValue(Settings::Computer::ArrangementX.arg("PC-with-a-long-name"), 500.0);
  Settings::setValue(Settings::Computer::ArrangementY.arg("PC-with-a-long-name"), 500.0);

  for (const bool used : {true, false}) {
    Settings::setValue(Settings::Server::UseArrangement, used);
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("tabWidget"));
    auto *computers = dialog.findChild<QWidget *>(QStringLiteral("tabComputers"));
    QVERIFY(tabs != nullptr && computers != nullptr);
    tabs->setCurrentWidget(computers);
    showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
    QVERIFY(dialog.isVisible());
    auto *panel = dialog.findChild<ArrangementPanel *>();
    QVERIFY(panel != nullptr);
    QCOMPARE(panel->isVisibleTo(&dialog), used);
    auto *offer = dialog.findChild<QPushButton *>(QStringLiteral("btnUseArrangement"));
    QVERIFY(offer != nullptr);
    QCOMPARE(offer->isVisibleTo(&dialog), !used);
    if (used) {
      // the canvas has room to be a canvas
      QVERIFY2(panel->canvas()->height() >= 150, qPrintable(describe(panel->canvas()->geometry())));
      QVERIFY(panel->hasErrors());
    }
    grabIfRequested(
        used ? QStringLiteral("ServerConfigDialogArrangement") : QStringLiteral("ServerConfigDialogOffer"), dialog
    );

    const auto violations = findLayoutViolations(dialog);
    QVERIFY2(
        violations.isEmpty(),
        qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
    );
  }
  Settings::setValue(Settings::Server::UseArrangement);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  ServerConfig::overrideServerMonitors(std::nullopt);
  for (const auto *name : {"srv", "PC-with-a-long-name"}) {
    Settings::setValue(Settings::Computer::ArrangementX.arg(QString::fromLatin1(name)));
    Settings::setValue(Settings::Computer::ArrangementY.arg(QString::fromLatin1(name)));
  }
}

void DialogLayoutTests::serverConfigDialog_computersTab_withReportedMonitors_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// A computer that reported its monitors is drawn from them, with the line that says so, and when it reports others
// while the dialog is open the canvas follows: none of it may be squeezed or overlap
void DialogLayoutTests::serverConfigDialog_computersTab_withReportedMonitors_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  Settings::setValue(Settings::Core::ComputerName, QStringLiteral("srv"));
  auto &proxy = Settings::proxy();
  proxy.beginGroup(QStringLiteral("internalConfig"));
  proxy.remove(QString());
  proxy.beginWriteArray(QStringLiteral("screens"));
  for (int i = 0; i < 15; ++i) {
    proxy.setArrayIndex(i);
    proxy.setValue(
        QStringLiteral("name"), i == 6   ? QStringLiteral("Windows-PC-with-a-long-name")
                                : i == 7 ? QStringLiteral("srv")
                                : i == 8 ? QStringLiteral("Mac-mini")
                                         : QString()
    );
  }
  proxy.endArray();
  proxy.endGroup();
  ServerConfig::overrideServerMonitors(
      std::vector<deskflow::server::MonitorRect>{{0, 0, 3440, 1440}, {753, 1440, 2064, 1347}}
  );
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  Settings::setValue(Settings::Server::UseArrangement, true);
  // the Windows PC reported two monitors the last time that it was connected, and is offline now
  Settings::setValue(
      Settings::Computer::ReportedMonitors.arg("Windows-PC-with-a-long-name"),
      QStringLiteral("0,0,2560,1440 scale=150 primary;2560,0,1920,1080 scale=100")
  );
  // a Mac says its points, a Windows PC its pixels
  Settings::setValue(
      Settings::Computer::ReportedMonitors.arg("Mac-mini"), QStringLiteral("0,0,1920,1080 scale=200 primary logical")
  );
  // the server, and a row to its right: the PC (at 150 %, which the first report gave it as its zoom, so 2987 units
  // wide, 3840 with a third monitor) touching it, and the Mac mini (in points, 1920 units wide) with room for the PC to
  // grow
  Settings::setValue(Settings::Computer::ArrangementZoom.arg("Windows-PC-with-a-long-name"), 100.0 / 150.0);
  Settings::setValue(Settings::Computer::ArrangementX.arg("srv"), 0.0);
  Settings::setValue(Settings::Computer::ArrangementX.arg("Windows-PC-with-a-long-name"), 3440.0);
  Settings::setValue(Settings::Computer::ArrangementX.arg("Mac-mini"), 7400.0);
  for (const auto *name : {"srv", "Windows-PC-with-a-long-name", "Mac-mini"})
    Settings::setValue(Settings::Computer::ArrangementY.arg(QString::fromLatin1(name)), 0.0);

  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("tabWidget"));
  auto *computers = dialog.findChild<QWidget *>(QStringLiteral("tabComputers"));
  QVERIFY(tabs != nullptr && computers != nullptr);
  tabs->setCurrentWidget(computers);
  showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
  QVERIFY(dialog.isVisible());
  auto *panel = dialog.findChild<ArrangementPanel *>();
  QVERIFY(panel != nullptr && panel->isVisibleTo(&dialog));
  auto *canvas = panel->canvas();
  QVERIFY2(canvas->height() >= 150, qPrintable(describe(canvas->geometry())));

  const auto monitorsOf = [&](const char *name) {
    const auto &arrangement = canvas->arrangement();
    const auto index = arrangement.indexOf(name);
    return index >= 0 ? arrangement.computers[static_cast<size_t>(index)].monitors.size() : size_t(0);
  };
  // drawn from what it reported, and said to be
  QCOMPARE(monitorsOf("Windows-PC-with-a-long-name"), size_t(2));
  QCOMPARE(monitorsOf("Mac-mini"), size_t(1));
  QVERIFY(canvas->reportedMonitorComputers().contains(QStringLiteral("Windows-PC-with-a-long-name")));
  QVERIFY(canvas->reportedMonitorComputers().contains(QStringLiteral("Mac-mini")));
  QVERIFY(!canvas->reportedMonitorComputers().contains(QStringLiteral("srv")));
  grabIfRequested(QStringLiteral("ServerConfigDialogReportedMonitors"), dialog);

  auto violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );

  // the Windows PC connects, and says that a third monitor is plugged in; the dialog is open and the canvas follows it
  QVERIFY(config.takeReportedMonitorsFromLog(QStringLiteral(
      "[2026-11-16T13:30:00.123] INFO: computer \"Windows-PC-with-a-long-name\" reports 3 monitors: 0,0,2560,1440 "
      "scale=150 primary;2560,0,1920,1080 scale=100;4480,0,1280,1024 scale=100"
  )));
  QCOMPARE(monitorsOf("Windows-PC-with-a-long-name"), size_t(3));
  QCoreApplication::processEvents();
  grabIfRequested(QStringLiteral("ServerConfigDialogReportedMonitorsChanged"), dialog);
  violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );

  Settings::setValue(Settings::Server::UseArrangement);
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  ServerConfig::overrideServerMonitors(std::nullopt);
  for (const auto *name : {"srv", "Windows-PC-with-a-long-name", "Mac-mini"}) {
    Settings::setValue(Settings::Computer::ArrangementX.arg(QString::fromLatin1(name)));
    Settings::setValue(Settings::Computer::ArrangementY.arg(QString::fromLatin1(name)));
    Settings::setValue(Settings::Computer::ReportedMonitors.arg(QString::fromLatin1(name)));
    Settings::setValue(Settings::Computer::ArrangementZoom.arg(QString::fromLatin1(name)));
  }
}

void DialogLayoutTests::serverConfigDialog_computersTab_withTheLayoutNotice_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// The row of the switch for remembering layouts and the line that says what it did (with Undo and Dismiss) are in the
// arrangement panel: a long notice is cut to the width, and nothing is squeezed or overlaps, in any style, font or size
void DialogLayoutTests::serverConfigDialog_computersTab_withTheLayoutNotice_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  layout_rig::arrangeT();
  Settings::setValue(Settings::Server::RememberArrangements, true);
  {
    ServerConfig config;
    ServerConfigDialog dialog(nullptr, config);
    auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("tabWidget"));
    auto *computers = dialog.findChild<QWidget *>(QStringLiteral("tabComputers"));
    QVERIFY(tabs != nullptr && computers != nullptr);
    tabs->setCurrentWidget(computers);
    auto *panel = dialog.findChild<ArrangementPanel *>();
    QVERIFY(panel != nullptr);
    panel->layoutNoticeBar()->setText(
        QStringLiteral("Layout restored: \"external 3440x1440 + built-in 1512x982\" (similar displays, 88% alike). Not in "
                       "that layout, left where they were: PC-with-a-long-name. The core was restarted."),
        true
    );
    showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
    QVERIFY(dialog.isVisible());
    QVERIFY(panel->isVisibleTo(&dialog));
    QVERIFY(panel->layoutNoticeBar()->isVisibleTo(&dialog));
    auto *box = panel->findChild<QCheckBox *>(QStringLiteral("cbRememberArrangements"));
    QVERIFY(box != nullptr && box->isVisibleTo(&dialog) && box->isChecked());
    QVERIFY(panel->canvas()->height() >= 150);
    grabIfRequested(QStringLiteral("ServerConfigDialogLayoutNotice"), dialog);

    const auto violations = findLayoutViolations(dialog);
    QVERIFY2(
        violations.isEmpty(),
        qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
    );
  }
  layout_rig::forgetEverything();
}

void DialogLayoutTests::layoutProfilesDialog_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// A list of the remembered layouts with the names that people give, the displays that each was made for, and the displays
// that there are now: nothing is squeezed or overlaps, and the list has room to be read
void DialogLayoutTests::layoutProfilesDialog_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  layout_rig::arrangeT();
  {
    layout_rig::ServiceRig rig(m_settingsDir.filePath(QStringLiteral("layout-profiles.json")));
    rig.turnOn();
    const auto layout = layout_rig::captureLayout(*rig.config);
    rig.service->store().upsert(
        layout_rig::sig(layout_rig::laptopOnly()), layout, 5000,
        QStringLiteral("On the move: the laptop alone with its own screen only"), true
    );
    rig.service->store().upsert(
        layout_rig::sig({layout_rig::fullHd(0, 0, true), layout_rig::fullHd(1920, 0), layout_rig::builtIn(0, 1080)}),
        layout, 6000
    );
    LayoutProfilesDialog dialog(*rig.service, [&] { return layout_rig::captureLayout(*rig.config); });
    showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
    QVERIFY(dialog.isVisible());
    auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("layoutProfilesTable"));
    QVERIFY(table != nullptr && table->isVisibleTo(&dialog));
    QCOMPARE(table->rowCount(), 3);
    QVERIFY2(table->height() >= table->fontMetrics().lineSpacing() * 5, qPrintable(describe(table->geometry())));
    grabIfRequested(QStringLiteral("LayoutProfilesDialog"), dialog);

    const auto violations = findLayoutViolations(dialog);
    QVERIFY2(
        violations.isEmpty(),
        qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
    );
  }
  layout_rig::forgetEverything();
  QFile::remove(m_settingsDir.filePath(QStringLiteral("layout-profiles.json")));
}

void DialogLayoutTests::monitorEditorDialog_showingTheReportedMonitors_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// The editor that shows what the computer reports has a line that says so and a button to draw one's own, next to the
// controls that are disabled: none of it may be squeezed or overlap
void DialogLayoutTests::monitorEditorDialog_showingTheReportedMonitors_isLaidOutWithoutSqueezing()
{
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  using deskflow::server::editor::Layout;
  using deskflow::server::editor::Monitor;
  Monitor main;
  main.w = 2560 / 1.5;
  main.h = 960;
  main.scale = 150;
  Monitor left;
  left.x = -1920;
  left.y = -120;
  left.w = 1920;
  left.h = 1080;
  MonitorEditorDialog dialog(
      nullptr, QStringLiteral("a-computer-with-a-long-name"), Layout{{main, left}}, QRect(-1920, 0, 4480, 1440)
  );
  dialog.showReportedMonitors();
  showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
  QVERIFY(dialog.isVisible());
  for (const auto *name : {"lblMonitorStatus", "monitorEditorWidget", "btnDrawOwn", "cbResolution", "cbScale"}) {
    auto *widget = dialog.findChild<QWidget *>(QString::fromLatin1(name));
    QVERIFY2(widget != nullptr, name);
    QVERIFY2(widget->isVisibleTo(&dialog), name);
  }
  QVERIFY2(dialog.editorWidget()->height() >= 150, qPrintable(describe(dialog.editorWidget()->geometry())));
  grabIfRequested(QStringLiteral("MonitorEditorDialogReportedMonitors"), dialog);

  const auto violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );
}

void DialogLayoutTests::connectionTestDialog_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// The result panel holds long wrapped text (what is wrong and what to try) for several addresses, a status line and
// three buttons: none of it may be squeezed or overlap, in any style, font size or window size
void DialogLayoutTests::connectionTestDialog_isLaidOutWithoutSqueezing()
{
  using deskflow::diagnosis::FailureKind;
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  probetest::FakeNetwork network;
  network.scripts[QStringLiteral("10.0.0.5")] = probetest::Script{};
  network.scripts[QStringLiteral("10.0.0.6")] = probetest::open(5);
  network.scripts[QStringLiteral("10.0.0.7")] =
      probetest::fail(FailureKind::Refused, QStringLiteral("Connection refused"), 5);
  network.scripts[QStringLiteral("10.0.0.8")] =
      probetest::fail(FailureKind::NoRoute, QStringLiteral("No route to host"), 5);
  network.resolveErrors[QStringLiteral("ghost.example")] = QStringLiteral("Host not found");
  ConnectionTestDialog dialog(
      nullptr, QStringLiteral("10.0.0.5, 10.0.0.6, 10.0.0.7, 10.0.0.8, ghost.example, 10.0.0.9:0"), 24800,
      [&network](QObject *parent) {
        auto *probe = probetest::makeProbe(network, 60);
        probe->setParent(parent);
        return probe;
      }
  );
  dialog.runTest();
  QTRY_VERIFY_WITH_TIMEOUT(!dialog.isTesting(), 5000);
  showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
  QVERIFY(dialog.isVisible());
  auto *results = dialog.findChild<QTextBrowser *>(QStringLiteral("resultsView"));
  QVERIFY(results != nullptr && results->isVisibleTo(&dialog));
  // the panel is a few lines of its own font, not a sliver
  QVERIFY2(results->height() >= results->fontMetrics().lineSpacing() * 10, qPrintable(describe(results->geometry())));
  QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("btnCopySummary"))->isVisibleTo(&dialog));
  QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("btnRunAgain"))->isVisibleTo(&dialog));
  grabIfRequested(QStringLiteral("ConnectionTestDialog"), dialog);

  const auto violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );
}

void DialogLayoutTests::serverAddressesDialog_isLaidOutWithoutSqueezing_data()
{
  addScenarios();
}

// A table of the addresses of the server, an explanation per kind of address and a tip: a table that has names of
// interfaces as long as those of Windows must not squeeze or overlap, in any style, font size or window size
void DialogLayoutTests::serverAddressesDialog_isLaidOutWithoutSqueezing()
{
  using deskflow::gui::AddressInfo;
  using deskflow::gui::AddressKind;
  QFETCH(QString, style);
  QFETCH(qreal, fontScale);
  QFETCH(int, sizeMode);

  QVERIFY(applyStyle(style) != nullptr);
  const QList<AddressInfo> addresses{
      {QStringLiteral("10.0.21.36"), QStringLiteral("en7"), AddressKind::Lan},
      {QStringLiteral("10.18.0.77"), QStringLiteral("en0"), AddressKind::Lan},
      {QStringLiteral("100.76.209.252"), QStringLiteral("utun100"), AddressKind::Overlay},
      {QStringLiteral("172.28.80.1"), QStringLiteral("vEthernet (WSL (Hyper-V firewall))"), AddressKind::Virtual},
      {QStringLiteral("169.254.88.9"), QStringLiteral("Ethernet 3"), AddressKind::LinkLocal},
  };
  ServerAddressesDialog dialog(nullptr, addresses);
  showAndSettle(dialog, fontScale, static_cast<SizeMode>(sizeMode));
  QVERIFY(dialog.isVisible());
  auto *view = dialog.findChild<QTextBrowser *>(QStringLiteral("addressesView"));
  QVERIFY(view != nullptr && view->isVisibleTo(&dialog));
  const auto text = view->toPlainText();
  for (const auto &info : addresses) {
    QVERIFY2(text.contains(info.address), qPrintable(text));
    QVERIFY(text.contains(info.interfaceName));
  }
  QVERIFY(text.contains(deskflow::gui::addressKindLabel(AddressKind::Overlay)));
  QVERIFY(text.contains(deskflow::gui::addressListTip()));
  QVERIFY2(view->height() >= view->fontMetrics().lineSpacing() * 10, qPrintable(describe(view->geometry())));
  grabIfRequested(QStringLiteral("ServerAddressesDialog"), dialog);

  const auto violations = findLayoutViolations(dialog);
  QVERIFY2(
      violations.isEmpty(),
      qPrintable(QStringLiteral("dialog is %1\n%2").arg(describe(dialog.geometry()), violations.join(u'\n')))
  );
}

QTEST_MAIN(DialogLayoutTests)
