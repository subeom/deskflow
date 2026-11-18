/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingWidgetsTests.h"

#include "gui/style/Appearance.h"
#include "gui/style/Contrast.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/style/Icons.h"
#include "gui/widgets/InlineHint.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/StatusHero.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPixmapCache>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest/qtestaccessible.h>

#include <algorithm>

using namespace deskflow::gui::style;

namespace {

Look lookOf(bool dark = false)
{
  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  // opaque panels and no motion: the pixels of a picture and the state after a change do not depend on a blend or on
  // time
  look.reduceTransparency = true;
  look.reduceMotion = true;
  return look;
}

DeskflowStyle *install(bool dark = false)
{
  auto *style = new DeskflowStyle;
  QApplication::setStyle(style);
  style->useLook(lookOf(dark));
  return style;
}

bool near(const QColor &a, const QColor &b, int tolerance = 3)
{
  return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance &&
         qAbs(a.blue() - b.blue()) <= tolerance;
}

QLabel *titleOf(SettingRow &row)
{
  for (auto *label : row.findChildren<QLabel *>()) {
    if (!qobject_cast<InlineHint *>(label))
      return label;
  }
  return nullptr;
}

InlineHint *hintOf(SettingRow &row)
{
  return row.findChild<InlineHint *>();
}

void showWithWidth(QWidget &widget, int width)
{
  widget.setFixedWidth(width);
  widget.adjustSize();
  widget.show();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
// whether an announcement has been told to the accessibility update handler of QTestAccessibility since it was cleared
// (QTestAccessibility::containsEventOfType is newer than Qt 6.9)
bool announced()
{
  const auto events = QTestAccessibility::events();
  return std::any_of(events.cbegin(), events.cend(), [](const QAccessibleEvent *event) {
    return event->type() == QAccessible::Announcement;
  });
}
#endif

} // namespace

void SettingWidgetsTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QPixmapCache::clear();
}

void SettingWidgetsTests::inlineHint_isSmallerSecondaryTextThatWrapsAndFollowsTheLook()
{
  auto *style = install();
  InlineHint hint(
      QStringLiteral("Stops sharing when a computer does not answer for a while, so that a lost link is noticed")
  );
  QVERIFY(hint.wordWrap());
  QVERIFY(hint.font().pointSizeF() < QApplication::font().pointSizeF());
  QCOMPARE(hint.palette().color(QPalette::WindowText), currentTokens().colors.textSecondary);
  QVERIFY(hint.heightForWidth(160) > hint.heightForWidth(600));

  style->useLook(lookOf(true));
  QCOMPARE(hint.palette().color(QPalette::WindowText), currentTokens().colors.textSecondary);
  QVERIFY(hint.palette().color(QPalette::WindowText).lightness() > 128);
}

void SettingWidgetsTests::statusChip_hasAnIconAndAWordForEveryKindButNeutral()
{
  using Kind = StatusChip::Kind;
  QVERIFY(StatusChip::iconName(Kind::Neutral).isEmpty());
  QVERIFY(StatusChip::kindWord(Kind::Neutral).isEmpty());
  for (auto kind : {Kind::Info, Kind::Success, Kind::Warning, Kind::Danger}) {
    QVERIFY(hasIcon(StatusChip::iconName(kind), IconWeight::Fill));
    QVERIFY(!StatusChip::kindWord(kind).isEmpty());
  }
  // the kind is told in words as well as in colour
  StatusChip chip(QStringLiteral("Experimental"), Kind::Warning);
  QCOMPARE(chip.accessibleName(), QStringLiteral("Warning: Experimental"));
  StatusChip neutral(QStringLiteral("Beta"));
  QCOMPARE(neutral.accessibleName(), QStringLiteral("Beta"));
  chip.setKind(Kind::Danger);
  QCOMPARE(chip.accessibleName(), QStringLiteral("Error: Experimental"));
}

