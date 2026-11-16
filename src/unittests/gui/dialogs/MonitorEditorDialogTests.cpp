/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MonitorEditorDialogTests.h"

#include "common/Settings.h"
#include "dialogs/MonitorEditorDialog.h"
#include "gui/widgets/MonitorEditorWidget.h"
#include "server/MonitorEditor.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
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

QTEST_MAIN(MonitorEditorDialogTests)
