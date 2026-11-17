/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorEditorDialogTests.h"

#include "common/Settings.h"
#include "dialogs/MonitorEditorDialog.h"
#include "gui/style/CanvasTheme.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/DisclosureBox.h"
#include "gui/widgets/MonitorEditorWidget.h"
#include "server/MonitorEditor.h"

#include <QAccessible>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMetaEnum>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOptionGroupBox>
#include <QTest>

#include <cmath>
#include <memory>
#include <optional>

using namespace deskflow::server::editor;
using deskflow::server::MonitorLayout;

namespace {

// a Windows PC of 2560x1440 at 150 % with a monitor of 1920x1080 at 100 % on its left, the bottoms in line
Layout windowsPc()
{
  Monitor main;
  main.x = 100;
  main.y = 100;
  main.w = 2560 / 1.5;
  main.h = 960;
  main.scale = 150;
  Monitor left;
  left.x = 100 - 1920;
  left.y = 100 + 960 - 1080;
  left.w = 1920;
  left.h = 1080;
  left.scale = 100;
  return Layout{{main, left}};
}

Layout twoApart()
{
  Monitor a;
  a.w = 1920;
  a.h = 1080;
  Monitor b = a;
  b.x = 2300;
  b.y = 300;
  return Layout{{a, b}};
}

const QRect kMatching(-2880, -180, 5440, 1620);
const QRect kPhysical(-1920, 0, 4480, 1440);

struct Opened
{
  std::unique_ptr<MonitorEditorDialog> dialog;
  MonitorEditorWidget *widget = nullptr;
};

Opened open(const Layout &layout, const std::optional<QRect> &reported = std::nullopt)
{
  Opened opened;
  opened.dialog = std::make_unique<MonitorEditorDialog>(nullptr, QStringLiteral("PC"), layout, reported);
  opened.dialog->resize(960, 660);
  opened.dialog->show();
  opened.widget = opened.dialog->editorWidget();
  QApplication::processEvents();
  return opened;
}

template <class T> T *child(const Opened &opened, const char *name)
{
  auto *found = opened.dialog->findChild<T *>(QString::fromLatin1(name));
  if (found == nullptr)
    qFatal("there is no %s", name);
  return found;
}

QString statusOf(const Opened &opened)
{
  return child<QLabel>(opened, "lblMonitorStatus")->text();
}

bool okIsEnabled(const Opened &opened)
{
  return opened.dialog->findChild<QDialogButtonBox *>(QStringLiteral("monitorEditorButtons"))
      ->button(QDialogButtonBox::Ok)
      ->isEnabled();
}

double scaleOf(const Opened &opened)
{
  return opened.widget->rectOf(0).width() / opened.widget->drawing().monitors[0].w;
}

QPoint centre(const Opened &opened, int index)
{
  return opened.widget->rectOf(index).center().toPoint();
}

void drag(QWidget *widget, const QPoint &from, const QPoint &to, bool release = true)
{
  QTest::mousePress(widget, Qt::LeftButton, {}, from);
  for (int i = 1; i <= 8; ++i)
    QTest::mouseMove(widget, from + (to - from) * i / 8);
  if (release)
    QTest::mouseRelease(widget, Qt::LeftButton, {}, to);
}

void saveGrab(QWidget &widget, const QString &name)
{
  const auto directory = qEnvironmentVariable("DESKFLOW_SCREENSHOT_DIR");
  if (directory.isEmpty())
    return;
  QDir().mkpath(directory);
  QVERIFY2(widget.grab().save(QDir(directory).filePath(name)), qPrintable(name));
}

Realised madeOf(const Opened &opened)
{
  return opened.dialog->realised();
}

// The look of the application while the test runs: the one of Appearance=Deskflow; the system look (Fusion) again at the end
struct DeskflowLook
{
  DeskflowLook()
  {
    auto *style = new deskflow::gui::style::DeskflowStyle;
    QApplication::setStyle(style);
    deskflow::gui::style::Look look;
    look.reduceTransparency = true;
    look.reduceMotion = true;
    style->useLook(look);
  }
  ~DeskflowLook()
  {
    QApplication::setStyle(QStringLiteral("Fusion"));
  }
};

// Counts the presses of a key that reach what it is installed on
struct KeyCounter : QObject
{
  explicit KeyCounter(Qt::Key key) : key(key)
  {
  }
  bool eventFilter(QObject *, QEvent *event) override
  {
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == key)
      ++count;
    return false;
  }
  Qt::Key key;
  int count = 0;
};

QString roleName(QAccessible::Role role)
{
  return QString::fromLatin1(QMetaEnum::fromType<QAccessible::Role>().valueToKey(role));
}

} // namespace

void MonitorEditorDialogTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("Deskflow.state")));
}

