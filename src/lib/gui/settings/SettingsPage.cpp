/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsPage.h"

#include "gui/style/DesignTokens.h"
#include "gui/style/PanelWidget.h"
#include "gui/widgets/PageTitle.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"

#include <QEvent>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace deskflow::gui::style;

namespace deskflow::gui::settings {

SettingsPage::SettingsPage(const QString &id, std::function<QString()> title, QWidget *parent)
    : QWidget(parent),
      m_id(id),
      m_titleText(std::move(title))
{
  setObjectName(QStringLiteral("settingsPage_%1").arg(id));
  const auto room = PanelWidget::shadowMargins();

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_title = new PageTitle(this);
  m_title->setObjectName(QStringLiteral("pageTitle"));
  auto *titleBox = new QWidget(this);
  auto *titleLayout = new QVBoxLayout(titleBox);
  titleLayout->setContentsMargins(room.left(), room.top() + 6, room.right(), 8);
  titleLayout->addWidget(m_title);
  layout->addWidget(titleBox);

  m_scroll = new QScrollArea(this);
  m_scroll->setObjectName(QStringLiteral("pageScroll"));
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto *content = new QWidget;
  content->setObjectName(QStringLiteral("pageContent"));
  m_cards = new QVBoxLayout(content);
  // the cards carry the room of their shadow: what is left to the edge is the little that the list adds
  m_cards->setContentsMargins(room.left(), metrics::spaceXs, room.right(), metrics::spaceXs);
  m_cards->setSpacing(0);
  m_cards->addStretch(1);
  m_scroll->setWidget(content);
  // the backdrop of the window shows through: the scroll area fills nothing
  content->setAutoFillBackground(false);
  m_scroll->viewport()->setAutoFillBackground(false);
  layout->addWidget(m_scroll, 1);

  follow([this] { m_title->setText(m_titleText()); });
}

QString SettingsPage::pageId() const
{
  return m_id;
}

PageTitle *SettingsPage::titleLabel() const
{
  return m_title;
}

QString SettingsPage::title() const
{
  return m_title->text();
}

QVBoxLayout *SettingsPage::cardList() const
{
  return m_cards;
}

SettingCard *SettingsPage::addCard(std::function<QString()> title)
{
  // the card has a header when it is made with a title
  auto *card = new SettingCard(title());
  addCard(card);
  follow([card, title = std::move(title)] {
    if (card->header())
      card->header()->setTitle(title());
  });
  return card;
}

void SettingsPage::addCard(SettingCard *card)
{
  // before the stretch that keeps the cards at the top
  m_cards->insertWidget(m_cards->count() - 1, card);
}

SettingRow *
SettingsPage::addRow(SettingCard *card, std::function<QString()> title, std::function<QString()> description)
{
  auto *row = new SettingRow;
  card->addRow(row);
  follow([row, title = std::move(title), description = std::move(description)] {
    row->setTitle(title());
    row->setDescription(description ? description() : QString());
  });
  return row;
}

void SettingsPage::follow(std::function<void()> apply)
{
  apply();
  m_followers.append(std::move(apply));
}

QList<SettingCard *> SettingsPage::cards() const
{
  QList<SettingCard *> cards;
  for (int i = 0; i < m_cards->count(); ++i) {
    if (auto *card = qobject_cast<SettingCard *>(m_cards->itemAt(i)->widget()))
      cards.append(card);
  }
  return cards;
}

void SettingsPage::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  Q_EMIT shown();
}

void SettingsPage::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    for (const auto &apply : std::as_const(m_followers))
      apply();
  }
}

} // namespace deskflow::gui::settings
