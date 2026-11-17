/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SearchWidget.h"

#include "gui/style/Appearance.h"
#include "gui/style/Icons.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

SearchWidget::SearchWidget(QWidget *parent)
    : QWidget{parent},
      m_btnToggle{new QPushButton(this)},
      m_btnNext{new QPushButton(this)},
      m_btnPrev{new QPushButton(this)},
      m_searchLine{new QLineEdit(this)}
{
  setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

  const auto iconSize = QSize(fontMetrics().height() - 2, fontMetrics().height() - 2);
  const auto maxBtnSize = QSize(fontMetrics().height() + 2, fontMetrics().height() + 2);

  m_btnToggle->setFixedSize(maxBtnSize);
  m_btnToggle->setCheckable(true);
  m_btnToggle->setChecked(false);
  m_btnToggle->setFlat(true);
  m_btnToggle->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::SystemSearch));
  m_btnToggle->setIconSize(iconSize);

  m_btnNext->setFixedSize(maxBtnSize);
  m_btnNext->setFlat(true);
  m_btnNext->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::GoDown));
  m_btnNext->setIconSize(iconSize);

  m_btnPrev->setFixedSize(maxBtnSize);
  m_btnPrev->setFlat(true);
  m_btnPrev->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::GoUp));
  m_btnPrev->setIconSize(iconSize);

  if (deskflow::gui::style::isDeskflowStyleInstalled()) {
    // the icons of the new look, in the size of a button that is easy to hit
    using deskflow::gui::style::icon;
    const QSize button(28, 28);
    for (auto *b : {m_btnToggle, m_btnNext, m_btnPrev}) {
      b->setFixedSize(button);
      b->setIconSize(QSize(18, 18));
    }
    m_btnToggle->setIcon(icon(QStringLiteral("magnifying-glass")));
    m_btnNext->setIcon(icon(QStringLiteral("caret-down")));
    m_btnPrev->setIcon(icon(QStringLiteral("caret-up")));
    m_searchLine->setMaximumHeight(button.height());
  } else {
    m_searchLine->setMaximumHeight(maxBtnSize.height());
  }

  connect(m_btnToggle, &QPushButton::toggled, this, &SearchWidget::toggleVisible);
  connect(m_btnNext, &QPushButton::clicked, this, &SearchWidget::next);
  connect(m_btnPrev, &QPushButton::clicked, this, &SearchWidget::previous);
  connect(m_searchLine, &QLineEdit::editingFinished, this, &SearchWidget::next);

  setText();

  auto mainLayout = new QHBoxLayout;
  mainLayout->addWidget(m_btnToggle);
  mainLayout->addWidget(m_searchLine);
  mainLayout->addWidget(m_btnNext);
  mainLayout->addWidget(m_btnPrev);

  setLayout(mainLayout);
  toggleVisible();
  adjustSize();
}

void SearchWidget::changeEvent(QEvent *e)
{
  QWidget::changeEvent(e);
  if (e->type() == QEvent::LanguageChange)
    setText();
}

void SearchWidget::toggleVisible(bool visible)
{
  m_searchLine->setVisible(visible);
  m_btnNext->setVisible(visible);
  m_btnPrev->setVisible(visible);
}

void SearchWidget::setText()
{
  m_btnToggle->setToolTip(tr("Search"));
  m_btnNext->setToolTip(tr("Find next"));
  m_btnPrev->setToolTip(tr("Find previous"));
  m_searchLine->setPlaceholderText(tr("Find..."));
  if (deskflow::gui::style::isDeskflowStyleInstalled()) {
    // a button of a picture only has no name for a screen reader but its tool tip
    m_btnToggle->setAccessibleName(m_btnToggle->toolTip());
    m_btnNext->setAccessibleName(m_btnNext->toolTip());
    m_btnPrev->setAccessibleName(m_btnPrev->toolTip());
    m_searchLine->setAccessibleName(tr("Find..."));
  }
}

void SearchWidget::next()
{
  Q_EMIT findNext(m_searchLine->text());
}

void SearchWidget::previous()
{
  Q_EMIT findPrevious(m_searchLine->text());
}
