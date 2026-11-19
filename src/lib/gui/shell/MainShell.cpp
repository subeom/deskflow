/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "MainShell.h"

#include "ClientConnectionSection.h"
#include "StatusModel.h"
#include "gui/settings/RestartBanner.h"
#include "gui/style/Appearance.h"
#include "gui/style/BackdropWidget.h"
#include "gui/style/FontScale.h"
#include "gui/style/Icons.h"
#include "gui/style/LookFollower.h"
#include "gui/style/PanelWidget.h"
#include "gui/style/TextSize.h"
#include "gui/widgets/NavRail.h"
#include "gui/widgets/PageTitle.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/SegmentedControl.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/SidebarShell.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/StatusHero.h"

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace deskflow::gui {

using namespace deskflow::gui::style;

namespace {

constexpr auto kHomeId = "home";
constexpr auto kPreferencesId = "preferences";
constexpr auto kLogId = "log";
constexpr auto kAboutId = "about";
constexpr auto kSecurityId = "security";
constexpr auto kFingerprintId = "fingerprint";
constexpr auto kUpdateId = "update";

QString oneLine(QString text)
{
  text.replace(QLatin1Char('\n'), QLatin1Char(' '));
  return text.simplified();
}

QString withoutDots(QString text)
{
  text = withoutMnemonic(text);
  while (text.endsWith(QLatin1Char('.')) || text.endsWith(QLatin1Char(' ')))
    text.chop(1);
  return text;
}

// The notice bar of the layout memory is shown and hidden by its own logic: the room around it follows it
class HolderFollower : public QObject
{
public:
  HolderFollower(QWidget *bar, QWidget *holder) : QObject(holder), m_bar(bar), m_holder(holder)
  {
    bar->installEventFilter(this);
    holder->setVisible(!bar->isHidden());
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (watched == m_bar && (event->type() == QEvent::ShowToParent || event->type() == QEvent::HideToParent))
      m_holder->setVisible(!m_bar->isHidden());
    return false;
  }

private:
  QWidget *m_bar;
  QWidget *m_holder;
};

} // namespace

MainShell::MainShell(const MainShellParts &parts, QObject *parent) : QObject(parent), m_parts(parts)
{
  const auto room = PanelWidget::shadowMargins();

  m_status = new StatusModel(this);
  m_backdrop = new BackdropWidget(parts.central);
  m_backdrop->setObjectName(QStringLiteral("mainShellBackdrop"));
  m_sidebar = new SidebarShell(m_backdrop);
  auto *backdropLayout = new QVBoxLayout(m_backdrop);
  backdropLayout->setContentsMargins(0, 0, 0, 0);
  backdropLayout->addWidget(m_sidebar);

  // the sidebar
  auto *rail = m_sidebar->rail();
  rail->addPage(QLatin1String(kHomeId), QStringLiteral("desktop"), {});
  rail->addSection({});
  rail->addAction(QLatin1String(kPreferencesId), QStringLiteral("gear"), {});
  rail->addSeparator();
  rail->addAction(QLatin1String(kLogId), QStringLiteral("terminal-window"), {})->setCheckable(true);
  rail->addAction(QLatin1String(kAboutId), QStringLiteral("info"), {});
  rail->addStatus(QLatin1String(kSecurityId), QStringLiteral("shield-check"), {}, NavRail::Region::Bottom)
      ->setIconColor(IconColor::Success);
  rail->addAction(QLatin1String(kFingerprintId), QStringLiteral("fingerprint"), {}, NavRail::Region::Bottom);
  rail->addAction(QLatin1String(kUpdateId), QStringLiteral("download-simple"), {}, NavRail::Region::Bottom)
      ->setIconColor(IconColor::Accent);
  rail->setItemVisible(QLatin1String(kFingerprintId), false);
  rail->setItemVisible(QLatin1String(kUpdateId), false);

  // the content: the notice of the layout memory above the pages
  auto *content = new QWidget;
  content->setObjectName(QStringLiteral("shellContent"));
  auto *contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  if (parts.noticeBar) {
    auto *holder = new QWidget(content);
    holder->setObjectName(QStringLiteral("noticeHolder"));
    auto *holderLayout = new QVBoxLayout(holder);
    holderLayout->setContentsMargins(room.left(), room.top(), room.right(), 6);
    holderLayout->addWidget(parts.noticeBar);
    new HolderFollower(parts.noticeBar, holder);
    contentLayout->addWidget(holder);
  }

  // the line that asks for a restart of the sharing, above every page
  {
    auto *holder = new QWidget(content);
    holder->setObjectName(QStringLiteral("restartHolder"));
    auto *holderLayout = new QVBoxLayout(holder);
    holderLayout->setContentsMargins(room.left(), room.top(), room.right(), 6);
    m_restartBanner = new settings::RestartBanner(holder);
    holderLayout->addWidget(m_restartBanner);
    new HolderFollower(m_restartBanner, holder);
    contentLayout->addWidget(holder);
  }

  m_pages = new QStackedWidget(content);
  m_pages->setObjectName(QStringLiteral("shellPages"));
  contentLayout->addWidget(m_pages, 1);

  // the home page: the title and the mode, the state, and what belongs to the mode
  auto *home = new QWidget;
  home->setObjectName(QStringLiteral("shellHome"));
  auto *homeLayout = new QVBoxLayout(home);
  homeLayout->setContentsMargins(0, 0, 0, 0);
  homeLayout->setSpacing(0);

  auto *header = new QWidget(home);
  auto *headerLayout = new QVBoxLayout(header);
  headerLayout->setContentsMargins(room.left(), room.top() + 6, room.right(), 8);
  headerLayout->setSpacing(6);
  auto *titleRow = new QHBoxLayout;
  titleRow->setSpacing(metrics::spaceLg);
  m_title = new PageTitle(header);
  m_title->setObjectName(QStringLiteral("shellTitle"));
  m_modeControl = new SegmentedControl(header);
  m_modeControl->setObjectName(QStringLiteral("modeControl"));
  m_modeControl->addSegment({});
  m_modeControl->addSegment({});
  titleRow->addWidget(m_title, 1);
  titleRow->addWidget(m_modeControl, 0, Qt::AlignVCenter);
  headerLayout->addLayout(titleRow);
  if (parts.nameRow)
    headerLayout->addWidget(parts.nameRow);
  homeLayout->addWidget(header);

  m_hero = new StatusHero(home);
  m_hero->setObjectName(QStringLiteral("statusHero"));
  if (parts.ipLabel)
    m_hero->addAccessory(parts.ipLabel);
  QList<QWidget *> actions;
  for (QWidget *button : {static_cast<QWidget *>(parts.toggleCore), static_cast<QWidget *>(parts.restartCore)}) {
    if (button)
      actions.append(button);
  }
  m_hero->setActions(actions);
  homeLayout->addWidget(m_hero);

  m_modePages = new QStackedWidget(home);
  m_modePages->setObjectName(QStringLiteral("modePages"));
  // A card shows and hides itself (a card without rows is not shown): it is the child of a page and not the page, so
  // that what it does cannot undo what the stack does
  auto wrapAsPage = [this](QWidget *card) {
    auto *page = new QWidget(m_modePages);
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(card);
    card->setParent(page);
    m_modePages->addWidget(page);
    return page;
  };
  // 0: no mode, 1: server, 2: client
  m_noModeCard = new SettingCard({});
  m_noModeCard->setObjectName(QStringLiteral("noModeCard"));
  m_noModeCard->addRow(new SettingRow);
  m_noModePage = wrapAsPage(m_noModeCard);

  m_computersCard = new SettingCard(tr("Other computers"));
  m_computersCard->setObjectName(QStringLiteral("computersCard"));
  // the buttons of the server are at the end of the header of the card
  auto *serverButtons = new QWidget(m_computersCard);
  auto *serverButtonLayout = new QHBoxLayout(serverButtons);
  serverButtonLayout->setContentsMargins(0, 0, 0, 0);
  serverButtonLayout->setSpacing(metrics::spaceSm);
  for (auto *button : {parts.configureServer, parts.exportServer}) {
    if (button)
      serverButtonLayout->addWidget(button);
  }
  m_computersCard->header()->setTrailingWidget(serverButtons);
  m_serverPage = wrapAsPage(m_computersCard);
  m_clientSection = new ClientConnectionSection(
      parts.hostLabel, parts.hostname, parts.testConnection, parts.configureClient, m_modePages
  );
  m_modePages->addWidget(m_clientSection);
  homeLayout->addWidget(m_modePages);
  homeLayout->addStretch(1);
  m_pages->addWidget(home);

  m_sidebar->setContent(content);

  if (parts.noMode)
    parts.noMode->hide();
  if (parts.oldGroup)
    parts.oldGroup->hide();

  // the buttons of the state are the buttons of the window, made for this place; they are as high as the size of text
  // asks
  if (parts.toggleCore)
    parts.toggleCore->setDefault(true);
  if (parts.restartCore)
    parts.restartCore->setIcon(icon(QStringLiteral("arrow-clockwise")));
  const QPointer<QPushButton> toggleCore = parts.toggleCore;
  const QPointer<QPushButton> restartCore = parts.restartCore;
  const auto sizeStateButtons = [toggleCore, restartCore] {
    if (toggleCore) {
      toggleCore->setMinimumSize(px(104), px(40));
      toggleCore->setMaximumSize(QWIDGETSIZE_MAX, px(40));
    }
    if (restartCore) {
      restartCore->setMinimumSize(0, px(40));
      restartCore->setMaximumSize(QWIDGETSIZE_MAX, px(40));
    }
  };
  sizeStateButtons();
  onTextSizeChanged(this, sizeStateButtons);

  // the mode: the control and the radio buttons are one choice. The radio buttons stay where they were (hidden): the
  // window reads them
  if (parts.modeServer && parts.modeClient) {
    connect(m_modeControl, &SegmentedControl::currentChanged, this, [this](int index) {
      (index == 0 ? m_parts.modeServer : m_parts.modeClient)->setChecked(true);
    });
    connect(parts.modeServer, &QRadioButton::toggled, this, [this](bool on) {
      if (on)
        m_modeControl->setCurrent(0);
    });
    connect(parts.modeClient, &QRadioButton::toggled, this, [this](bool on) {
      if (on)
        m_modeControl->setCurrent(1);
    });
    if (parts.modeServer->isChecked())
      m_modeControl->setCurrent(0);
    else if (parts.modeClient->isChecked())
      m_modeControl->setCurrent(1);
  }

  // the state
  connect(m_status, &StatusModel::changed, this, &MainShell::applyStatus);
  applyStatus();

  // the sidebar and the stack of pages
  connect(rail, &NavRail::pageSelected, this, [this](const QString &id) {
    // an entry of the sidebar that is chosen is the page that is shown, also when it is the entry of a sub page
    m_subPage.clear();
    if (id == QLatin1String(kHomeId))
      m_pages->setCurrentIndex(0);
  });

  parts.central->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  if (auto *layout = parts.central->layout()) {
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_backdrop);
  }

  retranslate();
  setMode(Mode::None);
}

