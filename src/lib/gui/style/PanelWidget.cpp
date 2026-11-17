/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "PanelWidget.h"

#include "Appearance.h"

#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

#include <algorithm>
#include <cmath>
#include <vector>

namespace deskflow::gui::style {

namespace {

// One pass of a box blur over one line of 8 bit values: the window of 2 * radius + 1 values around each of them
void boxBlurLine(const uint8_t *in, uint8_t *out, int count, int stride, int radius)
{
  const int window = 2 * radius + 1;
  int sum = 0;
  for (int i = -radius; i <= radius; ++i)
    sum += in[std::clamp(i, 0, count - 1) * stride];
  for (int i = 0; i < count; ++i) {
    out[i * stride] = static_cast<uint8_t>(sum / window);
    sum += in[std::clamp(i + radius + 1, 0, count - 1) * stride] - in[std::clamp(i - radius, 0, count - 1) * stride];
  }
}

// A gaussian blur as three box blurs (the error is well under what an 8 bit shadow can show) of the alpha values
void gaussianBlur(std::vector<uint8_t> &alpha, int width, int height, qreal sigma)
{
  // the width of three boxes that make a gaussian of the sigma
  const auto ideal = std::sqrt(12.0 * sigma * sigma / 3 + 1);
  const int radius = std::max(1, static_cast<int>(std::floor((ideal - 1) / 2)));
  std::vector<uint8_t> scratch(alpha.size());
  for (int pass = 0; pass < 3; ++pass) {
    for (int y = 0; y < height; ++y)
      boxBlurLine(&alpha[y * width], &scratch[y * width], width, 1, radius);
    for (int x = 0; x < width; ++x)
      boxBlurLine(&scratch[x], &alpha[x], height, width, radius);
  }
}

QRect pixelRect(const QRectF &logical, qreal dpr)
{
  return QRectF(logical.x() * dpr, logical.y() * dpr, logical.width() * dpr, logical.height() * dpr).toRect();
}

} // namespace

QPixmap shadowPatch(int radius, int blur, const QColor &color, qreal dpr)
{
  const auto key =
      QStringLiteral("deskflow/shadow/%1/%2/%3/%4").arg(radius).arg(blur).arg(color.name(QColor::HexArgb)).arg(dpr);
  QPixmap patch;
  if (QPixmapCache::find(key, &patch))
    return patch;

  // The corner (the curve and all the blur of it) and two pixels of what is stretched between two corners. The shape is
  // large enough that the middle of an edge is as dark as the edge of a panel of any size: the blur of the opposite
  // edge does not reach it
  const int corner = radius + 2 * blur;
  const int side = 2 * corner + 2;
  const int pixels = qRound(side * dpr);

  QImage shape(pixels, pixels, QImage::Format_Grayscale8);
  shape.fill(0);
  {
    QPainter painter(&shape);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    const QRectF body(blur * dpr, blur * dpr, pixels - 2 * blur * dpr, pixels - 2 * blur * dpr);
    painter.drawRoundedRect(body, radius * dpr, radius * dpr);
  }

  std::vector<uint8_t> alpha(static_cast<size_t>(pixels) * pixels);
  for (int y = 0; y < pixels; ++y)
    std::copy_n(shape.constScanLine(y), pixels, alpha.begin() + static_cast<ptrdiff_t>(y) * pixels);
  // a blur radius of 20 px in CSS is a gaussian of a sigma of 10
  gaussianBlur(alpha, pixels, pixels, blur * dpr / 2.0);

  QImage image(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < pixels; ++y) {
    auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
    for (int x = 0; x < pixels; ++x) {
      QColor pixel = color;
      pixel.setAlphaF(color.alphaF() * alpha[static_cast<size_t>(y) * pixels + x] / 255.0);
      line[x] = qPremultiply(pixel.rgba());
    }
  }
  patch = QPixmap::fromImage(image);
  patch.setDevicePixelRatio(dpr);
  QPixmapCache::insert(key, patch);
  return patch;
}

void paintShadow(QPainter *painter, const QRect &rect, int radius, int offsetY, const QColor &color, qreal dpr)
{
  const int blur = metrics::shadowBlur;
  const auto patch = shadowPatch(radius, blur, color, dpr);
  const int corner = radius + 2 * blur;
  // the shadow is the shape of the panel, lower by the offset, and the blur around it
  const QRect area = rect.translated(0, offsetY).adjusted(-blur, -blur, blur, blur);
  if (area.width() < 2 * corner || area.height() < 2 * corner) {
    painter->drawPixmap(area, patch);
    return;
  }

  const int innerWidth = area.width() - 2 * corner;
  const int innerHeight = area.height() - 2 * corner;
  const int middle = patch.width() / patch.devicePixelRatio() - 2 * corner;
  const qreal xs[] = {0.0, static_cast<qreal>(corner), static_cast<qreal>(corner + middle)};
  const qreal widths[] = {static_cast<qreal>(corner), static_cast<qreal>(middle), static_cast<qreal>(corner)};
  const qreal tx[] = {
      static_cast<qreal>(area.left()), static_cast<qreal>(area.left() + corner),
      static_cast<qreal>(area.left() + corner + innerWidth)
  };
  const qreal tw[] = {static_cast<qreal>(corner), static_cast<qreal>(innerWidth), static_cast<qreal>(corner)};
  const qreal ty[] = {
      static_cast<qreal>(area.top()), static_cast<qreal>(area.top() + corner),
      static_cast<qreal>(area.top() + corner + innerHeight)
  };
  const qreal th[] = {static_cast<qreal>(corner), static_cast<qreal>(innerHeight), static_cast<qreal>(corner)};

  painter->save();
  painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      const QRectF source(xs[column], xs[row], widths[column], widths[row]);
      painter->drawPixmap(QRectF(tx[column], ty[row], tw[column], th[row]), patch, pixelRect(source, dpr));
    }
  }
  painter->restore();
}

