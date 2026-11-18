/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SegmentedControl.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/Contrast.h"
#include "gui/style/LookFollower.h"
#include "gui/style/PixelSnap.h"
#include "gui/style/TextSize.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QPainter>

using namespace deskflow::gui::style;

namespace {

constexpr int kSegmentPadding = 20;
// the room at each side of a text that is cut short
constexpr int kSegmentLeast = 8;
// the characters that a text that is cut short still shows
constexpr int kSegmentLeastCharacters = 6;

} // namespace

SegmentedControlSegment::SegmentedControlSegment(QWidget *parent) : QAbstractButton(parent)
{
  installAccessibleInterfaces();
  setCheckable(true);
  setFocusPolicy(Qt::ClickFocus);
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_Hover);
  new LookFollower(this, [this] { update(); });
}

QSize SegmentedControlSegment::sizeHint() const
{
  auto font = this->font();
  font.setWeight(QFont::DemiBold);
  return QSize(
      QFontMetrics(font).horizontalAdvance(text()) + 2 * kSegmentPadding,
      SegmentedControl::controlHeight() - 2 * SegmentedControl::kPadding
  );
}

QSize SegmentedControlSegment::minimumSizeHint() const
{
  auto font = this->font();
  font.setWeight(QFont::DemiBold);
  const auto shortest = text().left(kSegmentLeastCharacters) +
                        (text().size() > kSegmentLeastCharacters ? QString(QChar(0x2026)) : QString());
  return QSize(
      qMin(QFontMetrics(font).horizontalAdvance(shortest) + 2 * kSegmentLeast, sizeHint().width()),
      SegmentedControl::controlHeight() - 2 * SegmentedControl::kPadding
  );
}

void SegmentedControlSegment::keyPressEvent(QKeyEvent *event)
{
  switch (event->key()) {
  case Qt::Key_Left:
  case Qt::Key_Right:
  case Qt::Key_Up:
  case Qt::Key_Down:
    Q_EMIT navigate(event->key());
    event->accept();
    return;
  default:
    break;
  }
  QAbstractButton::keyPressEvent(event);
}

void SegmentedControlSegment::focusInEvent(QFocusEvent *event)
{
  m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
                    event->reason() == Qt::ShortcutFocusReason;
  QAbstractButton::focusInEvent(event);
  update();
}

void SegmentedControlSegment::focusOutEvent(QFocusEvent *event)
{
  m_keyboardFocus = false;
  QAbstractButton::focusOutEvent(event);
  update();
}

void SegmentedControlSegment::paintEvent(QPaintEvent *)
{
  const auto tokens = currentTokens();
  const auto &c = tokens.colors;
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const auto dpr = painter.device()->devicePixelRatioF();
  const QRectF body = snapRect(QRectF(rect()), dpr);
  const bool hover = underMouse() && isEnabled() && !isChecked();

  if (!isEnabled())
    painter.setOpacity(0.45);
  painter.setPen(Qt::NoPen);
  if (isChecked()) {
    painter.setBrush(c.accent);
    painter.drawRoundedRect(body, body.height() / 2, body.height() / 2);
  } else if (hover) {
    auto fill = c.text;
    fill.setAlphaF(0.08);
    painter.setBrush(fill);
    painter.drawRoundedRect(body, body.height() / 2, body.height() / 2);
  }

  auto font = this->font();
  font.setWeight(QFont::DemiBold);
  painter.setFont(font);
  painter.setPen(isChecked() ? c.onAccent : c.text);
  // a control that is narrower than its text shows the beginning of it (the name is told in full to a screen reader)
  const auto room = qMax(0, width() - 2 * kSegmentLeast);
  painter.drawText(rect(), Qt::AlignCenter, QFontMetrics(font).elidedText(text(), Qt::ElideRight, room));

  if (m_keyboardFocus) {
    const auto ring = deviceLineWidth(metrics::focusRingWidth, dpr);
    // the ring is inside the capsule of the control, which has the room of its padding around the segment
    const auto outer = body.adjusted(-1, -1, 1, 1).adjusted(ring / 2, ring / 2, -ring / 2, -ring / 2);
    painter.setOpacity(1.0);
    painter.setPen(QPen(isChecked() ? c.onAccent : c.accent, ring));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(outer, outer.height() / 2, outer.height() / 2);
  }
}

// ---------------------------------------------------------------------------------------------------------------------

SegmentedControl::SegmentedControl(QWidget *parent) : QWidget(parent)
{
  installAccessibleInterfaces();
  // as wide as its texts, and narrower when there is not the room (the texts are cut short then)
  setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  new LookFollower(this, [this] { update(); });
}

