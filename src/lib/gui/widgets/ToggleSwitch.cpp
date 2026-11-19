/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ToggleSwitch.h"
#include "gui/style/TextSize.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/Contrast.h"
#include "gui/style/LookFollower.h"
#include "gui/style/PixelSnap.h"

#include <QEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QStyle>

using namespace deskflow::gui::style;

namespace {

// room around the track for the ring of the keyboard focus
constexpr int kRing = 3;
constexpr int kTextGap = 8;

QColor blend(const QColor &from, const QColor &to, qreal amount)
{
  return over(to, amount, from);
}

} // namespace

ToggleSwitch::ToggleSwitch(QWidget *parent) : QCheckBox(parent)
{
  installAccessibleInterfaces();
  m_animation.setEasingCurve(QEasingCurve::OutCubic);
  connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
    m_knob = value.toReal();
    update();
  });
  m_knob = m_target = targetOf(checkState());
  // setCheckState() reaches neither checkStateSet() nor nextCheckState() (see the header)
  connect(this, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState) { syncKnob(); });
  new LookFollower(this, [this] { update(); });
}

ToggleSwitch::ToggleSwitch(const QString &text, QWidget *parent) : ToggleSwitch(parent)
{
  setText(text);
}

QSize ToggleSwitch::trackSize(bool withText)
{
  return withText ? QSize(px(metrics::toggleCompactWidth), px(metrics::toggleCompactHeight))
                  : QSize(px(metrics::toggleWidth), px(metrics::toggleHeight));
}

qreal ToggleSwitch::knobPosition() const
{
  return m_knob;
}

qreal ToggleSwitch::targetOf(Qt::CheckState state)
{
  switch (state) {
  case Qt::Checked:
    return 1.0;
  case Qt::PartiallyChecked:
    return 0.5;
  case Qt::Unchecked:
    break;
  }
  return 0.0;
}

QSize ToggleSwitch::switchArea() const
{
  const auto track = trackSize(!text().isEmpty());
  return QSize(track.width() + 2 * kRing, track.height() + 2 * kRing);
}

QSize ToggleSwitch::sizeHint() const
{
  if (!isDeskflowStyleInstalled())
    return QCheckBox::sizeHint();

  auto area = switchArea();
  if (!text().isEmpty()) {
    const auto label = fontMetrics().size(Qt::TextShowMnemonic, text());
    area.setWidth(area.width() + kTextGap + label.width());
    area.setHeight(qMax(area.height(), label.height()));
  }
  return area;
}

QSize ToggleSwitch::minimumSizeHint() const
{
  return isDeskflowStyleInstalled() ? sizeHint() : QCheckBox::minimumSizeHint();
}

bool ToggleSwitch::hitButton(const QPoint &pos) const
{
  return isDeskflowStyleInstalled() ? rect().contains(pos) : QCheckBox::hitButton(pos);
}

void ToggleSwitch::moveKnobTo(qreal target)
{
  const auto duration = motionDuration(currentLook(), metrics::motionHover);
  m_animation.stop();
  if (!isDeskflowStyleInstalled() || !isVisible() || duration == 0) {
    m_knob = target;
    update();
    return;
  }
  m_animation.setDuration(duration);
  m_animation.setStartValue(m_knob);
  m_animation.setEndValue(target);
  m_animation.start();
}

void ToggleSwitch::syncKnob()
{
  const auto target = targetOf(checkState());
  if (target == m_target)
    return;
  m_target = target;
  moveKnobTo(target);
}

void ToggleSwitch::checkStateSet()
{
  QCheckBox::checkStateSet();
  syncKnob();
}

// A click of the mouse, the key Space and a group that lets go of the button that was chosen before come here, and Qt
// does not call checkStateSet() for them: the knob stood still when the switch was clicked
void ToggleSwitch::nextCheckState()
{
  QCheckBox::nextCheckState();
  syncKnob();
}

void ToggleSwitch::focusInEvent(QFocusEvent *event)
{
  m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
                    event->reason() == Qt::ShortcutFocusReason;
  QCheckBox::focusInEvent(event);
  update();
}

void ToggleSwitch::focusOutEvent(QFocusEvent *event)
{
  m_keyboardFocus = false;
  QCheckBox::focusOutEvent(event);
  update();
}

void ToggleSwitch::changeEvent(QEvent *event)
{
  QCheckBox::changeEvent(event);
  switch (event->type()) {
  case QEvent::StyleChange:
    m_animation.stop();
    m_knob = m_target = targetOf(checkState());
    updateGeometry();
    update();
    break;
  case QEvent::EnabledChange:
    update();
    break;
  default:
    break;
  }
}

void ToggleSwitch::paintEvent(QPaintEvent *event)
{
  if (!isDeskflowStyleInstalled()) {
    QCheckBox::paintEvent(event);
    return;
  }

  const auto &c = currentTokens().colors;
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const auto dpr = painter.device()->devicePixelRatioF();
  const bool withText = !text().isEmpty();
  const auto size = trackSize(withText);
  const QRectF track = snapRect(QRectF(kRing, (height() - size.height()) / 2.0, size.width(), size.height()), dpr);
  const bool hover = underMouse() && isEnabled();

  painter.save();
  if (!isEnabled())
    painter.setOpacity(0.45);

  // the track: a surface with a border (off) that fills with the accent (on)
  const auto line = deviceLineWidth(1.5, dpr);
  const auto fill = blend(c.surface, c.accent, m_knob);
  const auto border = blend(hover ? c.accent : c.controlBorder, c.accent, m_knob);
  const auto shape = track.adjusted(line / 2, line / 2, -line / 2, -line / 2);
  painter.setPen(QPen(border, line));
  painter.setBrush(fill);
  painter.drawRoundedRect(shape, shape.height() / 2, shape.height() / 2);

  // the knob: the colour of the border (off) or of what is on the accent (on)
  const auto diameter = withText ? 14.0 : 19.0;
  const auto margin = (track.height() - diameter) / 2;
  const auto travel = track.width() - 2 * margin - diameter;
  painter.setPen(Qt::NoPen);
  painter.setBrush(blend(c.controlBorder, c.onAccent, m_knob));
  painter.drawEllipse(QRectF(track.left() + margin + m_knob * travel, track.top() + margin, diameter, diameter));
  painter.restore();

  if (m_keyboardFocus) {
    const auto ring = deviceLineWidth(2, dpr);
    const auto outer = track.adjusted(-2, -2, 2, 2);
    painter.setPen(QPen(c.accent, ring));
    painter.setBrush(Qt::NoBrush);
    const auto shapeOfRing = outer.adjusted(ring / 2, ring / 2, -ring / 2, -ring / 2);
    painter.drawRoundedRect(shapeOfRing, shapeOfRing.height() / 2, shapeOfRing.height() / 2);
  }

  if (withText) {
    const QRect label(kRing + size.width() + kTextGap, 0, width() - (kRing + size.width() + kTextGap), height());
    style()->drawItemText(
        &painter, label, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextShowMnemonic, palette(), isEnabled(), text(),
        QPalette::WindowText
    );
  }
}