void SettingWidgetsTests::statusChip_textKeepsItsContrastOnItsFill_data()
{
  QTest::addColumn<int>("kind");
  QTest::addColumn<bool>("dark");
  QTest::addColumn<bool>("osColors");
  for (int kind = 0; kind <= static_cast<int>(StatusChip::Kind::Danger); ++kind) {
    for (bool dark : {false, true}) {
      for (bool osColors : {false, true}) {
        QTest::addRow("kind%d-%s-%s", kind, dark ? "dark" : "light", osColors ? "oscolors" : "tokens")
            << kind << dark << osColors;
      }
    }
  }
}

void SettingWidgetsTests::statusChip_textKeepsItsContrastOnItsFill()
{
  QFETCH(int, kind);
  QFETCH(bool, dark);
  QFETCH(bool, osColors);
  Look look;
  look.scheme = dark ? Scheme::Dark : Scheme::Light;
  look.osColors = osColors;
  const auto colors = StatusChip::paletteFor(static_cast<StatusChip::Kind>(kind), tokensFor(look, QPalette()));
  QCOMPARE(colors.fill.alpha(), 255);
  QVERIFY2(
      contrastRatio(colors.text, colors.fill) >= 4.5,
      qPrintable(QString::number(contrastRatio(colors.text, colors.fill)))
  );
}

void SettingWidgetsTests::statusChip_isAsWideAsItsIconAndText()
{
  install();
  StatusChip neutral(QStringLiteral("Beta"));
  StatusChip warning(QStringLiteral("Beta"), StatusChip::Kind::Warning);
  StatusChip longer(QStringLiteral("Beta and more"), StatusChip::Kind::Warning);
  QCOMPARE(neutral.sizeHint().height(), 24);
  // the icon and the gap after it
  QCOMPARE(warning.sizeHint().width() - neutral.sizeHint().width(), 14 + 5);
  QVERIFY(longer.sizeHint().width() > warning.sizeHint().width());
  QCOMPARE(warning.minimumSizeHint(), warning.sizeHint());
}

void SettingWidgetsTests::statusChip_isPaintedWithTheFillOfItsKind()
{
  install();
  for (auto kind :
       {StatusChip::Kind::Info, StatusChip::Kind::Success, StatusChip::Kind::Warning, StatusChip::Kind::Danger}) {
    StatusChip chip(QStringLiteral("Text"), kind);
    chip.resize(chip.sizeHint());
    const auto image = chip.grab().toImage();
    const auto fill = StatusChip::paletteFor(kind, currentTokens()).fill;
    // inside the padding, left of the icon, in the middle of the height
    QVERIFY2(near(image.pixelColor(5, 12), fill), qPrintable(image.pixelColor(5, 12).name()));
    // the corners are round: the corner pixel is not the fill
    QVERIFY(!near(image.pixelColor(0, 0), fill, 2) || image.pixelColor(0, 0).alpha() < 255);
  }
}

void SettingWidgetsTests::sectionHeader_showsATitleAndOwnsTheWidgetAtItsEnd()
{
  install();
  SectionHeader header(QStringLiteral("Network"));
  QCOMPARE(header.title(), QStringLiteral("Network"));
  QCOMPARE(header.accessibleName(), QStringLiteral("Network"));
  QVERIFY(header.trailingWidget() == nullptr);

  QPointer<StatusChip> first = new StatusChip(QStringLiteral("One"));
  header.setTrailingWidget(first);
  QCOMPARE(header.trailingWidget(), first.data());
  QCOMPARE(first->parentWidget(), &header);

  QPointer<StatusChip> second = new StatusChip(QStringLiteral("Two"));
  header.setTrailingWidget(second);
  // the header owns what is at its end: the one that was there is gone
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(first.isNull());
  QCOMPARE(header.trailingWidget(), second.data());

  header.setTitle(QStringLiteral("Heartbeat"));
  QCOMPARE(header.accessibleName(), QStringLiteral("Heartbeat"));
}

