/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DeskflowStyle.h"

#include "AppearanceHints.h"
#include "Contrast.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QStyleFactory>
#include <QStyleOption>

namespace deskflow::gui::style {

namespace {

// the least height of a button or an input: rounder shapes need more room than the flat ones of Fusion
constexpr int kMinControlHeight = 30;
// room that the text of a rounded input needs, at each side, so that it does not touch the curve
constexpr int kInputPadding = 4;

bool inItemView(const QWidget *widget)
{
  return qobject_cast<const QAbstractItemView *>(widget) ||
         (widget && qobject_cast<const QAbstractItemView *>(widget->parentWidget()));
}

// the line edit inside a combo box or a spin box: the box draws the frame
bool isEmbeddedEdit(const QWidget *widget)
{
  const auto *parent = widget ? widget->parentWidget() : nullptr;
  return qobject_cast<const QAbstractSpinBox *>(parent) || qobject_cast<const QComboBox *>(parent);
}

qreal radiusFor(const QRect &rect, int wanted)
{
  return qMin<qreal>(wanted, rect.height() / 2.0);
}

// A rounded rectangle whose border of the given width lies inside the rect. The edges are put on whole device pixels (a
// screen of 125 % has 1.25 device pixels in a logical one) and the border is a whole number of them, so that a line is
// as crisp at 125 %, 150 % and 175 % as at 100 % and 200 %
void drawRounded(
    QPainter *painter, const QRect &rect, qreal radius, const QBrush &fill, const QColor &border, qreal width
)
{
  const auto dpr = painter->device()->devicePixelRatioF();
  const auto snap = [dpr](qreal logical) { return std::round(logical * dpr) / dpr; };

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  const QRectF edges(
      QPointF(snap(rect.left()), snap(rect.top())),
      QPointF(snap(rect.left() + rect.width()), snap(rect.top() + rect.height()))
  );
  const auto penWidth = border.isValid() ? std::max(1.0, std::round(width * dpr)) / dpr : 0.0;
  const auto inset = penWidth / 2;
  const auto shape = edges.adjusted(inset, inset, -inset, -inset);
  painter->setPen(border.isValid() ? QPen(border, penWidth) : QPen(Qt::NoPen));
  painter->setBrush(fill);
  const auto r = qMax<qreal>(0, radius - inset);
  painter->drawRoundedRect(shape, r, r);
  painter->restore();
}

void drawChevron(QPainter *painter, const QPointF &center, qreal halfWidth, bool down, const QColor &color)
{
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  QPen pen(color, 1.5);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter->setPen(pen);
  painter->setBrush(Qt::NoBrush);
  const auto half = halfWidth / 2;
  const auto dy = down ? half : -half;
  QPainterPath path;
  path.moveTo(center.x() - halfWidth, center.y() - dy);
  path.lineTo(center.x(), center.y() + dy);
  path.lineTo(center.x() + halfWidth, center.y() - dy);
  painter->drawPath(path);
  painter->restore();
}

} // namespace

DeskflowStyle::DeskflowStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
{
  // until Qt hands over the palette of the OS (polish) the look is the one that the OS says now
  remake(QApplication::palette());
}

const Look &DeskflowStyle::look() const
{
  return m_look;
}

const Tokens &DeskflowStyle::tokens() const
{
  return m_tokens;
}

void DeskflowStyle::useLook(const std::optional<Look> &look)
{
  m_forced = look;
  // Qt takes the palette of the OS again and has it polished: that is the palette of the look
  QApplication::setPalette(QPalette());
}

void DeskflowStyle::remake(const QPalette &osPalette)
{
  const auto before = m_look;
  const auto beforeTokens = m_tokens.colors.bg;
  m_osPalette = osPalette;
  m_look = m_forced.value_or(lookFor(readSystemHints(), osPalette));
  m_tokens = tokensFor(m_look, osPalette);
  m_palette = paletteFor(m_tokens, m_look);
  if (before != m_look || beforeTokens != m_tokens.colors.bg)
    Q_EMIT lookChanged();
}

void DeskflowStyle::polish(QPalette &palette)
{
  remake(palette);
  palette = m_palette;
}

QPalette DeskflowStyle::standardPalette() const
{
  return m_palette;
}

int DeskflowStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
  switch (metric) {
  case PM_IndicatorWidth:
    if (!inItemView(widget))
      return metrics::toggleCompactWidth;
    break;
  case PM_IndicatorHeight:
    if (!inItemView(widget))
      return metrics::toggleCompactHeight;
    break;
  default:
    break;
  }
  return QProxyStyle::pixelMetric(metric, option, widget);
}