void MonitorEditorDialogTests::status_saysWhetherTheDrawingMatchesWhatTheComputerReports()
{
  auto matching = open(windowsPc(), kMatching);
  QVERIFY2(
      statusOf(matching).contains(QStringLiteral("Matches what PC reports: 5440 × 1620")),
      qPrintable(statusOf(matching))
  );
  QVERIFY(okIsEnabled(matching));
  QVERIFY(!child<QPushButton>(matching, "btnStretchToReported")->isEnabled());
  QCOMPARE(static_cast<int>(madeOf(matching).fit), static_cast<int>(Fit::Matches));

  auto unknown = open(windowsPc());
  QVERIFY2(statusOf(unknown).contains(QStringLiteral("Not verified yet")), qPrintable(statusOf(unknown)));
  QVERIFY(okIsEnabled(unknown));

  // the physical pixels of the monitors: the shape of the drawing is not the shape that the computer reports
  auto differs = open(windowsPc(), kPhysical);
  const auto text = statusOf(differs);
  QVERIFY2(text.contains(QStringLiteral("Does not match what PC reports (4480 × 1440)")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("your drawing is 5440 × 1620, 7.9 % different in shape")), qPrintable(text));
  QVERIFY(okIsEnabled(differs));
  QVERIFY(child<QPushButton>(differs, "btnStretchToReported")->isEnabled());
}

void MonitorEditorDialogTests::status_aShapeThatDiffers_canBeStretched()
{
  auto opened = open(windowsPc(), kPhysical);
  child<QPushButton>(opened, "btnStretchToReported")->click();
  QVERIFY2(statusOf(opened).contains(QStringLiteral("Matches what PC reports")), qPrintable(statusOf(opened)));
  QVERIFY(statusOf(opened).contains(QStringLiteral("stretched")));
  const auto made = madeOf(opened);
  QCOMPARE(static_cast<int>(made.fit), static_cast<int>(Fit::Matches));
  QVERIFY(static_cast<bool>(MonitorLayout::create(made.monitors)));
  QVERIFY(okIsEnabled(opened));

  // another change is not stretched until it is asked again
  child<QPushButton>(opened, "btnRotate")->click();
  QVERIFY(!statusOf(opened).contains(QStringLiteral("stretched")));
}

void MonitorEditorDialogTests::mouse_dragAMonitor_movesItWithSnapping_andUndoGoesBack()
{
  auto opened = open(twoApart());
  QSignalSpy changed(opened.widget, &MonitorEditorWidget::drawingChanged);
  const auto before = opened.dialog->drawing();
  const auto scale = scaleOf(opened);
  QCOMPARE(opened.widget->selected(), 0);

  // B is put 5 units from touching A and 5 from being in line with it: it snaps to both
  const auto from = centre(opened, 1);
  drag(
      opened.widget, from,
      from + QPoint(static_cast<int>(std::lround(-375 * scale)), static_cast<int>(std::lround(-295 * scale)))
  );
  QCOMPARE(changed.count(), 1);
  QCOMPARE(opened.widget->selected(), 1);
  QCOMPARE(opened.dialog->drawing().monitors[1].x, 1920.0);
  QCOMPARE(opened.dialog->drawing().monitors[1].y, 0.0);
  QVERIFY(okIsEnabled(opened));

  auto *undo = child<QPushButton>(opened, "btnUndo");
  auto *redo = child<QPushButton>(opened, "btnRedo");
  QVERIFY(undo->isEnabled());
  QVERIFY(!redo->isEnabled());
  undo->click();
  QCOMPARE(opened.dialog->drawing(), before);
  QVERIFY(redo->isEnabled());
  redo->click();
  QCOMPARE(opened.dialog->drawing().monitors[1].x, 1920.0);

  // it cannot be dropped on the other one: it stays where it was
  const auto after = opened.dialog->drawing();
  const auto onB = centre(opened, 1);
  drag(opened.widget, onB, centre(opened, 0));
  QCOMPARE(opened.dialog->drawing(), after);
}

void MonitorEditorDialogTests::mouse_dragAnEdge_resizesTheMonitor()
{
  auto opened = open(twoApart());
  const auto scale = scaleOf(opened);
  opened.widget->setSelected(1);
  const auto edge = opened.widget->rectOf(1);
  const QPoint right(static_cast<int>(std::lround(edge.right())), static_cast<int>(std::lround(edge.center().y())));

  // the pointer is a resize cursor over the edge of the selected monitor
  QTest::mouseMove(opened.widget, right);
  QCOMPARE(opened.widget->cursor().shape(), Qt::SizeHorCursor);
  QTest::mouseMove(opened.widget, centre(opened, 1));
  QVERIFY(opened.widget->cursor().shape() != Qt::SizeHorCursor);

  drag(opened.widget, right, right + QPoint(static_cast<int>(std::lround(200 * scale)), 0));
  const auto &b = opened.dialog->drawing().monitors[1];
  QVERIFY2(std::abs(b.w - 2120) < 3.0 / scale, qPrintable(QString::number(b.w)));
  QCOMPARE(b.x, 2300.0);
  QCOMPARE(b.h, 1080.0);
  // a drawn size says its pixels
  QVERIFY(std::abs(b.pixelsW() - 2120) <= 3.0 / scale);
}

