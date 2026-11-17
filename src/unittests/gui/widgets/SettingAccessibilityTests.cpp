/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingAccessibilityTests.h"

#include "gui/style/Appearance.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/StatusHero.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QAccessible>
#include <QApplication>
#include <QFile>
#include <QMetaEnum>
#include <QPixmapCache>
#include <QRadioButton>
#include <QSpinBox>

using namespace deskflow::gui::style;

namespace {

void install()
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  Look look;
  look.reduceTransparency = true;
  look.reduceMotion = true;
  style->useLook(look);
}

QAccessibleInterface *interfaceOf(QObject *object)
{
  auto *interface = QAccessible::queryAccessibleInterface(object);
  Q_ASSERT(interface);
  return interface;
}

QString roleName(QAccessible::Role role)
{
  return QString::fromLatin1(QMetaEnum::fromType<QAccessible::Role>().valueToKey(role));
}

QString stateFlags(const QAccessible::State &state)
{
  QStringList flags;
  if (state.checkable)
    flags << (state.checked ? QStringLiteral("checked") : QStringLiteral("unchecked"));
  if (state.disabled)
    flags << QStringLiteral("disabled");
  if (state.focusable)
    flags << QStringLiteral("focusable");
  return flags.join(QLatin1Char(','));
}

// One line for each thing that a screen reader walks to: its role, its name, its description and its state
void dumpTree(QAccessibleInterface *interface, int depth, QStringList &lines)
{
  auto line =
      QStringLiteral("%1%2 \"%3\"")
          .arg(QString(depth * 2, QLatin1Char(' ')), roleName(interface->role()), interface->text(QAccessible::Name));
  if (const auto description = interface->text(QAccessible::Description); !description.isEmpty())
    line += QStringLiteral(" (%1)").arg(description);
  if (const auto flags = stateFlags(interface->state()); !flags.isEmpty())
    line += QStringLiteral(" [%1]").arg(flags);
  lines << line;
  for (int i = 0; i < interface->childCount(); ++i) {
    if (auto *child = interface->child(i))
      dumpTree(child, depth + 1, lines);
  }
}

} // namespace

void SettingAccessibilityTests::initTestCase()
{
  QAccessible::setActive(true);
}

void SettingAccessibilityTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QPixmapCache::clear();
}

void SettingAccessibilityTests::toggleSwitch_isASwitchWithItsStateAndItsActions()
{
  install();
  ToggleSwitch toggle;
  toggle.setAccessibleName(QStringLiteral("Heartbeat"));
  toggle.setAccessibleDescription(QStringLiteral("Notices a computer that does not answer"));
  auto *interface = interfaceOf(&toggle);

  QCOMPARE(interface->role(), QAccessible::Switch);
  QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Heartbeat"));
  QCOMPARE(interface->text(QAccessible::Description), QStringLiteral("Notices a computer that does not answer"));
  QVERIFY(interface->state().checkable);
  QVERIFY(!interface->state().checked);

  auto *action = interface->actionInterface();
  QVERIFY(action);
  const auto names = action->actionNames();
  QVERIFY(names.contains(QAccessibleActionInterface::toggleAction()));
  QVERIFY(names.contains(QAccessibleActionInterface::pressAction()));

  action->doAction(QAccessibleActionInterface::toggleAction());
  QVERIFY(toggle.isChecked());
  QVERIFY(interface->state().checked);
  action->doAction(QAccessibleActionInterface::pressAction());
  QVERIFY(!toggle.isChecked());
  QVERIFY(!interface->state().checked);
}

void SettingAccessibilityTests::toggleSwitch_withATextIsNamedByIt()
{
  install();
  ToggleSwitch toggle(QStringLiteral("&Share the clipboard"));
  QCOMPARE(interfaceOf(&toggle)->text(QAccessible::Name), QStringLiteral("Share the clipboard"));
}

void SettingAccessibilityTests::toggleSwitch_whenDisabled_saysSoAndIgnoresTheAction()
{
  install();
  ToggleSwitch toggle;
  toggle.setEnabled(false);
  auto *interface = interfaceOf(&toggle);
  QVERIFY(interface->state().disabled);
  interface->actionInterface()->doAction(QAccessibleActionInterface::toggleAction());
  QVERIFY(!toggle.isChecked());
}

void SettingAccessibilityTests::toggleSwitch_withoutTheStyleLayer_isACheckBoxForAScreenReader()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QVERIFY(!isDeskflowStyleInstalled());
  ToggleSwitch toggle(QStringLiteral("Share"));
  QCOMPARE(interfaceOf(&toggle)->role(), QAccessible::CheckBox);
  QCheckBox reference(QStringLiteral("Share"));
  QCOMPARE(interfaceOf(&reference)->role(), QAccessible::CheckBox);
  QCOMPARE(interfaceOf(&toggle)->text(QAccessible::Name), interfaceOf(&reference)->text(QAccessible::Name));
}

