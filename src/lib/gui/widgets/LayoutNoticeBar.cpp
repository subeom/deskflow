/*
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "LayoutNoticeBar.h"

#include "gui/layout/LayoutMemoryService.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>

LayoutNoticeBar::LayoutNoticeBar(QWidget *parent)
    : QFrame(parent),
      m_label(new QLabel(this)),
      m_undo(new QPushButton(tr("Undo"), this)),
      m_dismiss(new QPushButton(tr("Dismiss"), this))
{
  setObjectName(QStringLiteral("layoutNoticeBar"));
  m_label->setObjectName(QStringLiteral("lblLayoutNotice"));
  m_undo->setObjectName(QStringLiteral("btnLayoutNoticeUndo"));
  m_dismiss->setObjectName(QStringLiteral("btnLayoutNoticeDismiss"));
  setFrameShape(QFrame::StyledPanel);

  // a line that does not wrap: a window does not grow its height for wrapped text, so it would be squeezed
  m_label->setWordWrap(false);
  m_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_undo->setToolTip(tr("Put the computers back where they were before the change."));
  m_dismiss->setToolTip(tr("Hide this. The change is kept, and cannot be undone from here any more."));

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(6, 2, 6, 2);
  layout->addWidget(m_label, 1);
  layout->addWidget(m_undo);
  layout->addWidget(m_dismiss);

  connect(m_undo, &QPushButton::clicked, this, &LayoutNoticeBar::undoRequested);
  connect(m_dismiss, &QPushButton::clicked, this, &LayoutNoticeBar::dismissRequested);
  hide();
}

void LayoutNoticeBar::setNotice(const std::optional<deskflow::gui::layout::Notice> &notice)
{
  if (!notice) {
    m_text.clear();
    hide();
    return;
  }
  setText(deskflow::gui::layout::LayoutMemoryService::noticeText(*notice), notice->canUndo);
}

void LayoutNoticeBar::setText(const QString &text, bool canUndo)
{
  m_text = text;
  m_undo->setVisible(canUndo);
  setToolTip(text);
  show();
  elide();
}

void LayoutNoticeBar::elide()
{
  const auto width = std::max(m_label->width(), 0);
  m_label->setText(m_label->fontMetrics().elidedText(m_text, Qt::ElideRight, width));
}

void LayoutNoticeBar::resizeEvent(QResizeEvent *event)
{
  QFrame::resizeEvent(event);
  elide();
}