void SettingWidgetsTests::row_takesTheTextOfASwitchAsItsTitle()
{
  install();
  SettingRow row;
  auto *toggle = new ToggleSwitch(QStringLiteral("Disable lock to computer"));
  row.addControl(toggle);
  QCOMPARE(row.title(), QStringLiteral("Disable lock to computer"));
  QVERIFY(toggle->text().isEmpty());
  QCOMPARE(toggle->parentWidget(), &row);
  QCOMPARE(toggle->sizeHint(), QSize(52, 34));
  QCOMPARE(titleOf(row)->text(), QStringLiteral("Disable lock to computer"));

  // a row that has a title of its own leaves the text of the switch
  SettingRow titled(QStringLiteral("Heartbeat"));
  auto *second = new ToggleSwitch(QStringLiteral("Enabled"));
  titled.addControl(second);
  QCOMPARE(titled.title(), QStringLiteral("Heartbeat"));
  QCOMPARE(second->text(), QStringLiteral("Enabled"));
}

void SettingWidgetsTests::row_showsTheMnemonicOfItsTitleAndReadsItWithoutIt()
{
  install();
  SettingRow row(QStringLiteral("&Misc"));
  QCOMPARE(row.rawTitle(), QStringLiteral("&Misc"));
  QCOMPARE(row.title(), QStringLiteral("Misc"));
  QCOMPARE(row.accessibleName(), QStringLiteral("Misc"));
  QCOMPARE(titleOf(row)->text(), QStringLiteral("&Misc"));

  row.setTitle(QStringLiteral("Cut && paste"));
  QCOMPARE(row.title(), QStringLiteral("Cut & paste"));

  // the key of a translation into Korean is in parentheses after the text, and is not read out
  row.setTitle(QStringLiteral("상대 좌표 마우스 이동 사용(&R)"));
  QCOMPARE(row.title(), QStringLiteral("상대 좌표 마우스 이동 사용"));
  QCOMPARE(row.rawTitle(), QStringLiteral("상대 좌표 마우스 이동 사용(&R)"));
  SectionHeader header(QStringLiteral("기타(&M)"));
  QCOMPARE(header.title(), QStringLiteral("기타"));
}

void SettingWidgetsTests::row_leavesItsControlsAsTheyWere()
{
  install();
  QWidget page;
  auto *spin = new QSpinBox(&page);
  spin->setSuffix(QStringLiteral(" MB"));
  spin->setValue(5);
  spin->setToolTip(QStringLiteral("The most that is shared"));
  QSignalSpy changed(spin, &QSpinBox::valueChanged);

  SettingRow row(QStringLiteral("Limit"), {}, &page);
  row.addControl(spin);
  QCOMPARE(spin->parentWidget(), &row);
  QCOMPARE(row.controls(), QList<QWidget *>{spin});
  QCOMPARE(row.primaryControl(), spin);
  QCOMPARE(row.toolTip(), QStringLiteral("The most that is shared"));

  // what reads and sets the control does as it did
  spin->setValue(7);
  QCOMPARE(changed.size(), 1);
  QCOMPARE(spin->value(), 7);
  QCOMPARE(spin->suffix(), QStringLiteral(" MB"));
}

void SettingWidgetsTests::row_isShownWhileOneOfItsControlsIsShown()
{
  install();
  QWidget page;
  auto *row = new SettingRow(QStringLiteral("Title"), {}, &page);
  auto *first = new QCheckBox;
  auto *second = new QCheckBox;
  row->addControl(first);
  row->addControl(second);
  page.show();
  QVERIFY(row->isVisible());

  first->hide();
  QVERIFY(row->isVisible());
  second->hide();
  QVERIFY(!row->isVisible());
  QVERIFY(row->isHidden());
  first->show();
  QVERIFY(row->isVisible());

  // a control that a form hid before it came here (a setting that this OS has not)
  auto *hiddenRow = new SettingRow(QStringLiteral("Only on Windows"), {}, &page);
  auto *only = new QCheckBox;
  only->setVisible(false);
  hiddenRow->addControl(only);
  QVERIFY(hiddenRow->isHidden());
  only->setVisible(true);
  QVERIFY(!hiddenRow->isHidden());
}