QWidget *MainShell::widget() const
{
  return m_backdrop;
}

SidebarShell *MainShell::sidebar() const
{
  return m_sidebar;
}

NavRail *MainShell::rail() const
{
  return m_sidebar->rail();
}

StatusModel *MainShell::status() const
{
  return m_status;
}

StatusHero *MainShell::hero() const
{
  return m_hero;
}

SegmentedControl *MainShell::modeControl() const
{
  return m_modeControl;
}

ClientConnectionSection *MainShell::clientSection() const
{
  return m_clientSection;
}

SettingCard *MainShell::computersCard() const
{
  return m_computersCard;
}

QStackedWidget *MainShell::pages() const
{
  return m_pages;
}

QLabel *MainShell::pageTitle() const
{
  return m_title;
}

MainShell::Mode MainShell::mode() const
{
  return m_mode;
}

void MainShell::setTexts(const Texts &texts)
{
  m_texts = texts;
  retranslate();
}

void MainShell::retranslate()
{
  auto *rail = m_sidebar->rail();
  rail->setToggleTexts(tr("Collapse sidebar"), tr("Expand sidebar"));
  rail->setSectionText(0, tr("Settings"));
  rail->setItemText(QLatin1String(kPreferencesId), withoutDots(m_texts.preferences));
  rail->setItemText(QLatin1String(kLogId), withoutDots(m_texts.log));
  rail->setItemText(QLatin1String(kAboutId), withoutDots(m_texts.about));
  rail->setItemText(QLatin1String(kFingerprintId), m_texts.fingerprint);
  setEncryptionEnabled(m_encryption);
  setUpdateVersion(m_updateVersion);

  m_modeControl->setSegmentText(0, tr("Share this computer"));
  m_modeControl->setSegmentText(1, tr("Use another computer"));
  if (m_parts.modeServer)
    m_modeControl->setSegmentDescription(0, oneLine(m_parts.modeServer->text()));
  if (m_parts.modeClient)
    m_modeControl->setSegmentDescription(1, oneLine(m_parts.modeClient->text()));
  m_modeControl->setAccessibleName(tr("Mode"));

  if (m_parts.restartCore) {
    // the tooltip of the button is its name in the language of the program
    m_parts.restartCore->setText(m_parts.restartCore->toolTip());
    m_parts.restartCore->setAccessibleName(m_parts.restartCore->toolTip());
  }
  // a button of an icon alone is named by its tooltip, which is in the language of the program
  if (m_parts.exportServer)
    m_parts.exportServer->setAccessibleName(m_parts.exportServer->toolTip());
  if (m_parts.nameRow) {
    // the pencil beside the name of this computer
    for (auto *button : m_parts.nameRow->findChildren<QPushButton *>()) {
      if (button->text().isEmpty() && !button->toolTip().isEmpty())
        button->setAccessibleName(button->toolTip());
    }
  }

  m_computersCard->header()->setTitle(tr("Other computers"));
  m_clientSection->retranslate();
  updateModeTexts();
  updateComputersPlaceholder();
}

