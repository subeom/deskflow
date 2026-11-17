/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerSettingsCardsTests.h"

#include "common/Settings.h"
#include "gui/config/Computer.h"
#include "gui/dialogs/ComputerSettingsDialog.h"
#include "gui/style/Appearance.h"
#include "gui/style/BackdropWidget.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmapCache>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

using namespace deskflow::gui::style;

namespace {

void installDeskflowStyle()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

template <class T> T *child(const QObject &parent, const char *name)
{
  auto *found = parent.findChild<T *>(QString::fromLatin1(name));
  Q_ASSERT_X(found, "child", name);
  return found;
}

SettingRow *rowOf(const QWidget *widget)
{
  for (auto *parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
    if (auto *row = qobject_cast<SettingRow *>(parent))
      return row;
  }
  return nullptr;
}

SettingCard *cardOf(const QWidget *widget)
{
  for (auto *parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
    if (auto *card = qobject_cast<SettingCard *>(parent))
      return card;
  }
  return nullptr;
}

QString nameOf(QWidget *widget)
{
  return QAccessible::queryAccessibleInterface(widget)->text(QAccessible::Name);
}

// The text of the label of a control, as the relation that a screen reader asks for
QString labelOf(QWidget *widget)
{
  const auto relations = QAccessible::queryAccessibleInterface(widget)->relations(QAccessible::Label);
  return relations.isEmpty() ? QString() : relations.first().first->text(QAccessible::Name);
}

// every control of the form that is set or read when the dialog is saved
const QStringList kSwitches = {
    QStringLiteral("chkShareClipboard"),
    QStringLiteral("chkDeadTopLeft"),
    QStringLiteral("chkDeadTopRight"),
    QStringLiteral("chkDeadBottomLeft"),
    QStringLiteral("chkDeadBottomRight"),
    QStringLiteral("chkFixScrollLock"),
    QStringLiteral("chkFixCapsLock"),
    QStringLiteral("chkFixNumLock"),
    QStringLiteral("chkFixXTest"),
    QStringLiteral("chkWeakX11Focus"),
    QStringLiteral("chkCrossOSInvertX"),
    QStringLiteral("chkCrossOSInvertY"),
    QStringLiteral("chkCrossOSSemanticMedia"),
};
const QStringList kCombos = {
    QStringLiteral("comboShift"),  QStringLiteral("comboCtrl"),
    QStringLiteral("comboAlt"),    QStringLiteral("comboMeta"),
    QStringLiteral("comboSuper"),  QStringLiteral("comboAltGr"),
    QStringLiteral("comboF18Key"), QStringLiteral("comboCrossOSInvertScope"),
};
const QStringList kNumbers = {
    QStringLiteral("sbSwitchCornerSize"),
    QStringLiteral("sbArrangementWidth"),
    QStringLiteral("sbArrangementHeight"),
    QStringLiteral("sbDisplayScale"),
};
const QStringList kOthers = {
    QStringLiteral("lineNameEdit"),
    QStringLiteral("lineAddAlias"),
    QStringLiteral("btnAddAlias"),
    QStringLiteral("listAliases"),
    QStringLiteral("btnRemoveAlias"),
    QStringLiteral("textMonitorLayout"),
    QStringLiteral("btnImportMonitorLayout"),
    QStringLiteral("lblMonitorLayoutStatus"),
};
const QStringList kGroups = {
    QStringLiteral("groupComputerName"),  QStringLiteral("groupModifers"), QStringLiteral("groupDeadCorners"),
    QStringLiteral("groupFixes"),         QStringLiteral("groupAliases"),  QStringLiteral("groupCrossOSInput"),
    QStringLiteral("groupMonitorLayout"),
};

// what the form changes: every control the other way, and a few that have a value
void changeEverything(ComputerSettingsDialog &dialog)
{
  child<QLineEdit>(dialog, "lineNameEdit")->setText(QStringLiteral("changed-pc"));
  for (const auto &name : kSwitches) {
    auto *box = child<QCheckBox>(dialog, qPrintable(name));
    box->setChecked(!box->isChecked());
  }
  for (const auto &name : kCombos) {
    auto *combo = child<QComboBox>(dialog, qPrintable(name));
    combo->setCurrentIndex((combo->currentIndex() + 1) % combo->count());
  }
  child<QSpinBox>(dialog, "sbSwitchCornerSize")->setValue(17);
  child<QSpinBox>(dialog, "sbArrangementWidth")->setValue(2560);
  child<QSpinBox>(dialog, "sbArrangementHeight")->setValue(1440);
  child<QSpinBox>(dialog, "sbDisplayScale")->setValue(150);
  child<QPlainTextEdit>(dialog, "textMonitorLayout")->setPlainText(QStringLiteral("0,0,1280,720"));
  child<QLineEdit>(dialog, "lineAddAlias")->setText(QStringLiteral("alias-one"));
  child<QPushButton>(dialog, "btnAddAlias")->click();
}

QStringList snapshot(const Computer &computer)
{
  using enum ComputerConfig::Fix;
  QStringList values{computer.name(), computer.aliases().join(QLatin1Char(',')), computer.monitorLayout()};
  values << QString::number(computer.shareClipboard());
  for (int i = 0; i < 6; ++i)
    values << QString::number(computer.modifier(i));
  for (int i = 0; i < 4; ++i)
    values << QString::number(computer.switchCorner(i));
  values << QString::number(computer.switchCornerSize());
  for (const auto fix : {CapsLock, NumLock, ScrollLock, XTest})
    values << QString::number(computer.fix(fix));
  values << QString::number(static_cast<int>(computer.f18Action())) << QString::number(computer.crossOSInvertX())
         << QString::number(computer.crossOSInvertY())
         << QString::number(static_cast<int>(computer.crossOSInvertScope()))
         << QString::number(computer.crossOSSemanticMedia()) << QString::number(computer.arrangementWidth())
         << QString::number(computer.arrangementHeight()) << QString::number(computer.arrangementZoom(), 'f', 4);
  return values;
}

} // namespace

void ComputerSettingsCardsTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
}

void ComputerSettingsCardsTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QPixmapCache::clear();
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor);
  Settings::setValue(Settings::Computer::WeakX11Focus.arg(QStringLiteral("changed-pc")));
  Settings::setValue(Settings::Computer::WeakX11Focus.arg(QStringLiteral("test-pc")));
}

void ComputerSettingsCardsTests::systemLook_isTheFormAsItWas()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QVERIFY(!isDeskflowStyleInstalled());
  Computer computer(QStringLiteral("test-pc"));
  ComputerSettingsDialog dialog(nullptr, &computer);

  QVERIFY(dialog.findChildren<SettingCard *>().isEmpty());
  QVERIFY(dialog.findChildren<SettingRow *>().isEmpty());
  QVERIFY(dialog.findChildren<BackdropWidget *>().isEmpty());
  for (const auto &name : kGroups)
    QVERIFY2(!child<QGroupBox>(dialog, qPrintable(name))->isHidden(), qPrintable(name));
  QVERIFY(!child<QLabel>(dialog, "lblNameError")->isHidden());
  QCOMPARE(child<QCheckBox>(dialog, "chkFixCapsLock")->parentWidget(), child<QGroupBox>(dialog, "groupFixes"));
  QCOMPARE(
      QAccessible::queryAccessibleInterface(child<QCheckBox>(dialog, "chkFixCapsLock"))->role(), QAccessible::CheckBox
  );
}

void ComputerSettingsCardsTests::deskflowLook_hasACardForEachGroup_andHidesTheGroups()
{
  installDeskflowStyle();
  Computer computer(QStringLiteral("test-pc"));
  ComputerSettingsDialog dialog(nullptr, &computer);

  QStringList titles;
  for (const auto *card : dialog.findChildren<SettingCard *>())
    titles << card->title();
  QCOMPARE(
      titles, (QStringList{
                  QStringLiteral("Computer Info"), QStringLiteral("Aliases"), QStringLiteral("Modifier Keys"),
                  QStringLiteral("Dead Corners"), QStringLiteral("Fixes"),
                  QStringLiteral("Cross-OS Input (macOS source to Windows destination)"),
                  QStringLiteral("Monitor layout (experimental)")
              })
  );
  for (const auto &name : kGroups)
    QVERIFY2(child<QGroupBox>(dialog, qPrintable(name))->isHidden(), qPrintable(name));
  QCOMPARE(dialog.findChildren<BackdropWidget *>().size(), 1);
  // the labels that the rows took the text of, and the labels of the errors, are not shown
  for (const auto &name :
       {"label_7", "label_2", "label_3", "label_4", "label_5", "label_6", "labelAltGr", "label", "lblF18Key",
        "lblCrossOSInvertScope", "lblArrangementSize", "lblDisplayScale", "lblNameError", "lblAliasError"})
    QVERIFY2(child<QLabel>(dialog, name)->isHidden(), name);
}

