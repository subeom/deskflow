/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutProfilesDialogTests.h"

#include "../layout/LayoutServiceRig.h"
#include "gui/dialogs/LayoutProfilesDialog.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/widgets/ArrangementCanvas.h"
#include "gui/widgets/ArrangementPanel.h"
#include "gui/widgets/LayoutNoticeBar.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTabWidget>
#include <QTest>

using namespace layout_rig;

namespace {

constexpr qint64 kEpoch = 1'700'000'000'000;

QPushButton *buttonIn(const QWidget &widget, const char *name)
{
  return widget.findChild<QPushButton *>(QString::fromLatin1(name));
}

QString textOf(const QTableWidget *table, int row, int column)
{
  return table->item(row, column)->text();
}

} // namespace

void LayoutProfilesDialogTests::initTestCase()
{
  QVERIFY(m_dir.isValid());
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("Deskflow.state")));
}

void LayoutProfilesDialogTests::init()
{
  QFile::remove(m_dir.filePath(QStringLiteral("layout-profiles.json")));
}

void LayoutProfilesDialogTests::cleanup()
{
  forgetEverything();
}

void LayoutProfilesDialogTests::noticeBar_isHiddenUntilItHasSomethingToSay()
{
  LayoutNoticeBar bar;
  QVERIFY(bar.isHidden());
  bar.setText(QStringLiteral("something happened"), true);
  QVERIFY(!bar.isHidden());
  bar.setNotice(std::nullopt);
  QVERIFY(bar.isHidden());
  QVERIFY(bar.fullText().isEmpty());

  Notice notice;
  notice.kind = Notice::Kind::Undone;
  bar.setNotice(notice);
  QVERIFY(!bar.isHidden());
  QVERIFY(bar.fullText().contains(QStringLiteral("undone"), Qt::CaseInsensitive));
}

void LayoutProfilesDialogTests::noticeBar_showsTheTextWithUndoOnlyWhenItCanBeUndone()
{
  QWidget parent;
  parent.resize(900, 60);
  auto *bar = new LayoutNoticeBar(&parent);
  bar->setGeometry(0, 0, 900, 40);
  parent.show();

  bar->setText(QStringLiteral("hello"), true);
  QCoreApplication::processEvents();
  QVERIFY(buttonIn(*bar, "btnLayoutNoticeUndo")->isVisibleTo(bar));
  QVERIFY(buttonIn(*bar, "btnLayoutNoticeDismiss")->isVisibleTo(bar));
  QCOMPARE(bar->findChild<QLabel *>(QStringLiteral("lblLayoutNotice"))->text(), QStringLiteral("hello"));

  bar->setText(QStringLiteral("hello"), false);
  QVERIFY(!buttonIn(*bar, "btnLayoutNoticeUndo")->isVisibleTo(bar));
  QVERIFY(buttonIn(*bar, "btnLayoutNoticeDismiss")->isVisibleTo(bar));
}

void LayoutProfilesDialogTests::noticeBar_cutsALongTextToItsWidthAndKeepsItAllInTheToolTip()
{
  QWidget parent;
  parent.resize(320, 60);
  auto *bar = new LayoutNoticeBar(&parent);
  bar->setGeometry(0, 0, 320, 40);
  parent.show();
  const auto text = QStringLiteral("Layout restored: \"a name that is long\" ") + QString(300, QLatin1Char('x'));
  bar->setText(text, true);
  QCoreApplication::processEvents();

  const auto *label = bar->findChild<QLabel *>(QStringLiteral("lblLayoutNotice"));
  QVERIFY(label->text().size() < text.size());
  QVERIFY(label->text().endsWith(QChar(0x2026)));
  QCOMPARE(bar->toolTip(), text);
  QCOMPARE(bar->fullText(), text);
  // and it is cut again when the bar is made wider
  parent.resize(2600, 60);
  bar->setGeometry(0, 0, 2600, 40);
  QCoreApplication::processEvents();
  QVERIFY(label->text().size() > 100);
}

void LayoutProfilesDialogTests::noticeBar_tellsWhatTheUserClicked()
{
  LayoutNoticeBar bar;
  bar.setText(QStringLiteral("x"), true);
  QSignalSpy undo(&bar, &LayoutNoticeBar::undoRequested);
  QSignalSpy dismiss(&bar, &LayoutNoticeBar::dismissRequested);
  buttonIn(bar, "btnLayoutNoticeUndo")->click();
  QCOMPARE(undo.count(), 1);
  QCOMPARE(dismiss.count(), 0);
  buttonIn(bar, "btnLayoutNoticeDismiss")->click();
  QCOMPARE(dismiss.count(), 1);
}

