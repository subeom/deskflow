/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "TrayIcons.h"

#include "StyleUtils.h"
#include "common/PlatformInfo.h"

#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QSettings>
#include <QSvgRenderer>

namespace deskflow::gui::tray {

namespace {

// The mark of the program, as it has always been the symbolic icon of the tray. It is a drawing of 64 units, and so are the
// shapes that are added to it
constexpr auto kMarkResource = ":/icons/deskflow-dark/apps/64/org.deskflow.deskflow-symbolic.svg";
constexpr qreal kUnits = 64.0;
// where the shape of a state sits (its centre), and where the dot of an update is
constexpr QPointF kBadgeCentre{49, 49};
constexpr QPointF kUpdateCentre{54, 10};

QPolygonF triangle(qreal radius)
{
  const auto cx = kBadgeCentre.x();
  const auto cy = kBadgeCentre.y();
  return QPolygonF({{cx, cy - radius}, {cx + radius * 1.12, cy + radius * 0.78}, {cx - radius * 1.12, cy + radius * 0.78}});
}

QPen roundPen(const QColor &color, qreal width)
{
  QPen pen(color, width);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  return pen;
}

// What is cleared from the mark around the shape of the state, so that the shape is not drawn over the waves
void knockOut(QPainter &painter, const TrayIconSpec &spec)
{
  painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::black);
  switch (spec.state) {
  case TrayState::Stopped:
    painter.setPen(roundPen(Qt::black, 11));
    painter.drawLine(QPointF(9, 55), QPointF(55, 9));
    break;
  case TrayState::Waiting:
  case TrayState::Connected:
    painter.drawEllipse(kBadgeCentre, 15, 15);
    break;
  case TrayState::Error:
    painter.setPen(roundPen(Qt::black, 4));
    painter.drawPolygon(triangle(17));
    break;
  case TrayState::Idle:
    break;
  }
  if (spec.update) {
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(kUpdateCentre, 11, 11);
  }
}

void drawBadge(QPainter &painter, const TrayIconSpec &spec)
{
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  const auto color = trayBadgeColor(spec);
  switch (spec.state) {
  case TrayState::Stopped:
    painter.setPen(roundPen(color, 5));
    painter.drawLine(QPointF(9, 55), QPointF(55, 9));
    break;
  case TrayState::Waiting:
    painter.setPen(QPen(color, 5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(kBadgeCentre, 9, 9);
    break;
  case TrayState::Connected:
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(kBadgeCentre, 9.5, 9.5);
    break;
  case TrayState::Error: {
    // a triangle with its exclamation mark cleared out of it
    QPainterPath shape;
    shape.addPolygon(triangle(12));
    shape.closeSubpath();
    QPainterPath mark;
    mark.addRoundedRect(QRectF(kBadgeCentre.x() - 1.7, kBadgeCentre.y() - 5, 3.4, 7.2), 1.2, 1.2);
    mark.addEllipse(QPointF(kBadgeCentre.x(), kBadgeCentre.y() + 6.4), 2, 2);
    painter.setPen(roundPen(color, 3));
    painter.setBrush(color);
    painter.drawPath(shape.subtracted(mark));
    painter.setPen(Qt::NoPen);
    break;
  }
  case TrayState::Idle:
    break;
  }
  if (spec.update) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(trayUpdateColor(spec));
    painter.drawEllipse(kUpdateCentre, 7, 7);
  }
}

QImage render(const TrayIconSpec &spec, int pixels)
{
  QImage image(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  if (pixels <= 0)
    return image;

  QPainter painter(&image);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
  painter.scale(pixels / kUnits, pixels / kUnits);

  // the mark, in the colour of the tone: the drawing is rendered in any colour and given the one that is wanted
  QImage mark(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
  mark.fill(Qt::transparent);
  {
    QSvgRenderer renderer{QString::fromLatin1(kMarkResource)};
    QPainter markPainter(&mark);
    markPainter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    if (renderer.isValid())
      renderer.render(&markPainter, QRectF(0, 0, pixels, pixels));
    markPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    markPainter.fillRect(mark.rect(), trayMarkColor(spec.tone));
  }
  painter.resetTransform();
  painter.drawImage(0, 0, mark);

  painter.scale(pixels / kUnits, pixels / kUnits);
  knockOut(painter, spec);
  drawBadge(painter, spec);
  return image;
}

class TrayIconEngine : public QIconEngine
{
public:
  explicit TrayIconEngine(const TrayIconSpec &spec) : m_spec(spec)
  {
  }

  void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
  {
    const auto scale = painter->device()->devicePixelRatioF();
    const auto side = qMin(rect.width(), rect.height());
    const QRect target(
        rect.x() + (rect.width() - side) / 2, rect.y() + (rect.height() - side) / 2, side, side
    );
    painter->drawPixmap(target, scaledPixmap(QSize(side, side), mode, state, scale));
  }

  QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
  {
    return scaledPixmap(size, mode, state, 1.0);
  }

  QPixmap scaledPixmap(const QSize &size, QIcon::Mode, QIcon::State, qreal scale) override
  {
    const auto side = qMin(size.width(), size.height());
    const auto pixels = qRound(side * scale);
    if (pixels <= 0)
      return {};
    const auto key = QStringLiteral("deskflow/tray/%1/%2/%3/%4/%5/%6")
                         .arg(trayStateName(m_spec.state))
                         .arg(m_spec.update)
                         .arg(static_cast<int>(m_spec.tone))
                         .arg(m_spec.coloredBadge)
                         .arg(pixels)
                         .arg(scale);
    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
      pixmap = QPixmap::fromImage(render(m_spec, pixels));
      pixmap.setDevicePixelRatio(scale);
      QPixmapCache::insert(key, pixmap);
    }
    return pixmap;
  }

  QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override
  {
    const auto side = qMin(size.width(), size.height());
    return {side, side};
  }

  QList<QSize> availableSizes(QIcon::Mode, QIcon::State) override
  {
    // what the trays ask for: the small icon of Windows at each scale, the 22 of the panels of Linux, the largest that
    // a D-Bus tray sends
    QList<QSize> sizes;
    for (const int side : {16, 20, 22, 24, 32, 48, 64})
      sizes.append({side, side});
    return sizes;
  }

  QIconEngine *clone() const override
  {
    return new TrayIconEngine(m_spec);
  }

  QString key() const override
  {
    return QStringLiteral("deskflow-tray");
  }

  QString iconName() override
  {
    return {};
  }

private:
  TrayIconSpec m_spec;
};

} // namespace

QImage trayIconImage(const TrayIconSpec &spec, int pixels)
{
  return render(spec, pixels);
}

QIcon trayIcon(const TrayIconSpec &spec)
{
  QIcon icon(new TrayIconEngine(spec));
  // macOS tints a template: black and clear is what it wants, and the shape alone tells the state
  icon.setIsMask(spec.tone == TrayTone::Template);
  return icon;
}

QString trayIconName(const TrayIconSpec &spec)
{
  QString name = QStringLiteral("org.deskflow.deskflow");
  if (spec.state != TrayState::Idle)
    name += QLatin1Char('-') + trayStateName(spec.state);
  if (spec.update)
    name += QStringLiteral("-update");
  return name + QStringLiteral("-symbolic");
}

QColor trayMarkColor(TrayTone tone)
{
  switch (tone) {
  case TrayTone::Template:
    return QColor(Qt::black);
  case TrayTone::OnLightBar:
    return QColor(0x1b, 0x1c, 0x1f);
  case TrayTone::OnDarkBar:
    return QColor(0xf2, 0xf3, 0xf5);
  }
  return QColor(Qt::black);
}

QColor trayBadgeColor(const TrayIconSpec &spec)
{
  if (!spec.coloredBadge || spec.tone == TrayTone::Template)
    return trayMarkColor(spec.tone);
  const bool light = spec.tone == TrayTone::OnLightBar;
  switch (spec.state) {
  case TrayState::Connected:
    return light ? QColor(0x1a, 0x7f, 0x37) : QColor(0x3f, 0xb9, 0x50);
  case TrayState::Waiting:
    return light ? QColor(0x09, 0x69, 0xda) : QColor(0x58, 0xa6, 0xff);
  case TrayState::Error:
    return light ? QColor(0xcf, 0x22, 0x2e) : QColor(0xff, 0x7b, 0x72);
  case TrayState::Stopped:
  case TrayState::Idle:
    break;
  }
  return trayMarkColor(spec.tone);
}

QColor trayUpdateColor(const TrayIconSpec &spec)
{
  if (!spec.coloredBadge || spec.tone == TrayTone::Template)
    return trayMarkColor(spec.tone);
  return spec.tone == TrayTone::OnLightBar ? QColor(0x09, 0x69, 0xda) : QColor(0x58, 0xa6, 0xff);
}

TrayTone detectTrayTone()
{
  if (deskflow::platform::isMac())
    return TrayTone::Template;
  if (deskflow::platform::isWindows()) {
    // Windows does not recolour the icons of the notification area: the one that suits the theme of the taskbar
    QSettings settings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat
    );
    return settings.value(QStringLiteral("SystemUsesLightTheme"), 1).toBool() ? TrayTone::OnLightBar
                                                                              : TrayTone::OnDarkBar;
  }
  return deskflow::gui::isDarkMode() ? TrayTone::OnDarkBar : TrayTone::OnLightBar;
}

} // namespace deskflow::gui::tray