void SettingAccessibilityTests::row_isAGroupNamedByItsTitle_andNamesItsControls()
{
  install();
  SettingRow row(QStringLiteral("&Switch after"), QStringLiteral("Waits before it switches"));
  auto *spin = new QSpinBox;
  auto *toggle = new ToggleSwitch;
  row.addControl(spin);
  row.addControl(toggle);

  auto *interface = interfaceOf(&row);
  QCOMPARE(interface->role(), QAccessible::Grouping);
  QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Switch after"));

  // the controls have no text of their own: the row names them, and describes the first
  QCOMPARE(interfaceOf(spin)->text(QAccessible::Name), QStringLiteral("Switch after"));
  QCOMPARE(interfaceOf(toggle)->text(QAccessible::Name), QStringLiteral("Switch after"));
  QCOMPARE(interfaceOf(toggle)->role(), QAccessible::Switch);
  QCOMPARE(interfaceOf(spin)->text(QAccessible::Description), QStringLiteral("Waits before it switches"));

  // a title that is set later is the name that is told
  row.setTitle(QStringLiteral("Delay"));
  QCOMPARE(interfaceOf(spin)->text(QAccessible::Name), QStringLiteral("Delay"));
  QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Delay"));
}

void SettingAccessibilityTests::row_leavesTheNameOfAControlThatHasOne()
{
  install();
  SettingRow row(QStringLiteral("Protocol"));
  auto *synergy = new QRadioButton(QStringLiteral("Synergy"));
  auto *named = new QSpinBox;
  named->setAccessibleName(QStringLiteral("Port"));
  row.addControl(synergy);
  row.addControl(named);

  // a radio is named by its text and a control that a form named keeps its name
  QCOMPARE(interfaceOf(synergy)->text(QAccessible::Name), QStringLiteral("Synergy"));
  QCOMPARE(interfaceOf(named)->text(QAccessible::Name), QStringLiteral("Port"));
}

void SettingAccessibilityTests::card_isAGroupNamedByItsHeader_andTheHeaderIsAHeading()
{
  install();
  SettingCard card(QStringLiteral("&Network"));
  auto *interface = interfaceOf(&card);
  QCOMPARE(interface->role(), QAccessible::Grouping);
  QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Network"));

  auto *header = interfaceOf(card.header());
  QCOMPARE(header->role(), QAccessible::Heading);
  QCOMPARE(header->text(QAccessible::Name), QStringLiteral("Network"));
}

void SettingAccessibilityTests::chip_isTextThatNamesItsKind()
{
  install();
  StatusChip chip(QStringLiteral("Experimental"), StatusChip::Kind::Warning);
  auto *interface = interfaceOf(&chip);
  QCOMPARE(interface->role(), QAccessible::StaticText);
  QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Warning: Experimental"));
}

void SettingAccessibilityTests::hero_isAGroupNamedByItsHeadlineAndDescribedByItsDetail()
{
  install();
  StatusHero hero;
  hero.setHeadline(QStringLiteral("Connected to 2 computers"));
  hero.setDetail(QStringLiteral("Sharing the keyboard and the mouse"));
  auto *interface = interfaceOf(&hero);
  QCOMPARE(interface->role(), QAccessible::Grouping);
  QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Connected to 2 computers"));
  QCOMPARE(interface->text(QAccessible::Description), QStringLiteral("Sharing the keyboard and the mouse"));
}

void SettingAccessibilityTests::card_tree_hasTheRolesAndNamesOfWhatIsOnTheScreen()
{
  install();
  QWidget page;
  auto *card = new SettingCard(QStringLiteral("Experimental"), &page);
  card->header()->setTrailingWidget(new StatusChip(QStringLiteral("Beta"), StatusChip::Kind::Warning));
  auto *row = new SettingRow(
      QStringLiteral("Multiple monitors"), QStringLiteral("Lets each monitor of a computer be a screen")
  );
  row->addControl(new ToggleSwitch);
  card->addRow(row);
  auto *spinRow = new SettingRow(QStringLiteral("Heartbeat"));
  spinRow->addControl(new QSpinBox);
  spinRow->addControl(new ToggleSwitch);
  card->addRow(spinRow);
  page.show();

  QStringList lines;
  dumpTree(interfaceOf(card), 0, lines);
  const auto dump = lines.join(QLatin1Char('\n'));

  // what is walked to: the card, its heading with the chip, and for each row its group with the controls in it
  QVERIFY2(dump.contains(QStringLiteral("Grouping \"Experimental\"")), qPrintable(dump));
  QVERIFY2(dump.contains(QStringLiteral("Heading \"Experimental\"")), qPrintable(dump));
  QVERIFY2(dump.contains(QStringLiteral("StaticText \"Warning: Beta\"")), qPrintable(dump));
  QVERIFY2(dump.contains(QStringLiteral("Grouping \"Multiple monitors\"")), qPrintable(dump));
  QVERIFY2(
      dump.contains(QStringLiteral(
          "Switch \"Multiple monitors\" (Lets each monitor of a computer be a screen) [unchecked,focusable]"
      )),
      qPrintable(dump)
  );
  QVERIFY2(dump.contains(QStringLiteral("Switch \"Heartbeat\"")), qPrintable(dump));

  // the walk is written for whoever wants to read it (the report of the step), when the environment says where
  if (const auto path = qEnvironmentVariable("DESKFLOW_A11Y_DUMP"); !path.isEmpty()) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
      file.write(dump.toUtf8() + '\n');
  }
}

QTEST_MAIN(SettingAccessibilityTests)
