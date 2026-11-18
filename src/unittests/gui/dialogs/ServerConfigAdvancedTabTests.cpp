/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigAdvancedTabTests.h"

#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "gui/config/ServerConfig.h"
#include "gui/dialogs/ServerConfigDialog.h"
#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/BackdropWidget.h"
#include "gui/style/DeskflowStyle.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QAbstractSpinBox>
#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPixmapCache>
#include <QScrollArea>
#include <QSpinBox>
#include <QTest>

using namespace deskflow::gui::style;

namespace {

// the check boxes of the Advanced tab, in the order that they are on the screen
const QStringList kSwitches = {
    QStringLiteral("cbDisableLockToComputer"),
    QStringLiteral("cbDefaultLockToComputerState"),
    QStringLiteral("cbRelativeMouseMoves"),
    QStringLiteral("cbWin32KeepForeground"),
    QStringLiteral("cbEnableClipboard"),
    QStringLiteral("cbClipboardTextOnly"),
    QStringLiteral("cbSwitchDelay"),
    QStringLiteral("cbSwitchDoubleTap"),
    QStringLiteral("cbHeartbeat"),
    QStringLiteral("cbExperimentalMultiMonitor"),
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

template <class T> T *child(const QObject &parent, const QString &name)
{
  auto *found = parent.findChild<T *>(name);
  Q_ASSERT_X(found, "child", qPrintable(name));
  return found;
}

SettingRow *rowOf(QWidget *widget)
{
  for (auto *parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
    if (auto *row = qobject_cast<SettingRow *>(parent))
      return row;
  }
  return nullptr;
}

SettingCard *cardTitled(const QObject &dialog, const QString &title)
{
  for (auto *card : dialog.findChildren<SettingCard *>()) {
    if (card->title() == title)
      return card;
  }
  return nullptr;
}

} // namespace

void ServerConfigAdvancedTabTests::initTestCase()
{
  QVERIFY(m_settingsDir.isValid());
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("Deskflow.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("Deskflow.state")));
  QAccessible::setActive(true);
}

void ServerConfigAdvancedTabTests::cleanup()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QApplication::setPalette(QPalette());
  QPixmapCache::clear();
  for (const auto &key :
       {Settings::Server::EnableHeartbeat, Settings::Server::RelativeMouseMoves, Settings::Server::ExternalConfig,
        Settings::Server::ExternalConfigFile, Settings::Server::EnableClipboard})
    Settings::setValue(key);
}

void ServerConfigAdvancedTabTests::systemMode_keepsTheGroupBoxesAndTheCheckBoxesAsTheyWere()
{
  QApplication::setStyle(QStringLiteral("Fusion"));
  QVERIFY(!isDeskflowStyleInstalled());
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  QVERIFY(dialog.findChildren<SettingCard *>().isEmpty());
  QVERIFY(dialog.findChildren<SettingRow *>().isEmpty());
  QVERIFY(!child<QWidget>(dialog, QStringLiteral("tabAdvanced"))->findChild<QScrollArea *>());

  const auto *misc = child<QGroupBox>(dialog, QStringLiteral("groupMisc"));
  QVERIFY(!misc->isHidden());
  auto *box = child<QCheckBox>(dialog, QStringLiteral("cbDisableLockToComputer"));
  QCOMPARE(box->parentWidget(), misc);
  QCOMPARE(box->text(), QStringLiteral("Disable lock to computer (scroll lock key)"));
  QCOMPARE(QAccessible::queryAccessibleInterface(box)->role(), QAccessible::CheckBox);
}

void ServerConfigAdvancedTabTests::deskflowMode_hasACardForEachGroupOnAScrollingBackdrop()
{
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  const auto cards = dialog.findChildren<SettingCard *>();
  QStringList titles;
  for (const auto *card : cards)
    titles << card->title();
  QCOMPARE(cards.size(), 5);
  QVERIFY2(titles.contains(QStringLiteral("Misc")), qPrintable(titles.join(QLatin1Char('|'))));
  QVERIFY(titles.contains(QStringLiteral("Switching")));
  QVERIFY(titles.contains(QStringLiteral("Network")));
  QVERIFY(titles.contains(QStringLiteral("Experimental")));
  // the card of the config file has no header: its first row says what it is
  QVERIFY(titles.contains(QString()));

  const auto *tab = child<QWidget>(dialog, QStringLiteral("tabAdvanced"));
  const auto *scroll = tab->findChild<QScrollArea *>();
  QVERIFY(scroll);
  QVERIFY(!scroll->viewport()->autoFillBackground());
  QVERIFY(!scroll->widget()->autoFillBackground());
  QVERIFY(scroll->frameShape() == QFrame::NoFrame);
  bool onBackdrop = false;
  for (auto *parent = scroll->parentWidget(); parent && parent != &dialog; parent = parent->parentWidget())
    onBackdrop = onBackdrop || qobject_cast<BackdropWidget *>(parent);
  QVERIFY(onBackdrop);
}

void ServerConfigAdvancedTabTests::deskflowMode_movesEveryControlIntoARow_andHidesTheGroupBoxes()
{
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  for (const auto &name : kSwitches) {
    auto *toggle = child<ToggleSwitch>(dialog, name);
    QVERIFY2(rowOf(toggle), qPrintable(name));
    QVERIFY2(rowOf(toggle)->primaryControl() == toggle, qPrintable(name));
    // the text is the title of the row
    QVERIFY2(toggle->text().isEmpty(), qPrintable(name));
    QVERIFY2(!rowOf(toggle)->title().isEmpty(), qPrintable(name));
  }
  for (const auto *name :
       {"sbClipboardSizeLimit", "sbSwitchDelay", "sbSwitchDoubleTap", "sbHeartbeat", "rbProtocolSynergy",
        "rbProtocolBarrier", "widgetExternalConfigControls"}) {
    QVERIFY2(rowOf(child<QWidget>(dialog, QString::fromLatin1(name))), name);
  }
  for (const auto *name : {"groupMisc", "groupSwitch", "groupNetwork", "groupExperimental", "groupExternalConfig"})
    QVERIFY2(child<QGroupBox>(dialog, QString::fromLatin1(name))->isHidden(), name);
}

void ServerConfigAdvancedTabTests::deskflowMode_describesWhatHasAPlainTooltip_andKeepsTheHelpOfShiftF1()
{
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  // a plain tooltip is guidance in the row, where it is read without hovering
  auto *textOnly = child<ToggleSwitch>(dialog, QStringLiteral("cbClipboardTextOnly"));
  QVERIFY(rowOf(textOnly)->description().startsWith(QStringLiteral("Only plain text of up to 10 KB")));
  QVERIFY(textOnly->toolTip().isEmpty());
  QCOMPARE(textOnly->accessibleDescription(), rowOf(textOnly)->description());

  // the help that is not a tooltip (Shift+F1, in rich text) stays on the control, and on the text of the row
  auto *heartbeat = child<ToggleSwitch>(dialog, QStringLiteral("cbHeartbeat"));
  QVERIFY(rowOf(heartbeat)->description().isEmpty());
  QVERIFY(heartbeat->whatsThis().contains(QStringLiteral("<html>")));
  QCOMPARE(rowOf(heartbeat)->whatsThis(), heartbeat->whatsThis());
  auto *protocol = child<QWidget>(dialog, QStringLiteral("rbProtocolSynergy"));
  QVERIFY(rowOf(protocol)->whatsThis().contains(QStringLiteral("Synergy 3 uses the Synergy protocol")));
}

void ServerConfigAdvancedTabTests::deskflowMode_namesTheRowsByTheirControls()
{
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  auto *heartbeat = child<ToggleSwitch>(dialog, QStringLiteral("cbHeartbeat"));
  // the mnemonic is in the title that is shown and not in the name that is read
  QCOMPARE(rowOf(heartbeat)->rawTitle(), QStringLiteral("&Check clients every"));
  QCOMPARE(heartbeat->accessibleName(), QStringLiteral("Check clients every"));
  auto *spin = child<QSpinBox>(dialog, QStringLiteral("sbHeartbeat"));
  QCOMPARE(spin->accessibleName(), QStringLiteral("Check clients every"));
  QCOMPARE(QAccessible::queryAccessibleInterface(heartbeat)->role(), switchRole());

  // the radios keep their own names, and the row says what they choose between
  auto *protocol = child<QWidget>(dialog, QStringLiteral("rbProtocolSynergy"));
  QCOMPARE(rowOf(protocol)->title(), QStringLiteral("Network protocol"));
  QCOMPARE(QAccessible::queryAccessibleInterface(protocol)->text(QAccessible::Name), QStringLiteral("Synergy"));
}

void ServerConfigAdvancedTabTests::deskflowMode_clipboardLimitHasTheLabelAsItsTitle()
{
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  auto *limit = child<QSpinBox>(dialog, QStringLiteral("sbClipboardSizeLimit"));
  QCOMPARE(rowOf(limit)->title(), QStringLiteral("Limit to"));
  QVERIFY(!limit->suffix().isEmpty());
  QVERIFY(child<QLabel>(dialog, QStringLiteral("label_7"))->isHidden());
}

void ServerConfigAdvancedTabTests::deskflowMode_experimentalCardHasAChipAndTheTitleWithoutItsPrefix()
{
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  auto *card = cardTitled(dialog, QStringLiteral("Experimental"));
  QVERIFY(card);
  auto *chip = qobject_cast<StatusChip *>(card->header()->trailingWidget());
  QVERIFY(chip);
  QCOMPARE(chip->kind(), StatusChip::Kind::Warning);

  // the card says "Experimental" already: the row does not start with it again
  auto *toggle = child<ToggleSwitch>(dialog, QStringLiteral("cbExperimentalMultiMonitor"));
  QCOMPARE(rowOf(toggle)->title(), QStringLiteral("Multi-monitor aware cursor mapping"));
  QVERIFY(rowOf(toggle)->description().contains(QStringLiteral("off = Deskflow behaves exactly as before")));
}

void ServerConfigAdvancedTabTests::deskflowMode_switchesDriveTheDialogAsBefore()
{
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));

  auto *heartbeat = child<ToggleSwitch>(dialog, QStringLiteral("cbHeartbeat"));
  auto *spin = child<QSpinBox>(dialog, QStringLiteral("sbHeartbeat"));
  QVERIFY(!heartbeat->isChecked());
  QVERIFY(!spin->isEnabled());
  QTest::mouseClick(heartbeat, Qt::LeftButton);
  QVERIFY(heartbeat->isChecked());
  QVERIFY(spin->isEnabled());

  // a click on the text of the row does the same
  QWidget *title = nullptr;
  for (auto *label : rowOf(heartbeat)->findChildren<QLabel *>()) {
    if (label->text() == rowOf(heartbeat)->rawTitle())
      title = label;
  }
  QVERIFY(title);
  QTest::mouseClick(title, Qt::LeftButton);
  QVERIFY(!heartbeat->isChecked());
  QVERIFY(!spin->isEnabled());

  // the clipboard: its size limit and the text-only switch follow it
  auto *clipboard = child<ToggleSwitch>(dialog, QStringLiteral("cbEnableClipboard"));
  auto *textOnly = child<ToggleSwitch>(dialog, QStringLiteral("cbClipboardTextOnly"));
  QVERIFY(clipboard->isChecked());
  QVERIFY(textOnly->isEnabled());
  QTest::mouseClick(clipboard, Qt::LeftButton);
  QVERIFY(!clipboard->isChecked());
  QVERIFY(!textOnly->isEnabled());
  // and the text of that row is dimmed with it
  QLabel *textOnlyTitle = nullptr;
  for (auto *label : rowOf(textOnly)->findChildren<QLabel *>()) {
    if (label->text() == rowOf(textOnly)->rawTitle())
      textOnlyTitle = label;
  }
  QVERIFY(textOnlyTitle);
  QVERIFY(!textOnlyTitle->isEnabled());
}