void SettingWidgetsTests::row_dimsItsTextWhenNoneOfItsControlsIsEnabled()
{
  install();
  SettingRow row(QStringLiteral("Switch after"), QStringLiteral("Waits before it switches"));
  auto *spin = new QSpinBox;
  auto *toggle = new ToggleSwitch;
  row.addControl(spin);
  row.addControl(toggle);
  QVERIFY(titleOf(row)->isEnabled());
  QVERIFY(hintOf(row)->isEnabled());

  spin->setEnabled(false);
  // one that is enabled keeps the text enabled
  QVERIFY(titleOf(row)->isEnabled());
  toggle->setEnabled(false);
  QVERIFY(!titleOf(row)->isEnabled());
  QVERIFY(!hintOf(row)->isEnabled());

  toggle->setEnabled(true);
  QVERIFY(titleOf(row)->isEnabled());
  QVERIFY(hintOf(row)->isEnabled());
}

void SettingWidgetsTests::row_followsTheRowThatIsDisabled()
{
  install();
  SettingRow row(QStringLiteral("Heartbeat"));
  auto *toggle = new ToggleSwitch;
  auto *spin = new QSpinBox;
  row.addControl(spin);
  row.addControl(toggle);

  row.setEnabled(false);
  QVERIFY(!toggle->isEnabled());
  QVERIFY(!titleOf(row)->isEnabled());

  row.setEnabled(true);
  QVERIFY(toggle->isEnabled());
  QVERIFY(titleOf(row)->isEnabled());

  // a control that was disabled before stays so when the row is enabled again, and so does the dimmed text with the
  // last
  spin->setEnabled(false);
  toggle->setEnabled(false);
  row.setEnabled(false);
  row.setEnabled(true);
  QVERIFY(!titleOf(row)->isEnabled());
}

void SettingWidgetsTests::row_aClickOnItsTextTogglesItsSwitch()
{
  install();
  SettingRow row(QStringLiteral("Heartbeat"), QStringLiteral("Notices a computer that does not answer"));
  auto *toggle = new ToggleSwitch;
  row.addControl(toggle);
  showWithWidth(row, 520);
  QSignalSpy toggled(toggle, &QAbstractButton::toggled);

  QTest::mouseClick(titleOf(row), Qt::LeftButton);
  QVERIFY(toggle->isChecked());
  QTest::mouseClick(hintOf(row), Qt::LeftButton);
  QVERIFY(!toggle->isChecked());
  QCOMPARE(toggled.size(), 2);
  QVERIFY(toggle->hasFocus() || !row.isActiveWindow());

  // the padding outside the text does nothing; a click on the switch itself toggles it once
  QTest::mouseClick(&row, Qt::LeftButton, Qt::NoModifier, QPoint(3, 3));
  QCOMPARE(toggled.size(), 2);
  QTest::mouseClick(toggle, Qt::LeftButton);
  QCOMPARE(toggled.size(), 3);

  toggle->setEnabled(false);
  QTest::mouseClick(titleOf(row), Qt::LeftButton);
  QCOMPARE(toggled.size(), 3);
}

void SettingWidgetsTests::row_aClickOnItsTextDoesNothingForOtherControls()
{
  install();
  SettingRow row(QStringLiteral("Protocol"));
  auto *synergy = new QRadioButton(QStringLiteral("Synergy"));
  auto *barrier = new QRadioButton(QStringLiteral("Barrier"));
  barrier->setChecked(true);
  row.addControl(synergy);
  row.addControl(barrier);
  showWithWidth(row, 520);

  QTest::mouseClick(titleOf(row), Qt::LeftButton);
  QVERIFY(!synergy->isChecked());
  QVERIFY(barrier->isChecked());

  SettingRow spinRow(QStringLiteral("Limit"));
  auto *spin = new QSpinBox;
  spin->setValue(3);
  spinRow.addControl(spin);
  showWithWidth(spinRow, 520);
  QTest::mouseClick(titleOf(spinRow), Qt::LeftButton);
  QCOMPARE(spin->value(), 3);
}

void SettingWidgetsTests::row_isAtLeast44PixelsHigh_andGrowsForAHintThatWraps()
{
  install();
  SettingRow bare(QStringLiteral("Title"));
  bare.addControl(new ToggleSwitch);
  QVERIFY(bare.sizeHint().height() >= SettingRow::minimumRowHeight());
  QCOMPARE(SettingRow::minimumRowHeight(), 44);

  SettingRow row(
      QStringLiteral("Title"),
      QStringLiteral("A long guidance that does not fit on one line when the dialog is narrow, and that wraps")
  );
  row.addControl(new ToggleSwitch);
  QVERIFY(row.hasHeightForWidth());
  QVERIFY(row.heightForWidth(260) > row.heightForWidth(700));
  QVERIFY(row.heightForWidth(700) > bare.sizeHint().height() - 1);
}