void MonitorEditorDialogTests::mouse_aRightClickInTheMiddleOfADrag_doesNotEndIt()
{
  auto opened = open(twoApart());
  QSignalSpy changed(opened.widget, &MonitorEditorWidget::drawingChanged);
  const auto scale = scaleOf(opened);
  const auto from = centre(opened, 1);
  const auto to = from + QPoint(static_cast<int>(std::lround(100 * scale)), static_cast<int>(std::lround(100 * scale)));
  QTest::mousePress(opened.widget, Qt::LeftButton, {}, from);
  QTest::mouseMove(opened.widget, from + (to - from) / 2);
  QTest::mousePress(opened.widget, Qt::RightButton, {}, from + (to - from) / 2);
  QTest::mouseRelease(opened.widget, Qt::RightButton, {}, from + (to - from) / 2);
  QCOMPARE(changed.count(), 0);
  QTest::mouseMove(opened.widget, to);
  QTest::mouseRelease(opened.widget, Qt::LeftButton, {}, to);
  QCOMPARE(changed.count(), 1);
}

void MonitorEditorDialogTests::controls_resolutionScaleRotateAddRemoveAndMain()
{
  Monitor one;
  auto opened = open(Layout{{one}});
  auto *resolution = child<QComboBox>(opened, "cbResolution");
  auto *scale = child<QComboBox>(opened, "cbScale");
  const auto choose = [](QComboBox *box, int index) {
    box->setCurrentIndex(index);
    Q_EMIT box->activated(index);
  };

  // a resolution is picked, not typed
  choose(resolution, resolution->findData(QPoint(2560, 1440)));
  QCOMPARE(opened.dialog->drawing().monitors[0].pixelsW(), 2560);
  QCOMPARE(opened.dialog->drawing().monitors[0].pixelsH(), 1440);

  // a scale changes the size in the drawing, not the pixels
  choose(scale, scale->findText(QStringLiteral("150 %")));
  const auto &m = opened.dialog->drawing().monitors[0];
  QCOMPARE(m.scale, 150.0);
  QCOMPARE(m.pixelsW(), 2560);
  QVERIFY(std::abs(m.w - 2560 / 1.5) < 1e-9);
  // and another one can be typed
  scale->setEditText(QStringLiteral("133"));
  Q_EMIT scale->lineEdit()->editingFinished();
  QCOMPARE(opened.dialog->drawing().monitors[0].scale, 133.0);
  // what is not a scale is not one
  scale->setEditText(QStringLiteral("abc"));
  Q_EMIT scale->lineEdit()->editingFinished();
  QCOMPARE(opened.dialog->drawing().monitors[0].scale, 133.0);

  child<QPushButton>(opened, "btnRotate")->click();
  QCOMPARE(opened.dialog->drawing().monitors[0].pixelsW(), 1440);
  QCOMPARE(opened.dialog->drawing().monitors[0].pixelsH(), 2560);

  child<QPushButton>(opened, "btnAddMonitor")->click();
  QCOMPARE(opened.dialog->drawing().monitors.size(), size_t{2});
  QCOMPARE(opened.widget->selected(), 1);
  QVERIFY(okIsEnabled(opened));

  const auto secondBefore = opened.dialog->drawing().monitors[1];
  child<QPushButton>(opened, "btnMakeMain")->click();
  QCOMPARE(opened.widget->selected(), 0);
  QCOMPARE(opened.dialog->drawing().monitors[0], secondBefore);
  QVERIFY(!child<QPushButton>(opened, "btnMakeMain")->isEnabled());

  child<QPushButton>(opened, "btnRemoveMonitor")->click();
  QCOMPARE(opened.dialog->drawing().monitors.size(), size_t{1});
  QVERIFY(!child<QPushButton>(opened, "btnRemoveMonitor")->isEnabled());

  // nothing that is done is lost: it can all be undone
  for (int i = 0; i < 20 && child<QPushButton>(opened, "btnUndo")->isEnabled(); ++i)
    child<QPushButton>(opened, "btnUndo")->click();
  QCOMPARE(opened.dialog->drawing().monitors[0].pixelsW(), 1920);
  QCOMPARE(opened.dialog->drawing().monitors[0].scale, 100.0);
}

void MonitorEditorDialogTests::controls_followTheSelectedMonitor()
{
  auto layout = windowsPc();
  layout.monitors[1].w = 1000;
  layout.monitors[1].h = 700;
  layout.monitors[1].y = 100 + 960 - 700;
  auto opened = open(layout);
  auto *resolution = child<QComboBox>(opened, "cbResolution");
  auto *scale = child<QComboBox>(opened, "cbScale");
  auto *w = child<QSpinBox>(opened, "sbPixelsW");
  auto *h = child<QSpinBox>(opened, "sbPixelsH");

  // the first one is a preset at 150 %
  opened.widget->setSelected(0);
  QCOMPARE(resolution->currentData().toPoint(), QPoint(2560, 1440));
  QCOMPARE(scale->currentText(), QStringLiteral("150 %"));
  QVERIFY(!w->isVisibleTo(opened.dialog.get()));

  // the second one is not a preset: its pixels are for the spin boxes
  opened.widget->setSelected(1);
  QCOMPARE(scale->currentText(), QStringLiteral("100 %"));
  QVERIFY(w->isVisibleTo(opened.dialog.get()));
  QCOMPARE(w->value(), 1000);
  QCOMPARE(h->value(), 700);
  w->setValue(1280);
  h->setValue(720);
  Q_EMIT w->editingFinished();
  QCOMPARE(opened.dialog->drawing().monitors[1].pixelsW(), 1280);
  QCOMPARE(opened.dialog->drawing().monitors[1].pixelsH(), 720);
  QCOMPARE(resolution->currentData().toPoint(), QPoint(1280, 720));

  // nothing selected: nothing to set
  opened.widget->setSelected(-1);
  QVERIFY(!resolution->isEnabled());
  QVERIFY(!scale->isEnabled());
  QVERIFY(!child<QPushButton>(opened, "btnRotate")->isEnabled());
  QVERIFY(!child<QPushButton>(opened, "btnRemoveMonitor")->isEnabled());
}

