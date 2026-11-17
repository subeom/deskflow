/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CanvasTheme.h"

#include "Appearance.h"
#include "Contrast.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <initializer_list>

namespace deskflow::gui::style {

namespace {

// The colour, moved as little as it takes to have the contrast against every one of the colours that it is seen on
QColor seenOn(QColor color, std::initializer_list<QColor> behind, double minimum = 3.0)
{
  for (const auto &under : behind)
    color = ensureContrast(color, under, minimum);
  // moving it for the last colour may have taken the contrast from an earlier one: a second round settles it
  for (const auto &under : behind)
    color = ensureContrast(color, under, minimum);
  return color;
}

} // namespace

CanvasTheme canvasThemeFor(const Tokens &tokens, const Look &look)
{
  const auto &c = tokens.colors;
  CanvasTheme t;

  t.background = c.surfaceAlt;
  t.fill = c.surface;
  t.fillThis = c.accentSoft;
  t.frame = seenOn(c.controlBorder, {t.background});
  t.showDots = !look.highContrast;
  t.dots = over(c.textSecondary, 0.32, t.background);

  t.border = seenOn(c.controlBorder, {t.background, t.fill});
  t.borderThis = seenOn(c.accent, {t.background, t.fillThis});
  t.text = c.text;
  t.textSecondary = c.textSecondary;
  t.mainBar = over(c.textSecondary, 0.55, t.fill);

  t.selected = seenOn(c.accent, {t.background, t.fill, t.fillThis});
  t.focus = seenOn(c.text, {t.background, t.fill, t.fillThis});

  t.link = seenOn(c.success, {t.background, t.fill, t.fillThis});
  t.linkEdge = t.background;

  t.error = seenOn(c.danger, {t.background, t.fill, t.fillThis});
  t.errorHatch = t.error;
  t.errorHatch.setAlphaF(0.30);

  t.connected = seenOn(c.success, {t.fill, t.fillThis});
  t.offline = seenOn(c.textSecondary, {t.fill, t.fillThis});

  t.guide = seenOn(c.accent, {t.background});

  t.bubble = c.text;
  t.bubbleText = c.surface;
  return t;
}

CanvasTheme currentCanvasTheme()
{
  return canvasThemeFor(currentTokens(), currentLook());
}

QPixmap paintCanvasBackdrop(const CanvasTheme &theme, const QSize &size, qreal dpr)
{
  QPixmap pixmap(QSizeF(size * dpr).toSize());
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  QPainterPath path;
  path.addRoundedRect(
      QRectF(QPointF(0, 0), QSizeF(size)).adjusted(0.5, 0.5, -0.5, -0.5), metrics::radiusControl, metrics::radiusControl
  );
  p.fillPath(path, theme.background);
  if (theme.showDots) {
    p.setClipPath(path);
    p.setPen(Qt::NoPen);
    p.setBrush(theme.dots);
    for (int y = 8; y < size.height(); y += 16) {
      for (int x = 8; x < size.width(); x += 16)
        p.drawEllipse(QPointF(x, y), 1.0, 1.0);
    }
    p.setClipping(false);
  }
  p.setPen(QPen(theme.frame, 1));
  p.setBrush(Qt::NoBrush);
  p.drawPath(path);
  return pixmap;
}

void paintCanvasFocus(QPainter *painter, const CanvasTheme &theme, const QSize &canvas, const QList<QRectF> &around)
{
  painter->save();
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(theme.focus, 2));
  if (around.isEmpty()) {
    painter->drawRoundedRect(
        QRectF(QPointF(0, 0), QSizeF(canvas)).adjusted(1, 1, -1, -1), metrics::radiusControl, metrics::radiusControl
    );
  } else {
    // the ring of the selection is 3 px wide, 1 px out: the ring of the keys leaves a gap of 1.5 px from it
    for (const auto &r : around)
      painter->drawRoundedRect(r.adjusted(-5, -5, 5, 5), 7, 7);
  }
  painter->restore();
}

} // namespace deskflow::gui::style