void ComputerSettingsCardsTests::deskflowLook_everyControlIsInARowOfACard()
{
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);
  installDeskflowStyle();
  Computer computer(QStringLiteral("test-pc"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));

  QStringList names = kSwitches + kCombos + kNumbers;
  for (const auto &name : names) {
    auto *control = child<QWidget>(dialog, qPrintable(name));
    QVERIFY2(rowOf(control) != nullptr, qPrintable(name));
    QVERIFY2(cardOf(control) != nullptr, qPrintable(name));
    QVERIFY2(control->isVisibleTo(&dialog), qPrintable(name));
    // a control is the one of its row, and the row is not empty
    QVERIFY2(!rowOf(control)->title().isEmpty(), qPrintable(name));
  }
  // what is not a row of its own is in a card all the same
  for (const auto &name : kOthers) {
    auto *control = child<QWidget>(dialog, qPrintable(name));
    QVERIFY2(cardOf(control) != nullptr, qPrintable(name));
    QVERIFY2(control->isVisibleTo(&dialog), qPrintable(name));
  }
  // the rows are 44 px high at the least
  for (auto *row : dialog.findChildren<SettingRow *>()) {
    if (row->isVisibleTo(&dialog))
      QVERIFY(row->height() >= SettingRow::minimumRowHeight());
  }
}

void ComputerSettingsCardsTests::deskflowLook_savesWhatTheFormSaved()
{
  Settings::setValue(Settings::Server::ExperimentalMultiMonitor, true);

  QApplication::setStyle(QStringLiteral("Fusion"));
  Computer before(QStringLiteral("test-pc"));
  const auto initial = snapshot(before);
  Computer system(QStringLiteral("test-pc"));
  {
    ComputerSettingsDialog dialog(nullptr, &system);
    changeEverything(dialog);
    dialog.accept();
  }
  const auto viaForm = snapshot(system);
  QVERIFY(viaForm != initial);
  const bool weakFocusViaForm =
      Settings::value(Settings::Computer::WeakX11Focus.arg(QStringLiteral("changed-pc"))).toBool();
  Settings::setValue(Settings::Computer::WeakX11Focus.arg(QStringLiteral("changed-pc")));

  installDeskflowStyle();
  Computer cards(QStringLiteral("test-pc"));
  {
    ComputerSettingsDialog dialog(nullptr, &cards);
    QVERIFY(!dialog.findChildren<SettingCard *>().isEmpty());
    changeEverything(dialog);
    dialog.accept();
  }
  QCOMPARE(snapshot(cards), viaForm);
  QCOMPARE(
      Settings::value(Settings::Computer::WeakX11Focus.arg(QStringLiteral("changed-pc"))).toBool(), weakFocusViaForm
  );
}

void ComputerSettingsCardsTests::deskflowLook_showsTheErrorOfTheNameAndOfTheAliasInTheirRows()
{
  installDeskflowStyle();
  Computer computer(QStringLiteral("test-pc"));
  ComputerSettingsDialog dialog(nullptr, &computer);

  auto *name = child<QLineEdit>(dialog, "lineNameEdit");
  auto *nameRow = rowOf(name);
  QVERIFY(nameRow->error().isEmpty());
  name->setText(QStringLiteral("Desk PC!"));
  QVERIFY(!child<QLabel>(dialog, "lblNameError")->text().isEmpty());
  QCOMPARE(nameRow->error(), child<QLabel>(dialog, "lblNameError")->text());
  QVERIFY(child<QLabel>(dialog, "lblNameError")->isHidden());
  QCOMPARE(QAccessible::queryAccessibleInterface(name)->text(QAccessible::Description), nameRow->error());
  // a name that is not good is not saved
  QSignalSpy accepted(&dialog, &QDialog::accepted);
  dialog.accept();
  QCOMPARE(accepted.count(), 0);
  QCOMPARE(computer.name(), QStringLiteral("test-pc"));
  name->setText(QStringLiteral("Desk-PC"));
  QVERIFY(nameRow->error().isEmpty());

  auto *alias = child<QLineEdit>(dialog, "lineAddAlias");
  auto *aliasRow = rowOf(alias);
  alias->setText(QStringLiteral("Desk PC!"));
  QVERIFY(!aliasRow->error().isEmpty());
  QVERIFY(!child<QPushButton>(dialog, "btnAddAlias")->isEnabled());
  alias->setText(QStringLiteral("desk-pc.local"));
  QVERIFY(aliasRow->error().isEmpty());
  QVERIFY(child<QPushButton>(dialog, "btnAddAlias")->isEnabled());
}