void SettingWidgetsTests::row_withExpandingControls_givesThemTheRoom()
{
  install();
  auto widthOfEdit = [](bool expanding) {
    SettingRow row(QStringLiteral("Config file path"));
    auto *edit = new QLineEdit;
    row.addControl(edit);
    row.setExpandingControls(expanding);
    showWithWidth(row, 640);
    return edit->width();
  };
  QVERIFY(widthOfEdit(true) > widthOfEdit(false) + 100);
}

void SettingWidgetsTests::row_mnemonicOfTheTitleFocusesTheControl()
{
  install();
  SettingRow row(QStringLiteral("&Limit to"));
  auto *spin = new QSpinBox;
  row.addControl(spin);
  QCOMPARE(titleOf(row)->buddy(), spin);

  // a control that takes no focus has nothing to be the buddy of
  SettingRow other(QStringLiteral("&Note"));
  auto *label = new QLabel(QStringLiteral("text"));
  other.addControl(label);
  QVERIFY(titleOf(other)->buddy() == nullptr);
}

// Qt before 6.11 finds the label of a control, the relation that a screen reader asks for, among the siblings of the
// control only: the title has to be in the widget that has the controls
void SettingWidgetsTests::row_titleIsASiblingOfItsControls()
{
  install();
  SettingRow row(QStringLiteral("Shift"), QStringLiteral("What the key does"));
  auto *combo = new QComboBox;
  row.addControl(combo);
  auto *title = titleOf(row);
  QCOMPARE(title->buddy(), combo);
  QCOMPARE(title->parentWidget(), combo->parentWidget());
  QCOMPARE(title->parentWidget(), &row);
  QCOMPARE(row.findChild<InlineHint *>()->parentWidget(), &row);
}

void SettingWidgetsTests::card_hasNoHairlineAboveItsFirstRowThatIsShown()
{
  install();
  QWidget page;
  auto *card = new SettingCard(QStringLiteral("Misc"), &page);
  QList<SettingRow *> rows;
  QList<QCheckBox *> boxes;
  for (const auto *title : {"One", "Two", "Three"}) {
    auto *row = new SettingRow(QString::fromLatin1(title));
    auto *box = new QCheckBox;
    row->addControl(box);
    card->addRow(row);
    rows << row;
    boxes << box;
  }
  page.show();
  QVERIFY(!rows[0]->hasDivider());
  QVERIFY(rows[1]->hasDivider());
  QVERIFY(rows[2]->hasDivider());

  boxes[0]->hide();
  QVERIFY(rows[0]->isHidden());
  QVERIFY(!rows[1]->hasDivider());
  QVERIFY(rows[2]->hasDivider());

  boxes[0]->show();
  QVERIFY(!rows[0]->hasDivider());
  QVERIFY(rows[1]->hasDivider());
  QCOMPARE(card->rows(), rows);
}

void SettingWidgetsTests::card_isHiddenWhenNoneOfItsRowsIsShown()
{
  install();
  QWidget page;
  auto *card = new SettingCard(QStringLiteral("Windows"), &page);
  auto *row = new SettingRow(QStringLiteral("Keep in the foreground"));
  auto *box = new QCheckBox;
  box->setVisible(false);
  row->addControl(box);
  card->addRow(row);
  page.show();
  QVERIFY(card->isHidden());

  box->setVisible(true);
  QVERIFY(!card->isHidden());
  QVERIFY(card->isVisible());
  box->setVisible(false);
  QVERIFY(card->isHidden());
}