void MainShell::updateModeTexts()
{
  QString title;
  switch (m_mode) {
  case Mode::Server:
    title = tr("Computers");
    break;
  case Mode::Client:
    title = tr("Connection");
    break;
  case Mode::None:
    title = tr("Home");
    break;
  }
  m_title->setText(title);
  m_sidebar->rail()->setItemText(QLatin1String(kHomeId), title);

  const auto noMode = m_parts.noMode ? m_parts.noMode->text() : QString();
  if (!m_noModeCard->rows().isEmpty())
    m_noModeCard->rows().first()->setTitle(noMode);
}

void MainShell::setMode(Mode mode)
{
  m_mode = mode;
  switch (mode) {
  case Mode::None:
    m_modePages->setCurrentWidget(m_noModePage);
    break;
  case Mode::Server:
    m_modePages->setCurrentWidget(m_serverPage);
    break;
  case Mode::Client:
    m_modePages->setCurrentWidget(m_clientSection);
    break;
  }
  updateModeTexts();
}

void MainShell::setComputers(const QList<ComputerEntry> &computers)
{
  if (m_computers.size() == computers.size()) {
    bool same = true;
    for (int i = 0; i < computers.size() && same; ++i)
      same = m_computers[i].name == computers[i].name && m_computers[i].detail == computers[i].detail &&
             m_computers[i].connected == computers[i].connected;
    if (same)
      return;
  }
  m_computers = computers;
  updateComputersPlaceholder();
}