int SegmentedControl::addSegment(const QString &text, const QString &description)
{
  auto *segment = new SegmentedControlSegment(this);
  segment->setText(text);
  segment->setAccessibleName(text);
  segment->setAccessibleDescription(description);
  segment->setToolTip(description);
  const int index = int(m_segments.size());
  m_segments.append(segment);
  segment->show();
  connect(segment, &QAbstractButton::clicked, this, [this, segment] {
    const int chosen = int(m_segments.indexOf(segment));
    // a click on the choice that is chosen already keeps it chosen
    setCurrent(chosen);
  });
  connect(segment, &SegmentedControlSegment::navigate, this, [this](int key) { navigate(key); });
  if (m_segments.size() == 1)
    updateFocusStop();
  updateGeometry();
  relayout();
  return index;
}

int SegmentedControl::count() const
{
  return int(m_segments.size());
}

SegmentedControlSegment *SegmentedControl::segment(int index) const
{
  return index >= 0 && index < m_segments.size() ? m_segments[index] : nullptr;
}

QString SegmentedControl::segmentText(int index) const
{
  return m_segments.value(index) ? m_segments.value(index)->text() : QString();
}

void SegmentedControl::setSegmentText(int index, const QString &text)
{
  if (auto *s = segment(index)) {
    s->setText(text);
    s->setAccessibleName(text);
    updateGeometry();
    relayout();
  }
}

void SegmentedControl::setSegmentDescription(int index, const QString &description)
{
  if (auto *s = segment(index)) {
    s->setAccessibleDescription(description);
    s->setToolTip(description);
  }
}

int SegmentedControl::current() const
{
  return m_current;
}

void SegmentedControl::setCurrent(int index)
{
  if (index < 0 || index >= m_segments.size())
    return;
  const bool changed = m_current != index;
  m_current = index;
  // a click toggles the segment that was pressed: the state of all of them is made again from the one that is chosen
  for (int i = 0; i < m_segments.size(); ++i)
    m_segments[i]->setChecked(i == index);
  updateFocusStop();
  if (changed)
    Q_EMIT currentChanged(index);
}

void SegmentedControl::updateFocusStop()
{
  const int stop = current() >= 0 ? current() : 0;
  for (int i = 0; i < m_segments.size(); ++i)
    m_segments[i]->setFocusPolicy(i == stop ? Qt::StrongFocus : Qt::ClickFocus);
}

void SegmentedControl::navigate(int key)
{
  if (m_segments.isEmpty())
    return;
  int index = current() >= 0 ? current() : 0;
  index += (key == Qt::Key_Left || key == Qt::Key_Up) ? -1 : 1;
  index = qBound(0, index, int(m_segments.size()) - 1);
  setCurrent(index);
  m_segments[index]->setFocus(Qt::TabFocusReason);
}

int SegmentedControl::controlHeight()
{
  return px(kHeight);
}

QSize SegmentedControl::sizeHint() const
{
  int width = 2 * kPadding;
  for (const auto *s : m_segments)
    width += s->sizeHint().width();
  return QSize(width, controlHeight());
}

QSize SegmentedControl::minimumSizeHint() const
{
  int width = 2 * kPadding;
  for (const auto *s : m_segments)
    width += s->minimumSizeHint().width();
  return QSize(width, controlHeight());
}

void SegmentedControl::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  relayout();
}

void SegmentedControl::relayout()
{
  // the segments share the width in the proportion of their text, and fill the capsule
  const int inner = width() - 2 * kPadding;
  int total = 0;
  for (const auto *s : m_segments)
    total += s->sizeHint().width();
  if (total <= 0)
    return;
  int x = kPadding;
  for (int i = 0; i < m_segments.size(); ++i) {
    const int w =
        i + 1 == m_segments.size() ? width() - kPadding - x : inner * m_segments[i]->sizeHint().width() / total;
    m_segments[i]->setGeometry(x, kPadding, w, height() - 2 * kPadding);
    x += w;
  }
}

void SegmentedControl::paintEvent(QPaintEvent *)
{
  const auto tokens = currentTokens();
  const auto &c = tokens.colors;
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const auto dpr = painter.device()->devicePixelRatioF();
  const auto line = deviceLineWidth(1, dpr);
  const QRectF body = snapRect(QRectF(rect()), dpr).adjusted(line / 2, line / 2, -line / 2, -line / 2);
  painter.setPen(QPen(c.controlBorder, line));
  painter.setBrush(c.surface);
  painter.drawRoundedRect(body, body.height() / 2, body.height() / 2);
}