void LayoutProfilesDialogTests::dialog_listsTheLayouts_theNewestUseFirst_andMarksTheOneThatIsCurrent()
{
  arrangeT();
  ServiceRig rig(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  rig.turnOn();
  rig.service->store().upsert(sig(laptopOnly()), captureLayout(*rig.config), kEpoch + 9000);

  LayoutProfilesDialog dialog(*rig.service, [&] { return captureLayout(*rig.config); });
  auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("layoutProfilesTable"));
  QVERIFY(table != nullptr);
  QCOMPARE(table->rowCount(), 2);
  QCOMPARE(table->columnCount(), 3);
  // the one that was used last is the first
  QCOMPARE(textOf(table, 0, 0), LayoutProfileStore::suggestName(sig(laptopOnly())));
  QCOMPARE(textOf(table, 0, 2), sig(laptopOnly()).summary());
  QVERIFY(textOf(table, 1, 0).contains(QStringLiteral("(current)")));
  QVERIFY(textOf(table, 1, 0).contains(LayoutProfileStore::suggestName(sig(tShape()))));
  QCOMPARE(textOf(table, 1, 2), sig(tShape()).summary());
  QVERIFY(!textOf(table, 1, 1).isEmpty());
  // the list is read only
  QVERIFY(!(table->item(0, 0)->flags() & Qt::ItemIsEditable));
  // nothing is selected, so nothing can be renamed or forgotten yet
  QVERIFY(!buttonIn(dialog, "btnRenameLayout")->isEnabled());
  QVERIFY(!buttonIn(dialog, "btnDeleteLayout")->isEnabled());
  QVERIFY(buttonIn(dialog, "btnSaveLayoutAs")->isEnabled());
  table->selectRow(0);
  QVERIFY(buttonIn(dialog, "btnRenameLayout")->isEnabled());
  QVERIFY(buttonIn(dialog, "btnDeleteLayout")->isEnabled());
}

void LayoutProfilesDialogTests::dialog_renamesForgetsAndSaves_asAtOnce()
{
  arrangeT();
  const auto path = m_dir.filePath(QStringLiteral("layout-profiles.json"));
  ServiceRig rig(path);
  rig.turnOn();
  rig.service->store().upsert(sig(laptopOnly()), captureLayout(*rig.config), kEpoch + 9000);

  LayoutProfilesDialog dialog(*rig.service, [&] { return captureLayout(*rig.config); });
  auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("layoutProfilesTable"));
  table->selectRow(0);
  QVERIFY(dialog.renameSelected(QStringLiteral("On the move")));
  QCOMPARE(textOf(table, 0, 0), QStringLiteral("On the move"));
  QVERIFY(rig.service->store().findExact(sig(laptopOnly()))->userNamed);
  QVERIFY(!dialog.renameSelected(QStringLiteral("   ")));
  QCOMPARE(textOf(table, 0, 0), QStringLiteral("On the move"));

  // it is in the file already: this is not undone by cancelling a dialog
  LayoutProfileStore reread(path);
  reread.load();
  QCOMPARE(reread.findExact(sig(laptopOnly()))->name, QStringLiteral("On the move"));

  QVERIFY(dialog.saveCurrentAs(QStringLiteral("At the desk")));
  reread.load();
  QCOMPARE(reread.findExact(sig(tShape()))->name, QStringLiteral("At the desk"));
  QVERIFY(reread.findExact(sig(tShape()))->userNamed);
  QCOMPARE(dialog.selectedId(), rig.service->store().findExact(sig(tShape()))->id);

  table->selectRow(0);
  const auto forgotten = dialog.selectedId();
  QVERIFY(dialog.deleteSelected());
  QCOMPARE(table->rowCount(), 1);
  QVERIFY(rig.service->store().find(forgotten) == nullptr);
  reread.load();
  QCOMPARE(reread.profiles().size(), size_t(1));
  // with nothing selected there is nothing to forget
  QVERIFY(dialog.selectedId().isEmpty() || dialog.selectedId() != forgotten);
}

void LayoutProfilesDialogTests::dialog_whenTheFeatureIsOff_isEmptyAndCannotChangeAnything()
{
  arrangeT();
  ServiceRig rig(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  QVERIFY(!rig.service->isRunning());

  LayoutProfilesDialog dialog(*rig.service, [&] { return captureLayout(*rig.config); });
  auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("layoutProfilesTable"));
  QCOMPARE(table->rowCount(), 0);
  QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("lblLayoutProfilesInfo"))->text().contains(QStringLiteral("Switched off")));
  QVERIFY(!buttonIn(dialog, "btnRenameLayout")->isEnabled());
  QVERIFY(!buttonIn(dialog, "btnDeleteLayout")->isEnabled());
  QVERIFY(!buttonIn(dialog, "btnSaveLayoutAs")->isEnabled());
  QVERIFY(!dialog.saveCurrentAs(QStringLiteral("x")));
  // and nothing was written
  QVERIFY(!QFile::exists(m_dir.filePath(QStringLiteral("layout-profiles.json"))));
}