void SettingWidgetsTests::card_hasAHeaderWithAWidgetAtItsEnd()
{
  install();
  SettingCard card(QStringLiteral("Experimental"));
  QVERIFY(card.header());
  QCOMPARE(card.title(), QStringLiteral("Experimental"));
  card.header()->setTrailingWidget(new StatusChip(QStringLiteral("Beta"), StatusChip::Kind::Warning));
  QVERIFY(qobject_cast<StatusChip *>(card.header()->trailingWidget()));
  QCOMPARE(card.accessibleName(), QStringLiteral("Experimental"));

  SettingCard headless;
  QVERIFY(headless.header() == nullptr);
  QVERIFY(headless.title().isEmpty());
}

void SettingWidgetsTests::card_isTheDensePanelWithRowsToItsEdge()
{
  install();
  SettingCard card;
  QCOMPARE(card.density(), PanelWidget::Density::Dense);
  // the room of the shadow and no padding: the rows carry their own
  QCOMPARE(card.contentsMargins(), PanelWidget::shadowMargins());
}

void SettingWidgetsTests::card_paintsTheHairlineBetweenRowsInTheTokenColour()
{
  install();
  SettingCard card;
  QList<SettingRow *> rows;
  for (const auto *title : {"First", "Second"}) {
    auto *row = new SettingRow(QString::fromLatin1(title));
    row->addControl(new QCheckBox);
    card.addRow(row);
    rows << row;
  }
  showWithWidth(card, 520);
  const auto image = card.grab().toImage();
  const int y = rows[1]->y();
  const auto line = image.pixelColor(300, y);
  const auto above = image.pixelColor(300, y - 3);
  // the line is the text colour at a small part: darker than the surface on a light look, and only at the row's top
  // edge
  QVERIFY2(line.lightness() < above.lightness() - 5, qPrintable(line.name() + QLatin1Char(' ') + above.name()));
  QVERIFY(near(image.pixelColor(300, y + 3), above, 2));
  // the first row has no line above it, and the line keeps the padding at its ends
  QVERIFY(near(image.pixelColor(300, rows[0]->y()), image.pixelColor(300, rows[0]->y() + 3), 2));
  QVERIFY(near(image.pixelColor(rows[1]->x() + 5, y), above, 2));
}

void SettingWidgetsTests::hero_showsTheStateTheDetailAndTheChips()
{
  install();
  StatusHero hero;
  QCOMPARE(hero.kind(), StatusChip::Kind::Neutral);
  QCOMPARE(hero.iconName(), QStringLiteral("power"));

  hero.setKind(StatusChip::Kind::Success);
  QCOMPARE(hero.iconName(), QStringLiteral("check-circle"));
  hero.setIconName(QStringLiteral("wifi-high"));
  QCOMPARE(hero.iconName(), QStringLiteral("wifi-high"));
  hero.setIconName({});
  QCOMPARE(hero.iconName(), QStringLiteral("check-circle"));

  hero.setHeadline(QStringLiteral("Connected to 2 computers"));
  QCOMPARE(hero.headline(), QStringLiteral("Connected to 2 computers"));
  const auto labels = hero.findChildren<QLabel *>();
  QCOMPARE(labels.size(), 2);
  // no detail: no empty line
  QVERIFY(labels[1]->isHidden());
  hero.setDetail(QStringLiteral("Sharing the keyboard and the mouse"));
  QVERIFY(!labels[1]->isHidden());
  QCOMPARE(hero.detail(), QStringLiteral("Sharing the keyboard and the mouse"));

  hero.addChip(QStringLiteral("Secure"), StatusChip::Kind::Success);
  hero.addChip(QStringLiteral("Server"));
  QCOMPARE(hero.chips().size(), 2);
  QCOMPARE(hero.chips()[0]->text(), QStringLiteral("Secure"));
  hero.clearChips();
  QCOMPARE(hero.chips().size(), 0);
}

