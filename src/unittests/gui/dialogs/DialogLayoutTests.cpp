/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DialogLayoutTests.h"

#include "common/Settings.h"
#include "gui/config/Computer.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ClientConfigDialog.h"
#include "gui/dialogs/ComputerSettingsDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
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
#include <QTest>
#include <QWidget>

namespace {

// Set to a directory to get a picture of every scenario there (to look at a failure; the checks stay numeric)
constexpr auto kGrabDirVariable = "DESKFLOW_LAYOUT_TEST_GRAB_DIR";

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

  // the styles every Qt build has, plus the platform's own when it is not one of them
  QStringList styles{QStringLiteral("Fusion"), QStringLiteral("Windows")};
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

  QVERIFY(QApplication::setStyle(style) != nullptr);
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

  QVERIFY(QApplication::setStyle(style) != nullptr);
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

  QVERIFY(QApplication::setStyle(style) != nullptr);
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

  QVERIFY(QApplication::setStyle(style) != nullptr);
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

QTEST_MAIN(DialogLayoutTests)