void LayoutProfilesDialogTests::dialog_saysWhatTheDisplaysAre()
{
  arrangeT();
  ServiceRig rig(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  rig.turnOn();
  LayoutProfilesDialog dialog(*rig.service, [&] { return captureLayout(*rig.config); });
  const auto text = dialog.findChild<QLabel *>(QStringLiteral("lblCurrentDisplays"))->text();
  QVERIFY2(text.contains(sig(tShape()).id()), qPrintable(text));
  QVERIFY(text.contains(QStringLiteral("3440x1440")));
  // a serial number is not in it
  QVERIFY(!text.contains(QStringLiteral("700001")));
}

void LayoutProfilesDialogTests::dialog_readsTheListAgainWhenTheLayoutsChange()
{
  arrangeT();
  ServiceRig rig(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  rig.turnOn();
  LayoutProfilesDialog dialog(*rig.service, [&] { return captureLayout(*rig.config); });
  auto *table = dialog.findChild<QTableWidget *>(QStringLiteral("layoutProfilesTable"));
  QCOMPARE(table->rowCount(), 1);

  rig.unplug();
  QCOMPARE(table->rowCount(), 2);
  QVERIFY(textOf(table, 0, 0).contains(QStringLiteral("(current)")));
}

void LayoutProfilesDialogTests::panel_showsWhatWasDone_andUndoTakesItBackInTheCopyOfTheDialog()
{
  arrangeT();
  ServiceRig rig(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  rig.turnOn();
  const auto layoutT = captureLayout(*rig.config);
  rig.unplug();
  QVERIFY(rig.service->notice().has_value());
  QCOMPARE(captureLayout(*rig.config).find("PC2")->x, 1512.0);

  ServerConfigDialog dialog(nullptr, *rig.config);
  dialog.show();
  auto *panel = dialog.findChild<ArrangementPanel *>();
  QVERIFY(panel != nullptr);
  auto *bar = panel->layoutNoticeBar();
  QVERIFY(bar->isVisibleTo(&dialog));
  QVERIFY(bar->fullText().contains(QStringLiteral("PC2")));
  auto *undo = buttonIn(*bar, "btnLayoutNoticeUndo");
  QVERIFY(undo->isVisibleTo(bar));

  const auto xOf = [&](const char *name) {
    const auto &arrangement = panel->canvas()->arrangement();
    return arrangement.computers[static_cast<size_t>(arrangement.indexOf(name))].box().x;
  };
  QCOMPARE(xOf("PC2"), 1512.0);
  undo->click();
  // the copy that this window works on has the computers where they were; the program's own arrangement is not touched
  // under an open window, and what is saved here is what is remembered
  QCOMPARE(xOf("PC2"), 3440.0);
  QCOMPARE(captureLayout(*rig.config).find("PC2")->x, 1512.0);
  QVERIFY(!rig.service->notice().has_value());
  QVERIFY(!rig.service->undoLayout().has_value());
  QVERIFY(bar->isHidden());
  QVERIFY(layoutT.find("PC2")->x == 3440.0);
}

void LayoutProfilesDialogTests::panel_dismissHidesTheNoticeForGood()
{
  arrangeT();
  ServiceRig rig(m_dir.filePath(QStringLiteral("layout-profiles.json")));
  rig.turnOn();
  rig.unplug();
  QVERIFY(rig.service->notice().has_value());

  ServerConfigDialog dialog(nullptr, *rig.config);
  dialog.show();
  auto *panel = dialog.findChild<ArrangementPanel *>();
  auto *bar = panel->layoutNoticeBar();
  QVERIFY(bar->isVisibleTo(&dialog));
  buttonIn(*bar, "btnLayoutNoticeDismiss")->click();
  QVERIFY(!rig.service->notice().has_value());
  QVERIFY(bar->isHidden());
  QVERIFY(!rig.service->undoLayout().has_value());
  // the change itself is kept
  QCOMPARE(captureLayout(*rig.config).find("PC2")->x, 1512.0);
}

void LayoutProfilesDialogTests::panel_hasNoNoticeAndNoListWithoutAService()
{
  arrangeT();
  QVERIFY(LayoutMemoryService::instance() == nullptr);
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  auto *panel = dialog.findChild<ArrangementPanel *>();
  QVERIFY(panel != nullptr);
  QVERIFY(panel->layoutNoticeBar()->isHidden());
  QVERIFY(!buttonIn(*panel, "btnRememberedLayouts")->isEnabled());
  QVERIFY(panel->findChild<QCheckBox *>(QStringLiteral("cbRememberArrangements")) != nullptr);
}

QTEST_MAIN(LayoutProfilesDialogTests)