void SettingWidgetsTests::hero_badgeHasTheColourOfItsKind()
{
  install();
  for (auto kind : {StatusChip::Kind::Success, StatusChip::Kind::Warning, StatusChip::Kind::Danger}) {
    StatusHero hero;
    hero.setKind(kind);
    hero.setHeadline(QStringLiteral("State"));
    showWithWidth(hero, 480);

    QWidget *orb = nullptr;
    for (auto *child : hero.findChildren<QWidget *>()) {
      if (child->size() == QSize(64, 64))
        orb = child;
    }
    QVERIFY(orb);
    const auto at = orb->mapTo(&hero, QPoint(5, 32));
    const auto image = hero.grab().toImage();
    const auto fill = StatusChip::paletteFor(kind, currentTokens()).fill;
    QVERIFY2(near(image.pixelColor(at), fill), qPrintable(image.pixelColor(at).name()));
  }
}

void SettingWidgetsTests::hero_announcesAHeadlineThatChanges()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  install();
  // The hero announces while assistive technology listens, which is what QAccessible::isActive says. Before Qt 6.10
  // QAccessible::setActive only tells the observers and never makes it true: a platform plugin does that when a screen
  // reader asks, and the offscreen platform has none that asks. Nothing is announced there, so there is nothing to see.
  QAccessible::setActive(true);
  const auto deactivate = qScopeGuard([] { QAccessible::setActive(false); });
  if (!QAccessible::isActive())
    QSKIP("QAccessible::setActive does not turn accessibility on before Qt 6.10 (hero_staysSilentWhileNothingListens)");
  QTestAccessibility::initialize();
  const auto cleanup = qScopeGuard([] { QTestAccessibility::cleanup(); });
  StatusHero hero;
  hero.setHeadline(QStringLiteral("Waiting for clients"));
  hero.show();
  QTestAccessibility::clearEvents();

  // the first headline is not announced, a change is
  hero.setHeadline(QStringLiteral("Connected to 1 computer"));
  QVERIFY(announced());
  QTestAccessibility::clearEvents();
  hero.setHeadline(QStringLiteral("Connected to 1 computer"));
  QVERIFY(!announced());
#else
  QSKIP("Announcements need Qt 6.8");
#endif
}

void SettingWidgetsTests::hero_staysSilentWhileNothingListens()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  install();
  QAccessible::setActive(false);
  QVERIFY(!QAccessible::isActive());
  QTestAccessibility::initialize();
  const auto cleanup = qScopeGuard([] { QTestAccessibility::cleanup(); });
  StatusHero hero;
  hero.setHeadline(QStringLiteral("Waiting for clients"));
  hero.show();
  QTestAccessibility::clearEvents();

  // the headline still changes, and the name that a screen reader reads when it comes is kept up to date
  hero.setHeadline(QStringLiteral("Connected to 1 computer"));
  QVERIFY(!announced());
  QCOMPARE(hero.accessibleName(), QStringLiteral("Connected to 1 computer"));
#else
  QSKIP("Announcements need Qt 6.8");
#endif
}

void SettingWidgetsTests::hero_putsItsActionsAtTheEndAndUnderTheTextWhenItIsNarrow()
{
  install();
  StatusHero hero;
  hero.setHeadline(QStringLiteral("Not sharing"));
  hero.setDetail(QStringLiteral("Deskflow is not running"));
  auto *start = new QPushButton(QStringLiteral("Start"));
  auto *restart = new QPushButton(QStringLiteral("Restart"));
  hero.setActions({start, restart});
  const auto detail = [&] {
    for (auto *label : hero.findChildren<QLabel *>()) {
      if (label->text() == hero.detail())
        return label;
    }
    return static_cast<QLabel *>(nullptr);
  }();
  QVERIFY(detail);
  const auto top = [&](const QWidget *w) { return w->mapTo(&hero, QPoint(0, 0)); };

  showWithWidth(hero, 800);
  QVERIFY(!hero.actionsAreBelow());
  // at the end of the line of the text, and the buttons are next to each other
  QVERIFY(top(start).x() > hero.width() / 2);
  QVERIFY(top(restart).x() > top(start).x());
  QVERIFY(top(start).y() < top(detail).y() + detail->height());

  hero.setFixedWidth(StatusHero::kWrapWidth - 40);
  QCoreApplication::processEvents();
  QVERIFY(hero.actionsAreBelow());
  QVERIFY(top(start).y() >= top(detail).y() + detail->height());
  QVERIFY(top(start).x() < hero.width() / 2);

  hero.setFixedWidth(800);
  QCoreApplication::processEvents();
  QVERIFY(!hero.actionsAreBelow());
}