void MonitorEditorDialogTests::controls_otherSize_staysChosenUntilTheSizeOrTheSelectionChanges()
{
  Monitor one;
  auto opened = open(Layout{{one, added(Layout{{one}}).monitors[1]}});
  auto *resolution = child<QComboBox>(opened, "cbResolution");
  auto *w = child<QSpinBox>(opened, "sbPixelsW");
  auto *h = child<QSpinBox>(opened, "sbPixelsH");
  opened.widget->setSelected(0);
  QCOMPARE(resolution->currentData().toPoint(), QPoint(1920, 1080));
  QVERIFY(!w->isVisibleTo(opened.dialog.get()));

  // "Other size..." on a monitor that has the size of a preset: the combo stays there, and the sizes are for typing
  const auto last = resolution->count() - 1;
  QCOMPARE(resolution->itemData(last).toPoint(), QPoint(0, 0));
  resolution->setCurrentIndex(last);
  Q_EMIT resolution->activated(last);
  QCOMPARE(resolution->currentIndex(), last);
  QVERIFY(w->isVisibleTo(opened.dialog.get()) && h->isVisibleTo(opened.dialog.get()));
  QVERIFY(w->isEnabled() && h->isEnabled());
  QCOMPARE(opened.dialog->drawing().monitors[0].pixelsW(), 1920);
  QCOMPARE(w->value(), 1920);

  // what is typed is the size, and it is still "other" when it is not a preset
  w->setValue(1000);
  h->setValue(700);
  Q_EMIT w->editingFinished();
  QCOMPARE(opened.dialog->drawing().monitors[0].pixelsW(), 1000);
  QCOMPARE(resolution->currentIndex(), last);

  // another monitor is selected and this one again: it is as it is, a size that is none of the presets
  opened.widget->setSelected(1);
  QCOMPARE(resolution->currentData().toPoint(), QPoint(1920, 1080));
  QVERIFY(!w->isVisibleTo(opened.dialog.get()));
  opened.widget->setSelected(0);
  QCOMPARE(resolution->currentIndex(), last);

  // and "other" on a preset again, then another monitor: it goes back to what the preset is
  opened.widget->setSelected(1);
  resolution->setCurrentIndex(last);
  Q_EMIT resolution->activated(last);
  QVERIFY(w->isVisibleTo(opened.dialog.get()));
  opened.widget->setSelected(0);
  opened.widget->setSelected(1);
  QCOMPARE(resolution->currentData().toPoint(), QPoint(1920, 1080));
  QVERIFY(!w->isVisibleTo(opened.dialog.get()));
}

void MonitorEditorDialogTests::problems_disableOk_andAreSaid()
{
  Monitor a;
  Monitor overlapping = a;
  overlapping.x = 1000;
  auto opened = open(Layout{{a, overlapping}});
  QVERIFY2(statusOf(opened).contains(QStringLiteral("overlap")), qPrintable(statusOf(opened)));
  QVERIFY(!okIsEnabled(opened));
  QVERIFY(!opened.dialog->canSave());

  Monitor far = a;
  far.x = 5000;
  auto apart = open(Layout{{a, far}});
  QVERIFY2(statusOf(apart).contains(QStringLiteral("does not touch")), qPrintable(statusOf(apart)));
  QVERIFY(!okIsEnabled(apart));
  // it is mended by dragging it back
  const auto scale = scaleOf(apart);
  apart.widget->setSelected(1);
  const auto from = centre(apart, 1);
  drag(apart.widget, from, from - QPoint(static_cast<int>(std::lround(3080 * scale)), 0));
  QVERIFY2(okIsEnabled(apart), qPrintable(statusOf(apart)));
}

void MonitorEditorDialogTests::numbers_areInStepWithTheDrawing_andCanBeUsed()
{
  auto opened = open(windowsPc(), kMatching);
  auto *group = child<QGroupBox>(opened, "groupAdvancedNumbers");
  auto *numbers = child<QPlainTextEdit>(opened, "textNumbers");
  QVERIFY(!group->isChecked());
  QVERIFY(!numbers->isVisibleTo(opened.dialog.get()));
  group->setChecked(true);
  QVERIFY(numbers->isVisibleTo(opened.dialog.get()));
  QCOMPARE(numbers->toPlainText(), QStringLiteral("0,0,2560,1440;-2880,-180,2880,1620"));

  // the drawing changes, the numbers follow
  child<QPushButton>(opened, "btnRotate")->click();
  QCOMPARE(numbers->toPlainText(), QString::fromStdString(madeOf(opened).text));

  // numbers that are used become the drawing
  numbers->setPlainText(QStringLiteral("0,0,1920,1080;1920,0,1920,1080"));
  child<QPushButton>(opened, "btnApplyNumbers")->click();
  QCOMPARE(opened.dialog->drawing().monitors.size(), size_t{2});
  QCOMPARE(opened.dialog->drawing().monitors[1].pixelsW(), 1920);
  QVERIFY(child<QLabel>(opened, "lblNumbersStatus")->text().isEmpty());

  // and numbers that are not a layout are said, and change nothing
  const auto before = opened.dialog->drawing();
  numbers->setPlainText(QStringLiteral("0,0,100"));
  child<QPushButton>(opened, "btnApplyNumbers")->click();
  QVERIFY(!child<QLabel>(opened, "lblNumbersStatus")->text().isEmpty());
  QCOMPARE(opened.dialog->drawing(), before);
}

