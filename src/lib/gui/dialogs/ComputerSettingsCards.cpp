/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ComputerSettingsCards.h"

#include "CardBuilders.h"
#include "gui/style/BackdropWidget.h"
#include "gui/style/DesignTokens.h"
#include "gui/style/LookFollower.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/ToggleSwitch.h"
#include "validators/ValidationError.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>

namespace deskflow::gui {

namespace {

using cards::find;
using cards::plainTip;
using cards::switchRow;
using cards::withoutColon;

// The cards ask for the room that they need, up to most of the screen: a scroll area that is not told asks for a small
// part of it (a few lines), and the dialog would open with a strip of its settings.
class CardScrollArea : public QScrollArea
{
public:
  using QScrollArea::QScrollArea;

  QSize sizeHint() const override
  {
    if (!widget())
      return QScrollArea::sizeHint();
    auto hint = widget()->sizeHint() + QSize(2 * frameWidth(), 2 * frameWidth());
    const auto *where = screen();
    const auto room = where ? where->availableGeometry().size() : QSize(1280, 800);
    const int tallest = room.height() * 8 / 10;
    if (hint.height() > tallest) {
      hint.setHeight(tallest);
      // the bar is there, and it is not on the cards
      hint.rwidth() += verticalScrollBar()->sizeHint().width();
    }
    return hint;
  }
};

// A row for a control that has a label: the text of the label is the title, and a tooltip in plain text is the guidance
// under it. The label is not shown.
SettingRow *fieldRow(QLabel *label, QWidget *control, const QList<QWidget *> &more = {})
{
  auto *row = new SettingRow(withoutColon(label->text()));
  const auto tip = plainTip(control);
  if (!tip.isEmpty()) {
    control->setToolTip({});
    row->setDescription(tip);
  }
  row->addControl(control);
  for (auto *extra : more)
    row->addControl(extra);
  label->hide();
  return row;
}

// A corner has no text, only a place in the grid of the old form and a tool tip that says which corner it is
SettingRow *cornerRow(ToggleSwitch *toggle)
{
  auto *row = new SettingRow(toggle->toolTip());
  toggle->setToolTip({});
  row->addControl(toggle);
  return row;
}

// The text of a group box without its mnemonic
QString plainTitle(const QGroupBox *group)
{
  return style::withoutMnemonic(group->title());
}

// A label that is a paragraph of the card: it wraps and takes the width that it is given
void asParagraph(QLabel *label)
{
  label->setWordWrap(true);
  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

} // namespace

bool applyComputerSettingsCards(
    QDialog *dialog, validators::ValidationError *nameError, validators::ValidationError *aliasError
)
{
  auto *box = qobject_cast<QBoxLayout *>(dialog->layout());
  auto *groupComputer = find<QGroupBox>(dialog, "groupComputerName");
  auto *groupModifiers = find<QGroupBox>(dialog, "groupModifers");
  auto *groupCorners = find<QGroupBox>(dialog, "groupDeadCorners");
  auto *groupFixes = find<QGroupBox>(dialog, "groupFixes");
  auto *groupAliases = find<QGroupBox>(dialog, "groupAliases");
  auto *groupCrossOs = find<QGroupBox>(dialog, "groupCrossOSInput");
  auto *groupMonitors = find<QGroupBox>(dialog, "groupMonitorLayout");
  auto *buttons = find<QWidget>(dialog, "buttonBox");

  auto *nameLabel = find<QLabel>(dialog, "label_7");
  auto *nameEdit = find<QLineEdit>(dialog, "lineNameEdit");
  auto *nameErrorLabel = find<QLabel>(dialog, "lblNameError");
  auto *shareClipboard = find<ToggleSwitch>(dialog, "chkShareClipboard");

  auto *aliasEdit = find<QLineEdit>(dialog, "lineAddAlias");
  auto *aliasAdd = find<QPushButton>(dialog, "btnAddAlias");
  auto *aliasErrorLabel = find<QLabel>(dialog, "lblAliasError");
  auto *aliasList = find<QListWidget>(dialog, "listAliases");
  auto *aliasRemove = find<QPushButton>(dialog, "btnRemoveAlias");

  struct Modifier
  {
    QLabel *label;
    QComboBox *combo;
  };
  const QList<Modifier> modifiers = {
      {find<QLabel>(dialog, "label_2"), find<QComboBox>(dialog, "comboShift")},
      {find<QLabel>(dialog, "label_3"), find<QComboBox>(dialog, "comboCtrl")},
      {find<QLabel>(dialog, "label_4"), find<QComboBox>(dialog, "comboAlt")},
      {find<QLabel>(dialog, "label_5"), find<QComboBox>(dialog, "comboMeta")},
      {find<QLabel>(dialog, "label_6"), find<QComboBox>(dialog, "comboSuper")},
      {find<QLabel>(dialog, "labelAltGr"), find<QComboBox>(dialog, "comboAltGr")},
  };

  const QList<ToggleSwitch *> corners = {
      find<ToggleSwitch>(dialog, "chkDeadTopLeft"), find<ToggleSwitch>(dialog, "chkDeadTopRight"),
      find<ToggleSwitch>(dialog, "chkDeadBottomLeft"), find<ToggleSwitch>(dialog, "chkDeadBottomRight")
  };
  auto *cornerSizeLabel = find<QLabel>(dialog, "label");
  auto *cornerSize = find<QSpinBox>(dialog, "sbSwitchCornerSize");

  const QList<ToggleSwitch *> fixes = {
      find<ToggleSwitch>(dialog, "chkFixScrollLock"), find<ToggleSwitch>(dialog, "chkFixCapsLock"),
      find<ToggleSwitch>(dialog, "chkFixNumLock"),    find<ToggleSwitch>(dialog, "chkFixXTest"),
      find<ToggleSwitch>(dialog, "chkWeakX11Focus"),
  };

  auto *crossOsIntro = find<QLabel>(dialog, "lblCrossOSInputDescription");
  auto *f18Label = find<QLabel>(dialog, "lblF18Key");
  auto *f18 = find<QComboBox>(dialog, "comboF18Key");
  auto *invertX = find<ToggleSwitch>(dialog, "chkCrossOSInvertX");
  auto *invertY = find<ToggleSwitch>(dialog, "chkCrossOSInvertY");
  auto *scopeLabel = find<QLabel>(dialog, "lblCrossOSInvertScope");
  auto *scope = find<QComboBox>(dialog, "comboCrossOSInvertScope");
  auto *media = find<ToggleSwitch>(dialog, "chkCrossOSSemanticMedia");
  auto *layering = find<QLabel>(dialog, "lblCrossOSScrollLayering");

  auto *layoutText = find<QPlainTextEdit>(dialog, "textMonitorLayout");
  auto *layoutImport = find<QPushButton>(dialog, "btnImportMonitorLayout");
  auto *layoutStatus = find<QLabel>(dialog, "lblMonitorLayoutStatus");
  auto *sizeLabel = find<QLabel>(dialog, "lblArrangementSize");
  auto *width = find<QSpinBox>(dialog, "sbArrangementWidth");
  auto *times = find<QLabel>(dialog, "lblArrangementTimes");
  auto *height = find<QSpinBox>(dialog, "sbArrangementHeight");
  auto *scaleLabel = find<QLabel>(dialog, "lblDisplayScale");
  auto *scale = find<QSpinBox>(dialog, "sbDisplayScale");

  QList<QObject *> needed = {box,
                             groupComputer,
                             groupModifiers,
                             groupCorners,
                             groupFixes,
                             groupAliases,
                             groupCrossOs,
                             groupMonitors,
                             buttons,
                             nameLabel,
                             nameEdit,
                             nameErrorLabel,
                             shareClipboard,
                             aliasEdit,
                             aliasAdd,
                             aliasErrorLabel,
                             aliasList,
                             aliasRemove,
                             cornerSizeLabel,
                             cornerSize,
                             crossOsIntro,
                             f18Label,
                             f18,
                             invertX,
                             invertY,
                             scopeLabel,
                             scope,
                             media,
                             layering,
                             layoutText,
                             layoutImport,
                             layoutStatus,
                             sizeLabel,
                             width,
                             times,
                             height,
                             scaleLabel,
                             scale,
                             nameError,
                             aliasError};
  for (const auto &modifier : modifiers)
    needed << modifier.label << modifier.combo;
  for (auto *control : corners + fixes)
    needed << control;
  if (needed.contains(nullptr))
    return false;

  // The dialog is as small as it was: what the groups needed is what the cards are given to scroll in.
  const auto oldMinimum = dialog->layout()->totalMinimumSize();

  // Computer: the name, and whether the clipboard is shared with it
  auto *computer = new SettingCard(groupComputer->title());
  auto *nameRow = fieldRow(nameLabel, nameEdit);
  nameRow->setExpandingControls(true);
  nameErrorLabel->hide();
  QObject::connect(nameError, &validators::ValidationError::messageChanged, nameRow, &SettingRow::setError);
  computer->addRow(nameRow);
  computer->addRow(switchRow(shareClipboard));

  // Aliases: a new one, and the list with a button that removes
  auto *aliases = new SettingCard(groupAliases->title());
  auto *aliasRow = new SettingRow(QCoreApplication::translate("ComputerSettingsCards", "New alias"));
  aliasRow->addControl(aliasEdit);
  aliasRow->addControl(aliasAdd);
  aliasRow->setExpandingControls(true);
  aliasErrorLabel->hide();
  QObject::connect(aliasError, &validators::ValidationError::messageChanged, aliasRow, &SettingRow::setError);
  aliases->addRow(aliasRow);
  auto *aliasBox = new QWidget;
  auto *aliasLayout = new QVBoxLayout(aliasBox);
  aliasLayout->setContentsMargins(0, 0, 0, 0);
  aliasLayout->setSpacing(style::metrics::spaceSm);
  aliasLayout->addWidget(aliasList);
  aliasLayout->addWidget(aliasRemove, 0, Qt::AlignRight);
  aliases->addContent(aliasBox);
  if (aliasList->accessibleName().isEmpty())
    aliasList->setAccessibleName(plainTitle(groupAliases));

  // Modifier keys
  auto *modifierCard = new SettingCard(groupModifiers->title());
  for (const auto &modifier : modifiers)
    modifierCard->addRow(fieldRow(modifier.label, modifier.combo));

  // Dead corners: the four corners, and how big they are
  auto *cornerCard = new SettingCard(groupCorners->title());
  for (auto *corner : corners)
    cornerCard->addRow(cornerRow(corner));
  cornerCard->addRow(fieldRow(cornerSizeLabel, cornerSize));

  // Fixes
  auto *fixCard = new SettingCard(groupFixes->title());
  for (auto *fix : fixes)
    fixCard->addRow(switchRow(fix));

  // Cross-OS input: what is said about it first, then the settings, and the note about the scrolling
  auto *crossOs = new SettingCard(groupCrossOs->title());
  crossOs->setToolTip(groupCrossOs->toolTip());
  asParagraph(crossOsIntro);
  crossOs->addNote(crossOsIntro);
  crossOs->addRow(fieldRow(f18Label, f18));
  crossOs->addRow(switchRow(invertX));
  crossOs->addRow(switchRow(invertY));
  crossOs->addRow(fieldRow(scopeLabel, scope));
  crossOs->addRow(switchRow(media));
  asParagraph(layering);
  crossOs->addContent(layering);

  // Monitor layout: the text of the monitors with its import, then the size of the screen and its scale
  auto *monitors = new SettingCard(groupMonitors->title());
  auto *monitorBox = new QWidget;
  auto *monitorLayout = new QVBoxLayout(monitorBox);
  monitorLayout->setContentsMargins(0, 0, 0, 0);
  monitorLayout->setSpacing(style::metrics::spaceSm);
  monitorLayout->addWidget(layoutText);
  auto *importLine = new QHBoxLayout;
  importLine->addWidget(layoutImport);
  importLine->addWidget(layoutStatus, 1);
  monitorLayout->addLayout(importLine);
  monitors->addContent(monitorBox);
  if (layoutText->accessibleName().isEmpty())
    layoutText->setAccessibleName(plainTitle(groupMonitors));
  monitors->addRow(fieldRow(sizeLabel, width, {times, height}));
  monitors->addRow(fieldRow(scaleLabel, scale));

  // The page: cards on the backdrop, which scrolls when the dialog is shorter than they are, and the buttons on it
  auto *backdrop = new style::BackdropWidget(dialog);
  backdrop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *scroll = new CardScrollArea(backdrop);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto *content = new QWidget;
  auto *list = new QVBoxLayout(content);
  // the cards carry the room of their shadow: the margin of the list is the little that is left to the edge
  list->setContentsMargins(
      style::metrics::spaceXs, style::metrics::spaceXs, style::metrics::spaceXs, style::metrics::spaceXs
  );
  list->setSpacing(0);
  for (auto *card : {computer, aliases, modifierCard, cornerCard, fixCard, crossOs, monitors})
    list->addWidget(card);
  list->addStretch(1);
  scroll->setWidget(content);
  // the scroll area fills what it holds with the colour of the window: the backdrop must show through
  content->setAutoFillBackground(false);
  scroll->viewport()->setAutoFillBackground(false);

  auto *footer = new QWidget(backdrop);
  auto *footerLayout = new QVBoxLayout(footer);
  footerLayout->setContentsMargins(
      style::metrics::spaceLg, style::metrics::spaceSm, style::metrics::spaceLg, style::metrics::spaceLg
  );
  footerLayout->addWidget(buttons);

  auto *frame = new QVBoxLayout(backdrop);
  frame->setContentsMargins(0, 0, 0, 0);
  frame->setSpacing(0);
  frame->addWidget(scroll, 1);
  frame->addWidget(footer);

  for (auto *group :
       {groupComputer, groupModifiers, groupCorners, groupFixes, groupAliases, groupCrossOs, groupMonitors})
    group->hide();
  box->setContentsMargins(0, 0, 0, 0);
  box->setSpacing(0);
  box->insertWidget(0, backdrop, 1);

  // what the groups asked for is the least that the cards are given: they scroll in it, and are never cut at the side
  const int cardsWidth = content->minimumSizeHint().width() + 2 * scroll->frameWidth();
  scroll->setMinimumHeight(
      qMax(scroll->minimumSizeHint().height(), oldMinimum.height() - footer->minimumSizeHint().height())
  );
  scroll->setMinimumWidth(qMax(cardsWidth, oldMinimum.width()));
  return true;
}

} // namespace deskflow::gui