void ComputerSettingsCardsTests::deskflowLook_namesTheControlsByTheirRows()
{
  installDeskflowStyle();
  Computer computer(QStringLiteral("test-pc"));
  ComputerSettingsDialog dialog(nullptr, &computer);

  QCOMPARE(nameOf(child<QWidget>(dialog, "lineNameEdit")), QStringLiteral("Name"));
  QCOMPARE(nameOf(child<QWidget>(dialog, "chkShareClipboard")), QStringLiteral("Share clipboard with this computer"));
  // a combo box is named by its value (Qt): the row is its label, and says so through the relation of a label
  QCOMPARE(labelOf(child<QWidget>(dialog, "comboShift")), QStringLiteral("Shift"));
  QCOMPARE(labelOf(child<QWidget>(dialog, "comboAltGr")), QStringLiteral("Alt Gr"));
  QCOMPARE(nameOf(child<QWidget>(dialog, "chkDeadTopLeft")), QStringLiteral("Top Left"));
  QCOMPARE(nameOf(child<QWidget>(dialog, "sbSwitchCornerSize")), QStringLiteral("Corner size"));
  QCOMPARE(nameOf(child<QWidget>(dialog, "chkFixCapsLock")), QStringLiteral("CAPS LOCK key"));
  QCOMPARE(labelOf(child<QWidget>(dialog, "comboF18Key")), QStringLiteral("F18 key"));
  QCOMPARE(child<QWidget>(dialog, "comboF18Key")->accessibleName(), QStringLiteral("F18 key"));
  QCOMPARE(nameOf(child<QWidget>(dialog, "sbDisplayScale")), QStringLiteral("Display scale"));
  // what has no row of its own is named by its card
  QCOMPARE(nameOf(child<QWidget>(dialog, "listAliases")), QStringLiteral("Aliases"));
  QCOMPARE(nameOf(child<QWidget>(dialog, "textMonitorLayout")), QStringLiteral("Monitor layout (experimental)"));
  // the cards are groups
  for (auto *card : dialog.findChildren<SettingCard *>())
    QCOMPARE(QAccessible::queryAccessibleInterface(card)->role(), QAccessible::Grouping);
}

void ComputerSettingsCardsTests::deskflowLook_isAsSmallAsTheFormWas_andTheCardsScrollInIt()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QSize form;
  {
    Computer computer(QStringLiteral("test-pc"));
    ComputerSettingsDialog dialog(nullptr, &computer);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    form = dialog.minimumSizeHint();
  }

  installDeskflowStyle();
  Computer computer(QStringLiteral("test-pc"));
  ComputerSettingsDialog dialog(nullptr, &computer);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  const auto cards = dialog.minimumSizeHint();
  qInfo() << "minimum size: form" << form << "cards" << cards << "default" << dialog.sizeHint();
  // the cards ask for what the groups did (the style has a little more room of its own, and the cards scroll in it)
  const auto describeSizes =
      QStringLiteral("%1 x %2 / %3 x %4").arg(cards.width()).arg(cards.height()).arg(form.width()).arg(form.height());
  QVERIFY2(
      cards.height() <= form.height() * 12 / 10 && cards.height() >= form.height() * 9 / 10, qPrintable(describeSizes)
  );
  QVERIFY2(
      cards.width() <= form.width() * 12 / 10 && cards.width() >= form.width() * 9 / 10, qPrintable(describeSizes)
  );

  dialog.resize(cards);
  QCoreApplication::processEvents();
  auto *scroll = dialog.findChild<QScrollArea *>();
  QVERIFY(scroll);
  // they scroll, and are not cut at the side
  QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
  QVERIFY(scroll->widget()->width() <= scroll->viewport()->width());
  // the last of them can be reached
  scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
  QCoreApplication::processEvents();
  auto *last = child<QWidget>(dialog, "sbDisplayScale");
  const auto spot = last->mapTo(scroll->viewport(), QPoint(0, last->height() - 1));
  QVERIFY(scroll->viewport()->rect().contains(spot));
}

QTEST_MAIN(ComputerSettingsCardsTests)
