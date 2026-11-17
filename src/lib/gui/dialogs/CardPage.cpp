/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CardPage.h"

#include "gui/style/BackdropWidget.h"
#include "gui/style/DesignTokens.h"
#include "gui/widgets/SettingCard.h"

#include <QBoxLayout>
#include <QDialog>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>

namespace deskflow::gui::cards {

namespace {

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

// A widget that is moved into a holder with the room that the edge of a page has around it
QWidget *held(QWidget *content, QWidget *parent, int top, int bottom)
{
  auto *holder = new QWidget(parent);
  auto *layout = new QVBoxLayout(holder);
  layout->setContentsMargins(style::metrics::spaceLg, top, style::metrics::spaceLg, bottom);
  layout->addWidget(content);
  return holder;
}

} // namespace

QScrollArea *assemblePage(
    QDialog *dialog, const QList<SettingCard *> &cards, QWidget *buttons, const QSize &oldMinimum, QWidget *header
)
{
  auto *box = qobject_cast<QBoxLayout *>(dialog->layout());
  if (!box)
    return nullptr;

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
  for (auto *card : cards)
    list->addWidget(card);
  list->addStretch(1);
  scroll->setWidget(content);
  // the scroll area fills what it holds with the colour of the window: the backdrop must show through
  content->setAutoFillBackground(false);
  scroll->viewport()->setAutoFillBackground(false);

  auto *frame = new QVBoxLayout(backdrop);
  frame->setContentsMargins(0, 0, 0, 0);
  frame->setSpacing(0);
  QWidget *headerHolder = nullptr;
  if (header) {
    headerHolder = held(header, backdrop, style::metrics::spaceLg, style::metrics::spaceSm);
    frame->addWidget(headerHolder);
  }
  frame->addWidget(scroll, 1);
  auto *footer = held(buttons, backdrop, style::metrics::spaceSm, style::metrics::spaceLg);
  frame->addWidget(footer);

  box->setContentsMargins(0, 0, 0, 0);
  box->setSpacing(0);
  box->insertWidget(0, backdrop, 1);

  // what the dialog asked for is the least that the cards are given: they scroll in it, and are never cut at the side
  const int fixedHeight =
      footer->minimumSizeHint().height() + (headerHolder ? headerHolder->minimumSizeHint().height() : 0);
  const int cardsWidth = content->minimumSizeHint().width() + 2 * scroll->frameWidth();
  scroll->setMinimumHeight(qMax(scroll->minimumSizeHint().height(), oldMinimum.height() - fixedHeight));
  scroll->setMinimumWidth(qMax(cardsWidth, oldMinimum.width()));
  return scroll;
}

} // namespace deskflow::gui::cards