void SettingWidgetsTests::hero_holdsAnAccessoryUnderTheChips_evenWhenChipsComeAfterIt()
{
  install();
  StatusHero hero;
  hero.setHeadline(QStringLiteral("Waiting for clients"));
  auto *address = new QLabel(QStringLiteral("192.168.0.12"));
  hero.addAccessory(address);
  hero.addChip(QStringLiteral("Encrypted"), StatusChip::Kind::Success);
  showWithWidth(hero, 700);

  QCOMPARE(hero.chips().size(), 1);
  const auto y = [&](const QWidget *w) { return w->mapTo(&hero, QPoint(0, 0)).y(); };
  QVERIFY(y(hero.chips().first()) < y(address));
  // the chips are made again with each state: the accessory stays where it is
  hero.clearChips();
  hero.addChip(QStringLiteral("Not encrypted"), StatusChip::Kind::Warning);
  hero.addChip(QStringLiteral("Beta"));
  QCoreApplication::processEvents();
  QVERIFY(y(hero.chips().first()) < y(address));
  QVERIFY(y(hero.chips().last()) < y(address));
  QCOMPARE(address->parentWidget() != nullptr, true);
  QVERIFY(address->isVisibleTo(&hero));
}

void SettingWidgetsTests::card_clearRowsDeletesTheRowsAndLeavesTheCardUsable()
{
  install();
  QWidget page;
  auto *card = new SettingCard(QStringLiteral("Other computers"), &page);
  QPointer<SettingRow> first = new SettingRow(QStringLiteral("PC"));
  card->addRow(first);
  card->addRow(new SettingRow(QStringLiteral("MINI")));
  page.show();
  QCOMPARE(card->rows().size(), 2);

  card->clearRows();
  QVERIFY(card->rows().isEmpty());
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(first.isNull());
  // a card that has no rows is not one that its rows hid
  QVERIFY(card->isVisible());

  auto *again = new SettingRow(QStringLiteral("PC"));
  card->addRow(again);
  QCOMPARE(card->rows(), QList<SettingRow *>{again});
}

void SettingWidgetsTests::card_hiddenByItsRowsIsShownAgainWhenTheRowsAreCleared()
{
  install();
  QWidget page;
  auto *card = new SettingCard(QStringLiteral("Windows"), &page);
  auto *row = new SettingRow(QStringLiteral("Keep in the foreground"));
  auto *box = new QCheckBox;
  box->setVisible(false);
  row->addControl(box);
  card->addRow(row);
  page.show();
  QVERIFY(card->isHidden());

  card->clearRows();
  QVERIFY(!card->isHidden());
}

void SettingWidgetsTests::widgets_followTheLookWhenItChanges()
{
  auto *style = install(false);
  SettingRow row(QStringLiteral("Heartbeat"), QStringLiteral("Hint"));
  row.addControl(new ToggleSwitch);
  StatusHero hero;
  hero.setHeadline(QStringLiteral("State"));
  hero.setDetail(QStringLiteral("Detail"));
  SectionHeader header(QStringLiteral("Header"));
  const auto light = currentTokens().colors;
  QCOMPARE(titleOf(row)->palette().color(QPalette::WindowText), light.text);
  QCOMPARE(hintOf(row)->palette().color(QPalette::WindowText), light.textSecondary);

  style->useLook(lookOf(true));
  const auto dark = currentTokens().colors;
  QVERIFY(dark.text != light.text);
  QCOMPARE(titleOf(row)->palette().color(QPalette::WindowText), dark.text);
  QCOMPARE(hintOf(row)->palette().color(QPalette::WindowText), dark.textSecondary);
  const auto labels = hero.findChildren<QLabel *>();
  QCOMPARE(labels[0]->palette().color(QPalette::WindowText), dark.text);
  QCOMPARE(labels[1]->palette().color(QPalette::WindowText), dark.textSecondary);
  QCOMPARE(header.findChild<QLabel *>()->palette().color(QPalette::WindowText), dark.text);
}

QTEST_MAIN(SettingWidgetsTests)