void ServerConfigAdvancedTabTests::deskflowMode_settingsThatAreLoaded_showWithoutMovingTheKnob()
{
  Settings::setValue(Settings::Server::EnableHeartbeat, true);
  Settings::setValue(Settings::Server::RelativeMouseMoves, true);
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  for (const auto *name : {"cbHeartbeat", "cbRelativeMouseMoves"}) {
    auto *toggle = child<ToggleSwitch>(dialog, QString::fromLatin1(name));
    QVERIFY2(toggle->isChecked(), name);
    QCOMPARE(toggle->knobPosition(), 1.0);
  }
  QVERIFY(child<QSpinBox>(dialog, QStringLiteral("sbHeartbeat"))->isEnabled());
}

void ServerConfigAdvancedTabTests::deskflowMode_aSettingOfAnotherOs_hasNoRow()
{
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  auto *row = rowOf(child<ToggleSwitch>(dialog, QStringLiteral("cbWin32KeepForeground")));
  QVERIFY(row);
  // the dialog hides the control on the other systems, and the row follows it, with the line above the next one
  QCOMPARE(row->isHidden(), !deskflow::platform::isWindows());
  auto *card = cardTitled(dialog, QStringLiteral("Misc"));
  QVERIFY(card);
  const auto rows = card->rows();
  QVERIFY(!rows.isEmpty());
  QVERIFY(!rows.first()->hasDivider());
  int dividers = 0;
  for (const auto *candidate : rows)
    dividers += candidate->hasDivider() ? 1 : 0;
  // one line between two rows that are shown
  int shown = 0;
  for (const auto *candidate : rows)
    shown += candidate->isHidden() ? 0 : 1;
  QCOMPARE(dividers, shown - 1);
}