void MainShell::updateComputersPlaceholder()
{
  m_computersCard->clearRows();
  if (m_computers.isEmpty()) {
    auto *row = new SettingRow(
        tr("No other computers yet"),
        tr("Use the configuration of the server to add the computers that share this keyboard and mouse.")
    );
    m_computersCard->addRow(row);
    return;
  }
  for (const auto &entry : std::as_const(m_computers)) {
    auto *row = new SettingRow(entry.name, entry.detail);
    row->setObjectName(QStringLiteral("computerRow"));
    row->addControl(new StatusChip(
        entry.connected ? tr("Connected") : tr("Not connected"),
        entry.connected ? StatusChip::Kind::Success : StatusChip::Kind::Neutral
    ));
    m_computersCard->addRow(row);
  }
}

void MainShell::applyStatus()
{
  const auto &view = m_status->view();
  m_hero->setKind(view.kind);
  m_hero->setIconName(view.icon);
  m_hero->setHeadline(view.headline);
  m_hero->setDetail(view.detail);
  m_hero->clearChips();
  for (const auto &chip : view.chips)
    m_hero->addChip(chip.text, chip.kind);
}

void MainShell::setEncryptionEnabled(bool enabled)
{
  m_encryption = enabled;
  auto *item = m_sidebar->rail()->item(QLatin1String(kSecurityId));
  if (!item)
    return;
  m_sidebar->rail()->setItemText(QLatin1String(kSecurityId), enabled ? tr("Encrypted (TLS)") : tr("Not encrypted"));
  item->setIconName(enabled ? QStringLiteral("shield-check") : QStringLiteral("warning"));
  item->setIconColor(enabled ? IconColor::Success : IconColor::Warning);
}

