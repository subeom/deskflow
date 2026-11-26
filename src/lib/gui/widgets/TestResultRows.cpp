/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/widgets/TestResultRows.h"

#include "gui/style/DesignTokens.h"
#include "gui/widgets/InlineHint.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"

#include <QCoreApplication>
#include <QPushButton>
#include <QVBoxLayout>

namespace deskflow::gui {

using namespace deskflow::gui::style;

TestResultRows::TestResultRows(QWidget *parent)
    : QWidget(parent),
      m_intro(new InlineHint(this)),
      m_conclusion(new InlineHint(this)),
      m_list(new QVBoxLayout)
{
  setObjectName(QStringLiteral("testResultRows"));
  m_intro->setObjectName(QStringLiteral("testIntro"));
  m_conclusion->setObjectName(QStringLiteral("testConclusion"));
  m_intro->hide();
  m_conclusion->hide();

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(metrics::spaceMd);
  layout->addWidget(m_intro);
  m_list->setContentsMargins(0, 0, 0, 0);
  m_list->setSpacing(metrics::spaceMd);
  layout->addLayout(m_list);
  layout->addWidget(m_conclusion);
  layout->addStretch(1);
}

void TestResultRows::setIntro(const QString &text)
{
  m_intro->setText(text);
  m_intro->setVisible(!text.isEmpty());
}

void TestResultRows::setConclusion(const QString &text, bool problem)
{
  m_conclusion->setText(text);
  m_conclusion->setDanger(problem);
  m_conclusion->setVisible(!text.isEmpty());
}

void TestResultRows::setRows(const QList<Row> &rows)
{
  if (rows == m_rows)
    return;
  // what the person opened stays open while the test goes on (a row that has the same title and the same hints)
  QList<bool> carried;
  for (const auto &row : rows) {
    bool wasOpen = false;
    for (int i = 0; i < m_rows.size(); ++i) {
      if (m_rows[i].title == row.title && m_rows[i].hints == row.hints)
        wasOpen = m_open.value(i);
    }
    carried << wasOpen;
  }
  m_rows = rows;
  m_open = carried;
  rebuild();
}

void TestResultRows::rebuild()
{
  for (auto *card : std::as_const(m_cards)) {
    m_list->removeWidget(card);
    card->deleteLater();
  }
  m_cards.clear();
  m_buttons.clear();
  m_blocks.clear();

  for (int i = 0; i < m_rows.size(); ++i) {
    const auto &data = m_rows[i];
    auto *card = new SettingCard({}, this);
    card->setObjectName(QStringLiteral("testResultCard"));
    auto *row = new SettingRow(data.title, data.detail);
    row->setObjectName(QStringLiteral("testResultRow"));
    auto *chip = new StatusChip(data.state, data.kind);
    chip->setObjectName(QStringLiteral("testResultChip"));
    row->addControl(chip);

    QPushButton *toggle = nullptr;
    QWidget *block = nullptr;
    if (!data.hints.isEmpty()) {
      toggle = new QPushButton(QCoreApplication::translate("TestResultRows", "What to try"));
      toggle->setObjectName(QStringLiteral("btnHints"));
      toggle->setCheckable(true);
      toggle->setChecked(m_open[i]);
      toggle->setFlat(true);
      row->addControl(toggle);
    }
    card->addRow(row);
    if (toggle) {
      block = new QWidget;
      block->setObjectName(QStringLiteral("testHints"));
      auto *blockLayout = new QVBoxLayout(block);
      blockLayout->setContentsMargins(0, 0, 0, 0);
      blockLayout->setSpacing(metrics::spaceXs);
      for (const auto &hint : data.hints) {
        auto *line = new InlineHint(hint);
        line->setObjectName(QStringLiteral("testHint"));
        blockLayout->addWidget(line);
      }
      block->setVisible(m_open[i]);
      card->addContent(block);
      connect(toggle, &QPushButton::toggled, block, &QWidget::setVisible);
      connect(toggle, &QPushButton::toggled, this, [this, i](bool checked) {
        if (i < m_open.size())
          m_open[i] = checked;
      });
    }
    m_list->addWidget(card);
    m_cards << card;
    m_buttons << toggle;
    m_blocks << block;
  }
}

QList<TestResultRows::Row> TestResultRows::rows() const
{
  return m_rows;
}

int TestResultRows::rowCount() const
{
  return int(m_rows.size());
}

SettingCard *TestResultRows::card(int index) const
{
  return m_cards.value(index);
}

QPushButton *TestResultRows::hintsButton(int index) const
{
  return m_buttons.value(index);
}

bool TestResultRows::hintsShown(int index) const
{
  const auto *block = m_blocks.value(index);
  return block != nullptr && !block->isHidden();
}

QString TestResultRows::intro() const
{
  return m_intro->text();
}

QString TestResultRows::conclusion() const
{
  return m_conclusion->text();
}

} // namespace deskflow::gui