void ServerConfigAdvancedTabTests::deskflowMode_tabKeyWalksTheControlsInTheOrderOfTheScreen()
{
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));

  // what the screen shows from the top: the controls of the rows of the cards, left to right (the path row holds the
  // field and the button of the config file)
  const auto *tab = child<QWidget>(dialog, QStringLiteral("tabAdvanced"));
  const auto *scroll = tab->findChild<QScrollArea *>();
  QVERIFY(scroll);
  auto takesFocus = [](const QWidget *widget) {
    return widget->isEnabled() && !widget->isHidden() && (widget->focusPolicy() & Qt::TabFocus) &&
           !qobject_cast<const QAbstractSpinBox *>(widget->parentWidget());
  };
  QList<QWidget *> expected;
  for (auto *card : scroll->widget()->findChildren<SettingCard *>(Qt::FindDirectChildrenOnly)) {
    for (auto *row : card->rows()) {
      if (row->isHidden())
        continue;
      for (auto *control : row->controls()) {
        if (control->objectName() == QStringLiteral("widgetExternalConfigControls")) {
          for (auto *inner : control->findChildren<QWidget *>(Qt::FindDirectChildrenOnly)) {
            if (takesFocus(inner))
              expected << inner;
          }
        } else if (takesFocus(control)) {
          expected << control;
        }
      }
    }
  }
  QVERIFY(expected.size() > 8);

  QList<QWidget *> walked;
  auto *widget = expected.first();
  for (int guard = 0; guard < 1000 && walked.size() < expected.size(); ++guard) {
    if (takesFocus(widget))
      walked << widget;
    widget = widget->nextInFocusChain();
  }
  QCOMPARE(walked, expected);
}

