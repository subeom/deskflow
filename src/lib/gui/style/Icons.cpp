/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Icons.h"

#include "Appearance.h"

#include <QDir>
#include <QFile>
#include <QIconEngine>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

// The resources of a static library are asked for by name, and the macro belongs outside of every namespace
static void initIconResources()
{
  Q_INIT_RESOURCE(phosphor);
}

namespace deskflow::gui::style {

namespace {

void ensureResources()
{
  static const bool once = (initIconResources(), true);
  Q_UNUSED(once)
}

QString weightDirectory(IconWeight weight)
{
  switch (weight) {
  case IconWeight::Duotone:
    return QStringLiteral("duotone");
  case IconWeight::Fill:
    return QStringLiteral("fill");
  case IconWeight::Regular:
    break;
  }
  return QStringLiteral("regular");
}

QString resourcePath(const QString &name, IconWeight weight)
{
  return QStringLiteral(":/icons/phosphor/%1/%2.svg").arg(weightDirectory(weight), name);
}

QSize squareOf(const QSize &size)
{
  const auto side = qMin(size.width(), size.height());
  return {side, side};
}

class IconEngine : public QIconEngine
{
public:
  IconEngine(const QString &name, IconColor color, IconWeight weight) : m_name(name), m_color(color), m_weight(weight)
  {
  }

  void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
  {
    const auto side = squareOf(rect.size());
    const QRect target(
        rect.x() + (rect.width() - side.width()) / 2, rect.y() + (rect.height() - side.height()) / 2, side.width(),
        side.height()
    );
    painter->drawPixmap(
        target,
        iconPixmap(
            m_name, m_weight, iconColor(m_color, mode, currentTokens()), side, painter->device()->devicePixelRatioF()
        )
    );
  }

  QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
  {
    return scaledPixmap(size, mode, state, 1.0);
  }

  QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State, qreal scale) override
  {
    return iconPixmap(m_name, m_weight, iconColor(m_color, mode, currentTokens()), size, scale);
  }

  QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override
  {
    return squareOf(size);
  }

  QIconEngine *clone() const override
  {
    return new IconEngine(m_name, m_color, m_weight);
  }

  QString key() const override
  {
    return QStringLiteral("deskflow-phosphor");
  }

  QString iconName() override
  {
    return m_name;
  }

  bool isNull() override
  {
    return !hasIcon(m_name, m_weight);
  }

private:
  QString m_name;
  IconColor m_color;
  IconWeight m_weight;
};

} // namespace

QStringList availableIcons(IconWeight weight)
{
  ensureResources();
  auto names = QDir(QStringLiteral(":/icons/phosphor/%1").arg(weightDirectory(weight)))
                   .entryList({QStringLiteral("*.svg")}, QDir::Files, QDir::Name);
  for (auto &name : names)
    name.chop(4);
  return names;
}

bool hasIcon(const QString &name, IconWeight weight)
{
  ensureResources();
  return !name.isEmpty() && QFile::exists(resourcePath(name, weight));
}

QByteArray coloredSvg(const QString &name, IconWeight weight, const QColor &color)
{
  if (!hasIcon(name, weight))
    return {};
  QFile file(resourcePath(name, weight));
  if (!file.open(QIODevice::ReadOnly))
    return {};
  // the icons draw in "currentColor": the colour of the text of whatever shows them
  return file.readAll().replace("currentColor", color.name(QColor::HexRgb).toLatin1());
}

QString iconCacheKey(const QString &name, IconWeight weight, const QColor &color, const QSize &size, qreal dpr)
{
  return QStringLiteral("deskflow/phosphor/%1/%2/%3/%4x%5@%6")
      .arg(weightDirectory(weight), name, color.name(QColor::HexArgb))
      .arg(size.width())
      .arg(size.height())
      .arg(dpr);
}

QPixmap iconPixmap(const QString &name, IconWeight weight, const QColor &color, const QSize &size, qreal dpr)
{
  const auto side = squareOf(size).width();
  if (side <= 0 || dpr <= 0)
    return {};

  const auto key = iconCacheKey(name, weight, color, size, dpr);
  QPixmap pixmap;
  if (QPixmapCache::find(key, &pixmap))
    return pixmap;

  const auto svg = coloredSvg(name, weight, color);
  if (svg.isEmpty())
    return {};
  QSvgRenderer renderer(svg);
  if (!renderer.isValid())
    return {};

  const auto pixels = qRound(side * dpr);
  pixmap = QPixmap(pixels, pixels);
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);
  {
    QPainter painter(&pixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    renderer.render(&painter, QRectF(0, 0, side, side));
  }
  QPixmapCache::insert(key, pixmap);
  return pixmap;
}

QColor iconColor(IconColor role, QIcon::Mode mode, const Tokens &tokens)
{
  const auto &c = tokens.colors;
  if (mode == QIcon::Disabled)
    return c.textDisabled;
  if (mode == QIcon::Selected)
    return c.onAccent;

  switch (role) {
  case IconColor::TextSecondary:
    return c.textSecondary;
  case IconColor::Accent:
    return c.accent;
  case IconColor::OnAccent:
    return c.onAccent;
  case IconColor::Success:
    return c.success;
  case IconColor::Warning:
    return c.warning;
  case IconColor::Danger:
    return c.danger;
  case IconColor::Text:
    break;
  }
  return c.text;
}

QIcon icon(const QString &name, IconColor color, IconWeight weight)
{
  if (!hasIcon(name, weight))
    return {};
  return QIcon(new IconEngine(name, color, weight));
}

} // namespace deskflow::gui::style
