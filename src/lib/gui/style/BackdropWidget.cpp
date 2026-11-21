/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BackdropWidget.h"

#include "Appearance.h"

#include <QEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>

namespace deskflow::gui::style {

namespace {

constexpr int kResizeSettleMs = 80;

// A soft blob: the colour at its centre, nothing at 70 % of its radius, an ellipse of the given radii (in pixels)
void paintBlob(QPainter *painter, const QPointF &center, qreal rx, qreal ry, const QColor &color)
{
  QColor edge = color;
  edge.setAlpha(0);
  painter->save();
  painter->translate(center);
  painter->scale(1.0, ry / rx);
  QRadialGradient gradient(QPointF(0, 0), rx);
  gradient.setColorAt(0.0, color);
  gradient.setColorAt(0.7, edge);
  painter->setPen(Qt::NoPen);
  painter->setBrush(gradient);
  painter->drawEllipse(QPointF(0, 0), rx, rx);
  painter->restore();
}

} // namespace

QPixmap backdropPixmap(const QSize &size, qreal dpr, const Tokens &tokens)
{
  if (size.isEmpty() || dpr <= 0)
    return {};

  QPixmap pixmap(QSize(qRound(size.width() * dpr), qRound(size.height() * dpr)));
  pixmap.setDevicePixelRatio(dpr);
  if (tokens.flatBackdrop) {
    pixmap.fill(tokens.colors.bg);
    return pixmap;
  }

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF rect(QPointF(0, 0), QSizeF(size));
  // 135 degrees: from the upper left to the lower right corner
  QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
  gradient.setColorAt(0.0, tokens.backdropStops[0]);
  gradient.setColorAt(0.55, tokens.backdropStops[1]);
  gradient.setColorAt(1.0, tokens.backdropStops[2]);
  painter.fillRect(rect, gradient);

  paintBlob(&painter, QPointF(rect.width() * 0.12, rect.height() * 0.08), 520, 380, tokens.blobUpperLeft);
  paintBlob(&painter, QPointF(rect.width() * 0.92, rect.height() * 0.94), 520, 420, tokens.blobLowerRight);
  return pixmap;
}

BackdropWidget::BackdropWidget(QWidget *parent) : QWidget(parent), m_tokens(currentTokens())
{
  // everything is painted: the widget itself is not transparent to what is behind it
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_resizeSettled.setSingleShot(true);
  m_resizeSettled.setInterval(kResizeSettleMs);
  connect(&m_resizeSettled, &QTimer::timeout, this, qOverload<>(&QWidget::update));
  watchLook();
}

int BackdropWidget::renderCount() const
{
  return m_renderCount;
}

void BackdropWidget::watchLook()
{
  QObject::disconnect(m_lookConnection);
  m_lookConnection = onLookChanged(this, [this] { invalidate(); });
}

void BackdropWidget::invalidate()
{
  m_tokens = currentTokens();
  m_picture = QPixmap();
  update();
}

void BackdropWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  switch (event->type()) {
  case QEvent::StyleChange:
    watchLook();
    invalidate();
    break;
  case QEvent::PaletteChange:
  case QEvent::ApplicationPaletteChange:
  case QEvent::ScreenChangeInternal:
  case QEvent::DevicePixelRatioChange:
    invalidate();
    break;
  default:
    break;
  }
}

void BackdropWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  if (!m_picture.isNull())
    m_resizeSettled.start();
  update();
}

void BackdropWidget::paintEvent(QPaintEvent *)
{
  const auto dpr = devicePixelRatioF();
  bool pictureMatches = !m_picture.isNull() && m_picture.deviceIndependentSize().toSize() == size() &&
                        qFuzzyCompare(m_picture.devicePixelRatio(), dpr);
  if (!pictureMatches && (m_picture.isNull() || !m_resizeSettled.isActive())) {
    m_picture = backdropPixmap(size(), dpr, m_tokens);
    ++m_renderCount;
    pictureMatches = !m_picture.isNull() && m_picture.deviceIndependentSize().toSize() == size() &&
                     qFuzzyCompare(m_picture.devicePixelRatio(), dpr);
  }
  QPainter painter(this);
  if (pictureMatches)
    painter.drawPixmap(0, 0, m_picture);
  else
    painter.drawPixmap(rect(), m_picture);
}

} // namespace deskflow::gui::style