void ServerConfigAdvancedTabTests::deskflowMode_externalConfig_isBoundBothWaysAndFollowsTheGroupBox()
{
  Settings::setValue(Settings::Server::ExternalConfig, true);
  installDeskflowStyle();
  ServerConfig config;
  ServerConfigDialog dialog(nullptr, config);

  auto *group = child<QGroupBox>(dialog, QStringLiteral("groupExternalConfig"));
  auto *toggle = child<ToggleSwitch>(dialog, QStringLiteral("cbExternalConfig"));
  auto *path = child<QWidget>(dialog, QStringLiteral("widgetExternalConfigControls"));
  QVERIFY(group->isChecked());
  QVERIFY(toggle->isChecked());
  QVERIFY(path->isEnabled());
  QCOMPARE(rowOf(toggle)->title(), QStringLiteral("Use a server config file"));
  QCOMPARE(rowOf(path)->title(), QStringLiteral("Config file path"));
  QVERIFY(child<QLabel>(dialog, QStringLiteral("labelConfigFile"))->isHidden());

  // the switch is the check box of the group box that is hidden, and the code that reads the group box sees it
  toggle->click();
  QVERIFY(!group->isChecked());
  QVERIFY(!path->isEnabled());
  group->setChecked(true);
  QVERIFY(toggle->isChecked());
  QVERIFY(path->isEnabled());

  // read-only settings disable the group box, and so the rows that took its place
  group->setEnabled(false);
  QVERIFY(!toggle->isEnabled());
  QVERIFY(!path->isEnabled());
  group->setEnabled(true);
  QVERIFY(toggle->isEnabled());
  QVERIFY(path->isEnabled());
  QVERIFY(child<QLineEdit>(dialog, QStringLiteral("lineConfigFile"))->isEnabled());
}

QTEST_MAIN(ServerConfigAdvancedTabTests)