void MonitorEditorDialogTests::keys_rotateDeleteAndNudge()
{
  auto opened = open(twoApart());
  opened.widget->setSelected(1);
  opened.widget->setFocus();
  QTest::keyClick(opened.widget, Qt::Key_Right);
  QCOMPARE(opened.dialog->drawing().monitors[1].x, 2301.0);
  QTest::keyClick(opened.widget, Qt::Key_Down, Qt::ShiftModifier);
  QCOMPARE(opened.dialog->drawing().monitors[1].y, 310.0);
  QTest::keyClick(opened.widget, Qt::Key_R);
  QCOMPARE(opened.dialog->drawing().monitors[1].pixelsW(), 1080);
  QTest::keyClick(opened.widget, Qt::Key_Delete);
  QCOMPARE(opened.dialog->drawing().monitors.size(), size_t{1});
  QTest::keyClick(opened.widget, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(opened.dialog->drawing().monitors.size(), size_t{2});
}

void MonitorEditorDialogTests::keys_deletingTheLastOne_leavesASelectionThatIsThere()
{
  auto opened = open(twoApart());
  opened.widget->setSelected(1);
  opened.widget->setFocus();
  QTest::keyClick(opened.widget, Qt::Key_Delete);
  QCOMPARE(opened.dialog->drawing().monitors.size(), size_t{1});
  // what is selected is the monitor that is left, and nothing is asked of one that is not there
  QCOMPARE(opened.widget->selected(), 0);
  QVERIFY(!child<QPushButton>(opened, "btnMakeMain")->isEnabled());
  QVERIFY(!child<QPushButton>(opened, "btnRemoveMonitor")->isEnabled());
  QTest::keyClick(opened.widget, Qt::Key_Right);
  QCOMPARE(opened.dialog->drawing().monitors[0].x, 1.0);
}

// The monitors that the computer reports are shown, and nothing of the drawing can be changed with the mouse, the keys,
// or the controls, until the user chooses to draw his own
void MonitorEditorDialogTests::reported_theMonitorsOfTheComputer_areShownAndCannotBeChanged()
{
  auto opened = open(windowsPc(), kMatching);
  // the buttons of the numbers are in a group that disables them while it is closed: it is open to see their own state
  child<QGroupBox>(opened, "groupAdvancedNumbers")->setChecked(true);
  // there is something to undo when the reported monitors are shown: the shortcut must not undo it
  child<QPushButton>(opened, "btnRotate")->click();
  QVERIFY(child<QPushButton>(opened, "btnUndo")->isEnabled());
  opened.dialog->showReportedMonitors();
  QApplication::processEvents();

  QVERIFY(opened.dialog->isShowingReportedMonitors());
  QVERIFY2(
      statusOf(opened).contains(QStringLiteral("These are the monitors that PC reports")), qPrintable(statusOf(opened))
  );
  QVERIFY(child<QPushButton>(opened, "btnDrawOwn")->isVisibleTo(opened.dialog.get()));
  for (const auto *name :
       {"cbResolution", "cbScale", "btnRotate", "btnMakeMain", "btnAddMonitor", "btnRemoveMonitor", "btnUndo",
        "btnRedo", "btnStretchToReported", "btnApplyNumbers", "btnImportNumbers", "sbPixelsW", "sbPixelsH"})
    QVERIFY2(!child<QWidget>(opened, name)->isEnabled(), name);
  QVERIFY(child<QPlainTextEdit>(opened, "textNumbers")->isReadOnly());
  // it can be closed
  QVERIFY(okIsEnabled(opened));

  // a drag, the keys and the shortcut of the undo do nothing
  QSignalSpy changed(opened.widget, &MonitorEditorWidget::drawingChanged);
  const auto before = opened.dialog->drawing();
  const auto from = centre(opened, 1);
  drag(opened.widget, from, from + QPoint(-40, 30));
  QTest::keyClick(opened.widget, Qt::Key_Delete);
  QTest::keyClick(opened.widget, Qt::Key_Right);
  QTest::keyClick(opened.dialog.get(), Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(changed.count(), 0);
  QCOMPARE(opened.dialog->drawing().monitors.size(), before.monitors.size());
  QVERIFY(opened.dialog->drawing().monitors[1].x == before.monitors[1].x);
  QVERIFY(opened.dialog->drawing().monitors[1].y == before.monitors[1].y);
  QVERIFY(opened.dialog->drawing() == before);
  QVERIFY(opened.dialog->isShowingReportedMonitors());
}

void MonitorEditorDialogTests::reported_drawingYourOwn_enablesTheEditing()
{
  auto opened = open(windowsPc(), kMatching);
  child<QGroupBox>(opened, "groupAdvancedNumbers")->setChecked(true);
  opened.dialog->showReportedMonitors();
  child<QPushButton>(opened, "btnDrawOwn")->click();
  QApplication::processEvents();

  QVERIFY(!opened.dialog->isShowingReportedMonitors());
  QVERIFY(!child<QPushButton>(opened, "btnDrawOwn")->isVisibleTo(opened.dialog.get()));
  QVERIFY(child<QPushButton>(opened, "btnAddMonitor")->isEnabled());
  QVERIFY(!child<QPlainTextEdit>(opened, "textNumbers")->isReadOnly());
  // the buttons of the numbers are enabled again, not only the ones of the drawing
  QVERIFY(child<QWidget>(opened, "btnApplyNumbers")->isEnabled());
  QVERIFY(child<QWidget>(opened, "btnImportNumbers")->isEnabled());
  QVERIFY2(statusOf(opened).contains(QStringLiteral("Matches what PC reports")), qPrintable(statusOf(opened)));

  // and now the drawing is the user's: it is edited like any other
  const auto before = opened.dialog->drawing();
  child<QPushButton>(opened, "btnRotate")->click();
  QVERIFY(!(opened.dialog->drawing() == before));
}

void MonitorEditorDialogTests::screenshots_areMadeOfTheEditor()
{
  // 1. a Windows PC of two monitors at different scales, and what it reports
  {
    auto opened = open(windowsPc(), kMatching);
    opened.widget->setSelected(0);
    saveGrab(*opened.dialog, QStringLiteral("9-monitor-editor-windows-pc-two-monitors.png"));
  }
  // 2. while an edge is dragged: the top of the left monitor goes to the top of the main one
  {
    auto opened = open(windowsPc(), kMatching);
    opened.widget->setSelected(1);
    const auto scale = scaleOf(opened);
    const auto r = opened.widget->rectOf(1);
    const QPoint top(static_cast<int>(std::lround(r.center().x())), static_cast<int>(std::lround(r.top())));
    // the top edge of the left monitor is 120 units above that of the main one; it is dragged to 3 units from it
    drag(opened.widget, top, top + QPoint(0, static_cast<int>(std::lround(117 * scale))), false);
    saveGrab(*opened.dialog, QStringLiteral("10-monitor-editor-dragging-an-edge.png"));
    QTest::mouseRelease(opened.widget, Qt::LeftButton, {}, top + QPoint(0, static_cast<int>(std::lround(117 * scale))));
    QCOMPARE(opened.dialog->drawing().monitors[1].y, opened.dialog->drawing().monitors[0].y);
  }
  // 3. a drawing that is not the shape of what the computer reports
  {
    auto opened = open(windowsPc(), kPhysical);
    opened.widget->setSelected(0);
    saveGrab(*opened.dialog, QStringLiteral("11-monitor-editor-mismatch-hint.png"));
  }
  // 4. the monitors that the computer reports, shown and not editable
  {
    auto opened = open(windowsPc(), kMatching);
    opened.dialog->showReportedMonitors();
    QApplication::processEvents();
    saveGrab(*opened.dialog, QStringLiteral("12-monitor-editor-reported-monitors.png"));
  }
}


void MonitorEditorDialogTests::newLook_hasTheBarOfActionsAndTheNumbersClosedUnderADisclosure()
{
  DeskflowLook look;
  auto opened = open(windowsPc(), kMatching);
  // the same widgets, found by the names that they always had
  for (const auto *name :
       {"btnRotate", "btnMakeMain", "btnAddMonitor", "btnRemoveMonitor", "btnUndo", "btnRedo", "cbResolution", "cbScale",
        "btnStretchToReported", "lblMonitorStatus", "groupAdvancedNumbers", "monitorEditorButtons"}) {
    QVERIFY2(opened.dialog->findChild<QWidget *>(QString::fromLatin1(name)), name);
  }
  // the actions on the monitors are a bar of flat buttons with an icon each, over the canvas, in the order of the design
  const QStringList bar = {"btnRotate", "btnMakeMain", "btnAddMonitor", "btnRemoveMonitor", "btnUndo", "btnRedo"};
  int previousX = -1;
  for (const auto &name : bar) {
    auto *button = child<QPushButton>(opened, qPrintable(name));
    QVERIFY2(button->isFlat() && !button->icon().isNull(), qPrintable(name));
    QVERIFY2(button->geometry().bottom() < opened.widget->geometry().top(), qPrintable(name));
    QVERIFY2(button->mapTo(opened.dialog.get(), QPoint(0, 0)).x() > previousX, qPrintable(name));
    previousX = button->mapTo(opened.dialog.get(), QPoint(0, 0)).x();
  }
  // what the selected monitor is goes in a column beside the canvas
  auto *resolution = child<QComboBox>(opened, "cbResolution");
  QVERIFY(resolution->mapTo(opened.dialog.get(), QPoint(0, 0)).x() > opened.widget->geometry().right());

  // the numbers are closed, and what ticks the disclosure opens them
  auto *group = child<QGroupBox>(opened, "groupAdvancedNumbers");
  QVERIFY(qobject_cast<DisclosureBox *>(group));
  QVERIFY(!group->isChecked());
  QVERIFY(!child<QPlainTextEdit>(opened, "textNumbers")->isVisibleTo(opened.dialog.get()));
  group->setChecked(true);
  QVERIFY(child<QPlainTextEdit>(opened, "textNumbers")->isVisibleTo(opened.dialog.get()));
  QCOMPARE(
      child<QPlainTextEdit>(opened, "textNumbers")->toPlainText(), QStringLiteral("0,0,2560,1440;-2880,-180,2880,1620")
  );
}

void MonitorEditorDialogTests::newLook_theDisclosureOpensWithTheMouseAndTheKeys()
{
  DeskflowLook look;
  auto opened = open(windowsPc(), kMatching);
  auto *group = child<QGroupBox>(opened, "groupAdvancedNumbers");
  auto *numbers = child<QPlainTextEdit>(opened, "textNumbers");
  QVERIFY(!numbers->isVisibleTo(opened.dialog.get()));
  // the title is what is clicked
  QStyleOptionGroupBox box;
  box.initFrom(group);
  box.text = group->title();
  box.subControls = QStyle::SC_GroupBoxLabel | QStyle::SC_GroupBoxCheckBox;
  box.features = QStyleOptionFrame::Flat;
  box.textAlignment = Qt::AlignLeft;
  const auto title = group->style()->subControlRect(QStyle::CC_GroupBox, &box, QStyle::SC_GroupBoxLabel, group);
  QVERIFY(!title.isEmpty());
  QTest::mouseClick(group, Qt::LeftButton, {}, title.center());
  QVERIFY(group->isChecked());
  QVERIFY(numbers->isVisibleTo(opened.dialog.get()));
  group->setFocus();
  QTest::keyClick(group, Qt::Key_Space);
  QVERIFY(!group->isChecked());
  QVERIFY(!numbers->isVisibleTo(opened.dialog.get()));

  // a screen reader is told that it is a button that is expanded or collapsed, and it can press it
  auto *interface = QAccessible::queryAccessibleInterface(group);
  QVERIFY(interface);
  QCOMPARE(roleName(interface->role()), QStringLiteral("Button"));
  QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Advanced: numbers"));
  QVERIFY(interface->state().expandable && interface->state().collapsed && !interface->state().expanded);
  interface->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
  QVERIFY(group->isChecked());
  QVERIFY(interface->state().expanded && !interface->state().collapsed);
}

void MonitorEditorDialogTests::newLook_tabGoesThroughTheMonitors_enterAndSpaceSelect()
{
  DeskflowLook look;
  auto opened = open(twoApart());
  opened.widget->setSelected(-1);
  opened.widget->setFocus();
  QVERIFY(opened.widget->hasFocus());
  QSignalSpy selected(opened.widget, &MonitorEditorWidget::selectionChanged);

  QTest::keyClick(opened.widget, Qt::Key_Tab);
  QCOMPARE(opened.widget->selected(), 0);
  QTest::keyClick(opened.widget, Qt::Key_Tab);
  QCOMPARE(opened.widget->selected(), 1);
  QTest::keyClick(opened.widget, Qt::Key_Tab, Qt::ShiftModifier);
  QCOMPARE(opened.widget->selected(), 0);
  QVERIFY(opened.widget->hasFocus());
  // after the last the focus leaves, as before the first: the keys are not held by the canvas
  QTest::keyClick(opened.widget, Qt::Key_Tab);
  QTest::keyClick(opened.widget, Qt::Key_Tab);
  QVERIFY(!opened.widget->hasFocus());
  QCOMPARE(opened.widget->selected(), 1);
  QVERIFY(selected.count() >= 4);

  // a click on the background takes the selection away: Enter and Space give it back, where the keys were
  opened.widget->setFocus();
  QTest::mouseClick(opened.widget, Qt::LeftButton, {}, QPoint(3, 3));
  QCOMPARE(opened.widget->selected(), -1);
  QTest::keyClick(opened.widget, Qt::Key_Space);
  QCOMPARE(opened.widget->selected(), 1);
  // a monitor that is selected leaves Enter to the dialog (its default button), as it was: the key reaches the dialog
  KeyCounter reaching(Qt::Key_Return);
  opened.dialog->installEventFilter(&reaching);
  QTest::keyClick(opened.widget, Qt::Key_Return);
  QCOMPARE(reaching.count, 1);
  opened.dialog->removeEventFilter(&reaching);

  // the keys that were there are still there
  QTest::keyClick(opened.widget, Qt::Key_Right);
  QCOMPARE(opened.dialog->drawing().monitors[1].x, 2301.0);
}

void MonitorEditorDialogTests::newLook_monitorsAreNeutral_andTheSelectedOneAndWhatIsWrongHaveColour()
{
  DeskflowLook look;
  const auto theme = deskflow::gui::style::currentCanvasTheme();
  auto opened = open(twoApart());
  opened.widget->setSelected(0);
  opened.widget->clearFocus();
  const auto image = opened.widget->grab().toImage();
  // away from the names and the handles: low in each monitor
  const auto at = [&](int index) {
    const auto r = opened.widget->rectOf(index);
    return image.pixelColor(QPoint(static_cast<int>(r.left() + r.width() * 0.85), static_cast<int>(r.top() + r.height() * 0.9)));
  };
  QCOMPARE(at(0), theme.fill);
  QCOMPARE(at(1), theme.fill);
  // the selected one has the ring of the accent
  const auto first = opened.widget->rectOf(0);
  QCOMPARE(image.pixelColor(QPoint(static_cast<int>(first.left() + first.width() * 0.25), static_cast<int>(first.top()) + 1)), theme.selected);

  // an overlap is hatched in the colour of an error
  auto wrong = open(Layout{{[] {
                     Monitor m;
                     m.w = 1920;
                     m.h = 1080;
                     return m;
                   }(),
                            [] {
                              Monitor m;
                              m.x = 600;
                              m.y = 300;
                              m.w = 1920;
                              m.h = 1080;
                              return m;
                            }()}});
  wrong.widget->setSelected(-1);
  const auto overlap = wrong.widget->grab().toImage();
  bool hatched = false;
  // (inside the outline, which is dashed in the same colour: it is the hatch that is looked for)
  const auto box = wrong.widget->rectOf(1).adjusted(8, 8, -8, -8);
  for (int y = static_cast<int>(box.top()); y < box.bottom() && !hatched; ++y) {
    for (int x = static_cast<int>(box.left()); x < box.right(); ++x) {
      const auto p = overlap.pixelColor(x, y);
      if (qAbs(p.hueF() - theme.error.hueF()) < 0.03 && p.saturationF() > 0.2) {
        hatched = true;
        break;
      }
    }
  }
  QVERIFY(hatched);
}

void MonitorEditorDialogTests::newLook_aScreenReaderGetsAListOfTheMonitors()
{
  DeskflowLook look;
  auto opened = open(windowsPc(), kMatching);
  opened.widget->setSelected(1);
  QAccessible::setActive(true);
  auto *list = QAccessible::queryAccessibleInterface(opened.widget);
  QVERIFY(list);
  QCOMPARE(roleName(list->role()), QStringLiteral("List"));
  QCOMPARE(list->text(QAccessible::Name), QStringLiteral("Monitors of the computer"));
  QCOMPARE(list->childCount(), 2);
  QCOMPARE(list->child(0)->text(QAccessible::Name), QStringLiteral("Monitor 1"));
  QCOMPARE(list->child(1)->text(QAccessible::Name), QStringLiteral("Monitor 2"));
  QCOMPARE(roleName(list->child(0)->role()), QStringLiteral("ListItem"));
  QCOMPARE(list->child(0)->text(QAccessible::Description), QStringLiteral("2560 × 1440 · 150 %, main"));
  QCOMPARE(list->child(1)->text(QAccessible::Description), QStringLiteral("1920 × 1080 · 100 %"));
  QVERIFY(!list->child(0)->state().selected);
  QVERIFY(list->child(1)->state().selected);
  list->child(0)->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
  QCOMPARE(opened.widget->selected(), 0);

  // what is wrong is said in words
  auto apart = open(Layout{{[] {
                              Monitor m;
                              m.w = 1920;
                              m.h = 1080;
                              return m;
                            }(),
                            [] {
                              Monitor m;
                              m.x = 5000;
                              m.y = 4000;
                              m.w = 1920;
                              m.h = 1080;
                              return m;
                            }()}});
  auto *apartList = QAccessible::queryAccessibleInterface(apart.widget);
  QVERIFY(apartList->child(1)->text(QAccessible::Description).contains(QStringLiteral("does not touch the others")));
  QAccessible::setActive(false);
}

void MonitorEditorDialogTests::systemLook_isTheEditorItWas()
{
  auto opened = open(windowsPc(), kMatching);
  // the controls are in the two rows that they have always had, under the canvas
  const auto below = opened.widget->geometry().bottom();
  for (const auto *name : {"cbResolution", "cbScale", "btnRotate", "btnMakeMain", "btnAddMonitor", "btnUndo"})
    QVERIFY2(child<QWidget>(opened, name)->geometry().top() > below, name);
  QVERIFY(!child<QPushButton>(opened, "btnRotate")->isFlat());
  QVERIFY(child<QPushButton>(opened, "btnRotate")->icon().isNull());
  // the numbers are a group box with a check box, and a screen reader is told so
  auto *group = child<QGroupBox>(opened, "groupAdvancedNumbers");
  QVERIFY(!group->isChecked());
  auto *interface = QAccessible::queryAccessibleInterface(group);
  QVERIFY(roleName(interface->role()) != QStringLiteral("Button"));
  // the monitors are not told to a screen reader one by one, and each has its own colour
  QCOMPARE(QAccessible::queryAccessibleInterface(opened.widget)->childCount(), 0);
  opened.widget->setSelected(-1);
  const auto image = opened.widget->grab().toImage();
  const auto at = [&](int index) {
    const auto r = opened.widget->rectOf(index);
    return image.pixelColor(QPoint(static_cast<int>(r.left() + r.width() * 0.85), static_cast<int>(r.top() + r.height() * 0.9)));
  };
  QVERIFY(at(0) != at(1));
}

QTEST_MAIN(MonitorEditorDialogTests)