QSize DeskflowStyle::sizeFromContents(
    ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget
) const
{
  auto size = QProxyStyle::sizeFromContents(type, option, contentsSize, widget);
  switch (type) {
  case CT_PushButton: {
    const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
    // a capsule needs the width of its curve at each end; a button with only an icon is as wide as it was
    if (button && !button->text.isEmpty())
      size.rwidth() += 2 * kInputPadding;
    size.setHeight(qMax(size.height(), kMinControlHeight));
    break;
  }
  case CT_LineEdit:
  case CT_SpinBox:
  case CT_ComboBox:
    size.rwidth() += 2 * kInputPadding;
    size.setHeight(qMax(size.height(), kMinControlHeight));
    break;
  default:
    break;
  }
  return size;
}

QRect DeskflowStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const
{
  auto rect = QProxyStyle::subElementRect(element, option, widget);
  if (element == SE_LineEditContents && !isEmbeddedEdit(widget))
    rect.adjust(kInputPadding, 0, -kInputPadding, 0);
  return rect;
}

QRect DeskflowStyle::subControlRect(
    ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget
) const
{
  auto rect = QProxyStyle::subControlRect(control, option, subControl, widget);
  if ((control == CC_ComboBox && subControl == SC_ComboBoxEditField) ||
      (control == CC_SpinBox && subControl == SC_SpinBoxEditField)) {
    rect.adjust(kInputPadding, 0, 0, 0);
  }
  return rect;
}

void DeskflowStyle::paintButton(const QStyleOptionButton *option, QPainter *painter, const QWidget *) const
{
  const auto &c = m_tokens.colors;
  const auto state = option->state;
  const bool enabled = state & State_Enabled;
  const bool hover = state & State_MouseOver;
  const bool pressed = state & State_Sunken;
  const bool checked = state & State_On;
  const bool focus = (state & State_HasFocus) && (state & State_KeyboardFocusChange);
  const bool primary = option->features & QStyleOptionButton::DefaultButton;
  const bool flat = option->features & QStyleOptionButton::Flat;

  const auto &rect = option->rect;
  painter->save();
  if (!enabled)
    painter->setOpacity(0.45);

  if (primary) {
    // away from the colour of its text: darker under white text, lighter under dark text
    const QColor away =
        relativeLuminance(c.onAccent) > relativeLuminance(c.accent) ? QColor(Qt::black) : QColor(Qt::white);
    auto fill = c.accent;
    if (pressed)
      fill = over(away, 0.20, c.accent);
    else if (hover)
      fill = over(away, 0.10, c.accent);
    drawRounded(painter, rect, rect.height() / 2.0, fill, {}, 0);
    if (focus)
      drawRounded(painter, rect.adjusted(2, 2, -2, -2), (rect.height() - 4) / 2.0, Qt::NoBrush, c.onAccent, 2);
  } else {
    const auto radius = radiusFor(rect, metrics::radiusControl);
    QColor fill = c.surface;
    QColor border = c.controlBorder;
    if (flat && !hover && !pressed && !checked) {
      fill = QColor();
      border = QColor();
    } else if (pressed) {
      fill = over(c.text, 0.10, c.accentSoft);
      border = c.accent;
    } else if (checked) {
      fill = c.accentSoft;
      border = c.accent;
    } else if (hover) {
      fill = c.accentSoft;
      border = c.accent;
    }
    if (flat && border.isValid() == false && fill.isValid() == false) {
      // nothing is drawn for a flat button that is at rest
    } else {
      drawRounded(painter, rect, radius, fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush), border, 1);
    }
    if (focus)
      drawRounded(painter, rect, radius, Qt::NoBrush, c.accent, 2);
  }
  painter->restore();
}

void DeskflowStyle::paintInput(
    const QRect &rect, State state, const QPalette &palette, QPainter *painter, const QWidget *widget
) const
{
  const auto &c = m_tokens.colors;
  const bool enabled = state & State_Enabled;
  const bool focus = state & State_HasFocus;
  const bool invalid = widget && widget->property(kInvalidProperty).toBool();

  QColor fill = palette.color(QPalette::Base);
  QColor border = c.controlBorder;
  qreal width = 1;
  if (!enabled) {
    fill = palette.color(QPalette::Window);
    border = c.textDisabled;
  } else if (invalid) {
    border = c.danger;
    width = focus ? 2 : 1;
  } else if (focus) {
    border = c.accent;
    width = 2;
  }
  drawRounded(painter, rect, radiusFor(rect, metrics::radiusControl), fill, border, width);
}