void MainShell::setFingerprintVisible(bool visible)
{
  m_sidebar->rail()->setItemVisible(QLatin1String(kFingerprintId), visible);
}

void MainShell::setUpdateVersion(const QString &version)
{
  m_updateVersion = version;
  m_sidebar->rail()->setItemVisible(QLatin1String(kUpdateId), !version.isEmpty());
  if (!version.isEmpty())
    m_sidebar->rail()->setItemText(
        QLatin1String(kUpdateId), QStringLiteral("%1 · v%2").arg(m_texts.updateAvailable, version)
    );
}

void MainShell::setLogShown(bool shown)
{
  if (auto *item = m_sidebar->rail()->item(QLatin1String(kLogId)))
    item->setChecked(shown);
}

void MainShell::addPage(const QString &id, const QString &iconName, const QString &text, QWidget *page)
{
  auto *rail = m_sidebar->rail();
  rail->addPage(id, iconName, text);
  registerPage(id, page);
}

void MainShell::addSettingsPage(const QString &id, const QString &iconName, const QString &text, QWidget *page)
{
  auto *rail = m_sidebar->rail();
  // the pages of the settings are in the group of the settings, above what ends it; they take the place of the entry
  // that opened the dialog of the settings
  rail->addPageBefore(QLatin1String(kPreferencesId), id, iconName, text);
  rail->setItemVisible(QLatin1String(kPreferencesId), false);
  registerPage(id, page);
}

void MainShell::addSubPage(const QString &id, const QString &parentId, QWidget *page)
{
  m_subPageParent.insert(id, parentId);
  registerPage(id, page);
}

void MainShell::registerPage(const QString &id, QWidget *page)
{
  const int index = m_pages->addWidget(page);
  m_pageIndex.insert(id, index);
  connect(m_sidebar->rail(), &NavRail::pageSelected, this, [this, id, index](const QString &selected) {
    if (selected == id)
      m_pages->setCurrentIndex(index);
  });
}

void MainShell::setPageText(const QString &id, const QString &text)
{
  m_sidebar->rail()->setItemText(id, text);
}

void MainShell::showPage(const QString &id)
{
  int index = -1;
  if (id == QLatin1String(kHomeId))
    index = 0;
  else
    index = m_pageIndex.value(id, -1);
  if (index < 0)
    return;
  m_pages->setCurrentIndex(index);
  const auto parent = m_subPageParent.value(id);
  m_subPage = parent.isEmpty() ? QString() : id;
  m_sidebar->rail()->setCurrentPage(parent.isEmpty() ? id : parent);
}

QString MainShell::currentPageId() const
{
  return m_subPage.isEmpty() ? m_sidebar->rail()->currentPage() : m_subPage;
}

settings::RestartBanner *MainShell::restartBanner() const
{
  return m_restartBanner;
}

int MainShell::minimumWindowWidth()
{
  return px(kMinimumWindowWidth);
}

void MainShell::setHeader(QWidget *header)
{
  if (auto *layout = qobject_cast<QVBoxLayout *>(m_backdrop->layout()))
    layout->insertWidget(0, header);
}

} // namespace deskflow::gui
