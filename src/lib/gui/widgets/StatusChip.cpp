/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StatusChip.h"

#include "gui/style/AccessibleInterfaces.h"
#include "gui/style/Appearance.h"
#include "gui/style/Contrast.h"
#include "gui/style/FontPolicy.h"
#include "gui/style/FontScale.h"
#include "gui/style/Icons.h"
#include "gui/style/LookFollower.h"
#include "gui/style/PixelSnap.h"
#include "gui/style/TextSize.h"

#include <QApplication>
#include <QPainter>

using namespace deskflow::gui::style;

namespace {

constexpr int kHeight = 24;
constexpr int kPadding = 9;
constexpr int kIcon = 14;
constexpr int kGap = 5;

// the height and the icon follow the size of text
int chipHeight()
{
  return px(kHeight);
}

int iconSize()
{
  return px(kIcon);
}
// what a text of 12 px needs on its fill (WCAG AA for small text)
constexpr double kTextContrast = 4.5;

QFont chipFont(const QWidget *widget)
{
  auto font = scaleFont(QApplication::font(widget), qreal(metrics::fontCaption) / metrics::fontBody);
  font.setWeight(fontWeight(QFont::DemiBold));
  return font;
}

} // namespace

StatusChip::StatusChip(QWidget *parent) : QWidget(parent)
{
  installAccessibleInterfaces();
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  new LookFollower(this, [this] { update(); });
}

StatusChip::StatusChip(const QString &text, Kind kind, QWidget *parent) : StatusChip(parent)
{
  m_text = text;
  m_kind = kind;
  updateAccessibleName();
}

QString StatusChip::text() const
{
  return m_text;
}

void StatusChip::setText(const QString &text)
{
  if (m_text == text)
    return;
  m_text = text;
  updateAccessibleName();
  updateGeometry();
  update();
}

StatusChip::Kind StatusChip::kind() const
{
  return m_kind;
}

void StatusChip::setKind(Kind kind)
{
  if (m_kind == kind)
    return;
  m_kind = kind;
  updateAccessibleName();
  updateGeometry();
  update();
}

QString StatusChip::iconName(Kind kind)
{
  switch (kind) {
  case Kind::Info:
    return QStringLiteral("info");
  case Kind::Success:
    return QStringLiteral("check-circle");
  case Kind::Warning:
    return QStringLiteral("warning");
  case Kind::Danger:
    return QStringLiteral("x-circle");
  case Kind::Neutral:
    break;
  }
  return {};
}

QString StatusChip::kindWord(Kind kind)
{
  switch (kind) {
  case Kind::Info:
    return tr("Info");
  case Kind::Success:
    return tr("OK");
  case Kind::Warning:
    return tr("Warning");
  case Kind::Danger:
    return tr("Error");
  case Kind::Neutral:
    break;
  }
  return {};
}

StatusChip::Palette StatusChip::paletteFor(Kind kind, const Tokens &tokens)
{
  const auto &c = tokens.colors;
  QColor fill = c.surfaceAlt;
  QColor strong = c.textSecondary;
  switch (kind) {
  case Kind::Info:
    fill = c.accentSoft;
    strong = c.accent;
    break;
  case Kind::Success:
    fill = c.successSoft;
    strong = c.success;
    break;
  case Kind::Warning:
    fill = c.warningSoft;
    strong = c.warning;
    break;
  case Kind::Danger:
    fill = c.dangerSoft;
    strong = c.danger;
    break;
  case Kind::Neutral:
    break;
  }
  // the fills are opaque; one that is not (surfaceAlt) is read over the surface it lies on
  const auto seen = fill.alpha() == 255 ? fill : over(fill, fill.alphaF(), c.surface);
  return {fill, ensureContrast(strong, seen, kTextContrast)};
}

void StatusChip::updateAccessibleName()
{
  const auto word = kindWord(m_kind);
  setAccessibleName(word.isEmpty() ? m_text : tr("%1: %2").arg(word, m_text));
}

QSize StatusChip::sizeHint() const
{
  const auto textWidth = QFontMetrics(chipFont(this)).horizontalAdvance(m_text);
  const auto iconWidth = iconName(m_kind).isEmpty() ? 0 : iconSize() + kGap;
  return QSize(2 * kPadding + iconWidth + textWidth, chipHeight());
}

QSize StatusChip::minimumSizeHint() const
{
  return sizeHint();
}

void StatusChip::paintEvent(QPaintEvent *)
{
  const auto tokens = currentTokens();
  const auto colors = paletteFor(m_kind, tokens);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const auto dpr = painter.device()->devicePixelRatioF();

  const auto line = deviceLineWidth(1, dpr);
  const QRectF body = snapRect(QRectF(rect()), dpr).adjusted(line / 2, line / 2, -line / 2, -line / 2);
  // the edge is the colour of the state; at the colours of the OS (no soft fill) it is what sets the chip off
  const auto edge = over(colors.text, tokens.colors.surface == colors.fill ? 0.6 : 0.18, colors.fill);
  painter.setPen(QPen(edge, line));
  painter.setBrush(colors.fill);
  painter.drawRoundedRect(body, body.height() / 2, body.height() / 2);

  int x = kPadding;
  const auto name = iconName(m_kind);
  if (!name.isEmpty()) {
    const auto pixmap = iconPixmap(name, IconWeight::Fill, colors.text, QSize(iconSize(), iconSize()), dpr);
    painter.drawPixmap(QRect(x, (height() - iconSize()) / 2, iconSize(), iconSize()), pixmap);
    x += iconSize() + kGap;
  }
  painter.setFont(chipFont(this));
  painter.setPen(colors.text);
  painter.drawText(QRect(x, 0, width() - x - kPadding, height()), Qt::AlignVCenter | Qt::AlignLeft, m_text);
}