void DeskflowStyle::paintToggle(const QStyleOptionButton *option, QPainter *painter) const
{
  const auto &c = m_tokens.colors;
  const auto state = option->state;
  const bool enabled = state & State_Enabled;
  const bool on = state & State_On;
  const bool partial = state & State_NoChange;
  const bool focus = (state & State_HasFocus) && (state & State_KeyboardFocusChange);

  const QSize size(metrics::toggleCompactWidth, metrics::toggleCompactHeight);
  const QRect track(
      QPoint(option->rect.center().x() - size.width() / 2 + 1, option->rect.center().y() - size.height() / 2 + 1), size
  );

  painter->save();
  if (!enabled)
    painter->setOpacity(0.45);
  const auto radius = track.height() / 2.0;
  if (on || partial) {
    drawRounded(painter, track, radius, partial ? over(c.accent, 0.5, c.surface) : c.accent, {}, 0);
  } else {
    drawRounded(painter, track, radius, c.surface, c.controlBorder, 1.5);
  }
  if (focus)
    drawRounded(painter, track, radius, Qt::NoBrush, c.accent, 2);

  // the knob: 4 px from the edge of the track at each end, the colour of the border (off) or of what is on the accent
  const auto knob = on || partial ? track.height() - 6 : track.height() - 8;
  const auto margin = (track.height() - knob) / 2;
  const auto x = partial ? track.center().x() - knob / 2 + 1
                 : on    ? track.right() + 1 - margin - knob
                         : track.left() + margin;
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(Qt::NoPen);
  painter->setBrush(on || partial ? c.onAccent : c.controlBorder);
  painter->drawEllipse(QRectF(x, track.top() + (track.height() - knob) / 2.0, knob, knob));
  painter->restore();
}

void DeskflowStyle::drawPrimitive(
    PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget
) const
{
  switch (element) {
  case PE_FrameDefaultButton:
    // the default button is drawn as the primary one
    return;
  case PE_PanelButtonCommand:
    if (const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option)) {
      paintButton(button, painter, widget);
      return;
    }
    break;
  case PE_PanelLineEdit:
    if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
      if (frame->lineWidth == 0) {
        // without a frame: what a combo box or a spin box draws around it is behind it; elsewhere it is on the surface
        if (!isEmbeddedEdit(widget))
          painter->fillRect(option->rect, option->palette.color(QPalette::Base));
        return;
      }
      paintInput(option->rect, option->state, option->palette, painter, widget);
      return;
    }
    break;
  case PE_IndicatorCheckBox:
    if (!inItemView(widget)) {
      if (const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option)) {
        paintToggle(button, painter);
        return;
      }
    }
    break;
  default:
    break;
  }
  QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void DeskflowStyle::drawControl(
    ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget
) const
{
  if (element == CE_PushButtonLabel) {
    if (const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
        button && (button->features & QStyleOptionButton::DefaultButton)) {
      // the text on the accent: the colour that has the contrast with it
      QStyleOptionButton primary(*button);
      for (const auto group : {QPalette::Active, QPalette::Inactive})
        primary.palette.setColor(group, QPalette::ButtonText, m_tokens.colors.onAccent);
      QProxyStyle::drawControl(element, &primary, painter, widget);
      return;
    }
  }
  QProxyStyle::drawControl(element, option, painter, widget);
}

void DeskflowStyle::drawComplexControl(
    ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget
) const
{
  const auto &c = m_tokens.colors;
  if (control == CC_ComboBox) {
    if (const auto *box = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
      paintInput(box->rect, box->state, box->palette, painter, widget);
      const auto arrow = subControlRect(CC_ComboBox, box, SC_ComboBoxArrow, widget);
      painter->save();
      if (!(box->state & State_Enabled))
        painter->setOpacity(0.45);
      drawChevron(painter, arrow.center() + QPointF(0.5, 0.5), 4, true, c.text);
      painter->restore();
      return;
    }
  } else if (control == CC_SpinBox) {
    if (const auto *spin = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
      paintInput(spin->rect, spin->state, spin->palette, painter, widget);
      if (spin->buttonSymbols != QAbstractSpinBox::NoButtons) {
        const auto up = subControlRect(CC_SpinBox, spin, SC_SpinBoxUp, widget);
        const auto down = subControlRect(CC_SpinBox, spin, SC_SpinBoxDown, widget);
        const bool enabled = spin->state & State_Enabled;
        painter->save();
        painter->setOpacity(enabled && (spin->stepEnabled & QAbstractSpinBox::StepUpEnabled) ? 1.0 : 0.4);
        drawChevron(painter, up.center() + QPointF(0.5, 0.5), 3.5, false, c.text);
        painter->setOpacity(enabled && (spin->stepEnabled & QAbstractSpinBox::StepDownEnabled) ? 1.0 : 0.4);
        drawChevron(painter, down.center() + QPointF(0.5, 0.5), 3.5, true, c.text);
        painter->restore();
      }
      return;
    }
  }
  QProxyStyle::drawComplexControl(control, option, painter, widget);
}

} // namespace deskflow::gui::style
