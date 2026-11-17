/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigAdvanced.h"

#include "gui/style/BackdropWidget.h"
#include "gui/style/DesignTokens.h"
#include "gui/style/LookFollower.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QCoreApplication>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextDocument>
#include <QVBoxLayout>

namespace deskflow::gui {

namespace {

// Follows the enabled state of a widget (the group box of the config file, which the form disables when the settings
// are read-only) with the widgets that took its place
class EnabledMirror : public QObject
{
public:
  EnabledMirror(QWidget *source, const QList<QWidget *> &targets)
      : QObject(source),
        m_source(source),
        m_targets(targets)
  {
    source->installEventFilter(this);
    apply();
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (watched == m_source && event->type() == QEvent::EnabledChange)
      apply();
    return false;
  }

private:
  void apply()
  {
    for (auto *target : std::as_const(m_targets))
      target->setEnabled(m_source->isEnabled());
  }

  QWidget *m_source;
  QList<QWidget *> m_targets;
};

template <class T> T *find(QWidget *tab, const char *name)
{
  return tab->findChild<T *>(QString::fromLatin1(name));
}

// A tooltip that is a line of text is guidance for the row; one in rich text (a list, italics) is not
QString plainTip(const QWidget *widget)
{
  const auto tip = widget->toolTip();
  return Qt::mightBeRichText(tip) ? QString() : tip;
}

// "Experimental: multi-monitor aware cursor mapping" under a card that is called "Experimental" is "Multi-monitor aware
// cursor mapping"
QString withoutPrefix(const QString &title, const QString &prefix)
{
  const auto lead = prefix + QStringLiteral(": ");
  if (prefix.isEmpty() || !title.startsWith(lead))
    return title;
  auto rest = title.mid(lead.size());
  if (!rest.isEmpty())
    rest[0] = rest[0].toUpper();
  return rest;
}

QString withoutColon(QString text)
{
  while (text.endsWith(QLatin1Char(':')) || text.endsWith(QLatin1Char(' ')))
    text.chop(1);
  return text;
}

// A row of a switch, with the controls that go before it (a number): the text of the switch is the title, and a
// tooltip in plain text is the guidance under it
SettingRow *switchRow(ToggleSwitch *toggle, const QList<QWidget *> &before = {}, const QString &cardTitle = {})
{
  auto *row = new SettingRow;
  const auto tip = plainTip(toggle);
  if (!tip.isEmpty())
    toggle->setToolTip({});
  for (auto *control : before)
    row->addControl(control);
  row->addControl(toggle);
  if (!tip.isEmpty())
    row->setDescription(tip);
  if (!cardTitle.isEmpty())
    row->setTitle(withoutPrefix(row->rawTitle(), cardTitle));
  return row;
}

QString plainTitle(const QGroupBox *group)
{
  return style::withoutMnemonic(group->title());
}

} // namespace

bool applyAdvancedCards(QWidget *tab)
{
  auto *groupMisc = find<QGroupBox>(tab, "groupMisc");
  auto *groupSwitch = find<QGroupBox>(tab, "groupSwitch");
  auto *groupNetwork = find<QGroupBox>(tab, "groupNetwork");
  auto *groupExperimental = find<QGroupBox>(tab, "groupExperimental");
  auto *groupExternal = find<QGroupBox>(tab, "groupExternalConfig");

  auto *disableLock = find<ToggleSwitch>(tab, "cbDisableLockToComputer");
  auto *defaultLock = find<ToggleSwitch>(tab, "cbDefaultLockToComputerState");
  auto *relativeMoves = find<ToggleSwitch>(tab, "cbRelativeMouseMoves");
  auto *win32Foreground = find<ToggleSwitch>(tab, "cbWin32KeepForeground");
  auto *clipboard = find<ToggleSwitch>(tab, "cbEnableClipboard");
  auto *clipboardLimitLabel = find<QLabel>(tab, "label_7");
  auto *clipboardLimit = find<QSpinBox>(tab, "sbClipboardSizeLimit");
  auto *textOnly = find<ToggleSwitch>(tab, "cbClipboardTextOnly");
  auto *switchDelay = find<ToggleSwitch>(tab, "cbSwitchDelay");
  auto *switchDelayValue = find<QSpinBox>(tab, "sbSwitchDelay");
  auto *doubleTap = find<ToggleSwitch>(tab, "cbSwitchDoubleTap");
  auto *doubleTapValue = find<QSpinBox>(tab, "sbSwitchDoubleTap");
  auto *heartbeat = find<ToggleSwitch>(tab, "cbHeartbeat");
  auto *heartbeatValue = find<QSpinBox>(tab, "sbHeartbeat");
  auto *heartbeatBox = find<QWidget>(tab, "widgetHeartbeat");
  auto *protocolLabel = find<QLabel>(tab, "labelProtocol");
  auto *synergy = find<QRadioButton>(tab, "rbProtocolSynergy");
  auto *barrier = find<QRadioButton>(tab, "rbProtocolBarrier");
  auto *multiMonitor = find<ToggleSwitch>(tab, "cbExperimentalMultiMonitor");
  auto *configControls = find<QWidget>(tab, "widgetExternalConfigControls");
  auto *configLabel = find<QLabel>(tab, "labelConfigFile");
  auto *configFile = find<QLineEdit>(tab, "lineConfigFile");

  const QList<QObject *> needed = {
      groupMisc,           groupSwitch,    groupNetwork,  groupExperimental, groupExternal,
      disableLock,         defaultLock,    relativeMoves, win32Foreground,   clipboard,
      clipboardLimitLabel, clipboardLimit, textOnly,      switchDelay,       switchDelayValue,
      doubleTap,           doubleTapValue, heartbeat,     heartbeatValue,    heartbeatBox,
      protocolLabel,       synergy,        barrier,       multiMonitor,      configControls,
      configLabel,         configFile
  };
  if (needed.contains(nullptr) || !tab->layout())
    return false;

  // The dialog is as small as its tabs let it be: the tab that is made over must not ask for less room than it did, or
  // the tabs that share the dialog with it (the grid of the computers) are squeezed at the smallest size. The cards are
  // taller than the groups were and scroll in the room that the groups needed.
  const int minimumHeight = tab->layout()->minimumSize().height();

  // Misc
  auto *misc = new SettingCard(groupMisc->title());
  misc->addRow(switchRow(disableLock));
  misc->addRow(switchRow(defaultLock));
  misc->addRow(switchRow(relativeMoves));
  misc->addRow(switchRow(win32Foreground));
  misc->addRow(switchRow(clipboard));
  auto *limitRow = new SettingRow(withoutColon(clipboardLimitLabel->text()));
  limitRow->addControl(clipboardLimit);
  clipboardLimitLabel->hide();
  misc->addRow(limitRow);
  misc->addRow(switchRow(textOnly));

  // Switching: how long to wait, and the number before the switch that turns it on
  auto *switching = new SettingCard(groupSwitch->title());
  switching->addRow(switchRow(switchDelay, {switchDelayValue}));
  switching->addRow(switchRow(doubleTap, {doubleTapValue}));

  // Network: the heartbeat, and the protocol with its two choices
  auto *network = new SettingCard(groupNetwork->title());
  network->addRow(switchRow(heartbeat, {heartbeatValue}));
  heartbeatBox->hide();
  auto *protocolRow = new SettingRow(protocolLabel->text());
  protocolRow->addControl(synergy);
  protocolRow->addControl(barrier);
  protocolRow->setToolTip(protocolLabel->toolTip());
  // the help of the label is for the row that took its place
  protocolRow->setWhatsThis(protocolLabel->whatsThis());
  protocolLabel->hide();
  network->addRow(protocolRow);

  // Experimental: the card says so with a chip as well as with its title
  auto *experimental = new SettingCard(groupExperimental->title());
  experimental->header()->setTrailingWidget(
      new StatusChip(QCoreApplication::translate("ServerConfigAdvanced", "Beta"), StatusChip::Kind::Warning)
  );
  experimental->addRow(switchRow(multiMonitor, {}, plainTitle(groupExperimental)));

  // The config file: the group box is a check box too (its title is the text of it), and the form reads it as one. The
  // switch is bound to it both ways.
  auto *external = new SettingCard;
  external->setAccessibleName(plainTitle(groupExternal));
  auto *externalSwitch = new ToggleSwitch;
  externalSwitch->setObjectName(QStringLiteral("cbExternalConfig"));
  externalSwitch->setChecked(groupExternal->isChecked());
  auto *externalRow = new SettingRow(plainTitle(groupExternal), plainTip(groupExternal));
  externalRow->setToolTip(groupExternal->toolTip());
  externalRow->addControl(externalSwitch);
  external->addRow(externalRow);
  auto *pathRow = new SettingRow(withoutColon(configLabel->text()));
  pathRow->addControl(configControls);
  pathRow->setExpandingControls(true);
  configLabel->hide();
  if (configFile->accessibleName().isEmpty())
    configFile->setAccessibleName(pathRow->title());
  external->addRow(pathRow);
  QObject::connect(externalSwitch, &QAbstractButton::toggled, groupExternal, &QGroupBox::setChecked);
  QObject::connect(groupExternal, &QGroupBox::toggled, externalSwitch, &QAbstractButton::setChecked);
  new EnabledMirror(groupExternal, {externalRow, pathRow});

  // The page: cards on the backdrop, which scrolls when the dialog is shorter than they are
  auto *backdrop = new style::BackdropWidget(tab);
  backdrop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *scroll = new QScrollArea(backdrop);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setMinimumHeight(minimumHeight);
  auto *content = new QWidget;
  auto *list = new QVBoxLayout(content);
  // the cards carry the room of their shadow: the margin of the list is the little that is left to the edge
  list->setContentsMargins(
      style::metrics::spaceXs, style::metrics::spaceXs, style::metrics::spaceXs, style::metrics::spaceXs
  );
  list->setSpacing(0);
  for (auto *card : {misc, switching, network, experimental, external})
    list->addWidget(card);
  list->addStretch(1);
  scroll->setWidget(content);
  // the scroll area fills what it holds with the colour of the window: the backdrop must show through
  content->setAutoFillBackground(false);
  scroll->viewport()->setAutoFillBackground(false);
  auto *frame = new QVBoxLayout(backdrop);
  frame->setContentsMargins(0, 0, 0, 0);
  frame->addWidget(scroll);

  for (auto *group : {groupMisc, groupSwitch, groupNetwork, groupExperimental, groupExternal})
    group->hide();
  tab->layout()->setContentsMargins(0, 0, 0, 0);
  tab->layout()->setSpacing(0);
  tab->layout()->addWidget(backdrop);
  return true;
}

} // namespace deskflow::gui
