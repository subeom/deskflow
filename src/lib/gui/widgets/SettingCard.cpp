/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingCard.h"

#include "SectionHeader.h"
#include "SettingRow.h"
#include "gui/style/AccessibleInterfaces.h"

#include <QEvent>
#include <QVBoxLayout>

using namespace deskflow::gui::style;

namespace {

constexpr int kHeaderTop = 14;
constexpr int kHeaderBottom = 4;

} // namespace

SettingCard::SettingCard(const QString &title, QWidget *parent)
    : PanelWidget(Density::Dense, parent),
      m_layout(new QVBoxLayout(this))
{
  installAccessibleInterfaces();
  // the rows go to the edge of the panel and carry their own padding; the room of the shadow is the margin of the panel
  setPadding(0);
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);

  if (!title.isEmpty()) {
    m_header = new SectionHeader(title, this);
    // the header lines up with the text of the rows, and it has a little air above and below
    auto *box = new QWidget(this);
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(metrics::spaceXl, kHeaderTop, metrics::spaceXl, kHeaderBottom);
    boxLayout->addWidget(m_header);
    m_layout->addWidget(box);
    setAccessibleName(m_header->title());
  }
}

SectionHeader *SettingCard::header() const
{
  return m_header;
}

QString SettingCard::title() const
{
  return m_header ? m_header->title() : QString();
}

void SettingCard::addRow(SettingRow *row)
{
  m_rows.append(row);
  m_layout->addWidget(row);
  row->installEventFilter(this);
  refreshDividers();
}

QList<SettingRow *> SettingCard::rows() const
{
  return m_rows;
}

void SettingCard::clearRows()
{
  for (auto *row : std::as_const(m_rows)) {
    m_layout->removeWidget(row);
    row->hide();
    row->deleteLater();
  }
  m_rows.clear();
  // a card without rows has nothing that hid it
  if (m_hiddenByRows) {
    m_hiddenByRows = false;
    show();
  }
}

void SettingCard::refreshDividers()
{
  bool first = true;
  bool any = false;
  for (auto *row : std::as_const(m_rows)) {
    if (row->isHidden()) {
      row->setDivider(false);
      continue;
    }
    row->setDivider(!first);
    first = false;
    any = true;
  }
  // a card whose rows are all hidden has nothing to show (a header alone says nothing); only a card that this hid is
  // shown again
  if (m_rows.isEmpty())
    return;
  if (!any && !m_hiddenByRows) {
    m_hiddenByRows = true;
    hide();
  } else if (any && m_hiddenByRows) {
    m_hiddenByRows = false;
    show();
  }
}

bool SettingCard::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::ShowToParent || event->type() == QEvent::HideToParent) {
    if (auto *row = qobject_cast<SettingRow *>(watched); row && m_rows.contains(row))
      refreshDividers();
  }
  return PanelWidget::eventFilter(watched, event);
}