PanelWidget::PanelWidget(QWidget *parent) : PanelWidget(Density::Normal, parent)
{
}

PanelWidget::PanelWidget(Density density, QWidget *parent)
    : QWidget(parent),
      m_density(density),
      m_tokens(currentTokens())
{
  updateMargins();
  watchLook();
}

void PanelWidget::watchLook()
{
  QObject::disconnect(m_lookConnection);
  m_lookConnection = onLookChanged(this, [this] {
    m_tokens = currentTokens();
    update();
  });
}

PanelWidget::Density PanelWidget::density() const
{
  return m_density;
}

void PanelWidget::setDensity(Density density)
{
  if (m_density == density)
    return;
  m_density = density;
  update();
}

bool PanelWidget::isElevated() const
{
  return m_elevated;
}

void PanelWidget::setElevated(bool elevated)
{
  if (m_elevated == elevated)
    return;
  m_elevated = elevated;
  updateMargins();
  update();
}

int PanelWidget::padding() const
{
  return m_padding;
}

void PanelWidget::setPadding(int padding)
{
  if (m_padding == padding)
    return;
  m_padding = padding;
  updateMargins();
}

QMargins PanelWidget::shadowMargins()
{
  return {10, 6, 10, 16};
}

QRect PanelWidget::panelRect() const
{
  return m_elevated ? rect().marginsRemoved(shadowMargins()) : rect();
}

void PanelWidget::updateMargins()
{
  const auto room = m_elevated ? shadowMargins() : QMargins();
  setContentsMargins(
      room.left() + m_padding, room.top() + m_padding, room.right() + m_padding, room.bottom() + m_padding
  );
}

void PanelWidget::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  switch (event->type()) {
  case QEvent::StyleChange:
    watchLook();
    m_tokens = currentTokens();
    update();
    break;
  case QEvent::PaletteChange:
  case QEvent::ApplicationPaletteChange:
    m_tokens = currentTokens();
    update();
    break;
  default:
    break;
  }
}

void PanelWidget::paintEvent(QPaintEvent *)
{
  const auto &c = m_tokens.colors;
  const auto body = panelRect();
  const qreal radius = qMin<qreal>(metrics::radiusCard, body.height() / 2.0);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  if (m_elevated && !m_tokens.flatBackdrop) {
    // the shadow is only around the panel: below a translucent panel it would darken the panel
    QPainterPath outside;
    outside.addRect(rect());
    outside.addRoundedRect(body, radius, radius);
    painter.setClipPath(outside);
    paintShadow(&painter, body, metrics::radiusCard, metrics::shadowOffsetY, c.shadow, devicePixelRatioF());
    painter.setClipping(false);
  }

  QColor fill = c.surface;
  fill.setAlphaF(m_density == Density::Dense ? m_tokens.panelDenseOpacity : m_tokens.panelOpacity);
  painter.setPen(Qt::NoPen);
  painter.setBrush(fill);
  painter.drawRoundedRect(body, radius, radius);

  // the edge of the panel, 1 px, and a second line of light along its upper edge
  const auto edge = QRectF(body).adjusted(0.5, 0.5, -0.5, -0.5);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(c.panelEdge, 1));
  painter.drawRoundedRect(edge, radius - 0.5, radius - 0.5);
  if (!m_tokens.flatBackdrop) {
    painter.save();
    painter.setClipRect(QRect(body.left(), body.top(), body.width(), static_cast<int>(radius)));
    painter.drawRoundedRect(edge.adjusted(1, 1, -1, -1), radius - 1.5, radius - 1.5);
    painter.restore();
  }
}

} // namespace deskflow::gui::style
